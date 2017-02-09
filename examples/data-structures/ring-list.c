#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "ring.h"

#include <klee/klee.h>

#define RING_SIZE 16

typedef struct ring_entry_t {
  data_t data;

  struct ring_entry_t *next;
} ring_entry_t;

ring_entry_t *ring_front = NULL;
ring_entry_t *ring_back = NULL;

void ring_init() {}

void ring_enqueue(data_t data) {
  if (ring_front && ring_back) {
    ring_back->next = calloc(1, sizeof(ring_entry_t));
    ring_back = ring_back->next;
  } else {
    ring_front = ring_back = calloc(1, sizeof(ring_entry_t));
  }
  ring_back->data = data;
}

data_t ring_dequeue() {
  if (!ring_front) {
    return 0;
  }

  data_t data = ring_front->data;
  ring_entry_t *old_front = ring_front;
  ring_front = ring_front->next;
  free(old_front);

  if (!ring_front) {
    ring_back = NULL;
  }

  return data;
}
