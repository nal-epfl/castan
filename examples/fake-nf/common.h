#include <stdio.h>
#include <sys/time.h>

struct timeval start_time;

void start() {
#ifndef __clang__
  gettimeofday(&start_time, NULL);
#endif
}

void stop(unsigned long num_packets) {
#ifndef __clang__
  struct timeval stop_time;

  gettimeofday(&stop_time, NULL);

  printf("Processed %ld packets in %ld us (%ld pkts/s)\n", num_packets,
         (stop_time.tv_sec - start_time.tv_sec) * 1000000 +
             (stop_time.tv_usec - start_time.tv_usec),
         num_packets * 1000000 /
             ((stop_time.tv_sec - start_time.tv_sec) * 1000000 +
              (stop_time.tv_usec - start_time.tv_usec)));
#endif
}
