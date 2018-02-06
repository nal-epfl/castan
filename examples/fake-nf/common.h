#include <assert.h>
#include <papi.h>
#include <stdio.h>
#include <time.h>

#ifndef __clang__
// int Events[] = {PAPI_LD_INS, PAPI_SR_INS};
// int Events[] = {PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM};
// int Events[] = {PAPI_L3_TCA, PAPI_L3_TCM};
struct timespec timestamp;


//     int num_hwcntrs = 0, retval;\
//     static int started = 0;\
//     if (started) {\
//       long long values[sizeof(Events) / sizeof(Events[0])];\
//       assert((retval = PAPI_read_counters(\
//                   values, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);\
//     } else {\
//       assert((num_hwcntrs = PAPI_num_counters()) > PAPI_OK);\
//       assert(sizeof(Events) / sizeof(Events[0]) <= num_hwcntrs);\
//       assert((retval = PAPI_start_counters(\
//                   Events, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);\
//       started = 1;\
//     }\

#define start()                                                                \
  do {                                                                         \
    assert(!clock_gettime(CLOCK_MONOTONIC, &timestamp));                       \
  } while (0)
#else
#define start()                                                                \
  do {                                                                         \
  } while (0)
#endif


//     long long li = values[0]-74;
//     long long si = values[1]-48;
//                " %lld Loads, %lld Stores.\n",
//            li, si);

//     long long l1_tcm = values[0];
//     long long l2_tcm = values[1];
//     long long l3_tcm = values[2];
//            " %lld L1 Misses, %lld L2 Hits, %lld L3 Hits, %lld L3 Misses.\n",
//            l1_tcm, l1_tcm - l2_tcm, l2_tcm - l3_tcm, l3_tcm);

//     long long values[sizeof(Events) / sizeof(Events[0])];\
//     int retval;\
//     assert((retval = PAPI_read_counters(\
//                 values, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);\
//     long long l3_tca = values[0];\
//     long long l3_tcm = values[1];\
//     printf("%lld L3 Hits, %lld L3 Misses.\n",\
//            l3_tca - l3_tcm, l3_tcm);\

#ifndef __clang__
#define stop(num_packets, fmt, ...)                                            \
  do {                                                                         \
    struct timespec new_timestamp;                                             \
    assert(!clock_gettime(CLOCK_MONOTONIC, &new_timestamp));                   \
                                                                               \
    long long duration_ns =                                                    \
        (new_timestamp.tv_sec - timestamp.tv_sec) * 1000000000 +               \
        (new_timestamp.tv_nsec - timestamp.tv_nsec);                           \
    printf(fmt "Processed %ld packets in %lld ns (%f Mpps).\n",                \
           ##__VA_ARGS__, (long)num_packets, duration_ns,                      \
           1e3 * num_packets / duration_ns);                                   \
  } while (0)
#else
#define stop(num_packets, fmt, ...)                                            \
  do {                                                                         \
  } while (0)
#endif
