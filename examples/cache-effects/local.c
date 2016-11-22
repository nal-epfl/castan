#include "common.h"
#include <assert.h>
#include <klee/klee.h>

int main() {
  int matrix[MATRIX_ROWS][MATRIX_COLS];

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

  return 0;
}
