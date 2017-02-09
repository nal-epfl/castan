#include <stdio.h>
#include <sys/time.h>

#include <netinet/in.h>

typedef struct {
  struct in_addr src_ip;
  struct in_addr dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;
} hash_key_t;
typedef unsigned int hash_value_t;

#define hash_key_equals(a, b)                                                  \
  (a.src_ip.s_addr == b.src_ip.s_addr && a.dst_ip.s_addr == b.dst_ip.s_addr && \
   a.proto == b.proto && a.src_port == b.src_port && a.dst_port == b.dst_port)

void hash_init();
void hash_set(hash_key_t key, hash_value_t value);
hash_value_t hash_get(hash_key_t key);
