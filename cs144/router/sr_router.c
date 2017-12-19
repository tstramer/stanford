/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_protocol.h"
#include "sr_router.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

#include "sr_arp.h"
#include "sr_icmp.h"
#include "sr_ip.h"
#include "sr_eth.h"
#include "sr_nat_handler.h"

void sr_recv_ip_pkt(
  struct sr_instance* sr,
  sr_ethernet_hdr_t* e_hdr,
  unsigned int len,
  char *interface
);

void sr_recv_arp_pkt(
  struct sr_instance* sr,
  sr_ethernet_hdr_t* e_hdr,
  unsigned int len,
  char* interface
);

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    
    /* Add initialization code here! */
} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(
    struct sr_instance* sr,
    uint8_t * packet/* lent */,
    unsigned int len,
    char* interface/* lent */
) {
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n", len);

  int minlength = sizeof(sr_ethernet_hdr_t);
  if (len < minlength) {
    fprintf(stderr, "Failed to process packet, insufficient length\n");
    return;
  }

  sr_ethernet_hdr_t* e_hdr = (sr_ethernet_hdr_t *)packet;
  sr_eth_hdr_ntoh(e_hdr);

  if (e_hdr->ether_type == ethertype_ip) {
    sr_recv_ip_pkt(sr, e_hdr, len, interface);
  } else if (e_hdr->ether_type == ethertype_arp) {
    sr_recv_arp_pkt(sr, e_hdr, len, interface);
  } else {
    fprintf(stderr, "Ignoring ethernet packet with type %x\n", e_hdr->ether_type);
  }

}/* end sr_ForwardPacket */

void sr_recv_ip_pkt(
    struct sr_instance* sr,
    sr_ethernet_hdr_t* e_hdr,
    unsigned int len,
    char *interface
) {
  int minlength = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);
  if (len < minlength) {
    fprintf(stderr, "Failed to process IP packet, insufficient length\n");
    return;
  }

  sr_ip_hdr_t *ip_hdr = sr_extract_ip_hdr(e_hdr);

  if (sr_ip_checksum_matches(ip_hdr) == false) {
    fprintf(stderr, "Failed to process IP packet, checksum mismatch\n");
    return;
  }

  sr_ip_hdr_ntoh(ip_hdr);

  if (sr->nat != NULL) {
    enum sr_nat_response resp = sr_rewrite_pkt_for_nat(sr, e_hdr, ip_hdr, interface); 

    if (resp == nat_no_mapping) {
      fprintf(stderr, "Nat no mapping!\n");
      return;
    }
  }

  if (!sr_is_router_ip(sr, ip_hdr->ip_dst)) {
    sr_recv_ip_pkt_for_other(sr, e_hdr, ip_hdr);
    return;
  }

  if (ip_hdr->ip_p == ip_protocol_icmp) {
    minlength += sizeof(sr_icmp_hdr_t);
    if (len < minlength) {
      fprintf(stderr, "Failed to process ICMP packet, insufficient length\n");
      return;
    }

    sr_icmp_hdr_t *icmp_hdr = sr_extract_icmp_hdr(e_hdr);
    sr_recv_icmp_pkt_for_us(sr, e_hdr, ip_hdr, icmp_hdr);
    return;
  }

  if(ip_hdr->ip_p == ip_protocol_tcp || ip_hdr->ip_p == ip_protocol_udp) {
    sr_recv_tcp_or_udp_pkt_for_us(sr, e_hdr, ip_hdr);
    return;
  }

  fprintf(stderr, "Ignoring IP packet with protocol %x\n", ip_hdr->ip_p);
}

void sr_recv_arp_pkt(
    struct sr_instance *sr,
    sr_ethernet_hdr_t* e_hdr,
    unsigned int len,
    char *interface
) {
  int minlength = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  if (len < minlength) {
    fprintf(stderr, "Failed to process ARP packet, insufficient length\n");
    return;
  }

  sr_arp_hdr_t *arp_hdr = sr_extract_arp_hdr(e_hdr);
  sr_arp_hdr_ntoh(arp_hdr);

  if (arp_hdr->ar_op == arp_op_request) {
    sr_recv_arp_req(sr, e_hdr, arp_hdr, interface);
  } else {
    sr_recv_arp_reply(sr, e_hdr, arp_hdr, interface);
  }
}
