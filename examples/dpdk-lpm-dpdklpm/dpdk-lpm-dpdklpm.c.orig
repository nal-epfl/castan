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
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include "x86intrin.h"

#ifdef arch_measure
#include "../../../papi-5.5.1/src/papi.h"
#endif

#ifdef uarch_measure
#include "../../../papi-5.5.1/src/papi.h"
#endif

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

void init_lpm(const char fname[], struct rte_lpm **lpm_out) {
  uint32_t nb_devices = rte_eth_dev_count();
  struct rte_lpm_config config_lpm;
  config_lpm.max_rules = LPM_MAX_RULES;
  config_lpm.number_tbl8s = LPM_NUMBER_TBL8S;
  config_lpm.flags = 0;
  *lpm_out = rte_lpm_create("route_table", rte_socket_id(), &config_lpm);
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
      result = rte_lpm_add(*lpm_out, rte_be_to_cpu_32(ip), depth, if_out);
      if (result < 0) {
        rte_exit(EXIT_FAILURE, "Cannot add entry %lu to the LPM table.", count);
      }
    }
  }
  fclose(pfx2as_file);
}

void initialize(struct nf_config *config, struct rte_lpm **lpm_out) {
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
                                        uint32_t portid, struct rte_lpm *lpm) {
  uint32_t next_hop;
  int success =
      rte_lpm_lookup(lpm, rte_be_to_cpu_32(ipv4_hdr->dst_addr), &next_hop);
  NF_DEBUG("lookup success: %d", success);
  return success ? portid : next_hop;
}

uint32_t dispatch_packet(struct nf_config *config, uint32_t device,
                         struct rte_lpm *lpm, struct rte_mbuf *mbuf) {
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

void run(struct nf_config *config, struct rte_lpm *lpm) {

  uint8_t nb_devices = rte_eth_dev_count();

 #ifdef arch_measure
  int retval,num_hwcntrs = 0, EventSet = PAPI_NULL, native;
  typeof (PAPI_OK) papi_ok_proxy ;

   retval = PAPI_library_init(PAPI_VER_CURRENT);
   assert(retval == PAPI_VER_CURRENT);
   papi_ok_proxy =  PAPI_create_eventset(&EventSet);
 //  assert(PAPI_create_eventset(&EventSet) == PAPI_OK);

   retval  = PAPI_event_name_to_code("CPU_CLK_UNHALTED:THREAD_P",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
//   assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("UOPS_RETIRED:TOTAL_CYCLES",&native);
    assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
//   assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("UOPS_RETIRED:STALL_CYCLES",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
//   assert(PAPI_add_event(EventSet, native) == PAPI_OK);

 retval  = PAPI_event_name_to_code("CYCLE_ACTIVITY:CYCLES_NO_EXECUTE",&native);
    assert(retval == PAPI_OK);
     papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("ICACHE:IFETCH_STALL",&native);
    assert(retval == PAPI_OK);
     papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("CYCLE_ACTIVITY:STALLS_LDM_PENDING",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
 //  assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("RESOURCE_STALLS:ANY",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);

 //  assert(PAPI_add_event(EventSet, native) == PAPI_OK);


/*   retval  = PAPI_event_name_to_code("UOPS_RETIRED:ALL",&native);
   assert(retval == PAPI_OK);
   assert(PAPI_add_event(EventSet, native) == PAPI_OK);
*/
   long long native_values[10] ;
/*   int Events[] = {PAPI_TOT_CYC};
assert((num_hwcntrs = PAPI_num_counters()) > PAPI_OK);
assert(sizeof(Events) / sizeof(Events[0]) <= num_hwcntrs);
assert((retval = PAPI_start_counters(Events, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);
long long values[sizeof(Events) / sizeof(Events[0])];
assert((retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);
long long ref_cycles = values[0];
*/
#endif

#ifdef uarch_measure
  int retval,num_hwcntrs = 0, EventSet = PAPI_NULL, native;
  typeof (PAPI_OK) papi_ok_proxy ;
/*
   retval = PAPI_library_init(PAPI_VER_CURRENT);
   assert(retval == PAPI_VER_CURRENT);
   papi_ok_proxy =  PAPI_create_eventset(&EventSet);
  //  assert(PAPI_create_eventset(&EventSet) == PAPI_OK);

   retval  = PAPI_event_name_to_code("CPU_CLK_UNHALTED:THREAD_P",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
 //  assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("INSTRUCTIONS_RETIRED",&native);
    assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
 //  assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("ICACHE:MISSES",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
 //  assert(PAPI_add_event(EventSet, native) == PAPI_OK);

  retval  = PAPI_event_name_to_code("MEM_UOPS_RETIRED:ALL_LOADS",&native);
     assert(retval == PAPI_OK);
     papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:L1_HIT",&native);
    assert(retval == PAPI_OK);
     papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:HIT_LFB",&native);
   assert(retval == PAPI_OK);
   papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:L2_HIT",&native);
     assert(retval == PAPI_OK);
    papi_ok_proxy = PAPI_add_event(EventSet, native);
  // assert(PAPI_add_event(EventSet, native) == PAPI_OK);
*/
/*   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:L3_HIT",&native);
    assert(retval == PAPI_OK);
  //   papi_ok_proxy = PAPI_add_event(EventSet, native);
   assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:L3_HIT",&native);
   assert(retval == PAPI_OK);
  // papi_ok_proxy = PAPI_add_event(EventSet, native);
   assert(PAPI_add_event(EventSet, native) == PAPI_OK);

   retval  = PAPI_event_name_to_code("MEM_LOAD_UOPS_RETIRED:L3_MISS",&native);
   assert(retval == PAPI_OK);
  // papi_ok_proxy = PAPI_add_event(EventSet, native);
   assert(PAPI_add_event(EventSet, native) == PAPI_OK);
*/
   long long native_values[10] ;
   int Events[] = {PAPI_TOT_CYC,PAPI_L1_DCM,PAPI_L2_DCM,PAPI_L3_TCM,PAPI_LD_INS,PAPI_SR_INS,PAPI_TOT_INS};
//assert((num_hwcntrs = PAPI_num_counters()) > PAPI_OK);
num_hwcntrs = PAPI_num_counters();
//assert(sizeof(Events) / sizeof(Events[0]) <= num_hwcntrs);
//assert((retval = PAPI_start_counters(Events, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);
retval = PAPI_start_counters(Events, sizeof(Events) / sizeof(Events[0]));
long long values[sizeof(Events) / sizeof(Events[0])];
//assert((retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]))) == PAPI_OK);
retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]));
long long ref_cycles = values[0];
long long l1_dcm = values[1];
long long l2_dcm = values[2];
long long l3_tcm = values[3];
long long loads = values[4];
long long stores = values[5];
long long tot_ins = values[6];

#endif

  while (1) {
    for (uint32_t device = 0; device < nb_devices; ++device) {
      struct rte_mbuf *mbuf[1];
      uint16_t actual_rx_len = rte_eth_rx_burst(device, 0, mbuf, 1);

      if (actual_rx_len != 0) {
#ifdef arch_measure 
   //assert(PAPI_start(EventSet)==PAPI_OK);
  papi_ok_proxy = PAPI_start(EventSet) ;

//retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]));
//assert((retval  == PAPI_OK));
#endif

#ifdef uarch_measure 
   //assert(PAPI_start(EventSet)==PAPI_OK);
  //papi_ok_proxy = PAPI_start(EventSet) ;
retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]));
//assert((retval  == PAPI_OK));
#endif

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
#endif


#ifdef arch_measure 
//  assert(PAPI_stop(EventSet,native_values)==PAPI_OK);
  papi_ok_proxy = PAPI_stop(EventSet,native_values);
 // retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]));
#endif

#ifdef uarch_measure 
 // assert(PAPI_stop(EventSet,native_values)==PAPI_OK);
 // papi_ok_proxy = PAPI_stop(EventSet,native_values);
  retval = PAPI_read_counters(values, sizeof(Events) / sizeof(Events[0]));
  //assert(retval == PAPI_OK);
 ref_cycles = values[0];
 l1_dcm = values[1];
 l2_dcm = values[2];
 l3_tcm = values[3];
 loads = values[4];
 stores = values[5];
 tot_ins = values[6];
#endif

#ifdef LATENCY
        NF_INFO("Latency: %ld ns.",
                (new_timestamp.tv_sec - timestamp.tv_sec) * 1000000000 +
                    (new_timestamp.tv_nsec - timestamp.tv_nsec));
#endif

#ifdef arch_measure 
//  NF_INFO("Total reference cycles using metric 1  %lld ",native_values[0]);
    NF_INFO("Total_cycles  %lld ",native_values[1]);
    NF_INFO("Num_cycles_stalled %lld",native_values[2]);
//  NF_INFO("Percentage_stalled %lld ", (100*native_values[2])/native_values[1]);
    NF_INFO("Fetch_stalls_cycles %lld",native_values[3]);
    NF_INFO("Mem_stalls_cycles %lld",native_values[4]);
    NF_INFO("Resource_stalls_cycles %lld",native_values[5]);
//  NF_INFO("Total cycles stalled due to LLC+Mem %lld",native_values[6]);
//  NF_INFO("Stalled_fraction_contributions %lld %lld %lld %lld ", (100*native_values[3])/native_values[2], (100*native_values[4])/native_values[2], (100*native_values[5])/native_values[2], (100*native_values[6])/native_values[2] );

// NF_INFO("Total micro-ops retired  %lld",native_values[4]);
#endif

#ifdef uarch_measure
/* 
    NF_INFO("Total_ref_cycles  %lld ",native_values[0]);
    NF_INFO("Instructions_retired  %lld ",native_values[1]);
    NF_INFO("ICache_misses %lld",native_values[2]);
    NF_INFO("Load_uops_retired %lld",native_values[3]);
    NF_INFO("L1D_hits %lld",native_values[4]);
    NF_INFO("Fill_Buffer_hits %lld",native_values[5]);
    NF_INFO("L2D_hits  %lld",native_values[6]);
 //  NF_INFO("L3_hits  %lld",native_values[7]);
//  NF_INFO("Total cycles stalled due to LLC+Mem %lld",native_values[6]);
//  NF_INFO("Stalled_fraction_contributions %lld %lld %lld %lld ", (100*native_values[3])/native_values[2], (100*native_values[4])/native_values[2], (100*native_values[5])/native_values[2], (100*native_values[6])/native_values[2] );
*/
// NF_INFO("Total micro-ops retired  %lld",native_values[4]);

//NF_INFO("Reference cycles %lld ",ref_cycles);
 NF_INFO("Ref_clock_cycles %llu",ref_cycles);
 NF_INFO("L1D_misses %llu",l1_dcm);
 NF_INFO("L2D_misses %llu", l2_dcm);
 NF_INFO("Total_L3_misses %llu", l3_tcm);
 NF_INFO("Total_memory_instructions_retired %llu", loads+stores);
 NF_INFO("Total_instructions_retired %llu", tot_ins);
#endif

      }
    }
  }
}

int main(int argc, char *argv[]) {
  struct nf_config config;
  struct rte_lpm *lpm;
  configure(&config, argc, argv);
  initialize(&config, &lpm);
  run(&config, lpm);
  // No tear down, as the previous function is not supposed to ever return.
}
