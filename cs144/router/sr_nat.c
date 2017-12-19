#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <limits.h>
#include <netinet/tcp.h>

#include "sr_protocol.h"
#include "sr_nat.h"
#include "sr_utils.h"
#include "sr_tcp.h"
#include "sr_icmp.h"

#define MIN_TCP_PORT 1024

#define UNSOLICITED_SYN_TIMEOUT 6

void nat_remove_stale_mappings(struct sr_nat *nat, time_t curtime);
void nat_respond_to_unsolicited_syns(struct sr_instance *sr, struct sr_nat *nat, time_t curtime);

void timeout_connections(struct sr_nat *nat, struct sr_nat_mapping *mapping, time_t curtime);
bool should_timeout_connection(struct sr_nat *nat, struct sr_nat_connection *conn, time_t curtime);

struct sr_nat_mapping *nat_lookup_external_no_lock(
  struct sr_nat *nat,
  uint16_t aux_ext,
  sr_nat_mapping_type type
);

struct sr_nat_mapping *nat_lookup_internal_no_lock(
  struct sr_nat *nat,
  uint32_t ip_int,
  uint16_t aux_int,
  sr_nat_mapping_type type
);

struct sr_nat_connection *nat_lookup_connection(
  struct sr_nat_mapping *mapping,
  uint32_t ip_ext,
  uint16_t port_ext
);

struct sr_nat_unsolicited_syn *nat_lookup_unsolicited_syn(
  struct sr_nat *nat,
  uint32_t ip_src,
  uint16_t port_src,
  uint16_t port_dst
);

void nat_insert_unsolicited_syn(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  uint16_t port_src,
  uint16_t port_dst
);

void nat_remove_unsolicited_syn(
  struct sr_nat *nat,
  uint32_t ip_src,
  uint16_t port_src,
  uint16_t port_dst
);

int sr_nat_init(
  struct sr_instance *sr,
  struct sr_nat *nat,
  unsigned int icmp_query_timeout,
  unsigned int tcp_established_idle_timeout,
  unsigned int tcp_transitory_idle_timeout
) {

  assert(nat);

  /* Acquire mutex lock */
  pthread_mutexattr_init(&(nat->attr));
  pthread_mutexattr_settype(&(nat->attr), PTHREAD_MUTEX_RECURSIVE);
  int success = pthread_mutex_init(&(nat->lock), &(nat->attr));

  /* Initialize timeout thread */

  pthread_attr_init(&(nat->thread_attr));
  pthread_attr_setdetachstate(&(nat->thread_attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_create(&(nat->thread), &(nat->thread_attr), sr_nat_timeout, sr);

  /* CAREFUL MODIFYING CODE ABOVE THIS LINE! */

  nat->mappings = NULL;
  nat->unsolicited_syns = NULL;

  nat->icmp_query_timeout = icmp_query_timeout;
  nat->tcp_established_idle_timeout = tcp_established_idle_timeout;
  nat->tcp_transitory_idle_timeout = tcp_transitory_idle_timeout;

  nat->tcp_id = MIN_TCP_PORT;
  nat->icmp_id = 0;

  return success;
}


int sr_nat_destroy(struct sr_nat *nat) {  /* Destroys the nat (free memory) */

  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_mapping *mapping, *mapping_next = NULL;
  for (mapping = nat->mappings; mapping != NULL; mapping = mapping_next) {
    struct sr_nat_connection *conn, *conn_next = NULL;
    for (conn = mapping->conns; conn != NULL; conn = conn_next) {
      conn_next = conn->next;
      free(conn);
    }
    mapping_next = mapping->next;
    free(mapping);
  }

  free(nat);
  
  pthread_kill(nat->thread, SIGKILL);
  return pthread_mutex_destroy(&(nat->lock)) &&
    pthread_mutexattr_destroy(&(nat->attr));
}

void *sr_nat_timeout(void *sr_ptr) {  /* Periodic Timout handling */
  struct sr_instance *sr = (struct sr_instance *)sr_ptr;
  struct sr_nat *nat = sr->nat;
  while (1) {
    sleep(1.0);
    pthread_mutex_lock(&(nat->lock));

    time_t curtime = time(NULL);
    nat_remove_stale_mappings(nat, curtime);
    nat_respond_to_unsolicited_syns(sr, nat, curtime);

    pthread_mutex_unlock(&(nat->lock));
  }
  return NULL;
}

void nat_remove_stale_mappings(struct sr_nat *nat, time_t curtime) {
  struct sr_nat_mapping *mapping, *prev = NULL, *next = NULL;
  for (mapping = nat->mappings; mapping != NULL; mapping = next) {
    bool remove_mapping = false;
    if (mapping->type == nat_mapping_icmp) {
      remove_mapping = difftime(curtime, mapping->last_updated) >= nat->icmp_query_timeout;
    } else if (mapping->type == nat_mapping_tcp) {
      timeout_connections(nat, mapping, curtime);
      remove_mapping = mapping->conns == NULL; 
    }

    if (remove_mapping) {
      if (prev) {
        next = mapping->next;
        prev->next = next;
      } else {
        next = mapping->next;
        nat->mappings = next;
      }
      free(mapping);
    } else {
      prev = mapping;
      next = mapping->next;
    }
  }
}

void nat_respond_to_unsolicited_syns(struct sr_instance *sr, struct sr_nat *nat, time_t curtime) {
  struct sr_nat_unsolicited_syn *curr, *next = NULL, *prev = NULL;
  for (curr = nat->unsolicited_syns; curr != NULL; curr = next) {
    if (difftime(curtime, curr->timestamp) >= UNSOLICITED_SYN_TIMEOUT) {
      sr_send_icmp_unreachable_pkt(sr, PORT_UNREACHABLE, curr->ip_hdr);
      if (prev) {
        next = curr->next;
        prev->next = next;
      } else {
        next = curr->next;
        nat->unsolicited_syns = next;
      }
      free(curr);
    } else {
      prev = curr;
      next = curr->next;
    }
  }
}

void timeout_connections(struct sr_nat *nat, struct sr_nat_mapping *mapping, time_t curtime) {
  struct sr_nat_connection *conn, *prev = NULL, *next = NULL;
  for (conn = mapping->conns; conn != NULL; conn = next) {
    if (should_timeout_connection(nat, conn, curtime)) {
      if (prev) {
        next = conn->next;
        prev->next = next;
      } else {
        next = conn->next;
        mapping->conns = next;
      }
      free(conn);
    } else {
      prev = conn;
      next = conn->next;
    }
  }
}

bool should_timeout_connection(struct sr_nat *nat, struct sr_nat_connection *conn, time_t curtime) {
  if (conn->curr_state == TCP_ESTABLISHED) {
    return difftime(curtime, conn->last_updated_state) >= nat->tcp_established_idle_timeout;
  } else if (conn->curr_state == TCP_SYN_SENT || conn->curr_state == TCP_SYN_RECV || conn->curr_state == TCP_CLOSE_WAIT || conn->curr_state == TCP_CLOSING || conn->curr_state == TCP_FIN_WAIT1 || conn->curr_state == TCP_FIN_WAIT2 || conn->curr_state == TCP_LAST_ACK) {
    return difftime(curtime, conn->last_updated_state) >= nat->tcp_transitory_idle_timeout;
  } else {
    return false;
  }
}

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type ) {
  pthread_mutex_lock(&(nat->lock));
  struct sr_nat_mapping *copy = nat_lookup_external_no_lock(nat, aux_ext, type);
  pthread_mutex_unlock(&(nat->lock));

  return copy;
}

struct sr_nat_mapping *nat_lookup_external_no_lock(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type ) {

  /* handle lookup here, malloc and assign to copy */
  struct sr_nat_mapping *copy = NULL;

  struct sr_nat_mapping *mapping;
  for (mapping = nat->mappings; mapping != NULL; mapping = mapping->next) {
    if (mapping->aux_ext == aux_ext && mapping->type == type) {
      break;
    }
  }

  if (mapping != NULL) {
    mapping->last_updated = time(NULL);

    copy = malloc(sizeof(struct sr_nat_mapping));
    memcpy(copy, mapping, sizeof(struct sr_nat_mapping));
  }

  return copy;
}

/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));
  struct sr_nat_mapping *copy = nat_lookup_internal_no_lock(nat, ip_int, aux_int, type);
  pthread_mutex_unlock(&(nat->lock));

  return copy;
}

struct sr_nat_mapping *nat_lookup_internal_no_lock(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  /* handle lookup here, malloc and assign to copy. */
  struct sr_nat_mapping *copy = NULL;

  struct sr_nat_mapping *mapping;
  for (mapping = nat->mappings; mapping != NULL; mapping = mapping->next) {
    if (mapping->ip_int == ip_int && mapping->aux_int == aux_int && mapping->type == type) {
      break;
    }
  }

  if (mapping != NULL) {
    mapping->last_updated = time(NULL);

    copy = malloc(sizeof(struct sr_nat_mapping));
    memcpy(copy, mapping, sizeof(struct sr_nat_mapping));
  }

  return copy;
}

/* Insert a new mapping into the nat's mapping table.
   Actually returns a copy to the new mapping, for thread safety.
 */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_mapping *existing = nat_lookup_internal_no_lock(nat, ip_int, aux_int, type);
  if (existing != NULL) {
    pthread_mutex_unlock(&(nat->lock));
    return existing;
  }

  /* handle insert here, create a mapping, and then return a copy of it */
  struct sr_nat_mapping *mapping = malloc(sizeof(struct sr_nat_mapping));
  struct sr_nat_mapping *copy = malloc(sizeof(struct sr_nat_mapping));

  mapping->type = type;
  mapping->ip_int = ip_int;
  mapping->aux_int = aux_int;
  mapping->last_updated = time(NULL);
  mapping->conns = NULL;

  mapping->next = nat->mappings;
  nat->mappings = mapping;

  if (type == nat_mapping_icmp) {
    mapping->aux_ext = nat->icmp_id++;
    if (nat->icmp_id == USHRT_MAX) {
      nat->icmp_id = 0;
    }
  } else if (type == nat_mapping_tcp) {
    mapping->aux_ext = nat->tcp_id++;
    if (nat->tcp_id == USHRT_MAX) {
      nat->tcp_id = MIN_TCP_PORT;
    }
  }
 
  memcpy(copy, mapping, sizeof(struct sr_nat_mapping));
 
  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

void sr_nat_update_tcp_sent_state(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
) {
  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_mapping *mapping = nat_lookup_internal_no_lock(nat, ip_hdr->ip_src, tcp_hdr->source, nat_mapping_tcp);
  if (mapping == NULL) {
    pthread_mutex_unlock(&(nat->lock));
    return;
  }

  struct sr_nat_connection *conn = nat_lookup_connection(mapping, ip_hdr->ip_dst, tcp_hdr->dest);
  if (conn == NULL) {
    conn = malloc(sizeof(struct sr_nat_connection));
    conn->ip_ext = ip_hdr->ip_dst;
    conn->port_ext = tcp_hdr->dest;
    conn->curr_state = sr_get_tcp_transition(tcp_hdr, TCP_CLOSE, true);
    conn->last_updated_state = time(NULL);
    conn->next = mapping->conns;
    mapping->conns = conn;
  } else {
    conn->curr_state = sr_get_tcp_transition(tcp_hdr, conn->curr_state, true);
    conn->last_updated_state = time(NULL);
  }

  if (tcp_hdr->syn) {
    nat_remove_unsolicited_syn(nat, ip_hdr->ip_dst, tcp_hdr->dest, mapping->aux_ext);
  }

  pthread_mutex_unlock(&(nat->lock));
}


void sr_nat_handle_unsolicited_syn(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
) {
  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_unsolicited_syn *existing = nat_lookup_unsolicited_syn(
    nat, ip_hdr->ip_src, tcp_hdr->source, tcp_hdr->dest);

  if (existing == NULL) {
    nat_insert_unsolicited_syn(nat, ip_hdr, tcp_hdr->source, tcp_hdr->dest);
  }
  
  pthread_mutex_unlock(&(nat->lock));
}

struct sr_nat_unsolicited_syn *nat_lookup_unsolicited_syn(
  struct sr_nat *nat,
  uint32_t ip_src,
  uint16_t port_src,
  uint16_t port_dst
) {
  struct sr_nat_unsolicited_syn *curr;
  for (curr = nat->unsolicited_syns; curr != NULL; curr = curr->next) {
    if (curr->ip_hdr->ip_src == ip_src && curr->port_src == port_src && curr->port_dst == port_dst) {
      return curr;
    }
  }

  return NULL;
}

void nat_insert_unsolicited_syn(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  uint16_t port_src,
  uint16_t port_dst
) {
  struct sr_nat_unsolicited_syn *unsolicited_syn = malloc(sizeof(struct sr_nat_unsolicited_syn));

  unsolicited_syn->ip_hdr = ip_hdr;
  unsolicited_syn->port_src = port_src;
  unsolicited_syn->port_dst = port_dst;
  unsolicited_syn->timestamp = time(NULL);

  unsolicited_syn->next = nat->unsolicited_syns;
  nat->unsolicited_syns = unsolicited_syn;
}

void nat_remove_unsolicited_syn(
  struct sr_nat *nat,
  uint32_t ip_src,
  uint16_t port_src,
  uint16_t port_dst
) {
  struct sr_nat_unsolicited_syn *curr, *prev = NULL, *next = NULL;
  for (curr = nat->unsolicited_syns; curr != NULL; curr = next) {
    if (curr->ip_hdr->ip_src == ip_src && curr->port_src == port_src && curr->port_dst == port_dst) {
      if (prev) {
        next = curr->next;
        prev->next = next;
      } else {
        next = curr->next;
        nat->unsolicited_syns = next;
      }
      free(curr);
      break;
    } else {
      prev = curr;
      next = curr->next;
    }
  }
}

void sr_nat_update_tcp_recvd_state(
  struct sr_nat *nat,
  sr_ip_hdr_t *ip_hdr,
  struct tcphdr *tcp_hdr
) {
  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_mapping *mapping = nat_lookup_external_no_lock(nat, tcp_hdr->dest, nat_mapping_tcp);
  if (mapping == NULL) {
    pthread_mutex_unlock(&(nat->lock));
    return;
  }

  struct sr_nat_connection *conn = nat_lookup_connection(mapping, ip_hdr->ip_src, tcp_hdr->source);
  if (conn != NULL) {
    conn->curr_state = sr_get_tcp_transition(tcp_hdr, conn->curr_state, false);
    conn->last_updated_state = time(NULL);
  }

  pthread_mutex_unlock(&(nat->lock));
}

struct sr_nat_connection *nat_lookup_connection(struct sr_nat_mapping *mapping, uint32_t ip_ext, uint16_t port_ext) {
  struct sr_nat_connection *conn;
  for (conn = mapping->conns; conn != NULL; conn = conn->next) {
    if (conn->ip_ext == ip_ext && conn->port_ext == port_ext) {
      return conn;
    }
  }
 
  return NULL;
}

void sr_print_nat_mappings(struct sr_nat *nat) {
  struct sr_nat_mapping *mapping;
  fprintf(stderr, "ip_int\taux_int\taux_ext\n");
  for (mapping = nat->mappings; mapping != NULL; mapping = mapping->next) {
    print_addr_ip_int(mapping->ip_int);
    fprintf(stderr, "\t%u", mapping->aux_int);    
    fprintf(stderr, "\t%u", mapping->aux_ext);    
    fprintf(stderr, "\n"); 
  }
}
