#include "common.h"
#include <assert.h>
#include <klee/klee.h>

int main() {
  int matrix[MATRIX_ROWS][MATRIX_COLS];

  start();
#ifdef __clang__
  memory_model_generic_start();
#endif

  int i = 0;
  for (unsigned int l = 0; l < sizeof(matrix) / sizeof(matrix[0]); l++) {
    for (unsigned int c = 0; c < sizeof(matrix[0]) / sizeof(matrix[0][0]);
         c++) {
      matrix[l][c] = i++;
    }
  }

  i = 0;
  for (unsigned int l = 0; l < sizeof(matrix) / sizeof(matrix[0]); l++) {
    for (unsigned int c = 0; c < sizeof(matrix[0]) / sizeof(matrix[0][0]);
         c++) {
      assert(matrix[l][c] == i++);
    }
  }

#ifdef __clang__
  memory_model_generic_stop();
#endif
  stop();

  return 0;
}
