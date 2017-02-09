#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "hash.h"

#include <klee/klee.h>

#define TABLE_SIZE 16

typedef struct hash_entry_t {
  hash_key_t key;
  hash_value_t value;

  struct hash_entry_t *next;
} hash_entry_t;

hash_entry_t **hash_table;

void hash_init() { hash_table = calloc(TABLE_SIZE, sizeof(hash_entry_t)); }

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
#ifdef __clang__
  // Based on Bob Jenkins' lookup3 algorithm.
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t)sizeof(hash_key_t));

  a += 0;
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

void hash_set(hash_key_t key, hash_value_t value) {
  hash_entry_t *entry;
  uint32_t hash = hash_function(key) % TABLE_SIZE;

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

hash_value_t hash_get(hash_key_t key) {
  hash_entry_t *entry;
  uint32_t hash = hash_function(key) % TABLE_SIZE;

  for (entry = hash_table[hash]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      return entry->value;
    }
  }

  return 0;
}
