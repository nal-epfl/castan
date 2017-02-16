#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "ring.h"

#include <klee/klee.h>

void memory_model_loop();

int main(int argc, char *argv[]) {
  ring_init();

  unsigned int num_items;
  data_t data;

#ifdef __clang__
  //   klee_make_symbolic((void *)&num_items, sizeof(num_items), "num_items");
  //   klee_assume(num_items < 10);
  num_items = 10;

  //   klee_make_symbolic((void*)&data, sizeof(data), "data");
  data = 1;
#else
  num_items = 1;
  data = 1;
#endif

  for (unsigned int i = 0; i < num_items; i++) {
    ring_enqueue(data);
  }

  for (unsigned int i = 0; i < num_items; i++) {
#ifdef __clang__
    memory_model_loop();
#endif

    ring_dequeue();
  }

  return 0;
}
