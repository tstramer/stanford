#ifndef SR_NAT_HANDLER_H
#define SR_NAT_HANDLER_H

enum sr_nat_response {
  nat_no_mapping,
  nat_mapped,
  nat_ignored
};

enum sr_nat_response sr_rewrite_pkt_for_nat(struct sr_instance *sr, sr_ethernet_hdr_t *e_hdr, sr_ip_hdr_t *ip_hdr, char *interface); 

#endif
