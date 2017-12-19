
#ifndef SR_NAT_TABLE_H
#define SR_NAT_TABLE_H

#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <netinet/tcp.h>

#include "sr_protocol.h"
#include "sr_router.h"

typedef enum {
  nat_mapping_icmp,
  nat_mapping_tcp
  /* nat_mapping_udp, */
} sr_nat_mapping_type;

struct sr_nat_connection {
  uint32_t ip_ext;
  uint16_t port_ext;
  uint8_t curr_state;
  time_t last_updated_state;
  struct sr_nat_connection *next;
};

struct sr_nat_mapping {
  sr_nat_mapping_type type;
  uint32_t ip_int; /* internal ip addr */
  uint32_t ip_ext; /* external ip addr */
  uint16_t aux_int; /* internal port or icmp id */
  uint16_t aux_ext; /* external port or icmp id */
  time_t last_updated; /* use to timeout mappings */
  struct sr_nat_connection *conns; /* list of connections. null for ICMP */
  struct sr_nat_mapping *next;
};

struct sr_nat_unsolicited_syn {
  sr_ip_hdr_t *ip_hdr; /* originating IP hdr of the unsolicited syn */ 
  uint16_t port_src; /* originating TCP port of the unsolicited syn */
  uint16_t port_dst; /* destination TCP port of the unsolicited syn */
  time_t timestamp; /* the time of the syn measured as seconds since unix epoch */
  struct sr_nat_unsolicited_syn *next;
};

struct sr_nat {
  /* add any fields here */
  struct sr_nat_mapping *mappings;
  struct sr_nat_unsolicited_syn *unsolicited_syns;

  /* threading */
  pthread_mutex_t lock;
  pthread_mutexattr_t attr;
  pthread_attr_t thread_attr;
  pthread_t thread;

  uint16_t tcp_id;
  uint16_t icmp_id;

  unsigned int icmp_query_timeout; /* ICMP query timeout interval in seconds */;
  unsigned int tcp_established_idle_timeout; /* TCP Established Idle Timeout in seconds */
  unsigned int tcp_transitory_idle_timeout; /* TCP Transitory Idle Timeout in seconds */
};


int sr_nat_init(
  struct sr_instance *sr,
  struct sr_nat *nat,
  unsigned int icmp_query_timeout,
  unsigned int tcp_established_idle_timeout,
  unsigned int tcp_transitory_idle_timeout
);     /* Initializes the nat */
int   sr_nat_destroy(struct sr_nat *nat);  /* Destroys the nat (free memory) */
void *sr_nat_timeout(void *nat_ptr);  /* Periodic Timout */

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

/* Insert a new mapping into the nat's mapping table.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

void sr_nat_handle_unsolicited_syn(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
);

void sr_nat_update_tcp_sent_state(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
);

void sr_nat_update_tcp_recvd_state(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
);

void sr_print_nat_mappings(struct sr_nat *nat);

#endif
