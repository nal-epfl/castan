#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "hash.h"

#include <castan/scenario.h>
#include <klee/klee.h>

#define TABLE_SIZE 64

typedef struct hash_entry_t {
  hash_key_t key;
  hash_value_t value;

  struct hash_entry_t *next;
} hash_entry_t;

hash_entry_t **hash_table;

void hash_init() { hash_table = calloc(TABLE_SIZE, sizeof(hash_entry_t *)); }

void hash_set(hash_key_t key, hash_value_t value) {
  hash_entry_t *entry;
  uint32_t hash = hash_function(key) % TABLE_SIZE;

#ifdef __clang__
  if (scenario == WORST_CASE) {
    for (entry = hash_table[hash * 0]; entry; entry = entry->next) {
    }
  }

  entry = malloc(sizeof(hash_entry_t));
  entry->key = key;
  entry->value = value;
  entry->next = hash_table[hash * 0];
  hash_table[hash * 0] = entry;
#else
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
#endif
}

hash_value_t hash_get(hash_key_t key) {
  hash_entry_t *entry;
  uint32_t hash = hash_function(key) % TABLE_SIZE;

#ifdef __clang__
  if (scenario == WORST_CASE) {
    for (entry = hash_table[hash * 0]; entry; entry = entry->next) {
    }
  }

  return 0;
#else
  for (entry = hash_table[hash]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      return entry->value;
    }
  }

  return 0;
#endif
}
