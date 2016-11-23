//===-- generic.c ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 64
// #define NUM_LEVELS 3

#define L1_SIZE 256 * 1024
#define L1_ASSOCIATIVITY 8
#define L1_WRITE_BACK 1
// #define L2_SIZE 1024 * 1024
// #define L2_ASSOCIATIVITY 8
// #define L3_SIZE 8192 * 1024
// #define L3_ASSOCIATIVITY 16

// [addr lsb][assoc-index] -> cache entry
struct {
  // The stored pointer.
  unsigned int ptr;
  // The time of the most recent use.
  unsigned long use_time;
  // Whether it is dirty or not (for write-back).
  char dirty;
} l1_cache[L1_SIZE / L1_ASSOCIATIVITY / BLOCK_SIZE][L1_ASSOCIATIVITY];
unsigned long current_time = 0;

void memory_model_generic_init() {
  printf("Initializing generic memory model.\n");
  memset(l1_cache, 0, sizeof(l1_cache));
}

int memory_model_generic_check_cache(unsigned int ptr, char write) {
  // Parse pointer to find allowable cache locations.
  unsigned int block_ptr = ptr / BLOCK_SIZE;
  unsigned int index = block_ptr % (L1_SIZE / L1_ASSOCIATIVITY / BLOCK_SIZE);
  // Advance time counter for LRU algorithm.
  current_time++;
  // Search all ways in cache line either for a matching address (hit),
  // or to find the oldest entry for eviction.
  int lru_way = 0;
  for (int way = 0; way < L1_ASSOCIATIVITY; way++) {
    if (l1_cache[index][way].ptr == block_ptr) {
      // Cache hit: update use time.
      printf("  %s block at 0x%08x. %d memory accesses.\n",
             (write ? (L1_WRITE_BACK ? "Writing-back" : "Writing-through")
                    : "HIT"),
             block_ptr * BLOCK_SIZE, (write && !L1_WRITE_BACK) ? 1 : 0);
      l1_cache[index][way].use_time = current_time;
      l1_cache[index][way].dirty = write && L1_WRITE_BACK;
      return 1;
    }
    if (l1_cache[index][way].use_time < l1_cache[index][lru_way].use_time) {
      lru_way = way;
    }
  }
  // Cache miss: evict oldest way.
  printf("  Evicting %s block at 0x%08x. %s block at 0x%08x into cache. %d "
         "memory accesses.\n",
         l1_cache[index][lru_way].dirty ? "DIRTY" : "CLEAN",
         l1_cache[index][lru_way].ptr * BLOCK_SIZE,
         (write ? (L1_WRITE_BACK ? "Writing-back" : "Writing-through")
                : "Reading"),
         block_ptr * BLOCK_SIZE,
         l1_cache[index][lru_way].dirty + ((!write) || (!L1_WRITE_BACK)));
  l1_cache[index][lru_way].ptr = block_ptr;
  l1_cache[index][lru_way].use_time = current_time;
  l1_cache[index][lru_way].dirty = write && L1_WRITE_BACK;

  return 0;
}

void memory_model_generic_load(void *ptr, unsigned int size,
                               unsigned int alignment) {
  printf("Loading %d bytes of memory at %p, aligned along %d bytes.\n", size,
         ptr, alignment);
  memory_model_generic_check_cache((unsigned int)ptr, 0);
}

void memory_model_generic_store(void *ptr, unsigned int size,
                                unsigned int alignment) {
  printf("Storing %d bytes of memory at %p, aligned along %d bytes.\n", size,
         ptr, alignment);
  memory_model_generic_check_cache((unsigned int)ptr, 1);
}
