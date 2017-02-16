#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lpm.h"

#include <castan/scenario.h>
#include <klee/klee.h>

typedef struct prefix_node {
  struct prefix_node *children[2];
  data_t data;
} prefix_node_t;

prefix_node_t *prefix_tree;

void lpm_init() {}

void lpm_set_prefix_data(struct in_addr *ip, int prefix_len, data_t data) {
  prefix_node_t **node = &prefix_tree;

  // Explore prefix match tree up to the prefix depth.
  for (int pos = 0; pos <= prefix_len; pos++) {
    // Create new nodes if needed.
    if (!(*node)) {
      *node = calloc(sizeof(prefix_node_t), 1);
    }
    // Pick child based on IP bit at this depth.
    if (pos < prefix_len) {
      if ((ntohl(ip->s_addr) & 1 << (sizeof(ip->s_addr) * 8 - pos - 1))) {
        node = &((*node)->children[1]);
      } else {
        node = &((*node)->children[0]);
      }
    }
  }

  (*node)->data = data;
}

data_t lpm_get_ip_data(struct in_addr *ip) {
  prefix_node_t *node = prefix_tree;
  data_t data;

  memset(&data, 0, sizeof(data));

#ifdef __clang__
  if (scenario == WORST_CASE) {
    while (node) {
      data = node->data;
      if (node->children[0]) {
        node = node->children[0];
      } else {
        node = node->children[1];
      }
    }
  }
#else
  // Explore prefix tree and remember the most specific ASN found.
  for (int pos = sizeof(ip->s_addr) * 8 - 1; pos >= 0 && node; pos--) {
    data = node->data;

    if (ntohl(ip->s_addr) & 1 << pos) {
      node = node->children[1];
    } else {
      node = node->children[0];
    }
  }
#endif

  return data;
}
