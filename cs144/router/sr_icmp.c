#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sr_ip.h"
#include "sr_icmp.h"
#include "sr_eth.h"
#include "sr_protocol.h"
#include "sr_utils.h"

bool sr_icmp_checksum_matches(sr_ip_hdr_t *ip_hdr, sr_icmp_hdr_t *icmp_hdr) {
  uint16_t len = ip_hdr->ip_len - sizeof(sr_ip_hdr_t);
  if (len <= 0) {
    return false;
  }

  uint16_t cksum_val = icmp_hdr->icmp_sum;
  icmp_hdr->icmp_sum = 0;

  uint16_t cksum_computed = cksum(icmp_hdr, len);
  icmp_hdr->icmp_sum = cksum_val;

  if (cksum_val == cksum_computed) {
    return true;
  } else {
    return false;
  }
}

void sr_recv_icmp_pkt_for_us(
    struct sr_instance* sr,
    sr_ethernet_hdr_t* e_hdr,
    sr_ip_hdr_t* ip_hdr,
    sr_icmp_hdr_t *icmp_hdr
) {
  if (sr_icmp_checksum_matches(ip_hdr, icmp_hdr) == false) {
    fprintf(stderr, "Failed to process ICMP packet, checksum mismatch\n");
    return;
  }

  if (icmp_hdr->icmp_type == ECHO_REQUEST) {
    sr_send_icmp_echo_reply_pkt(sr, ip_hdr);
  } else {
    fprintf(stderr, "Ignoring ICMP packet of type %d\n", icmp_hdr->icmp_type);
  } 
}

void sr_send_icmp_echo_reply_pkt(struct sr_instance *sr, sr_ip_hdr_t *orig_ip_hdr) {
  unsigned int e_len = sizeof(sr_ethernet_hdr_t) + orig_ip_hdr->ip_len;
  uint8_t e_frame[e_len];

  sr_ethernet_hdr_t *e_hdr = (sr_ethernet_hdr_t *)e_frame;
  e_hdr->ether_type = ethertype_ip;

  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);
  memcpy(ip_hdr, orig_ip_hdr, orig_ip_hdr->ip_len);
  ip_hdr->ip_src = orig_ip_hdr->ip_dst;
  ip_hdr->ip_dst = orig_ip_hdr->ip_src; 
  
  sr_icmp_hdr_t *icmp_hdr = sr_extract_icmp_hdr(e_hdr);
  icmp_hdr->icmp_type = ECHO_REPLY;

  icmp_hdr->icmp_sum = 0;
  icmp_hdr->icmp_sum = cksum(icmp_hdr, ip_hdr->ip_len - sizeof(sr_ip_hdr_t));

  sr_frwd_ip_pkt(sr, e_hdr);
}

void sr_send_icmp_unreachable_pkt(struct sr_instance *sr, uint8_t icmp_code, sr_ip_hdr_t *orig_ip_hdr) {
  sr_icmp_t3_hdr_t icmp_hdr;
  icmp_hdr.icmp_type = DEST_UNREACHABLE; 
  icmp_hdr.icmp_code = icmp_code;
  icmp_hdr.iden = 0;
  icmp_hdr.seqno = 0; 

  sr_ip_hdr_hton(orig_ip_hdr);
  memcpy(icmp_hdr.data, orig_ip_hdr, ICMP_DATA_SIZE); 
  sr_ip_hdr_ntoh(orig_ip_hdr);

  icmp_hdr.icmp_sum = 0;
  icmp_hdr.icmp_sum = cksum(&icmp_hdr, sizeof(sr_icmp_t3_hdr_t));

  /* Setting ip_dst (which becomes the source ip of the ICMP pkt) to 0 will
     cause sr_create_and_frwd_ip_pkt to fill it in with the matching
     routing entry's gateway address */
  uint32_t ip_dst = orig_ip_hdr->ip_dst;
  if (icmp_code == NET_UNREACHABLE || icmp_code == HOST_UNREACHABLE) {
    ip_dst = 0;
  }

  sr_create_and_frwd_ip_pkt(sr, ip_dst, orig_ip_hdr->ip_src, (uint8_t *)&icmp_hdr,
    sizeof(sr_icmp_t3_hdr_t), ip_protocol_icmp);
}

void sr_send_icmp_time_exceeded_pkt(struct sr_instance *sr, uint8_t icmp_code, sr_ip_hdr_t *orig_ip_hdr) {
  sr_icmp_t11_hdr_t icmp_hdr;
  icmp_hdr.icmp_type = TIME_EXCEEDED; 
  icmp_hdr.icmp_code = icmp_code;
  icmp_hdr.unused = 0;
  icmp_hdr.next_mtu = 0; 

  sr_ip_hdr_hton(orig_ip_hdr);
  memcpy(icmp_hdr.data, orig_ip_hdr, ICMP_DATA_SIZE); 
  sr_ip_hdr_ntoh(orig_ip_hdr);

  icmp_hdr.icmp_sum = 0;
  icmp_hdr.icmp_sum = cksum(&icmp_hdr, sizeof(sr_icmp_t11_hdr_t));

  sr_create_and_frwd_ip_pkt(sr, 0, orig_ip_hdr->ip_src, (uint8_t *)&icmp_hdr,
     sizeof(sr_icmp_t11_hdr_t), ip_protocol_icmp);
}

sr_icmp_hdr_t *sr_extract_icmp_hdr(sr_ethernet_hdr_t *e_hdr) {
  uint8_t *pkt = (uint8_t *)e_hdr;
  return (sr_icmp_hdr_t *)(pkt + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
}

void sr_icmp_t3_hdr_ntoh(sr_icmp_t3_hdr_t *hdr) {
  hdr->iden = ntohs(hdr->iden);
  hdr->seqno = ntohs(hdr->seqno);
}

void sr_icmp_t3_hdr_hton(sr_icmp_t3_hdr_t *hdr, sr_ip_hdr_t *ip_hdr) {
  hdr->iden = htons(hdr->iden);
  hdr->seqno = htons(hdr->seqno);

  hdr->icmp_sum = 0;
  hdr->icmp_sum = cksum(hdr, ip_hdr->ip_len - sizeof(sr_ip_hdr_t));
}
