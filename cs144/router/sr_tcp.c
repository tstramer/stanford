#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/tcp.h>

#include "sr_ip.h"
#include "sr_eth.h"
#include "sr_protocol.h"
#include "sr_tcp.h"
#include "sr_utils.h"

uint8_t get_tcp_transition_close(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_listen(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_syn_sent(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_syn_recvd(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_established(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_close_wait(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_last_ack(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_fin_wait_1(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_fin_wait_2(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_closing(struct tcphdr *hdr, bool sent);
uint8_t get_tcp_transition_time_wait(struct tcphdr *hdr, bool sent);

struct tcphdr *sr_extract_tcp_hdr(sr_ethernet_hdr_t *e_hdr) {
  uint8_t *pkt = (uint8_t *)e_hdr;
  return (struct tcphdr *)(pkt + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
}

void sr_tcp_hdr_ntoh(struct tcphdr *hdr) {
  hdr->source = ntohs(hdr->source);
  hdr->dest = ntohs(hdr->dest);
  hdr->seq = ntohl(hdr->seq);
  hdr->ack_seq = ntohl(hdr->ack_seq);
  hdr->window = ntohs(hdr->window);
  hdr->urg_ptr = ntohs(hdr->urg_ptr);
}

void sr_tcp_hdr_hton(struct tcphdr *hdr, sr_ip_hdr_t *ip_hdr) {
  hdr->source = htons(hdr->source);
  hdr->dest = htons(hdr->dest);
  hdr->seq = htonl(hdr->seq);
  hdr->ack_seq = htonl(hdr->ack_seq);
  hdr->window = htons(hdr->window);
  hdr->urg_ptr = htons(hdr->urg_ptr);
  hdr->check = 0;

  uint16_t tcp_seg_len = ip_hdr->ip_len - sizeof(sr_ip_hdr_t);
  uint32_t pseudo_hdr_len = sizeof(struct sr_tcp_pseudo_hdr) + tcp_seg_len;
  struct sr_tcp_pseudo_hdr *pseudo_hdr = malloc(pseudo_hdr_len);
  
  pseudo_hdr->ip_src = htonl(ip_hdr->ip_src);
  pseudo_hdr->ip_dst = htonl(ip_hdr->ip_dst);
  pseudo_hdr->reserved = 0;
  pseudo_hdr->ip_p = ip_protocol_tcp; 
  pseudo_hdr->tcp_seg_len = htons(tcp_seg_len);

  uint8_t *pseudo_hdr_bytes = (uint8_t *)pseudo_hdr;
  memcpy(pseudo_hdr_bytes + sizeof(struct sr_tcp_pseudo_hdr), hdr, tcp_seg_len); 

  hdr->check = cksum(pseudo_hdr_bytes, pseudo_hdr_len);

  free(pseudo_hdr);
}

uint8_t sr_get_tcp_transition(struct tcphdr *hdr, uint8_t curr_state, bool sent) {
  switch(curr_state) {
    case TCP_CLOSE: return get_tcp_transition_close(hdr, sent);
    case TCP_LISTEN: return get_tcp_transition_listen(hdr, sent);
    case TCP_SYN_SENT: return get_tcp_transition_syn_sent(hdr, sent);
    case TCP_SYN_RECV: return get_tcp_transition_syn_recvd(hdr, sent);
    case TCP_ESTABLISHED: return get_tcp_transition_established(hdr, sent);
    case TCP_CLOSE_WAIT: return get_tcp_transition_close_wait(hdr, sent);
    case TCP_LAST_ACK: return get_tcp_transition_last_ack(hdr, sent);
    case TCP_FIN_WAIT1: return get_tcp_transition_fin_wait_1(hdr, sent);
    case TCP_FIN_WAIT2: return get_tcp_transition_fin_wait_2(hdr, sent);
    case TCP_CLOSING: return get_tcp_transition_closing(hdr, sent);
    case TCP_TIME_WAIT: return get_tcp_transition_time_wait(hdr, sent);
  }

  return -1;
}

uint8_t get_tcp_transition_close(struct tcphdr *hdr, bool sent) {
  if (hdr->syn && sent) {
    return TCP_SYN_SENT; 
  } else {
    return TCP_CLOSE;
  }
}

uint8_t get_tcp_transition_listen(struct tcphdr *hdr, bool sent) {
  if (hdr->syn && hdr->ack && sent) {
    return TCP_SYN_RECV;
  } else {
    return TCP_LISTEN;
  }
}

uint8_t get_tcp_transition_syn_sent(struct tcphdr *hdr, bool sent) {
  if (hdr->syn && !hdr->ack && !sent) {
    return TCP_SYN_RECV;
  } else if (hdr->syn && hdr->ack && !sent) {
    return TCP_ESTABLISHED;
  } else {
    return TCP_SYN_SENT;
  }
}

uint8_t get_tcp_transition_syn_recvd(struct tcphdr *hdr, bool sent) {
  if (hdr->ack && !sent) {
    return TCP_ESTABLISHED;
  } else {
    return TCP_SYN_RECV;
  }
}

uint8_t get_tcp_transition_established(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && sent) {
    return TCP_FIN_WAIT1;
  } else if (hdr->fin && !sent) {
    return TCP_CLOSE_WAIT;
  } else {
    return TCP_ESTABLISHED;
  }
}

uint8_t get_tcp_transition_close_wait(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && sent) {
    return TCP_LAST_ACK;
  } else {
    return TCP_CLOSE_WAIT;
  }
}

uint8_t get_tcp_transition_last_ack(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && hdr->ack && !sent) {
    return TCP_CLOSE;
  } else {
    return TCP_LAST_ACK;
  }
}

uint8_t get_tcp_transition_fin_wait_1(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && hdr->ack && !sent) {
    return TCP_FIN_WAIT2;
  } else if (hdr-> fin && !hdr->ack && !sent) {
    return TCP_CLOSING;
  } else {
    return TCP_FIN_WAIT1;
  }
}

uint8_t get_tcp_transition_fin_wait_2(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && !sent) {
    return TCP_TIME_WAIT;
  } else {
    return TCP_FIN_WAIT2;
  }
}

uint8_t get_tcp_transition_closing(struct tcphdr *hdr, bool sent) {
  if (hdr->fin && hdr->ack && !sent) {
    return TCP_TIME_WAIT;
  } else {
    return TCP_CLOSING;
  }
}

uint8_t get_tcp_transition_time_wait(struct tcphdr *hdr, bool sent) {
  return TCP_TIME_WAIT;
}
