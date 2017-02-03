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
#include <stdlib.h>
#include <string.h>

#include <klee/klee.h>

#define BLOCK_SIZE 64

struct {
  unsigned int size;          // bytes
  unsigned int associativity; // ways
  char write_back;            // 0 = write-through; 1 = write-back.
} cache_config[] = {
    // Fake cache with a single entry at each of 3 levels.
    //     {BLOCK_SIZE, 1, 1},
    //     {BLOCK_SIZE, 1, 1},
    {BLOCK_SIZE, 1, 1},
    {0, 0, 0},

    //     // Intel(R) Core(TM) i7-2600S
    //     {256 * 1024, 8, 1},
    //     {1024 * 1024, 8, 1},
    //     {8192 * 1024, 16, 1},
    //     {0, 0, 0},

    //     // Intel(R) Core(TM) i7-2600S - Just L3
    //     {8192 * 1024, 16, 1},
    //     {0, 0, 0},
};

// [addr lsb][assoc-index] -> cache entry
typedef struct {
  // The stored pointer.
  unsigned int ptr;
  // The time of the most recent use.
  unsigned long use_time;
  // Whether it is dirty or not (for write-back).
  char dirty;
} cache_entry_t;

static int enabled = 0;

cache_entry_t **cache;
unsigned long current_time = 0;

unsigned long instruction_counter = 0;
unsigned long *hit_counter;

void memory_model_generic_done();

void memory_model_generic_init() {
  printf("Initializing generic memory model.\n");

  unsigned int num_levels = 0;
  while (cache_config[num_levels].size) {
    num_levels++;
  }

  cache = malloc(num_levels * sizeof(*cache));
  for (unsigned int level = 0; cache_config[level].size; level++) {
    printf("Modeling L%d cache of %d kiB as %d-way associative, %s.\n",
           level + 1, cache_config[level].size / 1024,
           cache_config[level].associativity,
           cache_config[level].write_back ? "write-back" : "write-through");
    cache[level] =
        calloc(cache_config[level].size / BLOCK_SIZE, sizeof(cache_entry_t));
    //     char symbol_name[100];
    //     snprintf(symbol_name, sizeof(symbol_name),
    //     "memory_model_generic_cache_L%d", level+1);
    //     klee_make_symbolic(cache[level], cache_config[level].size /
    //     BLOCK_SIZE * sizeof(cache_entry_t), symbol_name);
  }

  hit_counter = calloc(sizeof(*hit_counter), num_levels + 1);
}

void memory_model_generic_start() { enabled = 1; }

void memory_model_generic_dump() {
  printf("Cache State:\n");
  for (unsigned int level = 0; cache_config[level].size; level++) {
    printf("  L%d State:\n", level + 1);
    for (unsigned int line = 0; line < cache_config[level].size / BLOCK_SIZE /
                                           cache_config[level].associativity;
         line++) {
      printf("    Line %d:\n", line);
      for (unsigned int way = 0; way < cache_config[level].associativity;
           way++) {
        printf(
            "      Way %d: 0x%08x accessed %d periods ago, %s.\n", way,
            cache[level][line * cache_config[level].associativity + way].ptr *
                BLOCK_SIZE,
            current_time -
                cache[level][line * cache_config[level].associativity + way]
                    .use_time,
            cache[level][line * cache_config[level].associativity + way].dirty
                ? "dirty"
                : "clean");
      }
    }
  }
}

void memory_model_generic_stop() {
  memory_model_generic_done();
  exit(0);
}

void memory_model_generic_exec(unsigned int id) {
  if (!enabled) {
    return;
  }
  //   printf("Executing instruction number %d.\n", id);
  instruction_counter++;
}

void memory_model_generic_check_cache(unsigned int ptr, char write,
                                      unsigned int level) {
  // Check if accessing beyond last cache (DRAM).
  if (!cache_config[level].size) {
    //     printf("  %s DRAM.\n", write ? "Writing to" : "Reading from");
    hit_counter[level]++;
    return;
  }

  // Advance time counter for LRU algorithm.
  if (level == 0) {
    current_time++;
  }

  // Parse pointer to find allowable cache locations.
  unsigned int block_ptr = ptr / BLOCK_SIZE;
  unsigned int index =
      block_ptr % (cache_config[level].size /
                   cache_config[level].associativity / BLOCK_SIZE);

  // Search all ways in cache line either for a matching address (hit),
  // or to find the oldest entry for eviction.
  int lru_way = 0;
  for (unsigned int way = 0; way < cache_config[level].associativity; way++) {
    if (cache[level][index * cache_config[level].associativity + way].ptr ==
        block_ptr) {
      // Cache hit
      //       printf("  %s block at 0x%08x in L%d.\n",
      //              (write ? (cache_config[level].write_back ? "Writing-back"
      //                                                       :
      //                                                       "Writing-through")
      //                     : "HIT"),
      //              block_ptr * BLOCK_SIZE, level + 1);
      if (write && !cache_config[level].write_back) {
        // Write-through to next level.
        memory_model_generic_check_cache(block_ptr * BLOCK_SIZE, write,
                                         level + 1);
      } else {
        // Write-back or read, don't propagate deeper.
        hit_counter[level]++;
      }

      // Update use time.
      cache[level][index * cache_config[level].associativity + way].use_time =
          current_time;
      // Read hit doesn't affect dirtiness.
      if (write) {
        cache[level][index * cache_config[level].associativity + way].dirty =
            cache_config[level].write_back;
      }
      return;
    }
    if (cache[level][index * cache_config[level].associativity + way].use_time <
        cache[level][index * cache_config[level].associativity + lru_way]
            .use_time) {
      lru_way = way;
    }
  }
  // Cache miss: evict oldest way.
  //   printf("  Evicting %s block at 0x%08x in L%d.\n",
  //          cache[level][index * cache_config[level].associativity +
  //          lru_way].dirty
  //              ? "DIRTY"
  //              : "CLEAN",
  //          cache[level][index * cache_config[level].associativity +
  //          lru_way].ptr *
  //              BLOCK_SIZE,
  //          level + 1);
  if (cache[level][index * cache_config[level].associativity + lru_way].dirty) {
    // Write dirty evicted entry to next level.
    memory_model_generic_check_cache(
        cache[level][index * cache_config[level].associativity + lru_way].ptr *
            BLOCK_SIZE,
        1, level + 1);
  }
  //   printf("  %s block at 0x%08x in L%d.\n",
  //          (write ? (cache_config[level].write_back ? "Writing-back"
  //                                                   : "Writing-through")
  //                 : "Reading in"),
  //          block_ptr * BLOCK_SIZE, level + 1);
  if ((!write) || !cache_config[level].write_back) {
    // Read in or write-through new entry from next level.
    memory_model_generic_check_cache(block_ptr * BLOCK_SIZE, write, level + 1);
  }

  cache[level][index * cache_config[level].associativity + lru_way].ptr =
      block_ptr;
  cache[level][index * cache_config[level].associativity + lru_way].use_time =
      current_time;
  cache[level][index * cache_config[level].associativity + lru_way].dirty =
      write && cache_config[level].write_back;

  return;
}

void memory_model_generic_load(void *ptr, unsigned int size,
                               unsigned int alignment) {
  if (!enabled) {
    return;
  }
  //   printf("Loading %d bytes of memory at %p, aligned along %d bytes.\n",
  //   size,
  //          ptr, alignment);
  memory_model_generic_check_cache((unsigned int)ptr, 0, 0);
}

void memory_model_generic_store(void *ptr, unsigned int size,
                                unsigned int alignment) {
  if (!enabled) {
    return;
  }
  //   printf("Storing %d bytes of memory at %p, aligned along %d bytes.\n",
  //   size,
  //          ptr, alignment);
  memory_model_generic_check_cache((unsigned int)ptr, 1, 0);
}

void memory_model_generic_done() {
  printf("Memory Model Stats:\n");
  printf("  Instructions: %ld\n", instruction_counter);
  unsigned int level;
  for (level = 0; cache_config[level].size; level++) {
    printf("  L%d Hits: %ld\n", level + 1, hit_counter[level]);
  }
  printf("  DRAM Accesses: %ld\n", hit_counter[level]);
}
