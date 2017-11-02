#include <cmdline_parse_etheraddr.h>
#include <getopt.h>
#include <inttypes.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <stdio.h>

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
};

void config_print_usage(void) {
  printf("Usage:\n"
         "[DPDK EAL options] -- \n"
         "\t--eth-dest <device>,<mac>: MAC address of the"
         " endpoint linked to a device.\n");
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

void initialize(struct nf_config *config) {
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
}

void run(struct nf_config *config) {

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

        uint32_t dst_device = device ^ 0x01;

#ifdef PTP
        struct ptpv2_msg *ptp =
            (struct ptpv2_msg *)(rte_pktmbuf_mtod(mbuf[0], char *) +
                                 sizeof(struct ether_hdr));
        rte_pktmbuf_mtod(mbuf[0], struct ether_hdr *)->ether_type = 0xf788;
        ptp->msg_id = 0;
        ptp->version = 0x02;
#endif

        uint16_t actual_tx_len = rte_eth_tx_burst(dst_device, 0, mbuf, 1);

        if (actual_tx_len < 1) {
          rte_pktmbuf_free(mbuf[0]);
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
  configure(&config, argc, argv);
  initialize(&config);
  run(&config);
  // No tear down, as the previous function is not supposed to ever return.
}
