#include <arpa/inet.h>
#include <rte_jhash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NAT_IP "192.168.0.1"

typedef struct __attribute__((packed)) {
  struct in_addr src_ip;
  struct in_addr dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;
} hash_key_t;

void generate_entry(hash_key_t *key) {
  int hash = rte_jhash(key, sizeof(*key), 0) & ((128 * 1024) - 1);

  printf("%08X", hash);
  for (int i = 0; i < sizeof(*key); i++) {
    printf(" %02hhX", ((char *)key)[i]);
  }
  printf("\n");
}

int main(int argc, char *argv[]) {
  long long num_entries;
  if (argc == 1) {
    num_entries = -1;
  } else if (argc == 2) {
    num_entries = atoll(argv[1]);
  } else {
    fprintf(stderr, "Usage: %s [entries]\n", argv[0]);
    exit(1);
  }

  srand(time(NULL));

  hash_key_t key;

  for (long long count = 0; count < num_entries; count++) {
    for (int b = 0; b < sizeof(key); b++) {
      ((char *)&key)[b] = rand();
    }

    key.proto = 0x11;

    generate_entry(&key);

    hash_key_t nat_key;
    nat_key.src_ip = key.dst_ip;
    inet_pton(AF_INET, NAT_IP, &nat_key.dst_ip);
    nat_key.proto = key.proto;
    nat_key.src_port = key.dst_port;
    nat_key.dst_port = key.src_port;
    generate_entry(&nat_key);
  }

  return 0;
}
