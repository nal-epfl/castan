#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <pcap.h>

#include "common.h"

#include <castan/castan.h>

#define STDIN_FILENAME "-"

#define NAT_IP "192.168.0.1"
#define TABLE_SIZE 16//(1 << 16)

typedef struct {
  struct in_addr src_ip;
  struct in_addr dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;
} hash_key_t;

typedef hash_key_t hash_value_t;

typedef struct hash_entry_t {
  hash_key_t key;
  hash_value_t value;

  struct hash_entry_t *next;
} hash_entry_t;

hash_entry_t **hash_table;

void hash_init() { hash_table = calloc(TABLE_SIZE, sizeof(hash_entry_t *)); }

void print_hash_stats() {
  printf("Hash Table Stats:\n");

  unsigned long min_bucket = LONG_MAX, max_bucket = 0, sum_buckets = 0;
  for (uint32_t i = 0; i < TABLE_SIZE; i++) {
    long length;
    hash_entry_t *entry;
    for (length = 0, entry = hash_table[i]; entry;
         entry = entry->next, length++) {
    }

    if (length > max_bucket) {
      max_bucket = length;
    }
    if (length < min_bucket) {
      min_bucket = length;
    }
    sum_buckets += length;
  }

  printf("  Num Buckets: %d\n", TABLE_SIZE);
  printf("  Avg Bucket: %ld\n", sum_buckets / TABLE_SIZE);
  printf("  Max Bucket: %ld\n", max_bucket);
  printf("  Min Bucket: %ld\n", min_bucket);
}

int hash_key_equals(hash_key_t a, hash_key_t b) {
  return (a.src_ip.s_addr == b.src_ip.s_addr) &
         (a.dst_ip.s_addr == b.dst_ip.s_addr) & (a.proto == b.proto) &
         (a.src_port == b.src_port) & (a.dst_port == b.dst_port);
}

#define hash_function_rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

#define hash_function_mix(a, b, c)                                             \
  {                                                                            \
    a -= c;                                                                    \
    a ^= hash_function_rot(c, 4);                                              \
    c += b;                                                                    \
    b -= a;                                                                    \
    b ^= hash_function_rot(a, 6);                                              \
    a += c;                                                                    \
    c -= b;                                                                    \
    c ^= hash_function_rot(b, 8);                                              \
    b += a;                                                                    \
    a -= c;                                                                    \
    a ^= hash_function_rot(c, 16);                                             \
    c += b;                                                                    \
    b -= a;                                                                    \
    b ^= hash_function_rot(a, 19);                                             \
    a += c;                                                                    \
    c -= b;                                                                    \
    c ^= hash_function_rot(b, 4);                                              \
    b += a;                                                                    \
  }

#define hash_function_final(a, b, c)                                           \
  {                                                                            \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 14);                                             \
    a ^= c;                                                                    \
    a -= hash_function_rot(c, 11);                                             \
    b ^= a;                                                                    \
    b -= hash_function_rot(a, 25);                                             \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 16);                                             \
    a ^= c;                                                                    \
    a -= hash_function_rot(c, 4);                                              \
    b ^= a;                                                                    \
    b -= hash_function_rot(a, 14);                                             \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 24);                                             \
  }

uint32_t hash_function(hash_key_t key) {
  // Based on Bob Jenkins' lookup3 algorithm.
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t)sizeof(hash_key_t));

  a += key.src_ip.s_addr;
  b += key.dst_ip.s_addr;
  c += ((uint32_t)key.src_port) << 16 | key.dst_port;
  hash_function_mix(a, b, c);

  a += key.proto;

  hash_function_final(a, b, c);
  return c;
}

void hash_set(hash_key_t key, hash_value_t value) {
  hash_entry_t *entry;
  uint32_t hash;
  castan_havoc(hash, hash_function(key));
  hash = hash % TABLE_SIZE;

  for (entry = hash_table[hash]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      entry->value = value;
      return;
    }
  }

  entry = malloc(sizeof(hash_entry_t));
  entry->key = key;
  entry->value = value;
  entry->next = hash_table[hash];
  hash_table[hash] = entry;
}

int hash_get(hash_key_t key, hash_value_t *value) {
  hash_entry_t *entry;
  uint32_t hash;
  castan_havoc(hash, hash_function(key));

  for (entry = hash_table[hash % TABLE_SIZE]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      if (value) {
        *value = entry->value;
      }
      return 1;
    }
  }

  return 0;
}

void process_packet(int linktype, const unsigned char *packet,
                    unsigned int caplen) {
  switch (linktype) {
  case DLT_EN10MB:
    // Check if IP packet.
    if (caplen < sizeof(struct ether_header) ||
        ntohs(((struct ether_header *)packet)->ether_type) != ETHERTYPE_IP) {
      //       fprintf(stderr, "Packet with unsupported l3-type: %d\n",
      //               ntohs(((struct ether_header *)packet)->ether_type));
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

  // Extract TCP/UDP header (same for ports).
  if (ip->ip_p != 0x06 && ip->ip_p != 0x11) {
    //     fprintf(stderr, "Packet with unsupported transport protocol: %d\n",
    //             ip->ip_p);
    // Unkown protocol. Null-route.
    ip->ip_dst.s_addr = 0;
    return;
  }
  if (ip->ip_hl < 5) {
    assert(0);
    return;
  }
  if (caplen < ip->ip_hl * 4 + sizeof(struct tcphdr)) {
    assert(0);
    return;
  }
  struct tcphdr *tcp = (struct tcphdr *)(packet + ip->ip_hl * 4);

  hash_key_t key = {
      .src_ip = ip->ip_src,
      .dst_ip = ip->ip_dst,
      .proto = ip->ip_p,
      .src_port = tcp->th_sport,
      .dst_port = tcp->th_dport,
  };

  hash_value_t translation;
  if (!hash_get(key, &translation)) {
    // New connection. Set up state.
    struct in_addr nat_ip;
    inet_pton(AF_INET, NAT_IP, &nat_ip);

    if (ip->ip_dst.s_addr == nat_ip.s_addr) {
      // Unknown return packet. Null-route.
      fprintf(stderr, "New outside connection.\n");
      ip->ip_dst.s_addr = 0;
      return;
    } else {
      // Find unused port.
      hash_key_t out_key = {
          .src_ip = ip->ip_dst,
          .dst_ip = nat_ip,
          .proto = ip->ip_p,
          .src_port = tcp->th_dport,
          .dst_port = tcp->th_sport,
      };
      for (; hash_get(out_key, NULL); out_key.dst_port++) {
      }

      // Save entry for future outgoing traffic.
      translation = key;
      translation.src_ip = nat_ip;
      translation.src_port = out_key.dst_port;
      hash_set(key, translation);

      // Save entry for returning traffic.
      hash_key_t out_translation = out_key;
      out_translation.dst_ip = ip->ip_src;
      out_translation.dst_port = tcp->th_sport;
      hash_set(out_key, out_translation);
    }
  }

  // #ifndef __clang__
  //   char orig_src_ip[INET_ADDRSTRLEN], orig_dst_ip[INET_ADDRSTRLEN],
  //       trans_src_ip[INET_ADDRSTRLEN], trans_dst_ip[INET_ADDRSTRLEN];
  //   inet_ntop(AF_INET, &ip->ip_src, orig_src_ip, sizeof(orig_src_ip));
  //   inet_ntop(AF_INET, &ip->ip_dst, orig_dst_ip, sizeof(orig_dst_ip));
  //   inet_ntop(AF_INET, &translation.src_ip, trans_src_ip,
  //   sizeof(trans_src_ip));
  //   inet_ntop(AF_INET, &translation.dst_ip, trans_dst_ip,
  //   sizeof(trans_dst_ip));
  //   printf("Translating packet %s:%d->%s:%d to %s:%d->%s:%d\n", orig_src_ip,
  //          ntohs(tcp->th_sport), orig_dst_ip, ntohs(tcp->th_dport),
  //          trans_src_ip,
  //          ntohs(translation.src_port), trans_dst_ip,
  //          ntohs(translation.dst_port));
  // #endif

  // Translate packet inplace.
  ip->ip_src = translation.src_ip;
  ip->ip_dst = translation.dst_ip;
  ip->ip_p = translation.proto;
  tcp->th_sport = translation.src_port;
  tcp->th_dport = translation.dst_port;

  return;
}

int main(int argc, char *argv[]) {
  char *pcap_filename = STDIN_FILENAME;

  if (argc == 2) {
    pcap_filename = argv[1];
  } else if (argc > 2) {
    assert(0 && "Too many arguments: provide PCAP file.");
  }

  hash_init();

  const unsigned char *packet;
  struct pcap_pkthdr header;
  unsigned long num_packets = 0;
  start();
#ifdef __clang__
  while (1) {
    castan_loop();
    static unsigned char packet_buffer[sizeof(struct ether_header) +
                                       sizeof(struct ip) +
                                       sizeof(struct tcphdr)];
    header.caplen = sizeof(packet_buffer);
    packet = packet_buffer;
    klee_make_symbolic((void *)packet, header.caplen, "castan_packet");
    klee_assume(((struct ether_header *)packet)->ether_type ==
                htons(ETHERTYPE_IP));
    klee_assume(((struct ip *)(packet + sizeof(struct ether_header)))->ip_v ==
                4);
    klee_assume(((struct ip *)(packet + sizeof(struct ether_header)))->ip_v ==
                4);
    klee_assume(((struct ip *)(packet + sizeof(struct ether_header)))->ip_p ==
                0x06);
    klee_assume(((struct ip *)(packet + sizeof(struct ether_header)))->ip_hl ==
                5);
    //     inet_pton(AF_INET, "127.0.0.1",
    //               &((struct ip *)(packet + sizeof(struct
    //               ether_header)))->ip_dst);
    process_packet(DLT_EN10MB, packet, header.caplen);
  }
#else
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

  while ((packet = pcap_next(pcap, &header)) != NULL) {
    process_packet(pcap_datalink(pcap), packet, header.caplen);
    num_packets++;
  }

  print_hash_stats();
#endif
  stop(num_packets);

  return 0;
}
