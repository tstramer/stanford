#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/tcp.h>

#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_icmp.h"
#include "sr_ip.h"
#include "sr_rt.h"
#include "sr_utils.h"
#include "sr_nat_handler.h"
#include "sr_tcp.h"

#define INTERNAL_IFACE "eth1"

enum sr_nat_response handle_icmp_pkt(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, bool is_internal);
enum sr_nat_response handle_tcp_pkt(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, bool is_internal);

enum sr_nat_response handle_internal_icmp_echo_req_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, sr_icmp_t3_hdr_t *icmp_hdr);
enum sr_nat_response handle_external_icmp_echo_reply_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, sr_icmp_t3_hdr_t *icmp_hdr);

enum sr_nat_response handle_internal_tcp_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, struct tcphdr *tcp_hdr);
enum sr_nat_response handle_external_tcp_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, struct tcphdr *tcp_hdr);

bool rewrite_source_address(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr);

enum sr_nat_response sr_rewrite_pkt_for_nat(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, char *interface) {
  bool is_internal = strcmp(interface, INTERNAL_IFACE) == 0;
  bool is_router_ip = sr_is_router_ip(sr, ip_hdr->ip_dst);

  if (is_router_ip == is_internal) {
    return nat_ignored;
  }

  if (ip_hdr->ip_p == ip_protocol_icmp) {
    return handle_icmp_pkt(sr, e_hdr, ip_hdr, is_internal);
  } else if (ip_hdr->ip_p == ip_protocol_tcp) {
    return handle_tcp_pkt(sr, e_hdr, ip_hdr, is_internal);
  } else {
    return nat_ignored;
  }
}

enum sr_nat_response handle_icmp_pkt(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, bool is_internal) {
  sr_icmp_hdr_t *icmp_hdr = sr_extract_icmp_hdr(e_hdr);

  bool is_internal_echo_req = is_internal && icmp_hdr->icmp_type == ECHO_REQUEST;
  bool is_external_echo_reply = !is_internal && icmp_hdr->icmp_type == ECHO_REPLY;

  if (is_internal_echo_req || is_external_echo_reply) {
    enum sr_nat_response resp;

    sr_icmp_t3_hdr_t *icmp_t3_hdr = (sr_icmp_t3_hdr_t *)icmp_hdr;
    sr_icmp_t3_hdr_ntoh(icmp_t3_hdr);

    if (is_internal_echo_req) {
      resp = handle_internal_icmp_echo_req_pkt(sr, ip_hdr, icmp_t3_hdr);
    } else {
      resp = handle_external_icmp_echo_reply_pkt(sr, ip_hdr, icmp_t3_hdr);
    }

    sr_icmp_t3_hdr_hton(icmp_t3_hdr, ip_hdr);

    return resp;
  } else {
    fprintf(stderr, "Nat dropping %s icmp pkt of type %d", is_internal ? "internal" : "external", icmp_hdr->icmp_type);
    return nat_no_mapping;
  }
} 

enum sr_nat_response handle_internal_icmp_echo_req_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, sr_icmp_t3_hdr_t *icmp_hdr) {
  struct sr_nat_mapping *mapping = sr_nat_insert_mapping(sr->nat, ip_hdr->ip_src, icmp_hdr->iden, nat_mapping_icmp);

  if (!rewrite_source_address(sr, ip_hdr)) {
    free(mapping);
    return nat_no_mapping;
  }

  icmp_hdr->iden = mapping->aux_ext;

  free(mapping);

  return nat_mapped;
}

enum sr_nat_response handle_external_icmp_echo_reply_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, sr_icmp_t3_hdr_t *icmp_hdr) {
  struct sr_nat_mapping *mapping = sr_nat_lookup_external(sr->nat, icmp_hdr->iden, nat_mapping_icmp);

  if (mapping == NULL) {
    fprintf(stderr, "No nat mapping for external icmp echo reply pkt\n");
    return nat_no_mapping;
  }

  ip_hdr->ip_dst = mapping->ip_int;
  icmp_hdr->iden = mapping->aux_int;

  free(mapping);

  return nat_mapped;
}

enum sr_nat_response handle_tcp_pkt(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, bool is_internal) {
  enum sr_nat_response resp;

  struct tcphdr *tcp_hdr = sr_extract_tcp_hdr(e_hdr);
  sr_tcp_hdr_ntoh(tcp_hdr);

  if (is_internal) {
    resp = handle_internal_tcp_pkt(sr, ip_hdr, tcp_hdr);
  } else {
    resp = handle_external_tcp_pkt(sr, ip_hdr, tcp_hdr);
  }

  sr_tcp_hdr_hton(tcp_hdr, ip_hdr);

  return resp;
}

enum sr_nat_response handle_internal_tcp_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, struct tcphdr *tcp_hdr) {
  struct sr_nat_mapping *mapping = sr_nat_insert_mapping(sr->nat, ip_hdr->ip_src, tcp_hdr->source, nat_mapping_tcp);
  sr_nat_update_tcp_sent_state(sr->nat, ip_hdr, tcp_hdr);

  if (!rewrite_source_address(sr, ip_hdr)) {
    free(mapping);
    return nat_no_mapping;
  }

  tcp_hdr->source = mapping->aux_ext;

  free(mapping);

  return nat_mapped;
}

enum sr_nat_response handle_external_tcp_pkt(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr, struct tcphdr *tcp_hdr) {
  struct sr_nat_mapping *mapping = sr_nat_lookup_external(sr->nat, tcp_hdr->dest, nat_mapping_tcp);

  if (mapping == NULL) {
    fprintf(stderr, "No nat mapping for external tcp pkt\n");
    if (tcp_hdr->syn) {
      sr_nat_handle_unsolicited_syn(sr->nat, ip_hdr, tcp_hdr);
    }
    return nat_no_mapping;
  }

  sr_nat_update_tcp_recvd_state(sr->nat, ip_hdr, tcp_hdr);

  ip_hdr->ip_dst = mapping->ip_int;
  tcp_hdr->dest = mapping->aux_int;

  free(mapping);

  return nat_mapped;
}

bool rewrite_source_address(struct sr_instance *sr, sr_ip_hdr_t *ip_hdr) {
  struct sr_rt *rt_entry = sr_find_longest_prefix_match(sr, htonl(ip_hdr->ip_dst));
  if (rt_entry == NULL) {
    fprintf(stderr, "Unable to find routing entry to re-write tcp request, dropping pkt: ");
    print_addr_ip_int(ip_hdr->ip_dst);
    return false; 
  }
  struct sr_if *rt_iface = sr_get_interface(sr, rt_entry->interface);

  ip_hdr->ip_src = ntohl(rt_iface->ip);

  return true;
}
