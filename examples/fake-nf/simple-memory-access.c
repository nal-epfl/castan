#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include <castan/castan.h>

#define ARRAY_SIZE (64*1024*1024)

int sequence[] = {
0,
2621440,
5242880,
7864320,
8912896,
10485760,
14155776,
15728640,
17825792,
20447232,
20971520,
23592960,
26738688,
28311552,
29884416,
31457280,
35127296,
};

int main(int argc, char *argv[]) {
  uint32_t count = 0, idx = 0, step = 0, dummy = 0;

  uint8_t *array = aligned_alloc(1<<21, ARRAY_SIZE);

//   for (int i = 0; i < ARRAY_SIZE>>6; i++) {
//     *((int *) &array[i<<6]) = i<<6;
//   }
//   for (int i = 0; i < (ARRAY_SIZE>>6) - 1; i++) {
//     int j = i + rand() / (RAND_MAX / ((ARRAY_SIZE>>6) - i) + 1);
//     int t = *((int*)&array[j<<6]);
//     *((int*)&array[j<<6]) = *((int*)&array[i<<6]);
//     *((int*)&array[i<<6]) = t;
//   }

  int prev_value = 0;
  for (int i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++) {
    *((int *) &array[prev_value]) = sequence[i];
    prev_value = sequence[i];
  }
  *((int *) &array[prev_value]) = sequence[0];

  while (1) {
    castan_loop();
    uint32_t packet = 0;

#ifdef __clang__
    klee_make_symbolic((void *)&packet, sizeof(packet), "castan_packet");
    klee_assume((packet & ~((ARRAY_SIZE>>1) - 1)) == 0);
#else
//     if (scanf("%u", &packet) != 1) {
//       exit(0);
//     }
    if (count++ >= 1000000) exit(0);
#endif

//     for (int i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++){
//       idx = sequence[i] & ((ARRAY_SIZE/2)-1);
//       start();
//       dummy += array[idx];
//       stop(1, "");
//     }

  start();
  idx = *((int*) &array[idx]);
  stop(1, "");

//     count += 100;
//     for (int i = 0; i < 10000; i++) {
//       packet = ((idx)<<15) & (ARRAY_SIZE/2 - 1);
//       idx = ((rand()&((((1<<17)-1))<<6))&(ARRAY_SIZE/2 - 1));// | 0x0007FFFF;
//       printf("%08X\n", idx);
//       idx = rand();//+= ++step * 2 + 1;
//     }
  }

  return 0;
}
