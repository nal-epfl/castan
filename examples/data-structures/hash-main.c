#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "hash.h"

#include <klee/klee.h>

void memory_model_loop();

int main(int argc, char *argv[]) {
  hash_init();

  for (int i = 0; i < 16; i++) {
    hash_key_t set_key;
    hash_value_t set_value;

#ifdef __clang__
    //     klee_make_symbolic((void *)&set_key, sizeof(set_key), "set_key");
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

    hash_set(set_key, set_value);
  }

#ifdef __clang__
  while (1) {
    memory_model_loop();
#else
  for (int i = 0; i < 3; i++) {
#endif

    hash_key_t get_key;

#ifdef __clang__
    klee_make_symbolic((void *)&get_key, sizeof(get_key), "get_key");
#else
    inet_pton(AF_INET, "127.0.0.1", &get_key.src_ip);
    inet_pton(AF_INET, "127.0.0.2", &get_key.dst_ip);
    get_key.proto = 6;
    get_key.src_port = 10000;
    get_key.dst_port = 443;
#endif

    hash_get(get_key);
  }

  return 0;
}
