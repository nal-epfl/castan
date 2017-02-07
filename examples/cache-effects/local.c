#include "common.h"
#include <assert.h>
#include <klee/klee.h>

void memory_model_start();
void memory_model_stop();

int main() {
  int matrix[MATRIX_ROWS][MATRIX_COLS];

  start();
#ifdef __clang__
  memory_model_start();
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
  memory_model_stop();
#endif
  stop();

  return 0;
}
