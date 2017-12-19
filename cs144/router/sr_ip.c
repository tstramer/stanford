#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "sr_arpcache.h"
#include "sr_arp.h"
#include "sr_icmp.h"
#include "sr_ip.h"
#include "sr_eth.h"
#include "sr_protocol.h"
#include "sr_router.h"
#include "sr_rt.h"
#include "sr_utils.h"

int get_prefix_len(unsigned long mask);

bool sr_ip_checksum_matches(sr_ip_hdr_t *ip_hdr) {
  uint16_t ip_hdr_len = ip_hdr->ip_hl * 4;
  if (ip_hdr_len == 0) {
    return false;
  }

  uint16_t cksum_val = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;

  uint16_t cksum_computed = cksum(ip_hdr, ip_hdr_len);
  ip_hdr->ip_sum = cksum_val;

  if (cksum_val == cksum_computed) {
    return true;
  } else {
    return false;
  }
}

void sr_recv_tcp_or_udp_pkt_for_us(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr, sr_ip_hdr_t* ip_hdr) {
  sr_send_icmp_unreachable_pkt(sr, PORT_UNREACHABLE, ip_hdr);
}
 
void sr_recv_ip_pkt_for_other(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr, sr_ip_hdr_t* ip_hdr) {
  ip_hdr->ip_ttl--;

  if (ip_hdr->ip_ttl == 0) {
    sr_send_icmp_time_exceeded_pkt(sr, TTL_EXCEEDED, ip_hdr); 
    return;
  }
 
  int resp = sr_frwd_ip_pkt(sr, e_hdr);
  if (resp == -1) {
    sr_send_icmp_unreachable_pkt(sr, NET_UNREACHABLE, ip_hdr);
  }
}

void sr_create_and_frwd_ip_pkt(
    struct sr_instance* sr,
    uint32_t ip_src,
    uint32_t ip_dst,
    uint8_t *payload,
    uint16_t payload_len,
    uint8_t protocol
) {
  unsigned int e_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + payload_len;
  uint8_t e_frame[e_len];

  sr_ethernet_hdr_t *e_hdr = (sr_ethernet_hdr_t *)e_frame;
  e_hdr->ether_type = ethertype_ip;

  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);
  sr_init_ip_hdr(ip_hdr, ip_src, ip_dst, payload_len, protocol);

  uint8_t *ip_payload = ((uint8_t *)ip_hdr) + sizeof(sr_ip_hdr_t); 
  memcpy(ip_payload, payload, payload_len);

  sr_frwd_ip_pkt(sr, e_hdr);
}

int sr_frwd_ip_pkt(struct sr_instance* sr, sr_ethernet_hdr_t* e_hdr) {
  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);
  struct sr_rt *rt_entry = sr_find_longest_prefix_match(sr, htonl(ip_hdr->ip_dst));
  if (rt_entry == NULL) {
    fprintf(stderr, "Unable to find routing entry, dropping pkt: ");
    print_addr_ip_int(ip_hdr->ip_dst);
    return -1; 
  }

  struct sr_if *rt_iface = sr_get_interface(sr, rt_entry->interface); 

  if (ip_hdr->ip_src == 0) {
    ip_hdr->ip_src = ntohl(rt_iface->ip);
  }

  memcpy(e_hdr->ether_shost, rt_iface->addr, ETHER_ADDR_LEN);

  struct sr_arpentry *arp_entry = sr_arpcache_lookup(&sr->cache, rt_entry->gw.s_addr);
  if (arp_entry == NULL || !arp_entry->valid) {
    uint32_t e_len = get_eth_ip_pkt_len(e_hdr);
    struct sr_arpreq *arp_req = sr_arpcache_queuereq(&sr->cache, rt_entry->gw.s_addr,
      (uint8_t *)e_hdr, e_len, rt_entry->interface);
    sr_handle_cached_arp_req(sr, arp_req);
  } else {
    memcpy(e_hdr->ether_dhost, arp_entry->mac, ETHER_ADDR_LEN);
    sr_send_ip_pkt(sr, e_hdr, rt_entry->interface);
  }

  if (arp_entry != NULL) {
    free(arp_entry);
  }

  return 0;
}

void sr_send_ip_pkt(struct sr_instance* sr, sr_ethernet_hdr_t *e_hdr, char* iface) {
  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);
  uint32_t e_len = get_eth_ip_pkt_len(e_hdr);

  sr_eth_hdr_hton(e_hdr);
  sr_ip_hdr_hton(ip_hdr);

  sr_send_packet(sr, (uint8_t *)e_hdr, e_len, iface);
}

void sr_init_ip_hdr(
    sr_ip_hdr_t* hdr,
    uint32_t ip_src,
    uint32_t ip_dst,
    uint16_t payload_len,
    uint8_t protocol
) {
  hdr->ip_v = IP_VERSION; 
  hdr->ip_hl = sizeof(sr_ip_hdr_t) / 4;
  hdr->ip_tos = 0;
  hdr->ip_len = sizeof(sr_ip_hdr_t) + payload_len;
  hdr->ip_id = 0;
  hdr->ip_off = 0;
  hdr->ip_ttl = INIT_TTL;
  hdr->ip_p = protocol;
  hdr->ip_sum = 0;
  hdr->ip_src = ip_src;
  hdr->ip_dst = ip_dst; 
}

sr_ip_hdr_t *sr_extract_ip_hdr(sr_ethernet_hdr_t *e_hdr) {
  uint8_t *pkt = (uint8_t *)e_hdr;
  return (sr_ip_hdr_t *)(pkt + sizeof(sr_ethernet_hdr_t));
}

uint32_t get_eth_ip_pkt_len(sr_ethernet_hdr_t *e_hdr) {
  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);
  return sizeof(sr_ethernet_hdr_t) + ip_hdr->ip_len;
}

void sr_ip_hdr_ntoh(sr_ip_hdr_t *hdr) {
  hdr->ip_len = ntohs(hdr->ip_len);
  hdr->ip_id = ntohs(hdr->ip_id);
  hdr->ip_off = ntohs(hdr->ip_off);
  hdr->ip_src = ntohl(hdr->ip_src);
  hdr->ip_dst = ntohl(hdr->ip_dst);
}

void sr_ip_hdr_hton(sr_ip_hdr_t* hdr) {
  hdr->ip_len = htons(hdr->ip_len);
  hdr->ip_id = htons(hdr->ip_id);
  hdr->ip_off = htons(hdr->ip_off);
  hdr->ip_src = htonl(hdr->ip_src);
  hdr->ip_dst = htonl(hdr->ip_dst);

  hdr->ip_sum = 0;
  hdr->ip_sum = cksum(hdr, sizeof(sr_ip_hdr_t));
}

bool sr_is_router_ip(struct sr_instance *sr, uint32_t ip_dst) {
  struct sr_if* iface;

  uint32_t ip_dst_net = htonl(ip_dst);

  for (iface = sr->if_list; iface != NULL; iface = iface->next) {
    if (iface->ip == ip_dst_net) {
      return true;
    }
  }

  return false;
}

struct sr_rt *sr_find_longest_prefix_match(struct sr_instance* sr, uint32_t ip_dst) {
  int longest_prefix = -1;
  struct sr_rt *longest_prefix_entry = NULL;

  struct sr_rt *entry;
  for (entry = sr->routing_table; entry != NULL; entry = entry->next) {
    unsigned long dest_prefix = ip_dst & entry->mask.s_addr;
    unsigned long entry_prefix = entry->dest.s_addr & entry->mask.s_addr;
   
    if (dest_prefix == entry_prefix) {
      int prefix_len = get_prefix_len(entry->mask.s_addr);
      if (prefix_len > longest_prefix) {
        longest_prefix = prefix_len;
        longest_prefix_entry = entry;
      }
    }
  }

  return longest_prefix_entry;
}

int get_prefix_len(unsigned long mask) {
  int i;
  for (i = 0; i < 32; i++) {
    if (((mask >> (31-i)) & 1) == 0) {
      break;
    }
  }
  return i;
}
