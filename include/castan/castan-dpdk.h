#include <assert.h>
#include <castan/castan.h>
#include <castan/emmintrin.h>
#include <rte_eal_memconfig.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_tcp.h>
#include <rte_udp.h>

struct packet {
  struct ether_hdr ether;
  struct ipv4_hdr ipv4;
  union {
    struct tcp_hdr tcp;
    struct udp_hdr udp;
  };
};

struct rte_eth_dev __attribute__((weak)) rte_eth_devices[RTE_MAX_ETHPORTS];

struct rte_mempool_ops_table __attribute__((weak))
rte_mempool_ops_table = {.sl = RTE_SPINLOCK_INITIALIZER, .num_ops = 0};

__thread unsigned int __attribute__((weak)) per_lcore__lcore_id = 0;

void castan_rte_prefetch(const volatile void *p) {}

int rte_eal_tailqs_init(void);
int __attribute__((weak)) rte_eal_init(int argc, char **argv) {
  klee_alias_function("rte_memzone_reserve", "castan_rte_memzone_reserve");
  klee_alias_function("rte_prefetch0", "castan_rte_prefetch");
  klee_alias_function("rte_prefetch1", "castan_rte_prefetch");
  klee_alias_function("rte_prefetch2", "castan_rte_prefetch");
  klee_alias_function("rte_prefetch_non_temporal", "castan_rte_prefetch");

  if (rte_eal_tailqs_init() < 0)
    rte_panic("Cannot init tail queues for objects\n");

  return 0;
}

int __attribute__((weak))
__isoc99_fscanf(FILE *stream, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vfprintf(stream, format, args);
  va_end(args);
  return result;
}

struct rte_config __attribute__((weak)) * rte_eal_get_configuration() {
  static struct rte_mem_config mem_config = {
      .mlock.cnt = 0, .qlock.cnt = 0, .mplock.cnt = 0,
  };
  static struct rte_config config = {.mem_config = &mem_config};

  return &config;
}

void __attribute__((weak)) rte_exit(int exit_code, const char *format, ...) {
  assert(0);
  exit(exit_code);
}

void __attribute__((weak)) *
    rte_zmalloc(const char *type, size_t size, unsigned align) {
  if (align == 0) {
    align = 1;
  }

  void *ptr = calloc(size + align, 1);
  return ((char *)ptr) + ((unsigned long)ptr) % align;
}

void __attribute__((weak)) * rte_zmalloc_socket(const char *type, size_t size,
                                                unsigned align, int socket) {
  return rte_zmalloc(type, size, align);
}

unsigned __attribute__((weak)) rte_socket_id() { return 0; }

int rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature) { return 0; }

struct rte_memzone *castan_rte_memzone_reserve(const char *name, size_t len,
                                               int socket_id, unsigned flags) {
  struct rte_memzone *mz = calloc(sizeof(struct rte_memzone), 1);
  strncpy(mz->name, name, RTE_MEMZONE_NAMESIZE);
  mz->len = len;
  mz->flags = flags;
  mz->addr = malloc(len);
  mz->addr_64 = (uint64_t)mz->addr;
  return mz;
}

uint8_t __attribute__((weak)) rte_eth_dev_count() { return 2; }

void __attribute__((weak))
rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr) {
  static struct ether_addr addrs[] = {
      {
          0x08, 0x00, 0x27, 0x00, 0x44, 0x71,
      },
      {
          0x08, 0x00, 0x27, 0x00, 0x44, 0x72,
      },
  };

  if (port_id < sizeof(addrs) / sizeof(addrs[0])) {
    *mac_addr = addrs[port_id];
  }
}

__thread int __attribute__((weak)) per_lcore__rte_errno = 0;

struct rte_mempool __attribute__((weak)) *
    rte_pktmbuf_pool_create(const char *name, unsigned n, unsigned cache_size,
                            uint16_t priv_size, uint16_t data_room_size,
                            int socket_id) {
  struct rte_mempool *mp =
      (struct rte_mempool *)calloc(sizeof(struct rte_mempool), 1);

  strncpy(mp->name, name, RTE_MEMPOOL_NAMESIZE);
  mp->cache_size = cache_size;

  return mp;
}

int __attribute__((weak))
rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue,
                      uint16_t nb_tx_queue,
                      const struct rte_eth_conf *eth_conf) {
  return 0;
}

int __attribute__((weak)) rte_eth_dev_socket_id(uint8_t device) { return 0; }

enum rte_proc_type_t __attribute__((weak)) rte_eal_process_type() {
  return RTE_PROC_PRIMARY;
}

int __attribute__((weak))
rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
                       uint16_t nb_tx_desc, unsigned int socket_id,
                       const struct rte_eth_txconf *tx_conf) {
  return 0;
}

int __attribute__((weak))
rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
                       uint16_t nb_rx_desc, unsigned int socket_id,
                       const struct rte_eth_rxconf *rx_conf,
                       struct rte_mempool *mb_pool) {
  return 0;
}

int __attribute__((weak)) rte_eth_dev_start(uint8_t portid) { return 0; }

void __attribute__((weak)) rte_eth_promiscuous_enable(uint8_t portid) {
  return;
}

#define rte_eth_rx_burst castan_rte_eth_rx_burst
uint16_t __attribute__((weak))
castan_rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
                        struct rte_mbuf **rx_pkts, const uint16_t nb_pkts) {
  if (port_id == 0) {
    castan_loop();

    *rx_pkts = (struct rte_mbuf *)calloc(sizeof(struct rte_mbuf), 1);

    (*rx_pkts)->buf_addr = malloc(sizeof(struct packet));
    klee_make_symbolic((*rx_pkts)->buf_addr, sizeof(struct packet),
                       "castan_packet");

    (*rx_pkts)->buf_len = sizeof(struct packet);
    (*rx_pkts)->nb_segs = 1;
    (*rx_pkts)->port = 0;
    (*rx_pkts)->packet_type = RTE_PTYPE_L2_ETHER;
    (*rx_pkts)->pkt_len = sizeof(struct packet);
    (*rx_pkts)->data_len = sizeof(struct packet);

    klee_assume(((struct packet *)(*rx_pkts)->buf_addr)->ether.ether_type ==
                htons(ETHER_TYPE_IPv4));
    klee_assume(((struct packet *)(*rx_pkts)->buf_addr)->ipv4.version_ihl ==
                (4 << 4 | 5));
    klee_assume(((struct packet *)(*rx_pkts)->buf_addr)->ipv4.total_length ==
                htons(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr)));
    klee_assume(((struct packet *)(*rx_pkts)->buf_addr)->ipv4.next_proto_id ==
                IPPROTO_UDP);
    klee_assume(((struct packet *)(*rx_pkts)->buf_addr)->udp.dgram_len ==
                htons(8));

    if (ntohs(((struct packet *)(*rx_pkts)->buf_addr)->ether.ether_type) ==
        ETHER_TYPE_IPv4) {
      (*rx_pkts)->packet_type |= RTE_PTYPE_L3_IPV4;
    }
    switch (((struct packet *)(*rx_pkts)->buf_addr)->ipv4.next_proto_id) {
    case IPPROTO_UDP:
      (*rx_pkts)->packet_type |= RTE_PTYPE_L4_UDP;
      break;
    case IPPROTO_TCP:
      (*rx_pkts)->packet_type |= RTE_PTYPE_L4_TCP;
      break;
    }

    return 1;
  } else {
    return 0;
  }
}

#define rte_eth_tx_burst castan_rte_eth_tx_burst
uint16_t __attribute__((weak))
castan_rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
                        struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
  return nb_pkts;
}

uint16_t __attribute__((weak))
castan_rte_ipv4_phdr_cksum(const struct ipv4_hdr *ipv4_hdr, uint64_t ol_flags) {
  struct ipv4_psd_header {
    uint32_t src_addr; /* IP address of source host. */
    uint32_t dst_addr; /* IP address of destination host. */
    uint8_t zero;      /* zero. */
    uint8_t proto;     /* L4 protocol type. */
    uint16_t len;      /* L4 length. */
  } psd_hdr;

  psd_hdr.src_addr = ipv4_hdr->src_addr;
  psd_hdr.dst_addr = ipv4_hdr->dst_addr;
  psd_hdr.zero = 0;
  psd_hdr.proto = ipv4_hdr->next_proto_id;
  if (ol_flags & PKT_TX_TCP_SEG) {
    psd_hdr.len = 0;
  } else {
    psd_hdr.len = htons(
        (uint16_t)(ntohs(ipv4_hdr->total_length) - sizeof(struct ipv4_hdr)));
  }
  return rte_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

#define rte_ipv4_udptcp_cksum castan_rte_ipv4_udptcp_cksum
uint16_t __attribute__((weak))
castan_rte_ipv4_udptcp_cksum(const struct ipv4_hdr *ipv4_hdr,
                             const void *l4_hdr) {
  uint32_t cksum;
  uint32_t l4_len;

  l4_len = ntohs(ipv4_hdr->total_length) - sizeof(struct ipv4_hdr);

  cksum = rte_raw_cksum(l4_hdr, l4_len);
  cksum += castan_rte_ipv4_phdr_cksum(ipv4_hdr, 0);

  cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
  cksum = (~cksum) & 0xffff;
  if (cksum == 0)
    cksum = 0xffff;

  return cksum;
}
