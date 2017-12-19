#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sr_arpcache.h"
#include "sr_arp.h"
#include "sr_icmp.h"
#include "sr_if.h"
#include "sr_ip.h"
#include "sr_eth.h"
#include "sr_protocol.h"
#include "sr_router.h"
#include "sr_utils.h"

static uint8_t ETH_BROADCAST_ADDR[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static uint8_t ETH_ZERO_ADDR[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void sr_handle_cached_arp_req(struct sr_instance *sr, struct sr_arpreq *req) {
  time_t now;
  time(&now);

  if (req->sent == 0 || difftime(now, req->sent) >= 1.0) {
    if (req->times_sent >= 5) {
      sr_handle_host_unreachable(sr, req);
    } else {
      sr_send_arpreq(sr, req);
      req->sent = now;
      req->times_sent++;
    }
  }
}

void sr_send_arpreq(struct sr_instance *sr, struct sr_arpreq *req) {
  if (req->packets == NULL) {
    return;
  }
 
  unsigned int e_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  uint8_t e_frame[e_len];

  char *interface = req->packets->iface;
  struct sr_if* iface = sr_get_interface(sr, interface);

  sr_ethernet_hdr_t *e_hdr = (sr_ethernet_hdr_t *)e_frame;
  sr_init_eth_hdr(e_hdr, ETH_BROADCAST_ADDR, iface->addr, ethertype_arp);
  sr_eth_hdr_hton(e_hdr);

  sr_arp_hdr_t *arp_hdr = sr_extract_arp_hdr(e_hdr);
  sr_init_arp_hdr(arp_hdr, iface->addr, ntohl(iface->ip), ETH_ZERO_ADDR, ntohl(req->ip), arp_op_request);
  sr_arp_hdr_hton(arp_hdr);

  sr_send_packet(sr, e_frame, e_len, interface);
}

void sr_recv_arp_req(
    struct sr_instance* sr,
    sr_ethernet_hdr_t* e_hdr,
    sr_arp_hdr_t *arp_hdr,
    char* interface
) {
  unsigned int e_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  uint8_t e_resp[e_len];

  struct sr_if* iface = sr_get_interface(sr, interface);

  sr_ethernet_hdr_t *e_resp_hdr = (sr_ethernet_hdr_t *)e_resp;
  sr_init_eth_hdr(e_resp_hdr, ETH_BROADCAST_ADDR, iface->addr, ethertype_arp);
  sr_eth_hdr_hton(e_resp_hdr);

  sr_arp_hdr_t *arp_resp_hdr = sr_extract_arp_hdr(e_resp_hdr);
  sr_init_arp_hdr(arp_resp_hdr, iface->addr, ntohl(iface->ip), arp_hdr->ar_sha, arp_hdr->ar_sip, arp_op_reply);
  sr_arp_hdr_hton(arp_resp_hdr);

  sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, htonl(arp_hdr->ar_sip));

  sr_send_packet(sr, e_resp, e_len, interface);
}

void sr_recv_arp_reply(
    struct sr_instance* sr,
    sr_ethernet_hdr_t* e_hdr,
    sr_arp_hdr_t *arp_hdr,
    char* interface
) {
  struct sr_arpreq *arp_req = sr_arpcache_insert(&sr->cache, arp_hdr->ar_sha, htonl(arp_hdr->ar_sip));
  
  if (arp_req != NULL) {
    struct sr_packet *pkt;
    for (pkt = arp_req->packets; pkt != NULL; pkt = pkt->next) {
      sr_ethernet_hdr_t *queued_e_hdr = (sr_ethernet_hdr_t *)pkt->buf;
      memcpy(queued_e_hdr->ether_dhost, arp_hdr->ar_sha, ETHER_ADDR_LEN);
      sr_send_ip_pkt(sr, queued_e_hdr, pkt->iface);
    }
    sr_arpreq_destroy(&sr->cache, arp_req);
  }
}

void sr_handle_host_unreachable(struct sr_instance *sr, struct sr_arpreq *req) {
  struct sr_packet *pkt;
  for (pkt = req->packets; pkt != NULL; pkt = pkt->next) {
    sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr((sr_ethernet_hdr_t *)pkt->buf);
    sr_send_icmp_unreachable_pkt(sr, HOST_UNREACHABLE, ip_hdr); 
  }
  sr_arpreq_destroy(&sr->cache, req);
}

void sr_init_arp_hdr(
    sr_arp_hdr_t *hdr,
    unsigned char *sha,
    uint32_t sip,
    unsigned char *tha,
    uint32_t tip,
    unsigned short arp_op
) {
  hdr->ar_hrd = arp_hrd_ethernet;
  hdr->ar_pro = ethertype_ip;
  hdr->ar_hln = ETHER_ADDR_LEN;
  hdr->ar_pln = IP_ADDR_LEN;
  hdr->ar_op = arp_op;

  memcpy(hdr->ar_sha, sha, ETHER_ADDR_LEN);
  hdr->ar_sip = sip;

  memcpy(hdr->ar_tha, tha, ETHER_ADDR_LEN);
  hdr->ar_tip = tip;
}

sr_arp_hdr_t *sr_extract_arp_hdr(sr_ethernet_hdr_t *e_hdr) {
  uint8_t *pkt = (uint8_t *)e_hdr;
  return (sr_arp_hdr_t *)(pkt + sizeof(sr_ethernet_hdr_t));
}

void sr_arp_hdr_ntoh(sr_arp_hdr_t *hdr) {
  hdr->ar_hrd = ntohs(hdr->ar_hrd); 
  hdr->ar_pro = ntohs(hdr->ar_pro); 
  hdr->ar_op = ntohs(hdr->ar_op); 
  hdr->ar_sip = ntohl(hdr->ar_sip); 
  hdr->ar_tip = ntohl(hdr->ar_tip);
}

void sr_arp_hdr_hton(sr_arp_hdr_t *hdr) {
  hdr->ar_hrd = htons(hdr->ar_hrd); 
  hdr->ar_pro = htons(hdr->ar_pro); 
  hdr->ar_op = htons(hdr->ar_op); 
  hdr->ar_sip = htonl(hdr->ar_sip); 
  hdr->ar_tip = htonl(hdr->ar_tip);
}
