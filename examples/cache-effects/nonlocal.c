#include "common.h"
#include <assert.h>
#include <klee/klee.h>

int main() {
  int matrix[MATRIX_ROWS][MATRIX_COLS];

  start();

  int i = 0;
  for (unsigned int c = 0; c < sizeof(matrix[0]) / sizeof(matrix[0][0]); c++) {
    for (unsigned int l = 0; l < sizeof(matrix) / sizeof(matrix[0]); l++) {
      matrix[MATRIX_ROWS - l - 1][c] = i++;
    }
  }

  i = 0;
  for (unsigned int c = 0; c < sizeof(matrix[0]) / sizeof(matrix[0][0]); c++) {
    for (unsigned int l = 0; l < sizeof(matrix) / sizeof(matrix[0]); l++) {
      assert(matrix[MATRIX_ROWS - l - 1][c] == i++);
    }
  }

  stop();

  return 0;
}
