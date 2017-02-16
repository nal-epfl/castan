#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "lpm.h"

#include <klee/klee.h>

void memory_model_loop();

int main(int argc, char *argv[]) {
  lpm_init();

  struct in_addr set_ip;
  int set_prefix_len;
  data_t set_data;

#ifdef __clang__
  //   klee_make_symbolic((void*)&set_ip, sizeof(set_ip), "set_ip");
  inet_pton(AF_INET, "127.0.0.1", &set_ip);

  //   klee_make_symbolic((void *)&set_prefix_len, sizeof(set_prefix_len),
  //                      "set_prefix_len");
  //   klee_assume(set_prefix_len >= 0);
  //   klee_assume(set_prefix_len <= 24);
  set_prefix_len = 24;

  //   klee_make_symbolic((void *)&set_data, sizeof(set_data), "set_data");
  set_data = 1;
#else
  inet_pton(AF_INET, "127.0.0.1", &set_ip);
  set_prefix_len = 24;
  set_data = 1;
#endif

  lpm_set_prefix_data(&set_ip, set_prefix_len, set_data);

#ifdef __clang__
  while (1) {
    memory_model_loop();
#else
  for (int i = 0; i < 3; i++) {
#endif

    struct in_addr get_ip;

#ifdef __clang__
    klee_make_symbolic((void *)&get_ip, sizeof(get_ip), "get_ip");
#else
    inet_pton(AF_INET, "127.0.0.1", &get_ip);
#endif

    lpm_get_ip_data(&get_ip);
  }

  return 0;
}
