#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lpm.h"

#include <klee/klee.h>

typedef struct prefix_node {
  struct prefix_node *children[2];
  data_t data;
} prefix_node_t;

prefix_node_t *prefix_tree;

void init_prefix_db() {
}

void set_prefix_data(struct in_addr *ip, int prefix_len, data_t data) {
  prefix_node_t **node = &prefix_tree;

  // Explore prefix match tree up to the prefix depth.
  for (int pos = 0; pos <= prefix_len; pos++) {
    // Create new nodes if needed.
    if (!(*node)) {
      *node = calloc(sizeof(prefix_node_t), 1);
    }
    // Pick child based on IP bit at this depth.
    if (pos < prefix_len) {
      node = &((*node)->children[(ntohl(ip->s_addr) >>
                                  (sizeof(ip->s_addr) * 8 - pos - 1)) &
                                 0x01]);
    }
  }

  (*node)->data = data;
}

data_t get_ip_data(struct in_addr *ip) {
  prefix_node_t *node = prefix_tree;
  data_t data;

  memset(&data, 0, sizeof(data));

  // Explore prefix tree and remember the most specific ASN found.
  for (int pos = sizeof(ip->s_addr) * 8 - 1; pos >= 0 && node; pos--) {
    data = node->data;

    node = node->children[(ntohl(ip->s_addr) >> pos) & 0x01];
  }

  return data;
}
