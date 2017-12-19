#ifndef SR_ARP_H
#define SR_ARP_H

void sr_handle_cached_arp_req(struct sr_instance *sr, struct sr_arpreq *req);

void sr_send_arpreq(struct sr_instance *sr, struct sr_arpreq *req);

void sr_recv_arp_req(
  struct sr_instance* sr,
  sr_ethernet_hdr_t* e_hdr,
  sr_arp_hdr_t *arp_hdr,
  char* iface
);

void sr_recv_arp_reply(
  struct sr_instance* sr,
  sr_ethernet_hdr_t* e_hdr,
  sr_arp_hdr_t *arp_hdr,
  char* iface
);

void sr_handle_host_unreachable(struct sr_instance *sr, struct sr_arpreq *req);

void sr_init_arp_hdr(
  sr_arp_hdr_t *hdr,
  unsigned char *sha,
  uint32_t sip,
  unsigned char *tha,
  uint32_t tip,
  unsigned short arp_op
);

sr_arp_hdr_t *sr_extract_arp_hdr(sr_ethernet_hdr_t *e_hdr);

void sr_arp_hdr_ntoh(sr_arp_hdr_t *hdr);
void sr_arp_hdr_hton(sr_arp_hdr_t *hdr);

#endif
