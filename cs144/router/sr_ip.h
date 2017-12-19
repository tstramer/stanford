#ifndef SR_IP_H
#define SR_IP_H

#include <stdbool.h>

#include "sr_router.h"

#define IP_HDR_LEN 5
#define IP_ADDR_LEN 4
#define IP_VERSION 4

bool sr_ip_checksum_matches(sr_ip_hdr_t *ip_hdr);

void sr_recv_tcp_or_udp_pkt_for_us(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr, sr_ip_hdr_t* ip_hdr);

void sr_recv_ip_pkt_for_other(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr, sr_ip_hdr_t* ip_hdr);

void sr_create_and_frwd_ip_pkt(
  struct sr_instance* sr,
  uint32_t ip_src,
  uint32_t ip_dst,
  uint8_t *payload,
  uint16_t payload_len,
  uint8_t protocol
);

int sr_frwd_ip_pkt(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr);

void sr_send_ip_pkt(struct sr_instance* sr, sr_ethernet_hdr_t *e_hdr, char *iface); 

void sr_init_ip_hdr(
  sr_ip_hdr_t* ip_hdr,
  uint32_t ip_src,
  uint32_t ip_dst,
  uint16_t payload_len,
  uint8_t protocol
);

sr_ip_hdr_t *sr_extract_ip_hdr(sr_ethernet_hdr_t *e_hdr);

uint32_t get_eth_ip_pkt_len(sr_ethernet_hdr_t *e_hdr);

void sr_ip_hdr_ntoh(sr_ip_hdr_t *hdr);
void sr_ip_hdr_hton(sr_ip_hdr_t *hdr);

bool sr_is_router_ip(struct sr_instance *sr, uint32_t ip_dst);

struct sr_rt *sr_find_longest_prefix_match(struct sr_instance* sr, uint32_t ip_dst);

#endif
