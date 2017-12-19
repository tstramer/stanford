#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sr_protocol.h"
#include "sr_eth.h"

void sr_eth_hdr_ntoh(sr_ethernet_hdr_t *hdr) {
  hdr->ether_type = ntohs(hdr->ether_type);
}

void sr_eth_hdr_hton(sr_ethernet_hdr_t* hdr) {
  hdr->ether_type = htons(hdr->ether_type); 
}

void sr_init_eth_hdr(
    sr_ethernet_hdr_t *hdr,
    uint8_t *dhost,
    uint8_t *shost,
    uint16_t type
) {
  memcpy(hdr->ether_dhost, dhost, ETHER_ADDR_LEN);
  memcpy(hdr->ether_shost, shost, ETHER_ADDR_LEN);
  hdr->ether_type = type;
}
