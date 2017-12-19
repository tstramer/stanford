#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

char *progname;


struct copy_state {
  int in;
  int out;
  int error;
  char buf[8192];
};

void *
copy_data_one_direction (void *_st)
{
  struct copy_state *st = _st;
  int n;

  while ((n = read (st->in, st->buf, sizeof (st->buf))) > 0)
    if (write (st->out, st->buf, n) < 0) {
      st->error = 1;
      perror ("write");
      return NULL;
    }
  shutdown (st->out, SHUT_WR);
  if (n < 0)
    perror ("read");
  if (st->in)
    fprintf (stderr, "[received EOF]\n");
  else
    fprintf (stderr, "[sent EOF]\n");
  return NULL;
}

static void
copy_data (int s)
{
  pthread_t to_s;
  struct copy_state st[2];

  st[0].in = s;
  st[0].out = 1;
  st[0].error = 0;
  st[1].in = 0;
  st[1].out = s;
  st[1].error = 0;

  pthread_create (&to_s, NULL, copy_data_one_direction, &st[1]);
  copy_data_one_direction (&st[0]);
  pthread_join (to_s, NULL);
}

static int
sock (int family)
{
  int s = socket (family, SOCK_STREAM, 0);
  if (s < 0) {
    perror ("socket");
    exit (1);
  }
  return s;
}

static void
do_connect (const struct sockaddr *sap, socklen_t len)
{
  int s = sock (sap->sa_family);
  int r = connect (s, sap, len);
  if (r < 0) {
    perror ("connect");
    exit (1);
  }

  fprintf (stderr, "[established connection]\n");
  copy_data (s);
}

static void
do_listen (const struct sockaddr *sap, socklen_t len)
{
  int sl = sock (sap->sa_family);
  int r, s, n;
  const char *path = sap->sa_family == AF_UNIX
    ? ((struct sockaddr_un *) sap)->sun_path : NULL;
  struct sockaddr_storage garbage;

  n = 1;
  setsockopt (sl, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof (n));

  r = bind (sl, sap, len);
  if (r < 0) {
    if (errno == EADDRINUSE && path)
      fprintf (stderr, "%s: already exists; you should delete this socket"
	       " if you are\n%*s  not running another instance of %s.\n",
	       path, (int) strlen (path), "", progname);
    else
      perror (path ? path : "bind");
    exit (1);
  }

  if (listen (sl, 1) < 0) {
    perror ("listen");
    exit (1);
  }

  memset (&garbage, 0, sizeof (garbage));
  len = sizeof (garbage);
  s = accept (sl, (struct sockaddr *) &garbage, &len);
  if (s < 0) {
    perror ("accept");
    exit (1);
  }
  if (path)
    unlink (path);
  fprintf (stderr, "[accepted connection]\n");
  close (sl);

  copy_data (s);
}

int
get_address (struct sockaddr_storage *sa, char *host, char *port)
{
  struct addrinfo hints;
  struct addrinfo *ai;
  int err;
  //socklen_t len;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (host) {
    /* Look up the remote hostname and port we are supposed to
     * communicate with. */
    err = getaddrinfo (host, port, &hints, &ai);
    if (err) {
      fprintf (stderr, "%s:%s: %s\n", host, port,
	       gai_strerror (err));
      return -1;
    }
  }
  else {
    /* Get local address to bind to */
    hints.ai_flags = AI_PASSIVE; /* passive means for local address */
    err = getaddrinfo (NULL, port, &hints, &ai);
    if (err) {
      fprintf (stderr, "local port %s: %s\n", port, gai_strerror (err));
      return -1;
    }
  }

  //len = ai->ai_addrlen;
  assert (ai->ai_addrlen <= sizeof (*sa));
  memcpy (sa, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo (ai);
  return 0;
}

static void
usage (void)
{
  fprintf (stderr, "usage: %s { -u unix-socket | [host] tcp-port }"
	   "   (to connect)\n"
	   "       %s -l { -u unix-socket | tcp-port }"
	   "       (to listen)\n",
	   progname, progname);
  exit (1);
}

int
main (int argc, char **argv)
{
  int opt_listen = 0;
  int opt_unixdomain = 0;
  int opt;
  socklen_t len;
  struct sockaddr_un sun;
  struct sockaddr_storage ss;
  struct sockaddr *sap;


  progname = strrchr (argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  while ((opt = getopt (argc, argv, "lu")) != -1)
    switch (opt) {
    case 'l':
      opt_listen = 1;
      break;
    case 'u':
      opt_unixdomain = 1;
      break;
    default:
      usage ();
      break;
    }

  if (opt_unixdomain) {
    if (optind + 1 != argc)
      usage ();

    if (strlen (argv[optind]) >= sizeof (sun.sun_path)) {
      fprintf (stderr, "socket name exceeds maximum of %d characters\n",
	       (int) sizeof (sun.sun_path) - 1);
      exit (1);
    }

    memset (&sun, 0, sizeof (sun));
    sun.sun_family = AF_UNIX;
    strcpy (sun.sun_path, argv[optind]);
    len = sizeof (sun);
    sap = (struct sockaddr *) &sun;
  }
  else {
    int ret;
    if (optind + 1 == argc)
      ret = get_address (&ss, NULL, argv[optind]);
    else if (optind + 2 == argc)
      ret = get_address (&ss, argv[optind], argv[optind+1]);
    else
      usage ();
    if (ret < 0)
      exit (1);
    sap = (struct sockaddr *) &ss;
    if (ss.ss_family == AF_INET)
      len = sizeof (struct sockaddr_in);
    else
      len = sizeof (struct sockaddr_in6);
  }

  if (opt_listen)
    do_listen (sap, len);
  else
    do_connect (sap, len);

  return 0;
}
