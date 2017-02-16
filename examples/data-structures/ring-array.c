#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "ring.h"

#include <castan/scenario.h>
#include <klee/klee.h>

#define RING_SIZE 128

data_t ring[RING_SIZE];
int ring_front = 0;
int ring_back = 0;
int empty = 1;

void ring_init() {}

void ring_enqueue(data_t data) {
  if (ring_front == ring_back && !empty) {
    return;
  }

#ifdef __clang__
  if (scenario == BEST_CASE) {
    ring[0] = data;
  } else {
    static int pos = 0;
    ring[pos] = data;
    pos = (pos + 64) % RING_SIZE;
  }
#else
  ring[ring_back] = data;
#endif

  ring_back = (ring_back + 1) % RING_SIZE;

  empty = 0;
}

data_t ring_dequeue() {
  if (empty) {
    return 0;
  }

  data_t data;
#ifdef __clang__
  if (scenario == BEST_CASE) {
    data = ring[0];
  } else {
    static int pos = 0;
    data = ring[pos];
    pos = (pos + 64) % RING_SIZE;
  }
#else
  data = ring[ring_front];
#endif

  ring_front = (ring_front + 1) % RING_SIZE;
  empty = ring_front == ring_back;

  return data;
}
