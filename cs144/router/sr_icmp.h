#ifndef SR_ICMP_H
#define SR_ICMP_H

#include <stdbool.h>

#include "sr_router.h"

enum sr_icmp_type {
  ECHO_REPLY = 0,
  DEST_UNREACHABLE = 3,
  ECHO_REQUEST = 8,
  TIME_EXCEEDED = 11,
};

enum sr_t3_icmp_code {
  NET_UNREACHABLE = 0,
  HOST_UNREACHABLE = 1,
  PROTOCOL_UNREACHABLE = 2,
  PORT_UNREACHABLE = 3,
};

enum s3_t11_icmp_code {
  TTL_EXCEEDED = 0,
  FRAG_TTL_EXCEEDED = 1, 
};

bool sr_icmp_checksum_matches(sr_ip_hdr_t *ip_hdr, sr_icmp_hdr_t *icmp_hdr);

void sr_recv_icmp_pkt_for_us(
  struct sr_instance* sr,
  sr_ethernet_hdr_t* e_hdr,
  sr_ip_hdr_t* ip_hdr,
  sr_icmp_hdr_t *icmp_hdr
);

void sr_send_icmp_echo_reply_pkt(struct sr_instance *sr, sr_ip_hdr_t *orig_ip_hdr);
void sr_send_icmp_unreachable_pkt(struct sr_instance *sr, uint8_t icmp_code, sr_ip_hdr_t *orig_ip_hdr);
void sr_send_icmp_time_exceeded_pkt(struct sr_instance *sr, uint8_t icmp_code, sr_ip_hdr_t *orig_ip_hdr);

sr_icmp_hdr_t *sr_extract_icmp_hdr(sr_ethernet_hdr_t *e_hdr);

void sr_icmp_t3_hdr_ntoh(sr_icmp_t3_hdr_t *hdr);
void sr_icmp_t3_hdr_hton(sr_icmp_t3_hdr_t *hdr, sr_ip_hdr_t *ip_hdr);

#endif /* -- SR_ICMP_H -- */
