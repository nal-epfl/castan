#include <castan/castan.h>
#include <castan/emmintrin.h>
#include <rte_eal_memconfig.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>

struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];

struct rte_mempool_ops_table rte_mempool_ops_table = {
    .sl = RTE_SPINLOCK_INITIALIZER, .num_ops = 0};

__thread unsigned int __attribute__((weak)) per_lcore__lcore_id = 0;

int rte_eal_init(int argc, char **argv) { return 0; }

struct rte_config *rte_eal_get_configuration() {
  static struct rte_mem_config mem_config = {
      .mlock.cnt = 0, .qlock.cnt = 0, .mplock.cnt = 0,
  };
  static struct rte_config config = {.mem_config = &mem_config};

  return &config;
}

void rte_exit(int exit_code, const char *format, ...) {
  assert(0);
  exit(exit_code);
}

unsigned rte_socket_id() { return 0; }

uint8_t rte_eth_dev_count() { return 2; }

void rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr) {}

__thread int __attribute__((weak)) per_lcore__rte_errno = 0;

struct rte_mempool *rte_pktmbuf_pool_create(const char *name, unsigned n,
                                            unsigned cache_size,
                                            uint16_t priv_size,
                                            uint16_t data_room_size,
                                            int socket_id) {
  struct rte_mempool *mp = calloc(sizeof(struct rte_mempool), 1);

  strncpy(mp->name, name, RTE_MEMPOOL_NAMESIZE);
  mp->cache_size = cache_size;

  return mp;
}

int rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue,
                          const struct rte_eth_conf *eth_conf) {
  return 0;
}

int rte_eth_dev_socket_id(uint8_t device) { return 0; }

int rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
                           uint16_t nb_tx_desc, unsigned int socket_id,
                           const struct rte_eth_txconf *tx_conf) {
  return 0;
}

int rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
                           uint16_t nb_rx_desc, unsigned int socket_id,
                           const struct rte_eth_rxconf *rx_conf,
                           struct rte_mempool *mb_pool) {
  return 0;
}

int rte_eth_dev_start(uint8_t portid) { return 0; }

void rte_eth_promiscuous_enable(uint8_t portid) { return; }

#define rte_eth_rx_burst castan_rte_eth_rx_burst
uint16_t __attribute__((weak))
castan_rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
                        struct rte_mbuf **rx_pkts, const uint16_t nb_pkts) {
  if (port_id == 0) {
    castan_loop();

    return 0;
  } else {
    return 0;
  }
}
