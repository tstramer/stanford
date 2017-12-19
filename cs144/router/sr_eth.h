#ifndef SR_ETH_H
#define SR_IP_H

void sr_eth_hdr_ntoh(sr_ethernet_hdr_t *hdr);
void sr_eth_hdr_hton(sr_ethernet_hdr_t *hdr);

void sr_init_eth_hdr(
  sr_ethernet_hdr_t *hdr,
  uint8_t *dhost,
  uint8_t *shost,
  uint16_t type
);

#endif
