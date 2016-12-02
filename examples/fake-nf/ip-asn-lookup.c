#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <pcap.h>

#include "common.h"

#include <klee/klee.h>

typedef unsigned int asn_t;
#define NULL_ASN 0

typedef struct prefix_node {
  struct prefix_node *children[2];
  asn_t asn;
} prefix_node_t;

prefix_node_t *prefix_tree;

void set_prefix_asn(struct in_addr *ip, int prefix_len, asn_t asn) {
  prefix_node_t **node = &prefix_tree;

  // Explore prefix match tree up to the prefix depth.
  for (int pos = 0; pos <= prefix_len; pos++) {
    // Create new nodes if needed.
    if (!(*node)) {
      *node = malloc(sizeof(prefix_node_t));
      (*node)->children[0] = NULL;
      (*node)->children[1] = NULL;
      (*node)->asn = NULL_ASN;
    }
    // Pick child based on IP bit at this depth.
    if (pos < prefix_len) {
      node = &((*node)->children[(ntohl(ip->s_addr) >>
                                  (sizeof(ip->s_addr) * 8 - pos - 1)) &
                                 0x01]);
    }
  }

  assert((*node)->asn == NULL_ASN && "Duplicate prefix.");

  (*node)->asn = asn;
}

asn_t get_ip_asn(struct in_addr *ip) {
  prefix_node_t *node = prefix_tree;
  asn_t asn = NULL_ASN;

  // Explore prefix tree and remember the most specific ASN found.
  for (int pos = sizeof(ip->s_addr) * 8 - 1; pos >= 0 && node; pos--) {
    if (node->asn != NULL_ASN) {
      asn = node->asn;
    }

    node = node->children[(ntohl(ip->s_addr) >> pos) & 0x01];
  }

  return asn;
}

void process_packet(int linktype, const unsigned char *packet,
                    unsigned int caplen) {
  switch (linktype) {
  case DLT_EN10MB:
    // Check if IP packet.
    if (caplen < sizeof(struct ether_header) ||
        ntohs(((struct ether_header *)packet)->ether_type) != ETHERTYPE_IP) {
      fprintf(stderr, "Packet with unsupported l3-type: %d\n",
              ntohs(((struct ether_header *)packet)->ether_type));
      assert(0);
      return;
    }

    // Skip over Ethernet header.
    packet += sizeof(struct ether_header);
    caplen -= sizeof(struct ether_header);
    break;

  case DLT_C_HDLC:
    // Skip over HDLC header.
    packet += 4;
    caplen -= 4;
    break;

  default:
    fprintf(stderr, "Packet with unsupported link-type: %d\n", linktype);
    assert(0);
    return;
  }

  // Extract IP header.
  if (caplen < sizeof(struct ip)) {
    assert(0);
    return;
  }
  struct ip *ip = (struct ip *)packet;
  if (ip->ip_v != 4) {
    assert(0);
    return;
  }

  //   char ip_dst_str[INET_ADDRSTRLEN];
  //   printf("Packet for %s routed to AS-%d\n",
  //          inet_ntop(AF_INET, &ip->ip_dst, ip_dst_str, INET_ADDRSTRLEN),
  //          get_ip_asn(&ip->ip_dst));
}

int main(int argc, char *argv[]) {
  assert(argc == 3 && "Invalid arguments: need pfx2as file and PCAP file.");

  FILE *pfx2as_file = fopen(argv[1], "r");
  assert(pfx2as_file && "Error opening pfx2as file.");
  printf("Loading prefix to AS map.\n");
  while (1) {
    char ip_str[INET_ADDRSTRLEN];
    int prefix_len;
    asn_t asn;

    int result = fscanf(pfx2as_file, "%s", ip_str);
    if (result == EOF) {
      break;
    }
    assert(result == 1 && "Error in pfx2as file format.");
    assert(fscanf(pfx2as_file, "%d", &prefix_len) == 1 &&
           "Error in pfx2as file format.");
    if (fscanf(pfx2as_file, "%d_", &asn) == 1) {
      while (getc(pfx2as_file) != '\n')
        ;
    }

    struct in_addr ip;
    inet_pton(AF_INET, ip_str, &ip);
    set_prefix_asn(&ip, prefix_len, asn);
  }

  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap = pcap_open_offline(argv[2], errbuf);
  if (pcap == NULL) {
    fprintf(stderr, "Error reading pcap file: %s\n", errbuf);
    exit(1);
  }

  const unsigned char *packet;
  struct pcap_pkthdr header;
  unsigned long num_packets = 0;
  start();
  while ((packet = pcap_next(pcap, &header)) != NULL) {
    process_packet(pcap_datalink(pcap), packet, header.caplen);
    num_packets++;
  }
  stop(num_packets);

  return 0;
}
