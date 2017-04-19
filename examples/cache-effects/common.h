#define _POSIX_C_SOURCE 199309L

#include <assert.h>
#include <stdio.h>
#include <time.h>

#define MATRIX_ROWS 1000
#define MATRIX_COLS 1000

struct timespec timestamp;

#ifndef __clang__
#define start()                                                                \
  do {                                                                         \
    assert(!clock_gettime(CLOCK_MONOTONIC, &timestamp));                       \
  } while (0)
#else
#define start()                                                                \
  do {                                                                         \
  } while (0)
#endif

#ifndef __clang__
#define stop(fmt, ...)                                                         \
  do {                                                                         \
    struct timespec new_timestamp;                                             \
    assert(!clock_gettime(CLOCK_MONOTONIC, &new_timestamp));                   \
                                                                               \
    printf(fmt ": Latency: %ld ns.\n", ##__VA_ARGS__,                            \
           (new_timestamp.tv_sec - timestamp.tv_sec) * 1000000000 +            \
               (new_timestamp.tv_nsec - timestamp.tv_nsec));                   \
  } while (0)
#else
#define stop(fmt, ...)                                                         \
  do {                                                                         \
  } while (0)
#endif
