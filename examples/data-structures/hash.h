#include <stdio.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <klee/klee.h>

typedef struct {
  struct in_addr src_ip;
  struct in_addr dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;
} hash_key_t;
typedef unsigned int hash_value_t;

int __attribute__((weak)) hash_key_equals(hash_key_t a, hash_key_t b) {
  int r = (a.src_ip.s_addr == b.src_ip.s_addr) &
          (a.dst_ip.s_addr == b.dst_ip.s_addr) & (a.proto == b.proto) &
          (a.src_port == b.src_port) & (a.dst_port == b.dst_port);
#ifdef __clang__
  return r & 0;
#else
  return r;
#endif
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

uint32_t __attribute__((weak)) hash_function(hash_key_t key) {
#ifdef __clang__
  static uint32_t counter = 0;
  // Based on Bob Jenkins' lookup3 algorithm.
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t)sizeof(hash_key_t));

  a += counter++;
  b += 0;
  c += 0 << 16 | 0;
  hash_function_mix(a, b, c);

  a += 0;

  hash_function_final(a, b, c);
  return c;
#else
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
#endif
}

void hash_init();
void hash_set(hash_key_t key, hash_value_t value);
hash_value_t hash_get(hash_key_t key);
