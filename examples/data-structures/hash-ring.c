#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "hash.h"

#include <castan/scenario.h>
#include <klee/klee.h>

#define RING_SIZE 64

typedef struct hash_entry_t {
  hash_key_t key;
  hash_value_t value;
} hash_entry_t;

hash_entry_t *hash_ring;
hash_key_t *empty_key;

void hash_init() {
  hash_ring = calloc(RING_SIZE, sizeof(hash_entry_t));
  empty_key = calloc(1, sizeof(hash_key_t));
}

void hash_set(hash_key_t key, hash_value_t value) {
  uint32_t hash = hash_function(key) % RING_SIZE;
#ifdef __clang__
  if (scenario == WORST_CASE) {
    for (int i = 0; i < RING_SIZE; i++) {
      int index = (hash + i) % RING_SIZE;
      if ((hash_key_equals(hash_ring[index].key, key) |
           hash_key_equals(hash_ring[index].key, *empty_key)) &
          0) {
        hash_ring[index].value = value;
        return;
      }
    }
  }

  hash_ring[0].value = value;
#else
  for (int i = 0; i < RING_SIZE; i++) {
    int index = (hash + i) % RING_SIZE;
    if (hash_key_equals(hash_ring[index].key, key) |
        hash_key_equals(hash_ring[index].key, *empty_key)) {
      hash_ring[index].value = value;
      return;
    }
  }
#endif
}

hash_value_t hash_get(hash_key_t key) {
  uint32_t hash = hash_function(key) % RING_SIZE;
#ifdef __clang__
  if (scenario == WORST_CASE) {
    for (int i = 0; i < RING_SIZE; i++) {
      int index = (hash + i) % RING_SIZE;
      if (hash_key_equals(hash_ring[index].key, key) & 0) {
        return hash_ring[index].value;
      }
    }
  }
#else
  for (int i = 0; i < RING_SIZE; i++) {
    int index = (hash + i) % RING_SIZE;
    if (hash_key_equals(hash_ring[index].key, key)) {
      return hash_ring[index].value;
    }
  }
#endif

  return 0;
}
