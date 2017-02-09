#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "ring.h"

#include <klee/klee.h>

#define RING_SIZE 16

data_t ring[RING_SIZE];
int ring_front = 0;
int ring_back = 0;
int empty = 1;

void ring_init() {}

void ring_enqueue(data_t data) {
  if (ring_front == ring_back && !empty) {
    return;
  }

  ring[ring_back] = data;
  ring_back = (ring_back + 1) % RING_SIZE;
  empty = 0;
}

data_t ring_dequeue() {
  if (empty) {
    return 0;
  }

  data_t data = ring[ring_front];
  ring_front = (ring_front + 1) % RING_SIZE;
  empty = ring_front == ring_back;

  return data;
}
