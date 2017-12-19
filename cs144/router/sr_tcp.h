#ifndef SR_TCP_H
#define SR_TCP_H

#include <stdbool.h>
#include <netinet/tcp.h>

#include "sr_protocol.h"

struct sr_tcp_pseudo_hdr {
  uint32_t ip_src;
  uint32_t ip_dst;
  uint8_t reserved;
  uint8_t ip_p;
  uint16_t tcp_seg_len;
};

struct tcphdr *sr_extract_tcp_hdr(sr_ethernet_hdr_t *e_hdr);

void sr_tcp_hdr_ntoh(struct tcphdr *hdr);
void sr_tcp_hdr_hton(struct tcphdr *hdr, sr_ip_hdr_t *ip_hdr);

uint8_t sr_get_tcp_transition(struct tcphdr *hdr, uint8_t curr_state, bool sent);

#endif /* -- SR_TCP_H -- */
