#include "common.h"
#include <assert.h>
#include <stdlib.h>
#include <klee/klee.h>

#define ARRAY_SIZE_MIN 64
#define ARRAY_SIZE_MAX (2 * 50 * 1024 * 1024)

#define ARRAY_STEP_MIN 64
#define ARRAY_STEP_MAX 65536
#define ARRAY_STEP_STEP ARRAY_STEP_MIN

#define NUM_ITERATIONS 10
#define NUM_REPETITIONS 10

char array[ARRAY_SIZE_MAX];

int main() {
  srand(time(NULL));

  for (int repetition = 0; repetition < NUM_REPETITIONS; repetition++) {
    for (int size = ARRAY_SIZE_MIN; size <= ARRAY_SIZE_MAX; size *= 2) {
      for (int step = ARRAY_STEP_MIN; step <= ARRAY_STEP_MAX;
           step += ARRAY_STEP_STEP) {
        start();

        for (int iteration = 0; iteration < NUM_ITERATIONS; iteration++) {
          for (int i = 0; i < size; i += step) {
            array[rand() & (size - 1)]++;
          }
        }

        stop("Size: %d, Step: %d", size, step);
      }
    }
  }

  return 0;
}
