#include <assert.h>
#include <stdio.h>
#include <time.h>

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
#define stop(num_packets, fmt, ...)                                            \
  do {                                                                         \
    struct timespec new_timestamp;                                             \
    assert(!clock_gettime(CLOCK_MONOTONIC, &new_timestamp));                   \
                                                                               \
    long duration_ns =                                                         \
        (new_timestamp.tv_sec - timestamp.tv_sec) * 1000000000 +               \
        (new_timestamp.tv_nsec - timestamp.tv_nsec);                           \
    printf(fmt "Processed %ld packets in %ld ns (%f Mpps)\n", ##__VA_ARGS__,   \
           (long)num_packets, duration_ns, 1e3 * num_packets / duration_ns);   \
  } while (0)
#else
#define stop(num_packets, fmt, ...)                                            \
  do {                                                                         \
  } while (0)
#endif
