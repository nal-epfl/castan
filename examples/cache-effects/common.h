#include <stdio.h>
#include <sys/time.h>

#define MATRIX_ROWS 1000
#define MATRIX_COLS 1000

struct timeval start_time;

void start() {
#ifndef __clang__
  gettimeofday(&start_time, NULL);
#endif
}

void stop() {
#ifndef __clang__
  struct timeval stop_time;

  gettimeofday(&stop_time, NULL);

  printf("Run Time: %ld us\n", (stop_time.tv_sec - start_time.tv_sec) * 1000000 +
                              (stop_time.tv_usec - start_time.tv_usec));
#endif
}