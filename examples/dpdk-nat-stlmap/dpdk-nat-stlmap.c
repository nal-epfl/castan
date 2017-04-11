#include <arpa/inet.h>
#include <cmdline_parse_etheraddr.h>
#include <getopt.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <map>
#include <netinet/in.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef __clang__
#define NF_INFO(text, ...)                                                     \
  do {                                                                         \
  } while (0);
#define NF_DEBUG(text, ...)                                                    \
  do {                                                                         \
  } while (0);
#else
#define NF_INFO(text, ...)                                                     \
  printf(text "\n", ##__VA_ARGS__);                                            \
  fflush(stdout)
#define NF_DEBUG(text, ...)                                                    \
  printf(text "\n", ##__VA_ARGS__);                                            \
  fflush(stdout)
#endif

// Queue sizes for receiving/transmitting packets (set to their values from
// l3fwd sample)
static const uint16_t RX_QUEUE_SIZE = 128;
static const uint16_t TX_QUEUE_SIZE = 512;
// Memory pool #buffers and per-core cache size (set to their values from l3fwd
// sample)
static const unsigned MEMPOOL_BUFFER_COUNT = 8192;
static const unsigned MEMPOOL_CACHE_SIZE = 256;
// The number of devices used.
static const unsigned NUM_ETHPORTS = 2;
//
// // rte_ether
// struct ether_addr;

struct nf_config {
  // MAC addresses of devices
  struct ether_addr device_macs[NUM_ETHPORTS];

  // MAC addresses of the endpoints the devices are linked to
  struct ether_addr endpoint_macs[NUM_ETHPORTS];

  // External IP address of the NAT.
  uint32_t nat_ip;
};

void config_print_usage(void) {
  printf("Usage:\n"
         "[DPDK EAL options] -- \n"
         "\t--eth-dest <device>,<mac>: MAC address of the endpoint linked to a "
         "device.\n"
         "\t--nat-ip <ip>: NAT external IP address.\n");
}

static uintmax_t parse_int(const char *str, const char *name, int base,
                           char next) {
  char *temp;
  intmax_t result = strtoimax(str, &temp, base);

  // There's also a weird failure case with overflows, but let's not care
  if (temp == str || *temp != next) {
    rte_exit(EXIT_FAILURE, "Error while parsing '%s': %s\n", name, str);
  }

  return result;
}

#define PARSE_ERROR(format, ...)                                               \
  config_print_usage();                                                        \
  rte_exit(EXIT_FAILURE, format, ##__VA_ARGS__);

void config_init(struct nf_config *config, int argc, char **argv) {
  struct option long_options[] = {{"eth-dest", required_argument, NULL, 'd'},
                                  {"nat-ip", required_argument, NULL, 'i'},
                                  {NULL, 0, NULL, 0}};

  // Set the devices' own MACs
  for (uint8_t device = 0; device < NUM_ETHPORTS; device++) {
    rte_eth_macaddr_get(device, &(config->device_macs[device]));
  }

#ifdef __clang__
  static struct ether_addr endpoint_macs[] = {
      {0x08, 0x00, 0x27, 0x53, 0x8b, 0x38},
      {0x08, 0x00, 0x27, 0xc1, 0x13, 0x47},
  };
  config->endpoint_macs[0] = endpoint_macs[0];
  config->endpoint_macs[1] = endpoint_macs[1];
  inet_pton(AF_INET, "192.168.0.1", &config->nat_ip);
  return;
#endif

  int opt;
  while ((opt = getopt_long(argc, argv, "d:r", long_options, NULL)) != EOF) {
    unsigned device;
    switch (opt) {
    case 'd':
      device = parse_int(optarg, "eth-dest device", 10, ',');
      if (NUM_ETHPORTS <= device) {
        PARSE_ERROR("eth-dest: nb_devices (%d) <= device %d\n", NUM_ETHPORTS,
                    device);
      }
      optarg += 2;
      if (cmdline_parse_etheraddr(NULL, optarg,
                                  &(config->endpoint_macs[device]),
                                  sizeof(int64_t)) < 0) {
        PARSE_ERROR("Invalid MAC address: %s\n", optarg);
      }
      break;
    case 'i':

      if (!inet_pton(AF_INET, optarg, &config->nat_ip)) {
        PARSE_ERROR("Invalid IP address: %s\n", optarg);
      }
      break;
    default:
      PARSE_ERROR("Invalid key: %c\n", opt);
    }
  }

  // Reset getopt
  optind = 1;
}

char *nf_mac_to_str(struct ether_addr *addr) {
  // format is xx:xx:xx:xx:xx:xx\0
  uint16_t buffer_size = 6 * 2 + 5 + 1;
  char *buffer = (char *)calloc(buffer_size, sizeof(char));
  if (buffer == NULL) {
    rte_exit(EXIT_FAILURE, "Out of memory in nat_mac_to_str!");
  }

  ether_format_addr(buffer, buffer_size, addr);
  return buffer;
}

// TODO: automatic garbage management
char *nf_ipv4_to_str(uint32_t addr) {
  // format is xxx.xxx.xxx.xxx\0
  uint16_t buffer_size = 4 * 3 + 3 + 1;
  char *buffer = (char *)calloc(buffer_size, sizeof(char));
  if (buffer == NULL) {
    rte_exit(EXIT_FAILURE, "Out of memory in nf_ipv4_to_str!");
  }

  snprintf(buffer, buffer_size, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
           addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF,
           (addr >> 24) & 0xFF);
  return buffer;
}

static void config_print(struct nf_config *config) {
  NF_INFO("\n running with the configuration: \n");

  for (uint32_t dev = 0; dev < NUM_ETHPORTS; dev++) {
    char *dev_mac_str = nf_mac_to_str(&(config->device_macs[dev]));
    char *end_mac_str = nf_mac_to_str(&(config->endpoint_macs[dev]));

    NF_INFO("Device %" PRIu32 " own-mac: %s, endpnt-mac: %s", dev, dev_mac_str,
            end_mac_str);

    free(dev_mac_str);
    free(end_mac_str);
  }

  NF_INFO("NAT external IP: Device %s", nf_ipv4_to_str(config->nat_ip));
}

static void configure(struct nf_config *config, int argc, char *argv[]) {
  // Initialize the Environment Abstraction Layer (EAL)
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    rte_exit(EXIT_FAILURE, "Error with EAL initialization, ret=%d\n", ret);
  }
  argc -= ret;
  argv += ret;

  config_init(config, argc, argv);
  config_print(config);
}

static int nf_init_device(uint32_t device, struct rte_mempool *mbuf_pool) {
  static struct rte_eth_conf device_conf;
  memset(&device_conf, 0, sizeof(struct rte_eth_conf));
  device_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
  device_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
  device_conf.rxmode.split_hdr_size = 0;
  device_conf.rxmode.header_split = 0;   /**< Header Split disabled */
  device_conf.rxmode.hw_ip_checksum = 1; /**< IP checksum offload enabled */
  device_conf.rxmode.hw_vlan_filter = 0; /**< VLAN filtering disabled */
  device_conf.rxmode.jumbo_frame = 0;    /**< Jumbo Frame Support disabled */
  device_conf.rxmode.hw_strip_crc = 0;   /**< CRC stripped by hardware */
  device_conf.rx_adv_conf.rss_conf.rss_key = NULL;
  device_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP;
  device_conf.txmode.mq_mode = ETH_MQ_TX_NONE;

  int retval = rte_eth_dev_configure(device,
                                     1, // # RX queues
                                     1, // # TX queues
                                     &device_conf);
  if (retval < 0) {
    rte_exit(EXIT_FAILURE, "Cannot configure device %" PRIu32 ", err=%d",
             device, retval);
  }

  // Allocate and set up 1 RX queue per device
  retval = rte_eth_rx_queue_setup(device,
                                  0, // queue ID
                                  RX_QUEUE_SIZE, rte_eth_dev_socket_id(device),
                                  NULL, // config (NULL = default)
                                  mbuf_pool);
  if (retval < 0) {
    rte_exit(EXIT_FAILURE,
             "Cannot allocate RX queue for device %" PRIu32 ", err=%d", device,
             retval);
  }

  // Allocate and set up 1 TX queue per device
  retval = rte_eth_tx_queue_setup(device,
                                  0, // queue ID
                                  TX_QUEUE_SIZE, rte_eth_dev_socket_id(device),
                                  NULL); // config (NULL = default)
  if (retval < 0) {
    rte_exit(EXIT_FAILURE,
             "Cannot allocate TX queue for device %" PRIu32 ", err=%d", device,
             retval);
  }

  // Start the device
  retval = rte_eth_dev_start(device);
  if (retval < 0) {
    rte_exit(EXIT_FAILURE, "Cannot start device %" PRIu32 ", err=%d", device,
             retval);
  }

  // Enable RX in promiscuous mode for the device
  rte_eth_promiscuous_enable(device);

  return 0;
}

typedef struct __attribute__((packed)) hash_key_t {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint8_t proto;
  uint16_t src_port;
  uint16_t dst_port;

  bool operator<(const hash_key_t &other) const {
    return this->src_ip != other.src_ip
               ? this->src_ip < other.src_ip
               : this->dst_ip != other.dst_ip
                     ? this->dst_ip < other.dst_ip
                     : this->proto != other.proto
                           ? this->proto < other.proto
                           : this->src_port != other.src_port
                                 ? this->src_port < other.src_port
                                 : this->dst_port < other.dst_port;
  }
} hash_key_t;

typedef hash_key_t hash_value_t;
typedef std::map<hash_key_t, hash_value_t> *hash_table_t;

void hash_init(hash_table_t *hash_table) { *hash_table = new std::map<hash_key_t, hash_value_t>(); }

void hash_set(hash_table_t hash_table, hash_key_t key, hash_value_t value) {
  (*hash_table)[key] = value;
}

int hash_get(hash_table_t hash_table, hash_key_t key, hash_value_t *value) {
  if (hash_table->count(key)) {
    *value = (*hash_table)[key];
    return 1;
  }

  return 0;
}

void initialize(struct nf_config *config, hash_table_t *hash_table) {
  struct rte_mempool *mbuf_pool =
      rte_pktmbuf_pool_create("MEMPOOL",                           // name
                              MEMPOOL_BUFFER_COUNT * NUM_ETHPORTS, // # elements
                              MEMPOOL_CACHE_SIZE,
                              0, // application private area size
                              RTE_MBUF_DEFAULT_BUF_SIZE, // data buffer size
                              rte_socket_id());          // socket ID
  if (mbuf_pool == NULL) {
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }

  // Initialize all devices
  for (uint32_t device = 0; device < NUM_ETHPORTS; ++device) {
    if (nf_init_device(device, mbuf_pool) == 0) {
      NF_INFO("Initialized device %" PRIu32 ".", device);
    } else {
      rte_exit(EXIT_FAILURE, "Cannot init device %" PRIu32 ".", device);
    }
  }

  hash_init(hash_table);
}

uint32_t dispatch_packet(struct nf_config *config, uint32_t device,
                         hash_table_t hash_table, struct rte_mbuf *mbuf) {
  struct ether_hdr *ether_header = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
  if (!RTE_ETH_IS_IPV4_HDR(mbuf->packet_type) &&
      !(mbuf->packet_type == 0 &&
        ether_header->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))) {
    return device; // Non IPv4 packet, ignore
  }
  struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr *,
                                                sizeof(struct ether_hdr));

  if (ip == NULL) {
    return device; // Not IPv4 packet, ignore
  }

  uint16_t sport = 0, dport = 0;

  switch (ip->next_proto_id) {
  case IPPROTO_TCP: {
    if (mbuf->pkt_len < sizeof(struct ether_hdr) +
                            (ip->version_ihl & 0x0F) * 4 +
                            sizeof(struct tcp_hdr)) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    if (ip->total_length <
        (ip->version_ihl & 0x0F) * 4 + sizeof(struct tcp_hdr)) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    struct tcp_hdr *tcp = rte_pktmbuf_mtod_offset(
        mbuf, struct tcp_hdr *,
        sizeof(struct ether_hdr) + (ip->version_ihl & 0x0F) * 4);
    if (tcp->data_off < 5) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    sport = tcp->src_port;
    dport = tcp->dst_port;
  } break;
  case IPPROTO_UDP: {
    if (mbuf->pkt_len < sizeof(struct ether_hdr) +
                            (ip->version_ihl & 0x0F) * 4 +
                            sizeof(struct udp_hdr)) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    if (ip->total_length <
        (ip->version_ihl & 0x0F) * 4 + sizeof(struct udp_hdr)) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    struct udp_hdr *udp = rte_pktmbuf_mtod_offset(
        mbuf, struct udp_hdr *,
        sizeof(struct ether_hdr) + (ip->version_ihl & 0x0F) * 4);
    if (ntohs(udp->dgram_len) < 8) {
      NF_DEBUG("Truncated packet.");
      return device;
    }
    sport = udp->src_port;
    dport = udp->dst_port;
  } break;
  default:
    NF_DEBUG("Packet with unsupported transport protocol: %d",
             ip->next_proto_id);
    return device;
  }

  hash_key_t key = {
      .src_ip = ip->src_addr,
      .dst_ip = ip->dst_addr,
      .proto = ip->next_proto_id,
      .src_port = sport,
      .dst_port = dport,
  };

  hash_value_t translation;
  if (!hash_get(hash_table, key, &translation)) {
    NF_DEBUG("New connection.");

    // New connection. Set up state.
    if (ip->dst_addr == config->nat_ip) {
      // Unknown return packet. Null-route.
      NF_DEBUG("New outside connection.");
      return device;
    } else {
      // Find unused port.
      hash_key_t out_key = {
          .src_ip = ip->dst_addr,
          .dst_ip = config->nat_ip,
          .proto = ip->next_proto_id,
          .src_port = dport,
          .dst_port = sport,
      };
      for (; hash_get(hash_table, out_key, NULL); out_key.dst_port++) {
      }
      NF_DEBUG("Translating source port: %d -> %d.", rte_be_to_cpu_16(sport),
               rte_be_to_cpu_16(out_key.dst_port));

      // Save entry for future outgoing traffic.
      translation = key;
      translation.src_ip = config->nat_ip;
      translation.src_port = out_key.dst_port;
      hash_set(hash_table, key, translation);

      // Save entry for returning traffic.
      hash_key_t out_translation = out_key;
      out_translation.dst_ip = ip->src_addr;
      out_translation.dst_port = sport;
      hash_set(hash_table, out_key, out_translation);
    }
  }

  // Translate packet inplace.
  uint32_t dst_dev = device;

  ip->hdr_checksum = 0;
  switch (ip->next_proto_id) {
  case IPPROTO_TCP: {
    struct tcp_hdr *tcp = rte_pktmbuf_mtod_offset(
        mbuf, struct tcp_hdr *,
        sizeof(struct ether_hdr) + (ip->version_ihl & 0x0F) * 4);

    NF_DEBUG("Translating packet from port %d %s:%d:TCP:%s:%d to port %d "
             "%s:%d:TCP:%s:%d",
             device, nf_ipv4_to_str(ip->src_addr),
             rte_be_to_cpu_16(tcp->src_port), nf_ipv4_to_str(ip->dst_addr),
             rte_be_to_cpu_16(tcp->dst_port), device ^ 0x01,
             nf_ipv4_to_str(translation.src_ip),
             rte_be_to_cpu_16(translation.src_port),
             nf_ipv4_to_str(translation.dst_ip),
             rte_be_to_cpu_16(translation.dst_port));

    dst_dev = device ^ 0x01;
    ip->src_addr = translation.src_ip;
    ip->dst_addr = translation.dst_ip;
    ip->next_proto_id = translation.proto;
    tcp->src_port = translation.src_port;
    tcp->dst_port = translation.dst_port;

    tcp->cksum = 0;
    tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
  } break;
  case IPPROTO_UDP: {
    struct udp_hdr *udp = rte_pktmbuf_mtod_offset(
        mbuf, struct udp_hdr *,
        sizeof(struct ether_hdr) + (ip->version_ihl & 0x0F) * 4);

    NF_DEBUG("Translating packet from port %d %s:%d:UDP:%s:%d to port %d "
             "%s:%d:UDP:%s:%d",
             device, nf_ipv4_to_str(ip->src_addr),
             rte_be_to_cpu_16(udp->src_port), nf_ipv4_to_str(ip->dst_addr),
             rte_be_to_cpu_16(udp->dst_port), device ^ 0x01,
             nf_ipv4_to_str(translation.src_ip),
             rte_be_to_cpu_16(translation.src_port),
             nf_ipv4_to_str(translation.dst_ip),
             rte_be_to_cpu_16(translation.dst_port));

    dst_dev = device ^ 0x01;
    ip->src_addr = translation.src_ip;
    ip->dst_addr = translation.dst_ip;
    ip->next_proto_id = translation.proto;
    udp->src_port = translation.src_port;
    udp->dst_port = translation.dst_port;

    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
  } break;
  }
  ip->hdr_checksum = rte_ipv4_cksum(ip);

  if (dst_dev != device) {
    ether_header->s_addr = config->device_macs[dst_dev];
    ether_header->d_addr = config->endpoint_macs[dst_dev];
  }
  return dst_dev;
}

void run(struct nf_config *config, hash_table_t hash_table) {
  while (1) {
    for (uint32_t device = 0; device < NUM_ETHPORTS; ++device) {
      struct rte_mbuf *mbuf[1];
      uint16_t actual_rx_len = rte_eth_rx_burst(device, 0, mbuf, 1);

      if (actual_rx_len != 0) {
        uint32_t dst_device =
            dispatch_packet(config, device, hash_table, mbuf[0]);

        if (dst_device == device) {
          rte_pktmbuf_free(mbuf[0]);
        } else {
          uint16_t actual_tx_len = rte_eth_tx_burst(dst_device, 0, mbuf, 1);

          if (actual_tx_len < 1) {
            rte_pktmbuf_free(mbuf[0]);
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  struct nf_config config;
  hash_table_t hash_table;
  configure(&config, argc, argv);
  initialize(&config, &hash_table);
  run(&config, hash_table);
  // No tear down, as the previous function is not supposed to ever return.
}
