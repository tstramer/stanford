/* rlib version 5 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include "rlib.h"

char *progname;
int opt_debug;

int opt_drop = 0;
int opt_corrupt = 0;
int opt_delay = 0;
int opt_duplicate = 0;

int log_in = -1;
int log_out = -1;

struct config_client {
  struct config_common c;
  int listen_socket; 		/* Accept TCP connections on this socket */
  struct sockaddr_storage server; /* Tunnel them to this server over UDP */
};

struct config_server {
  struct config_common c;
  int udp_socket;		/* Receive all UDP over this socket */
  struct sockaddr_storage dest;	/* Demultiplex traffic and relay it to
				   individual TCP connections to this
				   address */
};

static struct config_server *serverconf;

static void conn_mkevents (void);
static int debug_recv (int s, packet_t *buf, size_t len, int flags,
		       struct sockaddr_storage *from);

int cevents_generation;
static struct pollfd *cevents;
static int ncevents;
static conn_t **evreaders;
static conn_t **evwriters;

struct chunk {
  struct chunk *next;
  size_t size;
  size_t used;
  char buf[1];
};
typedef struct chunk chunk_t;

struct conn {
  rel_t *rel;			/* Data from reliable */

  int rpoll;			/* offsets into cevents array */
  int wpoll;
  int npoll;

  int rfd;			/* input file descriptor */
  int wfd;			/* output file descriptor */
  int nfd;			/* network file descriptor */
  char server;			/* non-zero on server */
  struct sockaddr_storage peer;	/* network peer */

  char read_eof;	        /* zero if haven't received EOF */
  char write_eof;		/* send EOF when output queue drained */
  char write_err;	        /* zero if it's okay to write to wfd */
  char xoff;			/* non-zero to pause reading */
  char delete_me;		/* delete after draining */
  chunk_t *outq;		/* chunks not yet written */
  chunk_t **outqtail;

  struct conn *next;		/* Linked list of connections */
  struct conn **prev;
};

static conn_t *conn_list;
struct timespec last_timeout;

#if !DMALLOC
void *
xmalloc (size_t n)
{
  void *p = malloc (n);
  if (!p) {
    fprintf (stderr, "%s: out of memory allocating %d bytes\n",
	     progname, (int) n);
    abort ();
  }
  return p;
}
#endif /* !DMALLOC */

#if NEED_CLOCK_GETTIME
int
clock_gettime (int id, struct timespec *tp)
{
  struct timeval tv;

  switch (id) {
  case CLOCK_REALTIME:
  case CLOCK_MONOTONIC:		/* XXX */
    if (gettimeofday (&tv, NULL) < 0)
      return -1;
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000;
    return 0;
  default:
    errno = EINVAL;
    return -1;
  }
}
#endif /* NEED_CLOCK_GETTIME */

void
print_pkt (const packet_t *buf, const char *op, int n)
{
  static int pid = -1;
  int saved_errno = errno;
  if (pid == -1)
    pid = getpid ();
  if (n < 0) {
    if (errno != EAGAIN)
      fprintf (stderr, "%5d %s(%3d): %s\n", pid, op, n, strerror (errno));
  }
  else if (n == 8)
    fprintf (stderr, "%5d %s(%3d): cksum = %04x, len = %04x, ack = %08x\n",
	     pid, op, n, buf->cksum, ntohs (buf->len), ntohl (buf->ackno));
  else if (n >= 12)
    fprintf (stderr,
	     "%5d %s(%3d): cksum = %04x, len = %04x, ack = %08x, seq = %08x\n",
	     pid, op, n, buf->cksum, ntohs (buf->len), ntohl (buf->ackno),
	     ntohl (buf->seqno));
  else
    fprintf (stderr, "%5d %s(%3d):\n", pid, op, n);
  errno = saved_errno;
}

void flipbit(const void *data_, size_t bit) {
  char *data = ((char *) data_);
  unsigned int mask = 1 << (bit % 8);
  data[bit/8] ^= mask;
}

int rand_percent(int fork_level)
{
  #define SALT (0x12345)
  return (rand() + (fork_level * SALT)) % 100;
}

int
conn_sendpkt (conn_t *c, const packet_t *pkt, size_t len)
{
  int n;
  assert (!c->delete_me);

  int fork_level = 0;
  bool am_i_forked = false;

  /* packet drop */
  if (rand_percent(fork_level) < opt_drop) {
    if (opt_debug)
      print_pkt(pkt, "dropping: ", len);
    return len;
  }

  /* packet duplication */
  if (rand_percent(fork_level) < opt_duplicate) {
    if (opt_debug)
      print_pkt(pkt, "duplicating: ", len);

    if (fork() == 0) {
      am_i_forked = true;
      fork_level++;
    }
  }

  /* packet delay */
  bool do_delay = rand_percent(fork_level) < opt_delay;
  if (do_delay) {
    if (opt_debug)
      print_pkt(pkt,"delaying: ", len);

    if (fork() == 0) {
      am_i_forked = true;
      fork_level++;
      sleep(rand() % 5);
    } else if (am_i_forked) {
      exit(0);
    } else {
      return len;
    }
  }

  /* packet corruption */
  bool do_corrupt = rand_percent(fork_level) < opt_corrupt;
  int bit = (rand() + getpid())  % (len * 8 - 1);
  if (do_corrupt) {
    if (opt_debug)
      print_pkt(pkt, "corrupting: ", len);
    flipbit(pkt,bit);
  }

  if (c->server)
    n = sendto (c->nfd, pkt, len, 0,
    (const struct sockaddr *) &c->peer, addrsize (&c->peer));
  else
    n = send (c->nfd, pkt, len, 0);
  if (opt_debug)
    print_pkt (pkt, "send", n);

  /* corruption cleanup */
  if (do_corrupt)
    flipbit(pkt, bit); // undo changes.

  /* cleanup child processes */
  if (am_i_forked)
    exit(0);

  return n;
}

size_t
conn_bufspace (conn_t *c)
{
  chunk_t *ch;
  size_t used = 0;
  const size_t bufsize = 8192;


  for (ch = c->outq; ch; ch = ch->next)
    used += (ch->size - ch->used);
  return used > bufsize ? 0 : bufsize - used;
}

int
conn_output (conn_t *c, const void *_buf, size_t _n)
{
  const char *buf = _buf;
  int n = _n;

  assert (!c->delete_me && !c->write_eof);

  if (n == 0) {
    c->write_eof = 1;
    if (!c->outq)
      shutdown (c->wfd, SHUT_WR);
    return 0;
  }

  if (c->write_err) {
    if (c->write_err == 2)
      fprintf (stderr, "conn_output: attempt to write after error\n");
    c->write_err = 2;
    return -1;
  }

  if (!conn_bufspace (c))
    return 0;

  if (log_out >= 0)
    write (log_out, buf, n);

  if (!c->outq) {
    int r = write (c->wfd, buf, n);
    if (r < 0) {
      if (errno != EAGAIN) {
	perror ("write");
	c->write_err = 2;
	return -1;
      }
    }
    else {
      buf += r;
      n -= r;
    }
  }

  if (n > 0) {
    chunk_t *ch = xmalloc (offsetof (chunk_t, buf[n]));
    ch->next = NULL;
    ch->size = n;
    ch->used = 0;
    memcpy (ch->buf, buf, n);
    *c->outqtail = ch;
    c->outqtail = &ch->next;
  }

  if (c->wpoll && c->outq)
    cevents[c->wpoll].events |= POLLOUT;
  return _n;
}

int
conn_input (conn_t *c, void *buf, size_t n)
{
  int r;
  assert (!c->delete_me);

  if (c->read_eof)
    return -1;
  r = read (c->rfd, buf, n);
  if (r == 0 || (r < 0 && errno != EAGAIN)) {
    if (r == 0)
      errno = EIO;
    r = -1;
    c->read_eof = 1;
    return r;
  }
  if (r < 0 && errno == EAGAIN)
    r = 0;

  if (r > 0 && log_in >= 0)
    write (log_in, buf, r);

  c->xoff = 0;
  cevents[c->rpoll].events |= POLLIN;
  return r;
}

static conn_t *
conn_alloc (void)
{
  conn_t *c = xmalloc (sizeof (*c));
  memset (c, 0, sizeof (*c));
  c->prev = &conn_list;
  c->next = conn_list;
  c->outqtail = &c->outq;
  if (conn_list)
    conn_list->prev = &c->next;
  conn_list = c;

  cevents_generation++;

  return c;
}

conn_t *
conn_create (rel_t *rel, const struct sockaddr_storage *ss)
{
  int n;
  conn_t *c;

  /* conn_create is only when the program is running as a server (and
   * rel_recvpkt is called with NULL packets.  If you call conn_create
   * in the client, you will see this assertion fail. */
  assert (serverconf);

  if ((n = connect_to (0, &serverconf->dest)) < 0) {
    char addr[NI_MAXHOST] = "unknown";
    char port[NI_MAXSERV] = "unknown";
    int saved_errno = errno;
    getnameinfo ((const struct sockaddr *) &serverconf->dest,
		 sizeof (serverconf->dest),
		 addr, sizeof (addr), port, sizeof (port),
		 NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    fprintf (stderr, "%s:%s: connect: %s\n",
	     addr, port, strerror (saved_errno));
    return NULL;
  }

  c = conn_alloc ();
  c->peer = *ss;
  c->rel = rel;
  c->nfd = serverconf->udp_socket;
  c->rfd = c->wfd = n;
  c->server = 1;

  return c;
}

static void
conn_free (conn_t *c)
{
  chunk_t *ch, *nch;

  for (ch = c->outq; ch; ch = nch) {
    nch = ch->next;
    free (ch);
  }

  if (c->next)
    c->next->prev = c->prev;
  *c->prev = c->next;

  close (c->rfd);
  if (c->wfd != c->rfd)
    close (c->wfd);
  if (!c->server)
    close (c->nfd);

  cevents_generation++;

  /* to help catch errors */
  memset (c, 0xc5, sizeof (*c));
  free (c);
}

void
conn_destroy (conn_t *c)
{
  c->delete_me = 1;
}

void
conn_drain (conn_t *c)
{
  chunk_t *ch;
  int didsome = 0;

  if (c->wpoll)
    cevents[c->wpoll].events &= ~POLLOUT;

  if (c->write_err)
    return;

  while ((ch = c->outq)) {
    int n = write (c->wfd, ch->buf + ch->used,
		   ch->size - ch->used);
    if (n < 0) {
      if (errno != EAGAIN)
	c->write_err = 1;
      break;
    }
    didsome = 1;
    ch->used += n;
    if (ch->used < ch->size) {
      if (c->wpoll)
	cevents[c->wpoll].events |= POLLOUT;
      break;
    }
    c->outq = ch->next;
    if (!c->outq)
      c->outqtail = &c->outq;
    free (ch);
  }
  if (c->write_eof && !c->write_err && !c->outq) {
    c->write_err = 1;
    shutdown (c->wfd, SHUT_WR);
  }
  if (didsome && !c->delete_me)
    rel_output (c->rel);
}

static void
conn_mkevents (void)
{
  struct pollfd *e;
  conn_t **r, **w;
  size_t n = 2;
  conn_t *c;

  for (c = conn_list; c; c = c->next) {
    if (c->read_eof) {
      c->rpoll = 0;
      if (c->write_err)
	c->wpoll = 0;
      else
	c->wpoll = n++;
    }
    else {
      c->rpoll = n++;
      if (c->write_err)
	c->wpoll = 0;
      else if (c->wfd == c->rfd)
	c->wpoll = c->rpoll;
      else
	c->wpoll = n++;
    }
    if (c->server)
      c->npoll = 0;
    else
      c->npoll = n++;
  }

  e = xmalloc (n * sizeof (*e));
  memset (e, 0, n * sizeof (*e));
  if (cevents)
    e[0] = cevents[0];
  else
    e[0].fd = -1;
  e[1].fd = 2;			/* Do catch errors on stderr */

  for (c = conn_list; c; c = c->next) {
    if (c->rpoll) {
      e[c->rpoll].fd = c->rfd;
      if (!c->xoff)
	e[c->rpoll].events |= POLLIN;
    }
    if (c->wpoll) {
      e[c->wpoll].fd = c->wfd;
      if (c->outq)
	e[c->wpoll].events |= POLLOUT;
    }
    if (c->npoll) {
      e[c->npoll].fd = c->nfd;
      e[c->npoll].events |= POLLIN;
    }
  }

  r = xmalloc (n * sizeof (*r));
  memset (r, 0, n * sizeof (*r));
  w = xmalloc (n * sizeof (*w));
  memset (w, 0, n * sizeof (*w));
  for (c = conn_list; c; c = c->next) {
    if (c->rpoll > 0)
      r[c->rpoll] = c;
    if (c->npoll > 0)
      r[c->npoll] = c;
    if (c->wpoll > 0)
      w[c->wpoll] = c;
  }

  free (cevents);
  cevents = e;
  ncevents = n;
  free (evreaders);
  evreaders = r;
  free (evwriters);
  evwriters = w;
}

static void
conn_demux (const struct config_server *cs)
{
  packet_t pkt;
  struct sockaddr_storage ss;
  int n;

  memset (&ss, 0, sizeof (ss));
  while ((n = debug_recv (cs->udp_socket, &pkt, sizeof (pkt), 0, &ss)) >= 0) {
    rel_demux (&cs->c, &ss, &pkt, n);
    memset (&pkt, 0xc7, n);	     /* to help debugging */
    memset (&ss, 0x7c, sizeof (ss)); /* to help debugging */
  }
  if (errno != EAGAIN)
    perror ("UDP recv");
}

long
need_timer_in (const struct timespec *last, long timer)
{
  long to;
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);
  to = ts.tv_sec - last->tv_sec;
  if (to > timer / 1000)
    return 0;
  to = to * 1000 + (ts.tv_nsec - last->tv_nsec) / 1000000;
  if (to >= timer)
    return 0;
  return
    timer - to;
}

void
conn_poll (const struct config_common *cc)
{
  //int n, i;
  int  i;
  conn_t *c, *nc;
  static int last_cg;

  if (last_cg != cevents_generation) {
    conn_mkevents ();
    cevents_generation = last_cg;
  }

  if (cevents[0].fd >= 0)
    poll (cevents, ncevents, need_timer_in (&last_timeout, cc->timer));
  else
    poll (cevents+1, ncevents-1, need_timer_in (&last_timeout, cc->timer));

  for (i = 1; i < ncevents; i++) {
    if (cevents[i].revents & (POLLIN|POLLERR|POLLHUP)) {
      if ((c = evreaders[i]) && !c->delete_me) {
	if (cevents[i].fd == c->rfd) {
	  c->xoff = 1;
	  cevents[i].events &= ~POLLIN;
	  rel_read (c->rel);
	}
	else if (cevents[i].fd == c->nfd
		 && (cevents[i].revents & (POLLERR|POLLHUP))) {
	  char addr[NI_MAXHOST] = "unknown";
	  char port[NI_MAXSERV] = "unknown";
	  getnameinfo ((const struct sockaddr *) &c->peer, sizeof (c->peer),
		       addr, sizeof (addr), port, sizeof (port),
		       NI_DGRAM | NI_NUMERICHOST|NI_NUMERICSERV);
	  fprintf (stderr, "[received ICMP port unreachable;"
		   " assuming peer at %s:%s is dead]\n", addr, port);
	  if (cc->single_connection)
	    exit (1);
	  rel_destroy (c->rel);
	}
	else if (cevents[i].fd == c->nfd && !c->server) {
	  packet_t pkt;
	  int len = debug_recv (c->nfd, &pkt, sizeof (pkt), 0, NULL);
	  if (len < 0) {
	    if (errno != EAGAIN)
	      perror ("recv");
	  }
	  else {
	    rel_recvpkt (c->rel, &pkt, len);
	    memset (&pkt, 0xc9, len); /* for debugging */
	  }
	}
      }
    }
    if ((cevents[i].revents & (POLLOUT|POLLHUP|POLLERR))
	&& evwriters[i])
      conn_drain (evwriters[i]);
    if (cevents[i].revents & (POLLHUP|POLLERR)) {
#if 0
      fprintf (stderr, "%5d Error on fd %d (0x%x)\n",
	       getpid (), cevents[i].fd, cevents[i].revents);
#endif
      /* If stderr has an error, the tester has probably died, so exit
       * immediately. */
      if (cevents[i].fd == 2)
	exit (1);
      cevents[i].fd = -1;
    }
    cevents[i].revents = 0;
  }

  if (need_timer_in (&last_timeout, cc->timer) == 0) {
    rel_timer ();
    clock_gettime (CLOCK_MONOTONIC, &last_timeout);
  }

  for (c = conn_list; c; c = nc) {
    nc = c->next;
    if (c->delete_me && (c->write_err || !c->outq))
      conn_free (c);
  }
}

uint16_t cksum (const void *_data, int len)
{
  const uint8_t *data = _data;
  uint32_t sum;

  for (sum = 0;len >= 2; data += 2, len -= 2)
    sum += data[0] << 8 | data[1];
  if (len > 0)
    sum += data[0] << 8;
  while (sum > 0xffff)
    sum = (sum >> 16) + (sum & 0xffff);
  sum = htons (~sum);
  return sum ? sum : 0xffff;
}

int
make_async (int s)
{
  int n;
  if ((n = fcntl (s, F_GETFL)) < 0
      || fcntl (s, F_SETFL, n | O_NONBLOCK) < 0)
    return -1;
  return 0;
}

int
addreq (const struct sockaddr_storage *a, const struct sockaddr_storage *b)
{
  if (a->ss_family != b->ss_family)
    return 0;
  switch (a->ss_family) {
  case AF_INET:
    {
      const struct sockaddr_in *aa = (const struct sockaddr_in *) a;
      const struct sockaddr_in *bb = (const struct sockaddr_in *) b;
      return (aa->sin_addr.s_addr == bb->sin_addr.s_addr
	      && aa->sin_port == bb->sin_port);
    }
  case AF_INET6:
    {
      const struct sockaddr_in6 *aa = (const struct sockaddr_in6 *) a;
      const struct sockaddr_in6 *bb = (const struct sockaddr_in6 *) b;
      return (!memcmp (&aa->sin6_addr, &bb->sin6_addr, sizeof (aa->sin6_addr))
	      && aa->sin6_port == bb->sin6_port);
    }
  case AF_UNIX:
    {
      const struct sockaddr_un *aa = (const struct sockaddr_un *) a;
      const struct sockaddr_un *bb = (const struct sockaddr_un *) b;
      return !strcmp (aa->sun_path, bb->sun_path);
    }
  }
  fprintf (stderr, "addrhash: unknown address family %d\n",
	   a->ss_family);
  abort ();
}

size_t
addrsize (const struct sockaddr_storage *ss)
{
  switch (ss->ss_family) {
  case AF_INET:
    return sizeof (struct sockaddr_in);
  case AF_INET6:
    return sizeof (struct sockaddr_in6);
  case AF_UNIX:
    return sizeof (struct sockaddr_un);
  }
  fprintf (stderr, "addrsize: unknown address family %d\n",
	   ss->ss_family);
  abort ();
}

static inline unsigned int
hash_bytes (const void *_key, int len, unsigned int seed)
{
  const unsigned char *key = (const unsigned char *) _key;
  const unsigned char *end;

  for (end = key + len; key < end; key++)
    seed = ((seed << 5) + seed) ^ *key;
  return seed;
}
unsigned int
addrhash (const struct sockaddr_storage *ss)
{
  unsigned int r = 5381;
  switch (ss->ss_family) {
  case AF_INET:
    {
      const struct sockaddr_in *s = (const struct sockaddr_in *) ss;
      r = hash_bytes (&s->sin_port, 2, r);
      return hash_bytes (&s->sin_addr, 4, r);
    }
  case AF_INET6:
    {
      const struct sockaddr_in6 *s = (const struct sockaddr_in6 *) ss;
      r = hash_bytes (&s->sin6_port, 2, r);
      return hash_bytes (&s->sin6_addr, 16, r);
    }
  case AF_UNIX:
    {
      const struct sockaddr_un *s = (const struct sockaddr_un *) ss;
      return hash_bytes (s->sun_path, strlen (s->sun_path), r);
    }
  }
  fprintf (stderr, "addrhash: unknown address family %d\n",
	   ss->ss_family);
  abort ();
}

int
get_address (struct sockaddr_storage *ss, int local,
	     int dgram, int family, char *name)
{
  struct addrinfo hints;
  struct addrinfo *ai;
  int err;
  char *host, *port;

  memset (ss, 0, sizeof (*ss));

  if (family == AF_UNIX) {
    size_t len = strlen (name);
    struct sockaddr_un *sun = (struct sockaddr_un *) ss;
    if (offsetof (struct sockaddr_un, sun_path[len])
	>= sizeof (struct sockaddr_storage)) {
      fprintf (stderr, "%s: name too long\n", name);
      return -1;
    }
    sun->sun_family = AF_UNIX;
    strcpy (sun->sun_path, name);
    return 0;
  }

  assert (family == AF_UNSPEC || family == AF_INET || family || AF_INET6);

  if (name) {
    host = strsep (&name, ":");
    port = strsep (&name, ":");
    if (!port) {
      port = host;
      host = NULL;
    }
  }
  else {
    host = NULL;
    port = "0";
  }

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = family;
  hints.ai_socktype = dgram ? SOCK_DGRAM : SOCK_STREAM;

  if (local)
    hints.ai_flags = AI_PASSIVE; /* passive means for local address */
  err = getaddrinfo (host, port, &hints, &ai);
  if (err) {
    if (local)
      fprintf (stderr, "local port %s: %s\n", port, gai_strerror (err));
    else
      fprintf (stderr, "%s:%s: %s\n", host ? host : "localhost",
	       port, gai_strerror (err));
    return -1;
  }

  assert (ai->ai_addrlen <= sizeof (*ss));
  memcpy (ss, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo (ai);
  return 0;
}

int
listen_on (int dgram, struct sockaddr_storage *ss)
{
  int type = dgram ? SOCK_DGRAM : SOCK_STREAM;
  int s = socket (ss->ss_family, type, 0);
  int n = 1;
  socklen_t len;
  int err;
  char portname[NI_MAXSERV];

  if (s < 0) {
    perror ("socket");
    return -1;
  }
  if (!dgram)
    setsockopt (s, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof (n));
  if (bind (s, (const struct sockaddr *) ss, addrsize (ss)) < 0) {
    perror ("bind");
    close (s);
    return -1;
  }
  if (!dgram && listen (s, 5) < 0) {
    perror ("listen");
    close (s);
    return -1;
  }

  if (ss->ss_family == AF_UNIX) {
    fprintf (stderr, "[listening on %s]\n",
	     ((struct sockaddr_un *) ss)->sun_path);
    return s;
  }

  /* If bound port 0, kernel selectec port, so we need to read it back. */
  len = sizeof (*ss);
  if (getsockname (s, (struct sockaddr *) ss, &len) < 0) {
    perror ("getsockname");
    close (s);
    return -1;
  }
  err = getnameinfo ((struct sockaddr *) ss, len, NULL, 0,
		     portname, sizeof (portname),
		     (dgram ? NI_DGRAM : 0) | NI_NUMERICSERV);
  if (err) {
    fprintf (stderr, "%s\n", gai_strerror (err));
    close (s);
    return -1;
  }

  fprintf (stderr, "[listening on %s port %s]\n",
	   dgram ? "UDP" : "TCP", portname);
  return s;
}

int
connect_to (int dgram, const struct sockaddr_storage *ss)
{
  int type = dgram ? SOCK_DGRAM : SOCK_STREAM;
  int s = socket (ss->ss_family, type, 0);
  if (s < 0) {
    perror ("socket");
    return -1;
  }
  make_async (s);
  if (connect (s, (struct sockaddr *) ss, addrsize (ss)) < 0
      && errno != EINPROGRESS) {
    perror ("connect");
    close (s);
    return -1;
  }

  return s;
}

static int
debug_recv (int s, packet_t *buf, size_t len, int flags,
	    struct sockaddr_storage *from)
{
  socklen_t socklen = sizeof (*from);
  int n;
  if (from)
    n = recvfrom (s, buf, len, flags, (struct sockaddr *) from, &socklen);
  else
    n = recv (s, buf, len, flags);
  if (opt_debug)
    print_pkt (buf, "recv", n);
  return n;
}


void
do_client (struct config_client *cc)
{
  conn_mkevents ();
  make_async (cc->listen_socket);
  cevents[0].fd = cc->listen_socket;
  cevents[0].events = POLLIN;
  for (;;) {
    conn_poll (&cc->c);
    if (cevents[0].revents) {
      struct sockaddr_storage ss;
      socklen_t len = sizeof (ss);
      int s, u;
      conn_t *c;
      errno = EINVAL;
      s = accept (cc->listen_socket, (struct sockaddr *) &ss, &len);
      if (s < 0 && errno != EAGAIN)
	perror ("accept");
      if (s < 0)
	continue;
      make_async (s);
      if ((u = connect_to (1, &cc->server)) >= 0) {
	c = conn_alloc ();
	c->rfd = s;
	c->wfd = s;
	c->nfd = u;
	c->peer = cc->server;
	c->rel = rel_create (c, NULL, &cc->c);
	conn_mkevents ();
      }
      else
	close (s);
    }
  }
}

void
do_server (struct config_server *cs)
{
  serverconf = cs;
  conn_mkevents ();
  make_async (cs->udp_socket);
  cevents[0].fd = cs->udp_socket;
  cevents[0].events = POLLIN;
  for (;;) {
    conn_poll (&cs->c);
    if (cevents[0].revents)
      conn_demux (cs);
  }
}

static void
usage (void)
{
  fprintf (stderr,
	   "usage: %s udp-port [host:]udp-port\n"
	   "       %s -c {-u unix-socket | tcp-port} [host:]udp-port\n"
	   "       %s -s [-u] udp-port {unix-socket | [host:]tcp-port}\n"
	   , progname, progname, progname);
  exit (1);
}

int
main (int argc, char **argv)
{
  struct option o[] = {
    { "debug", no_argument, NULL, 'd' },
    { "drop", required_argument, NULL, 'r'},
    { "corrupt", required_argument, NULL, 'p'},
    { "delay", required_argument, NULL, 'y'},
    { "duplicate", required_argument, NULL, 'q'},
    { "seed", required_argument, NULL, 'e'},
    { "unix", no_argument, NULL, 'u' },
    { "server", no_argument, NULL, 's' },
    { "window", required_argument, NULL, 'w' },
    { "client", no_argument, NULL, 'c' },
    { NULL, 0, NULL, 0 }
  };
  int opt;
  int opt_unix = 0;
  int opt_client = 0;
  int opt_server = 0;
  char *local = NULL;
  char *remote = NULL;
  struct config_common c;
  struct sockaddr_storage ss;
  struct sigaction sa;

  /* Ignore SIGPIPE, since we may get a lot of these */
  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sa, NULL);

  memset (&c, 0, sizeof (c));
  c.window = 1;
  c.timeout = 2000;

  progname = strrchr (argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  while ((opt = getopt_long (argc, argv, "cdust:r:p:y:q:e:w:l", o, NULL)) != -1)
    switch (opt) {
    case 'c':
      opt_client = 1;
      break;
    case 'd':
      opt_debug = 1;
      break;
    case 'e':
      srand(atoi(optarg));
    case 'r':
      opt_drop = atoi(optarg);
      break;
    case 'p':
      opt_corrupt = atoi(optarg);
      break;
    case 'y':
      opt_delay = atoi(optarg);
      break;
    case 'q':
      opt_duplicate = atoi(optarg);
      break;
    case 'l':
      {
	char name[40];
	snprintf (name, sizeof (name), "%d.in.log", (int) getpid ());
	log_in = open (name, O_CREAT|O_TRUNC|O_WRONLY, 0666);
	if (log_in < 0)
	  perror (name);
	snprintf (name, sizeof (name), "%d.out.log", (int) getpid ());
	log_out = open (name, O_CREAT|O_TRUNC|O_WRONLY, 0666);
	if (log_out < 0)
	  perror (name);
      }
      break;
    case 'k':
      opt_unix = 1;
      break;
    case 's':
      opt_server = 1;
      break;
    case 'w':
      c.window = atoi (optarg);
      break;
    case 't':
      c.timeout = atoi (optarg);
      break;
    default:
      usage ();
      break;
    }

  if (optind + 2 != argc || c.window < 1 || c.timeout < 10
      || (opt_server && opt_client)
      || (!(opt_server || opt_client) && opt_unix))
    usage ();
  c.timer = c.timeout / 5;
  local = argv[optind];
  remote = argv[optind+1];

  if (opt_server) {
    struct config_server cs;
    cs.c = c;
    if (get_address (&cs.dest, 0, 0, opt_unix ? AF_UNIX : AF_INET, remote) < 0
	|| get_address (&ss, 1, 1, AF_INET, local) < 0
	|| (cs.udp_socket = listen_on (1, &ss)) < 0)
      exit (1);
    do_server (&cs);
  }
  else if (opt_client) {
    struct config_client cc;
    struct sockaddr_storage ss;
    cc.c = c;
    if (get_address (&cc.server, 0, 1, AF_INET, remote) < 0
	|| get_address (&ss, 1, 0, opt_unix ? AF_UNIX : AF_INET, local) < 0
	|| (cc.listen_socket = listen_on (0, &ss)) < 0)
      exit (1);
    do_client (&cc);
  }
  else {
    struct sockaddr_storage sl, sr;
    conn_t *cn = conn_alloc ();
    c.single_connection = 1;
    cn->rfd = 0;
    cn->wfd = 1;
    if (get_address (&sr, 0, 1, AF_INET, remote) < 0
	|| get_address (&sl, 1, 1, sr.ss_family, local) < 0
	|| (cn->nfd = listen_on (1, &sl)) < 0)
      exit (1);
    if (connect (cn->nfd, (struct sockaddr *) &sr, addrsize (&sr)) < 0) {
      perror ("connect");
      exit (1);
    }
    cn->server = 0;
    cn->peer = sr;
    make_async (cn->rfd);
    make_async (cn->wfd);
    make_async (cn->nfd);
    cn->rel = rel_create (cn, NULL, &c);

    conn_mkevents ();
    while (conn_list)
      conn_poll (&c);
  }

  return 0;
}
