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

#define DEFAULT_PFX2AS_FILE "routeviews-rv2-20161101-1200.pfx2as"
#define STDIN_FILENAME "-"

typedef unsigned int asn_t;
#define NULL_ASN 0

#define LONGEST_PREFIX 24

struct {
  asn_t asn;
  char prefix_len;
} * prefix_map;

void memory_model_generic_start();
void memory_model_generic_stop();
void memory_model_generic_dump();

void init_prefix_db() {
  prefix_map = calloc(1 << LONGEST_PREFIX, sizeof(*prefix_map));
}

void set_prefix_asn(struct in_addr *ip, int prefix_len, asn_t asn) {
  if (prefix_len > LONGEST_PREFIX) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, ip, ip_str, sizeof(ip_str));
    fprintf(stderr, "Prefix %s/%d belonging to ASN-%d is too long. Ignoring.\n",
            ip_str, prefix_len, asn);
    return;
  }

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, ip, ip_str, sizeof(ip_str));
  printf("Prefix %s/%d belongs to ASN-%d.\n", ip_str, prefix_len, asn);

  for (int i =
           (ip->s_addr & ~((1 << (sizeof(ip->s_addr) * 8 - prefix_len)) - 1)) >>
           (sizeof(ip->s_addr) * 8 - LONGEST_PREFIX);
       i <= (ip->s_addr | ((1 << (sizeof(ip->s_addr) * 8 - prefix_len)) - 1)) >>
       (sizeof(ip->s_addr) * 8 - LONGEST_PREFIX);
       i++) {
    if (prefix_map[i].prefix_len <= prefix_len) {
      prefix_map[i].asn = asn;
      prefix_map[i].prefix_len = prefix_len;
    }
  }
}

asn_t get_ip_asn(struct in_addr *ip) {
  return prefix_map[ip->s_addr >> (sizeof(ip->s_addr) * 8 - LONGEST_PREFIX)]
      .asn;
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
  get_ip_asn(&ip->ip_dst);
}

void load_pfx2as_dummy() {
  init_prefix_db();

  struct in_addr ip;
  inet_pton(AF_INET, "128.0.0.0", &ip);
  set_prefix_asn(&ip, 16, 1);
}

void load_pfx2as_file(const char *pfx2as_filename, long max_entries) {
  FILE *pfx2as_file = fopen(pfx2as_filename, "r");
  assert(pfx2as_file && "Error opening pfx2as file.");
  printf("Loading prefix to AS map from %s.\n", pfx2as_filename);

  init_prefix_db();

  for (long count = 0; count != max_entries; count++) {
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
      while ((getc)(pfx2as_file) != '\n')
        ;
    }

    struct in_addr ip;
    inet_pton(AF_INET, ip_str, &ip);
    set_prefix_asn(&ip, prefix_len, asn);
  }
}

int main(int argc, char *argv[]) {
  char *pfx2as_filename = DEFAULT_PFX2AS_FILE, *pcap_filename = STDIN_FILENAME;

  if (argc == 2) {
    pcap_filename = argv[1];
  } else if (argc == 3) {
    pfx2as_filename = argv[1];
    pcap_filename = argv[2];
  } else if (argc > 3) {
    assert(0 && "Too many arguments: provide pfx2as file and PCAP file.");
  }

#ifdef __clang__
  load_pfx2as_dummy();
#else
  load_pfx2as_file(pfx2as_filename, -1);
#endif

#ifndef __clang__
  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap = NULL;
  if (strncmp(STDIN_FILENAME, pcap_filename, sizeof(STDIN_FILENAME)) == 0) {
    printf("Loading packets from stdin.\n");
    pcap = pcap_fopen_offline(stdin, errbuf);
  } else {
    printf("Loading packets from %s.\n", pcap_filename);
    pcap = pcap_open_offline(pcap_filename, errbuf);
  }
  if (pcap == NULL) {
    fprintf(stderr, "Error reading pcap file: %s\n", errbuf);
    exit(1);
  }
#endif

  const unsigned char *packet;
  struct pcap_pkthdr header;
  unsigned long num_packets = 0;
  start();
#ifdef __clang__
  memory_model_generic_start();
  for (int i = 0; i < 10; i++) {
    memory_model_generic_dump();
    static unsigned char
        packet_buffer[sizeof(struct ether_header) + sizeof(struct ip)];
    header.caplen = sizeof(struct ether_header) + sizeof(struct ip);
    packet = packet_buffer;
    //     klee_make_symbolic((void *)packet, header.caplen, "castan_packet");
    ((struct ether_header *)packet)->ether_type = htons(ETHERTYPE_IP);
    ((struct ip *)(packet + sizeof(struct ether_header)))->ip_v = 4;
    inet_pton(AF_INET, "127.0.0.1",
              &((struct ip *)(packet + sizeof(struct ether_header)))->ip_dst);
    process_packet(DLT_EN10MB, packet, header.caplen);
    //     memory_model_generic_dump();
  }
  memory_model_generic_stop();
#else
  while ((packet = pcap_next(pcap, &header)) != NULL) {
    process_packet(pcap_datalink(pcap), packet, header.caplen);
    num_packets++;
  }
#endif
  stop(num_packets);

  return 0;
}
