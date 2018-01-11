#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_memzone.h>

#define PAGE_SIZE (1 << 30)
#define ARRAY_SIZE (1ul * PAGE_SIZE)
#define DELAY_DELTA_THRESHOLD 300
#define LOOP_REPETITIONS 100
#define PROBE_TRIALS 10

struct timespec timestamp;

char *array;

static inline void start() {
  assert(!clock_gettime(CLOCK_MONOTONIC, &timestamp));
}

static inline long stop() {
  struct timespec new_timestamp;
  assert(!clock_gettime(CLOCK_MONOTONIC, &new_timestamp));

  if (new_timestamp.tv_sec == timestamp.tv_sec) {
    return new_timestamp.tv_nsec - timestamp.tv_nsec;
  } else {
    return 1000000000 - timestamp.tv_nsec +
           (new_timestamp.tv_sec - timestamp.tv_sec - 1) * 1000000000 +
           new_timestamp.tv_nsec;
  }
}

int is_empty(long entry) { return entry < 0; }

long get_size(long entry) {
  if (entry < 0) {
    return 0;
  }

  if (*((long *)&array[entry]) == entry) {
    return 1;
  }

  long pos = entry, size = 0;
  do {
    pos = *((long *)&array[pos]);
    size++;
  } while (pos != entry);

  return size;
}

void insert(long *pos, long item) {
  assert(item >= 0);
  assert(*((long *)&array[item]) == item);

  if (*pos < 0) {
    *pos = item;
  }

  if (*pos != item) {
    *((long *)&array[item]) = *((long *)&array[*pos]);
    *((long *)&array[*pos]) = item;
  }
}

long drop_next(long *entry, long pos) {
  assert(*entry >= 0);
  assert(pos >= 0);

  long drop = *((long *)&array[pos]);

  if (drop == *entry) {
    if (pos == *entry) {
      *entry = -1;
    } else {
      *entry = pos;
    }
  }

  *((long *)&array[pos]) = *((long *)&array[drop]);
  *((long *)&array[drop]) = drop;

  return drop;
}

int probe(long entry) {
  if (entry < 0) {
    return 0;
  }

  long idx = entry;

  // Prime.
  do {
    idx = *((long *)&array[idx]);
  } while (idx != entry);

  // Probe.
  start();
  for (int i = 0; i < LOOP_REPETITIONS; i++) {
    do {
      idx = *((long *)&array[idx]);
    } while (idx != entry);
  }
  int delay = stop();

  return delay / LOOP_REPETITIONS;
}

int min_probe(long entry) {
  int min = probe(entry);
  for (int i = 1; i < PROBE_TRIALS; i++) {
    int p = probe(entry);
    if (p < min) {
      min = p;
    }
  }
  return min;
}

int main(int argc, char *argv[]) {
  // Initialize the Environment Abstraction Layer (EAL)
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    rte_exit(EXIT_FAILURE, "Error with EAL initialization, ret=%d\n", ret);
  }
  argc -= ret;
  argv += ret;

  assert(argc >= 2 && argc <= 3 &&
         "Usage: dpdk-check-cache <input-set-file> [output-set-file]");
  FILE *input_file = fopen(argv[1], "r");
  assert(input_file && "Unable to open input set file.");

  FILE *output_file = NULL;
  if (argc == 3) {
    output_file = fopen(argv[2], "w");
    assert(output_file && "Unable to open output set file.");
  }

  const struct rte_memzone *mz = rte_memzone_reserve_aligned(
      "Array", ARRAY_SIZE, rte_socket_id(), RTE_MEMZONE_1GB, PAGE_SIZE);
  assert(mz && "Unable to allocate memory zone.");
  array = (char *)mz->addr;
  printf("Array physical address: %016lX\n", rte_mem_virt2phy(array));

  // Init line sets.
  for (long i = 0; i < ARRAY_SIZE; i += sizeof(long)) {
    *((long *)&array[i]) = i;
  }

  char *line = NULL;
  size_t size = 0;
  while (1) {
    if (getline(&line, &size, input_file) < 0) {
      free(line);
      break;
    }
    if (*line == '\n') {
      continue;
    }
    int associativity = atoi(line);

    long contention_set = -1;
    long count = 0;
    while (getline(&line, &size, input_file) >= 0 && *line != '\n') {
      long address = atol(line);

      assert(address < ARRAY_SIZE);
      insert(&contention_set, address);
      count++;
    }
    printf("Loaded %d-way contention set with %ld addresses.\n", associativity,
           count);
    assert(count >= associativity);

    // Warm-up
    min_probe(contention_set);

    long running_set = -1;
    for (int i = 0; i < associativity; i++) {
      insert(&running_set, drop_next(&contention_set, contention_set));
    }

    long output_set = -1;
    while (!is_empty(contention_set)) {
      int baseline_probe = probe(running_set);
      insert(&running_set, drop_next(&contention_set, contention_set));
      int contended_probe = probe(running_set);
      printf("Baseline probe: %d, Contended probe: %d, delta: %d\n",
             baseline_probe, contended_probe, contended_probe - baseline_probe);

      long output_value = drop_next(&running_set, running_set);
      if (contended_probe - baseline_probe > DELAY_DELTA_THRESHOLD) {
        output_value = -1;
      }

      baseline_probe = probe(running_set);
      printf("Baseline probe: %d, Contended probe: %d, delta: %d\n",
             baseline_probe, contended_probe, contended_probe - baseline_probe);
      if (output_value >= 0 &&
          contended_probe - baseline_probe > DELAY_DELTA_THRESHOLD) {
        insert(&output_set, output_value);
      } else {
        printf("Filtering probed value out.\n");
      }
    }

    while (!is_empty(running_set)) {
      insert(&output_set, drop_next(&running_set, running_set));
    }

    if (output_file && get_size(output_set) > associativity) {
      fprintf(output_file, "%d\n", associativity);

      while (!is_empty(output_set)) {
        fprintf(output_file, "%ld\n", drop_next(&output_set, output_set));
      }
      fprintf(output_file, "\n");
    } else {
      if (get_size(output_set) > associativity) {
        printf("Contention set no longer holds. Filtering out entire set.\n");
      }

      while (!is_empty(output_set)) {
        drop_next(&output_set, output_set);
      }
    }
  }

  fclose(input_file);
  if (output_file) {
    fclose(output_file);
  }

  return 0;
}
