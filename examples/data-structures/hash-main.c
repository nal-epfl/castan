#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "hash.h"

#include <klee/klee.h>

void memory_model_start();
void memory_model_dump();
void memory_model_stop();

int main(int argc, char *argv[]) {
  hash_init();

  hash_key_t set_key;
  hash_value_t set_value;

#ifdef __clang__
//   klee_make_symbolic((void*)&set_key, sizeof(set_key), "set_key");
  inet_pton(AF_INET, "127.0.0.1", &set_key.src_ip);
  inet_pton(AF_INET, "127.0.0.2", &set_key.dst_ip);
  set_key.proto = 6;
  set_key.src_port = 10000;
  set_key.dst_port = 443;

//   klee_make_symbolic((void*)&set_value, sizeof(set_value), "set_value");
  set_value = 1;
#else
  inet_pton(AF_INET, "127.0.0.1", &set_key.src_ip);
  inet_pton(AF_INET, "127.0.0.2", &set_key.dst_ip);
  set_key.proto = 6;
  set_key.src_port = 10000;
  set_key.dst_port = 443;
  set_value = 1;
#endif

// #ifdef __clang__
//   memory_model_start();
//   memory_model_dump();
// #endif

  hash_set(set_key, set_value);

  hash_key_t get_key;

#ifdef __clang__
  klee_make_symbolic((void*)&get_key, sizeof(get_key), "get_key");
#else
  inet_pton(AF_INET, "127.0.0.1", &get_key.src_ip);
  inet_pton(AF_INET, "127.0.0.2", &get_key.dst_ip);
  get_key.proto = 6;
  get_key.src_port = 10000;
  get_key.dst_port = 443;
#endif

#ifdef __clang__
  memory_model_start();
//   memory_model_dump();
#endif

  hash_get(get_key);

#ifdef __clang__
//   memory_model_dump();
  memory_model_stop();
#endif

  return 0;
}
