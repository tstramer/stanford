#if DMALLOC
#include <dmalloc.h>
#endif /* DMALLOC */

#include <stdint.h>
#include <sys/types.h>

/* -----------------------------------------------------------------------

   Simple reliable sliding window protocol.

   There are two kinds of packets, Data packets and Ack-only packets.
   You can tell the type of a packet by length.  Ack packets are 8
   bytes, while Data packets vary from 12 to 500 bytes.

   Every Data packet contains a 32-bit sequence number as well as 0 or
   more bytes of payload.

   Both Data and Ack packets contain the following fields.  The
   length, seqno, and ackno fields are always in big-endian order.

   - cksum: 16-bit IP checksum (you can set the cksum field to 0 and
            use the cksum() on a packet to compute the value of the
            checksum that should be in there).

   - len:   16-bit total length of the packet.  This will be 8 for Ack
            packets, and 12 + payload-size for data packets (since 12
            bytes are used for the header).

	    An end-of-file condition is transmitted to the other side
            of a connection by a data packet containing 0 bytes of
            payload, and hence a len of 12.

	    Note: You must examine the length field, and should not
            assume that the UDP packet you receive is the correct
            length.  The network might truncate or pad packets.

   - ackno: 32-bit cumulative acknowledgement number.  This says that
            the sender of a packet has received all packets with
            sequence numbers earlier than ackno, and is waiting for
            the packet with a seqno of ackno.

            Note that the ackno is the sequence number you are waiting
            for, that you have not received yet.  The first sequence
            number in any connection is 1, so if you have not received
            any packets yet, you should set the ackno field to 1.

   The following fields only exist in a data packet:

   - seqno: Each packet transmitted in a stream of data must me
            numbered with a seqno.  The first packet in a stream has
            seqno 1.  Note that in TCP, sequence numbers indicate
            bytes, whereas by contrast this protocol just numbers
            packets.  That means that once a packet is transmitted, it
            cannot be merged with another packet for retransmission.

   - data:  Contains (len - 12) bytes of payload data for the
            application.

   To conserve packets, a sender should not send more than one
   unacknowledged Data frame with less than the maximum number of
   packets (500), somewhat like TCP's Nagle algorithm.

 */


/* Ack-only packets are only 8 bytes */
struct ack_packet {
  uint16_t cksum;
  uint16_t len;
  uint32_t ackno;
};

struct packet {
  uint16_t cksum;
  uint16_t len;
  uint32_t ackno;
  uint32_t seqno;		/* Only valid if length > 8 */
  char data[500];
};
typedef struct packet packet_t;

/* -----------------------------------------------------------------------

   Important notes about the library:

   * A connection is named by the tuple:

     <local-IP-address, local-UDP-port, remote-IP-address, remote-UDP-port>

     In stand-alone mode, and in the client, the task of connection
     demultiplexing is handled automatically for you.  In server
     mode, you must demultiplex packets yourself.  A useful function
     for this is addreq(), which compares two sockaddr_storage
     structures and tells you if they are the same.

   * The configuration of the program is described by a structure
     config_common that gets passed to various functions.  The most
     important fields are:

       - window:  Tells you the size of the sliding window (which will
                  be 1 for stop-and-wait).

       - timeout: Tells you what your retransmission timer should be,
                  in milliseconds.  If after this many milliseconds a
                  packet you sent has still not been acknowledged, you
                  must retransmit the packet.  You may find the
                  function clock_gettime with parameter
                  CLOCK_MONOTONIC useful for keeping track of when
                  packets are sent.  Run "man clock_gettime".

   * Your task is to implement the following seven functions:

       rel_create, rel_destroy, rel_recvpkt, rel_demux,
       rel_read, rel_output, rel_timer

     as well to augment the reliable_state data structure.  All the
     changes you need to make are in the file reliable.c.

   * The reliable_state structure encapsulates the state of each
     connection.  The structure is typedefed to rel_t in this file,
     but the contents of the reliable_state structure is defined in
     reliable.c, where you should add more fields as needed to keep
     your per-connection state.

   * A rel_t is created by the rel_create function.  When running in
     single-connection or client mode, the library will call
     rel_create directly for you.  When running as a server, you will
     need to invoke rel_create yourself from within rel_demux when you
     notice a new connection (which will show up as a packet with
     sequence number 1 from a sockaddr_storage that you have not seen
     before (which you can test for using addreq()).

   * A rel_t is deallocated by rel_destroy().  The library will call
     rel_destroy when it receives an ICMP port unreachable (signifying
     that the other end of the connection has died).  You should also
     call rel_destroy when all of the following hold:

       - You have read an EOF from the other side (i.e., a Data packet
         of len 12, where the payload field is 0 bytes).

       - You have read an EOF or error from your input (conn_input
         returned -1).

       - All package you have sent have been acknowledged.

     Note that to be correct, at least one side should also wait
     around in case the last ack it sent got lost, the way TCP uses
     the FIN_WAIT state, but this is not required.

   * When a packet is received, the library will call either
     rel_recvpkt or rel_demux.  rel_recvpkt is called when running in
     single-connection or client mode.  In that case, the library
     already knows what rel_t to use for the particular UDP port
     receiving the packet, and supplies you with the rel_t.  In the
     case of the server, all UDP packets go to the same port, so you
     must demultiplex the connections in rel_demux.

   * To get the input data that you must send in your packets, call
     conn_input.  If no data is available, conn_input will return 0.
     At that point, the library will call rel_read once data is again
     available to from conn_input.

   * To output data you have received in decoded UDP packets, call
     conn_output.  The function conn_bufspace tells you how much space
     is available.  If you try to write more than this, conn_output
     may return that it has accepted fewer bytes than you have asked
     for.  You should flow control the sender by not acknowledging
     packets if there is no buffer space available for conn_output.
     The library calls rel_output when output has drained, at which
     point you can send out more Acks to get more data from the remote
     side.

   * The function rel_timer is called periodically, currently at a
     rate 1/5 of the retransmission interval.  You can use this timer
     to inspect packets and retransmit packets that have not been
     acknowledged.  Do not retransmit every packet every time the
     timer is fired!  You must keep track of which packets need to be
     retransmitted when.

*/

struct config_common {
  int window;			/* # of unacknowledged packets in flight */
  int timer;			/* How often rel_timer called in milliseconds */
  int timeout;			/* Retransmission timeout in milliseconds */
  int single_connection;        /* Exit after first connection failure */
};

typedef struct reliable_state rel_t;

extern char *progname;		/* Set to name of program by main */
extern int opt_debug;		/* When != 0, print packets */

#if !DMALLOC
void *xmalloc (size_t);
#endif /* !DMALLOC */
uint16_t cksum (const void *_data, int len); /* compute TCP-like checksum */


/* Returns 1 when two addresses equal, 0 otherwise */
int addreq (const struct sockaddr_storage *a, const struct sockaddr_storage *b);

/* Hash a socket address down to a number.  Multiple addresses may
   hash to the same number, and the hash value is not guaranteed to be
   the same on different machines, but this may be useful for
   implementing a hash table. */
unsigned int addrhash (const struct sockaddr_storage *s);

/* Actual size of the real socket address structure stashed in a
   sockaddr_storage. */
size_t addrsize (const struct sockaddr_storage *ss);

/* Useful for debugging. */
void print_pkt (const packet_t *buf, const char *op, int n);

/* This is an opaque structure provided by rlib.  You only need
 * pointers to it.  */
typedef struct conn conn_t;

/* You only need to call this in the server, when rel_create gets a
 * NULL conn_t. */
conn_t *conn_create (rel_t *, const struct sockaddr_storage *);

/* Call this function to send a UDP packet to the other side. */
int conn_sendpkt (conn_t *c, const packet_t *pkt, size_t len);

/* This function tells you how many bytes of output buffering are free
 * for conn_output to store your data.  conn_output is guaranteed not
 * to return 0 if you write less than this many bytes. */
size_t conn_bufspace (conn_t *c);

/* Call this function to produce output from the UDP packets you have
 * received.  If you call it with len == 0, then it will send an EOF
 * to the other side.  Returns number of bytes written (>= 0) on
 * success, or -1 if there has been an error.  Note that if you call
 * it with more bytes than are reported available by conn_bufspace, it
 * may return that it wrote fewer bytes than you asked it to write, in
 * which case you will have to call conn_output again (passing in
 * whatever portion of the message it did not write the first time).
 * Note that the rel_output function is called whenever more output
 * buffer space opens up, giving you a chance to finish up your
 * write. */
int conn_output (conn_t *c, const void *buf, size_t len);

/* Get some input from the reliable side.  You must must then put the
 * data into UDP sockets which you send out with conn_sendpkt.  This
 * function returns the number of bytes received, 0 if there is no
 * data currently available, and -1 on EOF or error. */
int conn_input (conn_t *c, void *buf, size_t len);

/* Deallocate a connection */
void conn_destroy (conn_t *c);

/* Functions you must provide (in reliable.c). */

rel_t *rel_create (conn_t *, const struct sockaddr_storage *,
		   const struct config_common *);
void rel_destroy (rel_t *);

/* This function gets called on clients, when packets arrive: */
void rel_recvpkt (rel_t *, packet_t *pkt, size_t len);
/* This function gets called on servers, when packets arrive: */
void rel_demux (const struct config_common *cc,
		const struct sockaddr_storage *client,
		packet_t *pkt, size_t len);

/* Notification handlers */
void rel_read (rel_t *);    /* Invoked when you can call conn_input */
void rel_output (rel_t *);  /* Invoked when some output drained */
void rel_timer (void); /* Invoked roughly each timer/5 milliseconds */



/* Below are some utility functions you don't need for this lab */

/* Fill in a sockaddr_storage with a socket address.  If local is
 * non-zero, the socket will be used for binding.  If dgram is
 * non-zero, use datagram sockets (e.g., UDP).  If unixdom is
 * non-zero, use unix-domain sockets.  name is either "port" or
 * "host:port". */
int get_address (struct sockaddr_storage *ss, int local,
		 int dgram, int unixdom, char *name);

/* Put socket in non-blocking mode */
int make_async (int s);

/* Bind to a particular socket (and listen if not dgram). */
int listen_on (int dgram, struct sockaddr_storage *ss);

/* Convenient way to get a socket connected to a destination */
int connect_to (int dgram, const struct sockaddr_storage *ss);

#include <time.h>
#include <sys/time.h>

#ifndef CLOCK_REALTIME
# define NEED_CLOCK_GETTIME 1
# define CLOCK_REALTIME 0
# undef CLOCK_MONOTONIC
# define CLOCK_MONOTONIC 1
#endif /* !HAVE_CLOCK_GETTIME */

#if NEED_CLOCK_GETTIME
int clock_gettime (int, struct timespec *);
#endif /* NEED_CLOCK_GETTIME */
