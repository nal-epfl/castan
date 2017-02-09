#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "ring.h"

#include <klee/klee.h>

void memory_model_start();
void memory_model_dump();
void memory_model_stop();

int main(int argc, char *argv[]) {
  ring_init();

  unsigned int num_items;
  data_t data;

#ifdef __clang__
  klee_make_symbolic((void *)&num_items, sizeof(num_items), "num_items");
  klee_assume(num_items < 10);
  //   num_items = 1;

  //   klee_make_symbolic((void*)&data, sizeof(data), "data");
  data = 1;
#else
  num_items = 1;
  data = 1;
#endif

  // #ifdef __clang__
  //   memory_model_start();
  //   memory_model_dump();
  // #endif

  for (unsigned int i = 0; i < num_items; i++) {
    ring_enqueue(data);
  }

#ifdef __clang__
  memory_model_start();
//   memory_model_dump();
#endif

  for (unsigned int i = 0; i < num_items; i++) {
    ring_dequeue();
  }

#ifdef __clang__
  //   memory_model_dump();
  memory_model_stop();
#endif

  return 0;
}
