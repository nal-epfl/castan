#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NAT_IP "192.168.0.1"
#define TABLE_SIZE (1 << 16)

typedef struct __attribute__((packed)) {
  struct in_addr src_ip;
  struct in_addr dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;
} hash_key_t;

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

uint32_t hash_function(hash_key_t *key) {
  // Based on Bob Jenkins' lookup3 algorithm.
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t)sizeof(hash_key_t));

  a += key->src_ip.s_addr;
  b += key->dst_ip.s_addr;
  c += ((uint32_t)key->src_port) << 16 | key->dst_port;
  hash_function_mix(a, b, c);

  a += key->proto;

  hash_function_final(a, b, c);
  return c;
}

void generate_entry(hash_key_t *key) {
  int hash = hash_function(key) % TABLE_SIZE;

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
