#include <castan/castan.h>
#include <castan/emmintrin.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>

struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];

struct rte_mempool_ops_table rte_mempool_ops_table = {
    .sl = RTE_SPINLOCK_INITIALIZER, .num_ops = 0};

__thread unsigned int per_lcore__lcore_id = 0;

int rte_eal_init(int argc, char **argv) { return 0; }

void rte_exit(int exit_code, const char *format, ...) { exit(exit_code); }

unsigned rte_socket_id() { return 0; }

struct rte_mempool *rte_pktmbuf_pool_create(const char *name, unsigned n,
                                            unsigned cache_size,
                                            uint16_t priv_size,
                                            uint16_t data_room_size,
                                            int socket_id) {
  return NULL;
}

uint8_t rte_eth_dev_count() { return 2; }

void rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr) {}

#define rte_eth_rx_burst castan_rte_eth_rx_burst
uint16_t castan_rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
                                 struct rte_mbuf **rx_pkts,
                                 const uint16_t nb_pkts) {
  if (port_id == 0) {
    castan_loop();

    return 0;
  } else {
    return 0;
  }
}