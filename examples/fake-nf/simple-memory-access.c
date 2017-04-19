#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include <castan/castan.h>

#define ARRAY_SIZE (64*1024*1024)

uint8_t array[ARRAY_SIZE];

int main(int argc, char *argv[]) {
  uint32_t count = 10;

  while (1) {
    castan_loop();
    uint32_t packet = 0;

#ifdef __clang__
    klee_make_symbolic((void *)&packet, sizeof(packet), "castan_packet");
    klee_assume((packet & ~(ARRAY_SIZE - 1)) == 0);
#else
    if (scanf("%u", &packet) != 1) {
      exit(0);
    }
#endif

    start();
    array[packet]++;
    stop(1, "");
  }

  return 0;
}
