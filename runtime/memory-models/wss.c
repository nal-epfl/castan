//===-- wss.c -------------------------------------------------------------===//
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

#include <castan/scenario.h>
#include <klee/klee.h>

#define BLOCK_SIZE 64

// Intel(R) Core(TM) i7-2600S
#define NUM_LINES (256 * 1024 / 8 / BLOCK_SIZE) // L1
// #define NUM_LINES (1024 * 1024 / 8 / BLOCK_SIZE) // L2
// #define NUM_LINES (8192 * 1024 / 16 / BLOCK_SIZE) // L3

typedef struct cache_entry_t {
  // The stored pointer.
  unsigned int ptr;
  // Use counters
  unsigned long read_count;
  unsigned long write_count;
  // Linked list.
  struct cache_entry_t *next;
} cache_entry_t;

static int memory_model_wss_enabled = 0;
// Cache entry linked list head.
static cache_entry_t **memory_model_wss_cache;

// Per loop memory access counters
static unsigned long memory_model_wss_read_counter = 0;
static unsigned long memory_model_wss_loop_reads = 0;
static unsigned long memory_model_wss_write_counter = 0;
static unsigned long memory_model_wss_loop_writes = 0;
static unsigned long memory_model_wss_access_counter = 0;
static unsigned long memory_model_wss_loop_accesses = 0;

// Pe loop instruction counters
static unsigned long memory_model_wss_instruction_counter = 0;
static unsigned long memory_model_wss_loop_instructions = 0;

// Simpler cache state representing just address presence.
static unsigned int *memory_model_wss_ptrs = NULL;
static unsigned long memory_model_wss_num_ptrs = 0;

static long memory_model_wss_priority = 0;

int castan_state_seen(void *state, int size);

void memory_model_wss_done();

// MergeSort for linked lists.
static cache_entry_t *memory_model_wss_merge_lists(cache_entry_t *la,
                                                   cache_entry_t *lb) {
  if (la == NULL) {
    return lb;
  }
  if (lb == NULL) {
    return la;
  }

  if (la->read_count + la->write_count < lb->read_count + lb->write_count) {
    lb->next = memory_model_wss_merge_lists(la, lb->next);
    return lb;
  } else {
    la->next = memory_model_wss_merge_lists(la->next, lb);
    return la;
  }
}

static unsigned long memory_model_wss_list_length(cache_entry_t *l) {
  unsigned long count = 0;
  for (cache_entry_t *e = l; e; e = e->next) {
    count++;
  }
  return count;
}

static cache_entry_t *memory_model_wss_merge_sort(cache_entry_t *l) {
  if (l == NULL || l->next == NULL) {
    return l;
  }

  // Get mid-point to divide and conquer.
  cache_entry_t *mid_point = l;
  for (unsigned long offset = memory_model_wss_list_length(l) / 2; offset > 1;
       offset--) {
    mid_point = mid_point->next;
  }
  // Split list.
  cache_entry_t *mid_head = mid_point->next;
  mid_point->next = NULL;
  // Sort and merge.
  return memory_model_wss_merge_lists(memory_model_wss_merge_sort(l),
                                      memory_model_wss_merge_sort(mid_head));
}

void memory_model_wss_init() {
  printf("Initializing wss memory model.\n");
  memory_model_wss_cache = calloc(sizeof(cache_entry_t *), NUM_LINES);
  memory_model_wss_ptrs = malloc(0);

  scenario_init();
}

// void memory_model_wss_start() { memory_model_wss_enabled = 1; }

void memory_model_wss_dump() {
  printf("Working Set:\n");
  for (int l = 0; l < NUM_LINES; l++) {
    if (memory_model_wss_cache[l]) {
      printf("  Cache line for 0x%08x:\n", l * BLOCK_SIZE);

      // Sort cache entries by the number of accesses.
      memory_model_wss_cache[l] =
          memory_model_wss_merge_sort(memory_model_wss_cache[l]);

      for (cache_entry_t *entry = memory_model_wss_cache[l]; entry;
           entry = entry->next) {
        printf("    0x%08x, read %ld times, written %ld times\n",
               entry->ptr * BLOCK_SIZE, entry->read_count, entry->write_count);
      }
    }
  }
  printf("  Blocks:\n");
  for (unsigned long p = 0; p < memory_model_wss_num_ptrs; p++) {
    printf("    0x%08x\n", memory_model_wss_ptrs[p] * BLOCK_SIZE);
  }
}

// void memory_model_wss_stop() {
//   memory_model_wss_done();
//   exit(0);
// }

void memory_model_wss_update_loop_counter(unsigned long *counter,
                                          unsigned long *metric) {
  if (*metric) {
    if (scenario == BEST_CASE && *counter < *metric) {
      *metric = *counter;
    } else if (scenario == WORST_CASE && *counter > *metric) {
      *metric = *counter;
    }
  } else {
    *metric = *counter;
  }
  *counter = 0;
}

void memory_model_wss_loop() {
//   static int loop_count = 0;
//   printf("Processing loop %d\n", ++loop_count);

  if (memory_model_wss_enabled) {
    memory_model_wss_update_loop_counter(&memory_model_wss_instruction_counter,
                                         &memory_model_wss_loop_instructions);
    memory_model_wss_update_loop_counter(&memory_model_wss_read_counter,
                                         &memory_model_wss_loop_reads);
    memory_model_wss_update_loop_counter(&memory_model_wss_write_counter,
                                         &memory_model_wss_loop_writes);
    memory_model_wss_update_loop_counter(&memory_model_wss_access_counter,
                                         &memory_model_wss_loop_accesses);
  }

  if (castan_state_seen(memory_model_wss_ptrs,
                        memory_model_wss_num_ptrs *
                            sizeof(*memory_model_wss_ptrs)) &&
      memory_model_wss_enabled) {
    //   if (castan_state_seen(&memory_model_wss_num_ptrs,
    //                         sizeof(memory_model_wss_num_ptrs))) {
    memory_model_wss_done();
    exit(0);
  } else {
    //     memory_model_wss_dump();
  }

  memory_model_wss_enabled = 1;
}

void memory_model_wss_exec(unsigned int id) {
  if (!memory_model_wss_enabled) {
    return;
  }
  //   printf("Executing instruction number %d.\n", id);
  memory_model_wss_instruction_counter++;
}

int memory_model_wss_ptrcmp(const void *a, const void *b) {
  return *((unsigned int *)a) - *((unsigned int *)b);
}

void memory_model_wss_check_cache(unsigned int ptr, char write) {
  // Parse pointer.
  unsigned int block_ptr = ptr / BLOCK_SIZE;
  unsigned int line = block_ptr % NUM_LINES;

  // Find corresponding cache entry of one exists.
  cache_entry_t *entry;
  for (entry = memory_model_wss_cache[line]; entry; entry = entry->next) {
    if (entry->ptr == block_ptr) {
      break;
    }
  }

  if (!entry) {
    // No cache entry, first access. Add new entry.
    entry = calloc(1, sizeof(cache_entry_t));
    entry->ptr = block_ptr;
    entry->next = memory_model_wss_cache[line];
    memory_model_wss_cache[line] = entry;

    memory_model_wss_ptrs =
        realloc(memory_model_wss_ptrs, (memory_model_wss_num_ptrs + 1) *
                                           sizeof(*memory_model_wss_ptrs));
    memory_model_wss_ptrs[memory_model_wss_num_ptrs++] = block_ptr;
    memory_model_wss_enabled = 0;
    qsort(memory_model_wss_ptrs, memory_model_wss_num_ptrs,
          sizeof(*memory_model_wss_ptrs), memory_model_wss_ptrcmp);
    memory_model_wss_enabled = 1;

    //     if (memory_model_wss_cache[line] == NULL) { // Priority = cache
    //     lines
    //     used.
    memory_model_wss_priority++;
    //     }
  }

  // Update stats.
  if (write) {
    entry->write_count++;
  } else {
    entry->read_count++;
  }
  memory_model_wss_access_counter++;

  return;
}

void memory_model_wss_load(void *ptr, unsigned int size,
                           unsigned int alignment) {
  if (!memory_model_wss_enabled) {
    return;
  }
  //   printf("Loading %d bytes of memory at %p, aligned along %d bytes.\n",
  //   size,
  //          ptr, alignment);
  memory_model_wss_check_cache((unsigned int)ptr, 0);

  memory_model_wss_read_counter++;
}

void memory_model_wss_store(void *ptr, unsigned int size,
                            unsigned int alignment) {
  if (!memory_model_wss_enabled) {
    return;
  }
  //   printf("Storing %d bytes of memory at %p, aligned along %d bytes.\n",
  //   size,
  //          ptr, alignment);
  memory_model_wss_check_cache((unsigned int)ptr, 1);

  memory_model_wss_write_counter++;
}

void memory_model_wss_done() {
  unsigned long read_count = 0, write_count = 0, wss = 0, max_ways = 0;
  for (int l = 0; l < NUM_LINES; l++) {
    unsigned long ways = 0;
    for (cache_entry_t *entry = memory_model_wss_cache[l]; entry;
         entry = entry->next) {
      read_count += entry->read_count;
      write_count += entry->write_count;
      wss += BLOCK_SIZE;
      ways++;
    }
    if (ways > max_ways) {
      max_ways = ways;
    }
  }

  printf("Memory Model Stats: "
         "Instructions per Loop: %ld, "
         "Memory Reads per Loop: %ld, "
         "Memory Writes per Loop: %ld, "
         "Memory Accesses per Loop: %ld, "
         "Working Set Size: %ld, "
         "Maximum Ways Used: %ld\n",
         memory_model_wss_loop_instructions, memory_model_wss_loop_reads,
         memory_model_wss_loop_writes, memory_model_wss_loop_accesses, wss,
         max_ways);
}
