#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include <castan/castan.h>

int main(int argc, char *argv[]) {
  uint32_t count = 10;

  while (1) {
    castan_loop();
    uint32_t packet = 0;

#ifdef __clang__
    klee_make_symbolic((void *)&packet, sizeof(packet), "castan_packet");
    klee_assume(packet <= 10);
#else
    if (scanf("%u", &packet) != 1) {
      exit(0);
    }
#endif
    packet = (count++) / 10;

    start();
    while (packet) {
      packet--;
    }
    stop(1, "");
  }

  return 0;
}
