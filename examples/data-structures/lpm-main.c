#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "lpm.h"

#include <klee/klee.h>

void memory_model_generic_start();
void memory_model_generic_dump();
void memory_model_generic_stop();

int main(int argc, char *argv[]) {
  init_prefix_db();

  struct in_addr ip;
  int prefix_len;
  data_t data;

#ifdef __clang__
  klee_make_symbolic((void*)&ip, sizeof(ip), "ip");

  klee_make_symbolic((void*)&prefix_len, sizeof(prefix_len), "prefix_len");
  klee_assume(prefix_len >= 0);
  klee_assume(prefix_len <= 24);

  klee_make_symbolic((void*)&data, sizeof(data), "data");
#else
  inet_pton(AF_INET, "127.0.0.1", &ip);
  prefix_len = 24;
  data = 1;
#endif

#ifdef __clang__
  memory_model_generic_start();
  memory_model_generic_dump();
#endif

  set_prefix_data(&ip, prefix_len, data);

#ifdef __clang__
  memory_model_generic_dump();
#endif

  get_ip_data(&ip);

#ifdef __clang__
  memory_model_generic_dump();
  memory_model_generic_stop();
#endif

  return 0;
}
