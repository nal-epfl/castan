#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <cmdline_parse_etheraddr.h>
#include <rte_lpm.h>
#include <rte_ip.h>

#define NF_INFO(text, ...) printf(text "\n", ##__VA_ARGS__); fflush(stdout)
#define NF_DEBUG(text, ...) printf(text "\n", ##__VA_ARGS__); fflush(stdout)

// Queue sizes for receiving/transmitting packets (set to their values from l3fwd sample)
static const uint16_t RX_QUEUE_SIZE = 128;
static const uint16_t TX_QUEUE_SIZE = 512;
// Memory pool #buffers and per-core cache size (set to their values from l3fwd sample)
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

static uintmax_t parse_int(const char* str, const char* name,
                           int base, char next) {
	char* temp;
	intmax_t result = strtoimax(str, &temp, base);

	// There's also a weird failure case with overflows, but let's not care
	if(temp == str || *temp != next) {
		rte_exit(EXIT_FAILURE, "Error while parsing '%s': %s\n", name, str);
	}

	return result;
}

#define PARSE_ERROR(format, ...)                  \
  config_print_usage();                           \
  rte_exit(EXIT_FAILURE, format, ##__VA_ARGS__);

void config_init(struct nf_config* config, int argc, char** argv) {
  unsigned nb_devices = rte_eth_dev_count();

  struct option long_options[] = {
    {"eth-dest", required_argument, NULL, 'd'},
    {"pfx2as", required_argument, NULL, 'r'},
    {NULL, 0, NULL, 0}
  };

  // Set the devices' own MACs
  for (uint8_t device = 0; device < nb_devices; device++) {
    rte_eth_macaddr_get(device, &(config->device_macs[device]));
  }

  int opt;
  while ((opt = getopt_long(argc, argv, "d:r", long_options, NULL)) != EOF) {
    unsigned device;
    switch (opt) {
    case 'd':
      device = parse_int(optarg, "eth-dest deice", 10, ',');
      if (nb_devices <= device) {
        PARSE_ERROR("eth-dest: nb_devices (%d) <= device %d\n",
                    nb_devices, device);
      }
      optarg += 2;
      if (cmdline_parse_etheraddr(NULL, optarg,
                                  &(config->endpoint_macs[device]),
                                  sizeof(int64_t)) < 0) {
        PARSE_ERROR("Invalid MAC address: %s\n", optarg);
      }
      break;
    case 'r':
      strncpy(config->route_table_fname, optarg,
              MAX_ROUTE_TABLE_FNAME_LEN - 1);
      config->route_table_fname[MAX_ROUTE_TABLE_FNAME_LEN - 1] = '\0';
      break;
    default:
      PARSE_ERROR("Invalid key: %c\n", opt);
    }
  }

  // Reset getopt
  optind = 1;
}

char* nf_mac_to_str(struct ether_addr* addr)
{
	// format is xx:xx:xx:xx:xx:xx\0
	uint16_t buffer_size = 6 * 2 + 5 + 1;
	char* buffer = (char*) calloc(buffer_size, sizeof(char));
	if (buffer == NULL) {
		rte_exit(EXIT_FAILURE, "Out of memory in nat_mac_to_str!");
	}

	ether_format_addr(buffer, buffer_size, addr);
	return buffer;
}

static void config_print(struct nf_config* config) {
  NF_INFO("\n running with the configuration: \n");

  uint32_t nb_devices = rte_eth_dev_count();
  for (uint32_t dev = 0; dev < nb_devices; dev++) {
    char* dev_mac_str = nf_mac_to_str(&(config->device_macs[dev]));
    char* end_mac_str = nf_mac_to_str(&(config->endpoint_macs[dev]));

    NF_INFO("Device %" PRIu32 " own-mac: %s, endpnt-mac: %s",
            dev, dev_mac_str, end_mac_str);

    free(dev_mac_str);
    free(end_mac_str);
  }
  NF_INFO("Routing table file: %s", config->route_table_fname);
}

static void configure(struct nf_config* config, int argc, char* argv[]) {
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

static int nf_init_device(uint32_t device, struct rte_mempool* mbuf_pool) {
  static struct rte_eth_conf device_conf;
	memset(&device_conf, 0, sizeof(struct rte_eth_conf));
  device_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
  device_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
  device_conf.rxmode.split_hdr_size = 0;
  device_conf.rxmode.header_split   = 0; /**< Header Split disabled */
  device_conf.rxmode.hw_ip_checksum = 1; /**< IP checksum offload enabled */
  device_conf.rxmode.hw_vlan_filter = 0; /**< VLAN filtering disabled */
  device_conf.rxmode.jumbo_frame    = 0; /**< Jumbo Frame Support disabled */
  device_conf.rxmode.hw_strip_crc   = 0; /**< CRC stripped by hardware */
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
                                  RX_QUEUE_SIZE,
                                  rte_eth_dev_socket_id(device),
                                  NULL, // config (NULL = default)
                                  mbuf_pool);
  if (retval < 0) {
    rte_exit(EXIT_FAILURE,
             "Cannot allocate RX queue for device %" PRIu32 ", err=%d",
             device, retval);
  }

  // Allocate and set up 1 TX queue per device
  retval = rte_eth_tx_queue_setup(device,
                                  0, // queue ID
                                  TX_QUEUE_SIZE,
                                  rte_eth_dev_socket_id(device),
                                  NULL); // config (NULL = default)
  if (retval < 0) {
    rte_exit(EXIT_FAILURE,
             "Cannot allocate TX queue for device %" PRIu32 ", err=%d",
             device, retval);
  }

  // Start the device
  retval = rte_eth_dev_start(device);
  if (retval < 0) {
    rte_exit(EXIT_FAILURE,
             "Cannot start device %" PRIu32 ", err=%d",
             device, retval);
  }

  // Enable RX in promiscuous mode for the device
  rte_eth_promiscuous_enable(device);

  return 0;
}

// TODO: automatic garbage management
char* nf_ipv4_to_str(uint32_t addr)
{
	// format is xxx.xxx.xxx.xxx\0
	uint16_t buffer_size = 4 * 3 + 3 + 1;
	char* buffer = (char*) calloc(buffer_size, sizeof(char));
	if (buffer == NULL) {
		rte_exit(EXIT_FAILURE, "Out of memory in nf_ipv4_to_str!");
	}

	snprintf(buffer, buffer_size, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
           addr        & 0xFF,
           (addr >>  8) & 0xFF,
           (addr >> 16) & 0xFF,
           (addr >> 24) & 0xFF
           );
	return buffer;
}

void init_lpm(const char fname[], struct rte_lpm** lpm_out) {
  uint32_t nb_devices = rte_eth_dev_count();
  struct rte_lpm_config config_lpm;
  config_lpm.max_rules = LPM_MAX_RULES;
  config_lpm.number_tbl8s = LPM_NUMBER_TBL8S;
  config_lpm.flags = 0;
  *lpm_out = rte_lpm_create("route_table", rte_socket_id(),
                            &config_lpm);
  if (*lpm_out == NULL) {
    rte_exit(EXIT_FAILURE,
             "Cannot allocate the LPM table on socket %d",
             rte_socket_id());
  }

  FILE *pfx2as_file = fopen(fname, "r");
  if (pfx2as_file == NULL) {
    rte_exit(EXIT_FAILURE,
             "Error opening pfx2as file: %s.\n",
             fname);
  }

  //rte_lpm_add(*lpm_out, 0, 0, nb_devices);;
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
      rte_exit(EXIT_FAILURE,
               "Error in ipaddr in pfx2as file %s:%lu\n",
               fname, count);
    }

    result = fscanf(pfx2as_file, "%hh" PRIu8, &depth);
    if (result != 1) {
      rte_exit(EXIT_FAILURE,
               "Error in prefix detpth in pfx2as file %s:%lu\n",
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
      NF_DEBUG("adding rule: %s/%" PRIu8 " -> %" PRIu32,
               nf_ipv4_to_str(ip), depth, if_out);
      result = rte_lpm_add(*lpm_out, rte_be_to_cpu_32(ip), depth, if_out);
      if (result < 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot add entry %lu to the LPM table.",
                 count);
      }
    }
  }
  fclose(pfx2as_file);
}

void initialize(struct nf_config* config, struct rte_lpm** lpm_out, struct rte_mempool** mbuf_pool_out) {
  uint32_t nb_devices = rte_eth_dev_count();
  struct rte_mempool* mbuf_pool =
    rte_pktmbuf_pool_create("MEMPOOL", // name
                            MEMPOOL_BUFFER_COUNT * nb_devices, // # elements
                            MEMPOOL_CACHE_SIZE,
                            0, // application private area size
                            RTE_MBUF_DEFAULT_BUF_SIZE, // data buffer size
                            rte_socket_id()); // socket ID
  if (mbuf_pool == NULL) {
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }
  *mbuf_pool_out = mbuf_pool;

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
                                       uint32_t portid,
                                       struct rte_lpm *lpm)
{
  uint32_t next_hop;
  int success = rte_lpm_lookup(lpm,
                               rte_be_to_cpu_32(ipv4_hdr->dst_addr),
                               &next_hop);
  NF_DEBUG("lookup success: %d", success);
  return success ? portid : next_hop;
}

uint32_t dispatch_packet(struct nf_config* config,
                         uint32_t device,
                         struct rte_lpm* lpm,
                         struct rte_mbuf* mbuf) {
	uint8_t nb_devices = rte_eth_dev_count();
  struct ether_hdr* ether_header = rte_pktmbuf_mtod(mbuf, struct ether_hdr*);
	if (!RTE_ETH_IS_IPV4_HDR(mbuf->packet_type) &&
      !(mbuf->packet_type == 0 &&
        ether_header->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))) {
		return device; // Non IPv4 packet, ignore
	}
  struct ipv4_hdr* ipv4_header =
    rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr*, sizeof(struct ether_hdr));

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

// Code to measure CPU cycles taken
typedef long long int ticks_t;

static inline void start(unsigned int *cycles_low, unsigned int *cycles_high) {
  asm volatile ("LFENCE\n\t"
                "RDTSC\n\t"
                "mov %%edx, %0\n\t"
                "mov %%eax, %1\n\t": "=r" (*cycles_high), "=r" (*cycles_low)::
                "%rax", "%rbx", "%rcx", "%rdx");
}

static inline ticks_t stop(unsigned int *start_cycles_low, unsigned int *start_cycles_high) {
  unsigned end_cycles_low, end_cycles_high;
  asm volatile("RDTSCP\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "LFENCE\n\t": "=r" (end_cycles_high), "=r" (end_cycles_low):: "%rax",
               "%rbx", "%rcx", "%rdx");
  ticks_t start_cycles = ((ticks_t) *start_cycles_high << 32) | *start_cycles_low;
  ticks_t end_cycles = ((ticks_t) end_cycles_high << 32) | end_cycles_low;
  return end_cycles - start_cycles;
}

uint16_t fuzzer_rx(struct rte_mempool *mbuf_pool, struct rte_mbuf **bufs) {
  const uint32_t PACKET_LEN = 128;
  uint8_t packet[PACKET_LEN];
  size_t read = fread(packet, sizeof packet[0], PACKET_LEN, stdin);
  if (read == 0) {
    return 0;
  }

  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
  if (mbuf == NULL) {
      rte_exit(EXIT_FAILURE, "Can't allocate mbuf?\n");
  }

  uint16_t buf_size = (uint16_t)(rte_pktmbuf_data_room_size(mbuf_pool) -
      RTE_PKTMBUF_HEADROOM);
  uint16_t effective_size = buf_size < read ? buf_size : read;
  rte_memcpy(rte_pktmbuf_mtod(mbuf, void *), packet, effective_size);
  mbuf->data_len = effective_size;
  mbuf->pkt_len = effective_size;
  bufs[0] = mbuf;
  return 1;
}

void run(struct nf_config* config, struct rte_lpm* lpm, struct rte_mempool* mbuf_pool) {

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

  // We've simplified this a bit: Ignore the interfaces. Read packets from
  // stdin, as long as they are available. Process each, without transmitting.
  // Then exit.
  while (1) {
    struct rte_mbuf* mbuf[1];
    //uint16_t actual_rx_len = rte_eth_rx_burst(0, 0, mbuf, 1);
    uint16_t actual_rx_len = fuzzer_rx(mbuf_pool, mbuf);

    if (actual_rx_len == 0) {
      // No more packet; exit
      rte_exit(EXIT_SUCCESS, "Bye...\n");
    }

    dispatch_packet(config, 0, lpm, mbuf[0]);
    rte_pktmbuf_free(mbuf[0]);
  }
}

int main(int argc, char* argv[]) {
  struct nf_config config;
  struct rte_lpm* lpm;
  struct rte_mempool* mbuf_pool;

  configure(&config, argc, argv);
  initialize(&config, &lpm, &mbuf_pool);
  run(&config, lpm, mbuf_pool);
  // No tear down, as the previous function is not supposed to ever return.
}
