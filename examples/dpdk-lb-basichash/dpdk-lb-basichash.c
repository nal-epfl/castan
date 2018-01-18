#include <arpa/inet.h>
#include <castan/castan.h>
#include <cmdline_parse_etheraddr.h>
#include <getopt.h>
#include <inttypes.h>
#include <linux/limits.h>
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
  } while (0)
#define NF_DEBUG(text, ...)                                                    \
  do {                                                                         \
  } while (0)

#else

#define NF_INFO(text, ...)                                                     \
  printf(text "\n", ##__VA_ARGS__);                                            \
  fflush(stdout)

#ifdef NDEBUG
#define NF_DEBUG(...)                                                          \
  do {                                                                         \
  } while (0)
#else
#define NF_DEBUG(text, ...)                                                    \
  printf(text "\n", ##__VA_ARGS__);                                            \
  fflush(stdout)
#endif

#endif

#ifdef PTP
struct ptpv2_msg {
  uint8_t msg_id;
  uint8_t version;
  uint8_t unused[34];
};
#endif

#ifdef NODROP
#define DROP_PACKET(mbuf, device)                                              \
  {                                                                            \
    uint16_t actual_tx_len = rte_eth_tx_burst(1 - device, 0, mbuf, 1);         \
    if (actual_tx_len < 1) {                                                   \
      rte_pktmbuf_free(mbuf[0]);                                               \
    }                                                                          \
  }
#else // NODROP
#define DROP_PACKET(mbuf, device) rte_pktmbuf_free(mbuf[0])
#endif // NODROP

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
// The number of servers to load balance.
static const unsigned NUM_DIPS = 2;
//
// // rte_ether
// struct ether_addr;

struct nf_config {
  // MAC addresses of devices
  struct ether_addr device_macs[NUM_ETHPORTS];

  // MAC addresses of the endpoints the devices are linked to
  struct ether_addr endpoint_macs[NUM_ETHPORTS];

  // External IP address of the load balancer.
  uint32_t vip;

  // Internal IP addresses of the servers.
  uint32_t dips[NUM_DIPS];
};

void config_print_usage(void) {
  printf("Usage:\n"
         "[DPDK EAL options] -- \n"
         "\t--eth-dest <device>,<mac>: MAC address of the endpoint linked to a "
         "device.\n"
         "\t--vip <ip>: LB virtual IP address.\n"
         "\t--dip <ip>: Server direct IP address(es).\n");
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
  struct option long_options[] = {{"eth-dest", required_argument, NULL, 'e'},
                                  {"vip", required_argument, NULL, 'v'},
                                  {"dip", required_argument, NULL, 'd'},
                                  {NULL, 0, NULL, 0}};

  // Set the devices' own MACs
  for (uint8_t device = 0; device < NUM_ETHPORTS; device++) {
    rte_eth_macaddr_get(device, &(config->device_macs[device]));
  }
  for (int i = 0; i < NUM_DIPS; i++) {
    config->dips[i] = 0;
  }

#ifdef __clang__
  static struct ether_addr endpoint_macs[] = {
      {0x08, 0x00, 0x27, 0x53, 0x8b, 0x38},
      {0x08, 0x00, 0x27, 0xc1, 0x13, 0x47},
  };
  config->endpoint_macs[0] = endpoint_macs[0];
  config->endpoint_macs[1] = endpoint_macs[1];
  inet_pton(AF_INET, "192.168.0.1", &config->vip);
  inet_pton(AF_INET, "10.0.0.1", &config->dips[0]);
  inet_pton(AF_INET, "10.0.0.2", &config->dips[1]);
  return;
#endif

  int opt;
  int dip_idx = 0;
  while ((opt = getopt_long(argc, argv, "v:d:e", long_options, NULL)) != EOF) {
    unsigned device;
    switch (opt) {
    case 'e':
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
    case 'v':
      if (!inet_pton(AF_INET, optarg, &config->vip)) {
        PARSE_ERROR("Invalid IP address: %s\n", optarg);
      }
      break;
    case 'd':
      if (dip_idx >= NUM_DIPS) {
        PARSE_ERROR("Too many direct IPs.");
      }
      if (!inet_pton(AF_INET, optarg, &config->dips[dip_idx++])) {
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

  NF_INFO("LB external VIP: %s", nf_ipv4_to_str(config->vip));
  for (int i = 0; i < NUM_DIPS && config->dips[i]; i++) {
    NF_INFO("Server %d DIP: %s", i, nf_ipv4_to_str(config->dips[i]));
  }
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

#define TABLE_SIZE (1 << 16)

typedef struct __attribute__((packed)) {
  uint32_t src_ip;
  // uint8_t proto;
  // uint16_t src_port;
} hash_key_t;

typedef struct __attribute__((packed)) { uint32_t dst_ip; } hash_value_t;

typedef struct hash_entry_t {
  hash_key_t key;
  hash_value_t value;

  struct hash_entry_t *next;
} hash_entry_t;

typedef hash_entry_t **hash_table_t;

void hash_init(hash_table_t *hash_table) {
  *hash_table = (hash_table_t)calloc(TABLE_SIZE, sizeof(hash_entry_t *));
}

int hash_key_equals(hash_key_t a, hash_key_t b) {
  return (a.src_ip == b.src_ip); 
         // & (a.proto == b.proto);
         // & (a.src_port == b.src_port);
}

#define hash_function_rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

#define hash_function_mix(a, b, c)                                             \
  {                                                                            \
    a -= c;                                                                    \
    a ^= hash_function_rot(c, 4);                                              \
    c += b;                                                                    \
    b -= a;                                                                    \
    b ^= hash_function_rot(a, 6);                                              \
    a += c;                                                                    \
    c -= b;                                                                    \
    c ^= hash_function_rot(b, 8);                                              \
    b += a;                                                                    \
    a -= c;                                                                    \
    a ^= hash_function_rot(c, 16);                                             \
    c += b;                                                                    \
    b -= a;                                                                    \
    b ^= hash_function_rot(a, 19);                                             \
    a += c;                                                                    \
    c -= b;                                                                    \
    c ^= hash_function_rot(b, 4);                                              \
    b += a;                                                                    \
  }

#define hash_function_final(a, b, c)                                           \
  {                                                                            \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 14);                                             \
    a ^= c;                                                                    \
    a -= hash_function_rot(c, 11);                                             \
    b ^= a;                                                                    \
    b -= hash_function_rot(a, 25);                                             \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 16);                                             \
    a ^= c;                                                                    \
    a -= hash_function_rot(c, 4);                                              \
    b ^= a;                                                                    \
    b -= hash_function_rot(a, 14);                                             \
    c ^= b;                                                                    \
    c -= hash_function_rot(b, 24);                                             \
  }

uint32_t hash_function(hash_key_t key) {
  // Based on Bob Jenkins' lookup3 algorithm.
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t)sizeof(hash_key_t));

  a += key.src_ip;
//   b += key.proto;
//   c += key.src_port;

  hash_function_final(a, b, c);
  return c;
}

void hash_set(hash_table_t hash_table, hash_key_t key, hash_value_t value, uint32_t hash) {
  hash_entry_t *entry;

  for (entry = hash_table[hash]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      entry->value = value;
      return;
    }
  }

  entry = (hash_entry_t *)malloc(sizeof(hash_entry_t));
  entry->key = key;
  entry->value = value;
  entry->next = hash_table[hash];
  hash_table[hash] = entry;
}

int hash_get(hash_table_t hash_table, hash_key_t key, hash_value_t *value, uint32_t hash) {
  hash_entry_t *entry;

  for (entry = hash_table[hash]; entry; entry = entry->next) {
    if (hash_key_equals(entry->key, key)) {
      if (value) {
        *value = entry->value;
      }
      return 1;
    }
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

  uint16_t sport = 0;

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
  } break;
  default:
    NF_DEBUG("Packet with unsupported transport protocol: %d",
             ip->next_proto_id);
    return device;
  }

  // Translate packet inplace.
  uint32_t dst_dev = device ^ 0x01;
  ip->hdr_checksum = 0;

  if (ip->dst_addr == config->vip) { // Incoming packet.
    hash_key_t key = {
        .src_ip = ip->src_addr, // .proto = ip->next_proto_id, .src_port = sport,
    };
    uint32_t hash;
    castan_havoc(key, hash, hash_function(key) % TABLE_SIZE);

    hash_value_t translation;
    if (!hash_get(hash_table, key, &translation, hash)) {
      NF_DEBUG("New connection.");
      // New connection. Set up state.
      // Pick next IP.
      static int next_dip = 0;
      translation.dst_ip = config->dips[next_dip++];
      if (next_dip >= NUM_DIPS || !config->dips[next_dip]) {
        next_dip = 0;
      }

      // Save entry for future Incoming traffic.
      hash_set(hash_table, key, translation, hash);
    }

    NF_DEBUG("Translating packet from port %d %s:%s to port %d %s:%s", device,
             nf_ipv4_to_str(ip->src_addr), nf_ipv4_to_str(ip->dst_addr),
             dst_dev, nf_ipv4_to_str(ip->src_addr),
             nf_ipv4_to_str(translation.dst_ip));

    ip->dst_addr = translation.dst_ip;
  } else { // Outgoing packet: set source to public IP
    int found_dip = 0;
    for (int i = 0; i < NUM_DIPS && config->dips[i]; i++) {
      if (ip->src_addr == config->dips[i]) {
        found_dip = 1;
        break;
      }
    }
    if (!found_dip) {
      NF_DEBUG(
          "Packet from port %d %s:%s not involved in load-balancing. Dropping",
          device, nf_ipv4_to_str(ip->src_addr), nf_ipv4_to_str(ip->dst_addr));
      return device;
    }

    NF_DEBUG("Translating packet from port %d %s:%s to port %d %s:%s", device,
             nf_ipv4_to_str(ip->src_addr), nf_ipv4_to_str(ip->dst_addr),
             dst_dev, nf_ipv4_to_str(config->vip),
             nf_ipv4_to_str(ip->dst_addr));

    ip->src_addr = config->vip;
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
#ifdef LATENCY
        struct timespec timestamp;
        if (clock_gettime(CLOCK_MONOTONIC, &timestamp)) {
          rte_exit(EXIT_FAILURE, "Cannot get timestamp.\n");
        }
#endif

        uint32_t dst_device =
            dispatch_packet(config, device, hash_table, mbuf[0]);

#ifdef PTP
        struct ptpv2_msg *ptp =
            (struct ptpv2_msg *)(rte_pktmbuf_mtod(mbuf[0], char *) +
                                 sizeof(struct ether_hdr));
        rte_pktmbuf_mtod(mbuf[0], struct ether_hdr *)->ether_type = 0xf788;
        ptp->msg_id = 0;
        ptp->version = 0x02;
#endif

        if (dst_device == device) {
          DROP_PACKET(mbuf, device);
        } else {
          uint16_t actual_tx_len = rte_eth_tx_burst(dst_device, 0, mbuf, 1);

          if (actual_tx_len < 1) {
            rte_pktmbuf_free(mbuf[0]);
          }
        }

#ifdef LATENCY
        struct timespec new_timestamp;
        if (clock_gettime(CLOCK_MONOTONIC, &new_timestamp)) {
          rte_exit(EXIT_FAILURE, "Cannot get timestamp.\n");
        }
        NF_INFO("Latency: %ld ns.",
                (new_timestamp.tv_sec - timestamp.tv_sec) * 1000000000 +
                    (new_timestamp.tv_nsec - timestamp.tv_nsec));
#endif
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
