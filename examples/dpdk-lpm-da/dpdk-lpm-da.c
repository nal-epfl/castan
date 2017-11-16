#include <arpa/inet.h>
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

#define PAGE_SIZE (1 << 30)
#ifdef __clang__
const struct rte_memzone *rte_memzone_reserve_aligned(const char *name,
                                                      size_t len, int socket_id,
                                                      unsigned flags,
                                                      unsigned align) {
  struct rte_memzone *mz = malloc(sizeof(struct rte_memzone));
  mz->len = len;
  mz->flags = flags;
  char *ptr = calloc(1, len + align);
  mz->addr = ptr + (align - ((unsigned long long)ptr) % align);
  return mz;
}
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
// The length of the preallocated buffer for the route table file name.
static const unsigned MAX_ROUTE_TABLE_FNAME_LEN = 1024;
static const unsigned LPM_MAX_RULES = 1e6;
static const unsigned LPM_NUMBER_TBL8S = 1 << 8;

// rte_ether
struct ether_addr;

struct nf_config {
  // MAC addresses of devices
  struct ether_addr device_macs[RTE_MAX_ETHPORTS];

  // MAC addresses of the endpoints the devices are linked to
  struct ether_addr endpoint_macs[RTE_MAX_ETHPORTS];

  // The name of the init file containing the IP prefixes table
  char route_table_fname[MAX_ROUTE_TABLE_FNAME_LEN];
};

void config_print_usage(void) {
  printf("Usage:\n"
         "[DPDK EAL options] -- \n"
         "\t--eth-dest <device>,<mac>: MAC address of the"
         " endpoint linked to a device.\n"
         "\t--pfx2as <fname>: the name of the file with the routing table.\n");
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
  unsigned nb_devices = rte_eth_dev_count();

  struct option long_options[] = {{"eth-dest", required_argument, NULL, 'd'},
                                  {"pfx2as", required_argument, NULL, 'r'},
                                  {NULL, 0, NULL, 0}};

  // Set the devices' own MACs
  for (uint8_t device = 0; device < nb_devices; device++) {
    rte_eth_macaddr_get(device, &(config->device_macs[device]));
  }

#ifdef __clang__
  static struct ether_addr endpoint_macs[] = {
      {0x08, 0x00, 0x27, 0x53, 0x8b, 0x38},
      {0x08, 0x00, 0x27, 0xc1, 0x13, 0x47},
  };
  config->endpoint_macs[0] = endpoint_macs[0];
  config->endpoint_macs[1] = endpoint_macs[1];
  strncpy(config->route_table_fname, "testbed/routing-table.pfx2as",
          MAX_ROUTE_TABLE_FNAME_LEN);
  return;
#endif

  int opt;
  while ((opt = getopt_long(argc, argv, "d:r", long_options, NULL)) != EOF) {
    unsigned device;
    switch (opt) {
    case 'd':
      device = parse_int(optarg, "eth-dest deice", 10, ',');
      if (nb_devices <= device) {
        PARSE_ERROR("eth-dest: nb_devices (%d) <= device %d\n", nb_devices,
                    device);
      }
      optarg += 2;
      if (cmdline_parse_etheraddr(NULL, optarg,
                                  &(config->endpoint_macs[device]),
                                  sizeof(int64_t)) < 0) {
        PARSE_ERROR("Invalid MAC address: %s\n", optarg);
      }
      break;
    case 'r':
      strncpy(config->route_table_fname, optarg, MAX_ROUTE_TABLE_FNAME_LEN - 1);
      config->route_table_fname[MAX_ROUTE_TABLE_FNAME_LEN - 1] = '\0';
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

static void config_print(struct nf_config *config) {
  NF_INFO("\n running with the configuration: \n");

  uint32_t nb_devices = rte_eth_dev_count();
  for (uint32_t dev = 0; dev < nb_devices; dev++) {
    char *dev_mac_str = nf_mac_to_str(&(config->device_macs[dev]));
    char *end_mac_str = nf_mac_to_str(&(config->endpoint_macs[dev]));

    NF_INFO("Device %" PRIu32 " own-mac: %s, endpnt-mac: %s", dev, dev_mac_str,
            end_mac_str);

    free(dev_mac_str);
    free(end_mac_str);
  }
  NF_INFO("Routing table file: %s", config->route_table_fname);
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

#define LONGEST_PREFIX 27

typedef struct {
  int port;
  char prefix_len;
} prefix_node_t;
typedef prefix_node_t *lpm_t;

lpm_t lpm_create() {
  const struct rte_memzone *mz = rte_memzone_reserve_aligned(
      "LPM", (1 << LONGEST_PREFIX) * sizeof(prefix_node_t), rte_socket_id(),
      RTE_MEMZONE_1GB, PAGE_SIZE);
  if (!mz) {
    rte_exit(EXIT_FAILURE, "Unable to allocate LPM table.\n");
  }

#ifndef __clang__
  memset(mz->addr, 0, (1 << LONGEST_PREFIX) * sizeof(prefix_node_t));
#endif

  NF_INFO("Table physical address: %016lX\n", rte_mem_virt2phy(mz->addr));

  return (lpm_t)mz->addr;
}

int lpm_set_prefix_port(lpm_t lpm, uint32_t ip, int prefix_len, int port) {
#ifndef __clang__
  if (prefix_len > LONGEST_PREFIX) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str));
    fprintf(stderr, "Prefix %s/%d is too long. Ignoring.\n", ip_str,
            prefix_len);
    return -1;
  }

  for (int i = (ip & ~((1 << (sizeof(ip) * 8 - prefix_len)) - 1)) >>
               (sizeof(ip) * 8 - LONGEST_PREFIX);
       i <= (ip | ((1 << (sizeof(ip) * 8 - prefix_len)) - 1)) >>
       (sizeof(ip) * 8 - LONGEST_PREFIX);
       i++) {
    if (lpm[i].prefix_len <= prefix_len) {
      lpm[i].port = port;
      lpm[i].prefix_len = prefix_len;
    }
  }
#endif
  return 0;
}

int lpm_get_ip_port(lpm_t lpm, uint32_t ip, uint32_t *out_port) {
  *out_port = lpm[ip >> (sizeof(ip) * 8 - LONGEST_PREFIX)].port;

  return 0;
}

void init_lpm(const char fname[], lpm_t *lpm_out) {
  uint32_t nb_devices = rte_eth_dev_count();
  *lpm_out = lpm_create();
  if (*lpm_out == NULL) {
    rte_exit(EXIT_FAILURE, "Cannot allocate the LPM table on socket %d",
             rte_socket_id());
  }

  FILE *pfx2as_file = fopen(fname, "r");
  if (pfx2as_file == NULL) {
    rte_exit(EXIT_FAILURE, "Error opening pfx2as file: %s.\n", fname);
  }

  // rte_lpm_add(*lpm_out, 0, 0, nb_devices);;
  for (unsigned long count = 0; count < LPM_MAX_RULES; ++count) {
    char ip_str[INET_ADDRSTRLEN];
    uint32_t ip;
    uint8_t depth;
    uint32_t if_out;
    int asn;

    int result = fscanf(pfx2as_file, "%s", ip_str);
    if (result == EOF) {
      break;
    }

    if (result != 1) {
      rte_exit(EXIT_FAILURE, "Error in ipaddr in pfx2as file %s:%lu\n", fname,
               count);
    }

    result = fscanf(pfx2as_file, "%hh" PRIu8, &depth);
    if (result != 1) {
      rte_exit(EXIT_FAILURE, "Error in prefix detpth in pfx2as file %s:%lu\n",
               fname, count);
    }

    result = fscanf(pfx2as_file, "%d_", &asn);
    if (result == 1) {
      while (getc(pfx2as_file) != '\n')
        continue;
    }
    inet_pton(AF_INET, ip_str, &ip);
    if (asn < 0 || nb_devices < asn) {
      NF_INFO("asn # %d does not correspond to any "
              "device (0 - %u), skipping\n",
              asn, nb_devices - 1);
    } else {
      if_out = asn;
      NF_DEBUG("adding rule: %s/%" PRIu8 " -> %" PRIu32, nf_ipv4_to_str(ip),
               depth, if_out);
      result =
          lpm_set_prefix_port(*lpm_out, rte_be_to_cpu_32(ip), depth, if_out);
      if (result < 0) {
        rte_exit(EXIT_FAILURE, "Cannot add entry %lu to the LPM table.", count);
      }
    }
  }
  fclose(pfx2as_file);
}

void initialize(struct nf_config *config, lpm_t *lpm_out) {
  uint32_t nb_devices = rte_eth_dev_count();
  struct rte_mempool *mbuf_pool =
      rte_pktmbuf_pool_create("MEMPOOL",                         // name
                              MEMPOOL_BUFFER_COUNT * nb_devices, // # elements
                              MEMPOOL_CACHE_SIZE,
                              0, // application private area size
                              RTE_MBUF_DEFAULT_BUF_SIZE, // data buffer size
                              rte_socket_id());          // socket ID
  if (mbuf_pool == NULL) {
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }

  // Initialize all devices
  for (uint32_t device = 0; device < nb_devices; ++device) {
    if (nf_init_device(device, mbuf_pool) == 0) {
      NF_INFO("Initialized device %" PRIu32 ".", device);
    } else {
      rte_exit(EXIT_FAILURE, "Cannot init device %" PRIu32 ".", device);
    }
  }

  init_lpm(config->route_table_fname, lpm_out);
}

static inline uint32_t lpm_get_dst_port(struct ipv4_hdr *ipv4_hdr,
                                        uint32_t portid, lpm_t lpm) {
  uint32_t next_hop;
  int success =
      lpm_get_ip_port(lpm, rte_be_to_cpu_32(ipv4_hdr->dst_addr), &next_hop);
  NF_DEBUG("lookup success: %d", success);
  return success ? portid : next_hop;
}

uint32_t dispatch_packet(struct nf_config *config, uint32_t device, lpm_t lpm,
                         struct rte_mbuf *mbuf) {
  uint8_t nb_devices = rte_eth_dev_count();
  struct ether_hdr *ether_header = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
  if (!RTE_ETH_IS_IPV4_HDR(mbuf->packet_type) &&
      !(mbuf->packet_type == 0 &&
        ether_header->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))) {
    return device; // Non IPv4 packet, ignore
  }
  struct ipv4_hdr *ipv4_header = rte_pktmbuf_mtod_offset(
      mbuf, struct ipv4_hdr *, sizeof(struct ether_hdr));

  if (ipv4_header == NULL) {
    return device; // Not IPv4 packet, ignore
  }

  NF_DEBUG("dispatching: %s (%" PRIu32 ") -> %s",
           nf_ipv4_to_str(ipv4_header->src_addr), device,
           nf_ipv4_to_str(ipv4_header->dst_addr));

  uint32_t dst_dev = lpm_get_dst_port(ipv4_header, device, lpm);

  NF_DEBUG("it goes to %" PRIu32, dst_dev);

  if (dst_dev != device) {
    if (nb_devices <= dst_dev) {
      rte_exit(EXIT_FAILURE,
               "The LPM table contains a out-of-range device id (%d) >= %d",
               dst_dev, nb_devices);
    }
    ether_header->s_addr = config->device_macs[dst_dev];
    ether_header->d_addr = config->endpoint_macs[dst_dev];
  }
  return dst_dev;
}

void run(struct nf_config *config, lpm_t lpm) {

  uint8_t nb_devices = rte_eth_dev_count();

  while (1) {
    for (uint32_t device = 0; device < nb_devices; ++device) {
      struct rte_mbuf *mbuf[1];
      uint16_t actual_rx_len = rte_eth_rx_burst(device, 0, mbuf, 1);

      if (actual_rx_len != 0) {
#ifdef LATENCY
        struct timespec timestamp;
        if (clock_gettime(CLOCK_MONOTONIC, &timestamp)) {
          rte_exit(EXIT_FAILURE, "Cannot get timestamp.\n");
        }
#endif

        uint32_t dst_device = dispatch_packet(config, device, lpm, mbuf[0]);

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
  lpm_t lpm;
  configure(&config, argc, argv);
  initialize(&config, &lpm);
  run(&config, lpm);
  // No tear down, as the previous function is not supposed to ever return.
}
