# CASTAN: Cycle Approximating Symbolic Timing Analysis for Network Function

## Overview

Software network functions promise to simplify the deployment of network services and reduce network operation cost.
However, they face the challenge of unpredictable performance.
Given this performance variability, it is imperative that during deployment, network operators consider the performance of the NF not only for typical but also adversarial workloads.
We contribute a tool that helps solve this challenge: it takes as input the LLVM code of a network function and outputs packet sequences that trigger slow execution paths.
Under the covers, it combines directed symbolic execution with a sophisticated cache model to look for execution paths that incur many CPU cycles and involve adversarial memory-access patterns.
We used our tool on 11 network functions that implement a variety of data structures and discovered workloads that can in some cases triple latency and cut throughput by 19% relative to typical testing workloads.


## Source Code

CASTAN was developed as a fork of [KLEE](https://klee.github.io/) and so follows a similar code structure.
At a high level, code is organized as follows:

 * [examples/](examples/) - NF code to be analyzed.
 * [include/](include/) - Header files.
 * [lib/](lib/) - KLEE and CASTAN libraries.
 * [tools/](tools/) - Main files for the final executables.

The core components of CASTAN are:

 * The CPU cache model ([ContentionSetCacheModel.cpp](lib/CASTAN/ContentionSetCacheModel.cpp)).
 * The directed symbolic execution heuristic ([CastanSearcher.cpp](lib/CASTAN/CastanSearcher.cpp)).
 * Havoc reconciliation ([castan.cpp](tools/castan/castan.cpp), within the KleeHandler::processTestCase function).

Additionally, several NFs were implemented and analyzed (in the examples/ directory):

 * dpdk-lb-basichash: Load Balancer implemented with a hash table.
 * dpdk-lb-hashring: Load Balancer implemented with a hash ring.
 * dpdk-lb-stlmap: Load Balancer implemented with a red-black tree.
 * dpdk-lb-tree: Load Balancer implemented with an unbalanced tree.
 * dpdk-lpm-btrie: Longest Prefix Match implemented with a patricia trie.
 * dpdk-lpm-da: Longest Prefix Match implemented with a lookup table.
 * dpdk-lpm-dpdklpm: Longest Prefix Match implemented with a hierarchical lookup table.
 * dpdk-nat-basichash: Network Address Translator implemented with a hash table.
 * dpdk-nat-hashring: Network Address Translator implemented with a hash ring.
 * dpdk-nat-stlmap: Network Address Translator implemented with a red-black tree.
 * dpdk-nat-tree: Network Address Translator implemented with an unbalanced tree.
 * dpdk-nop: NOP network function.


## Building CASTAN

CASTAN follows the same build procedure as KLEE.
It depends on LLVM and CLang 3.4, STP, MiniSAT, and KLEE uClibC (branch: klee_0_9_29).
We have tested it on Ubuntu 16.04 LTS.
We build CASTAN with the following commands (adapt as needed):

    $ CXXFLAGS="-std=c++11" \
      ./configure --with-llvm=../llvm-3.4 \
                  --with-stp=../stp \
                  --with-uclibc=../klee-uclibc \
                  --enable-posix-runtime
    $ make ENABLE_OPTIMIZED=1

CASTAN analyzes network functions built on the DPDK framework.
Although CASTAN does not require any changes to DPDK itself to work (other than what is done in castan-dpdk.h), if the NF uses any DPDK libraries you may need to compile parts of DPDK into LLVM bit-code for analysis.
We have prepared a fork of the DPDK repository with scripts to handle such scenarios:
https://github.com/nal-epfl/castan-dpdk/


## Building the Cache Model

CASTAN uses a cache model to predict the performance of memory accesses.
The model is built using standard documented cache parameters ([ContentionSetCacheModel.h](include/castan/Internal/ContentionSetCacheModel.h), and learned contention sets which are loaded from a file.
The [contention set file for the Intel(R) Xeon(R) CPU E5-2667v2](examples/XeonE52667v2.dat.bz2) is included in this repo for convenience, and can be used once decompressed with bunzip2.

Generating new models is done with the [dpdk-probe-cache](examples/dpdk-probe-cache/), [process-contention-sets](examples/cache-effects/process-contention-sets.cpp), and [dpdk-check-cache](examples/dpdk-check-cache) tools.
dpdk-probe-cache generates a contention set file based on a single probe within a single 1GB huge page.
process-contention-sets processes files from multiple probes to find the contention sets that hold across multiple pages.
Finally, dpdk-check-cache validates the model within a single page and optionally filters out contention sets that no longer hold.


## Using CASTAN

To analyze an NF, it must first be built into LLVM bit-code.
The NFs implemented in examples/ already do this automatically when built with make.

CASTAN uses the following argument syntax:

    $ castan --max-loops=<n> \
             [--worst-case-sym-indices] \
             [--rainbow-table <rainbow-table-file>] \
             [--output-unreconciled] \
             [-max-memory=<n>] \
             <NF-bit-code-file>

Where the arguments mean:

 * --max-loops=<n>: The number of packets to generate.
 * --worst-case-sym-indices: Compute adversarial values for symbolic pointers.
 * --rainbow-table <rainbow-table-file>: Specify a rainbow table to use during havoc reconciliation.
 * --output-unreconciled: Enable outputting packets that have unreconciled havocs.
 * -max-memory: CASTAN may need a fair bit of memory to process some NFs and KLEE default to a 2GB cap which in some cases is not enough. This option can increase the cap by specifying a larger value in MB.
 * \<NF-bit-code-file\>: Specify the NF's LLVM bit-code.

CASTAN creates output files in the klee-last folder:

 * test*.ktest: Concretized adversarial inputs.
 * test*.cache: Report with predicted performance metrics.

KTEST files can be converted into PCAP files with the ktest2pcap tool:

    $ ktest2pcap <input-ktest-file> <output-pcap-file>

Many of the NFs in the examples directory have an additional make target that automates these steps:

    make castan

This generates nf.pcap with the adversarial workload.


## Measuring the Resulting Performance

The [CASTAN paper](https://dl.acm.org/citation.cfm?id=3230573) evaluates CASTAN by comparing the performance of several of the NFs in the examples directory under varying workloads, including the CASTAN generated one.
We automated the performance measurements using the scripts in the [scripts/perf/](scripts/perf/) directory.
The test-bed configuration is loaded from [config.sh](scripts/perf/config.sh).
We run the following command from the DUT to perform a single run for a single set of <NF, workload, metric>:

    bench.sh <NF> <thru-1p|latency> <workload>

Where the arguments are:
 * \<NF\>: the name of the NF to run, i.e. the subdirectory in examples where it resides.
 * <thru-1p|latency>: *thru-1p* performs a throughput experiment where packets are sent at varying throughputs to find the maximum point at which only 1% of packets are dropped. *latency* performs a latency experiment, where packets are sent one at a time and the latency is measured using hardware timestamps on the TG.
 * \<workload\>: The name of the pcap file to replay during the experiment.

Several workloads were used in the performance evaluation of the paper.
We include them in the [pcaps/](pcaps/) folder for convenience and reproducibility.
These include generic workloads used across all NFs and NF specific workloads.
The generic workloads also have special variants for load-balancer NFs that set the destination IP to the VIP, as described in the paper.


### Generic Workloads:

 * [1packet.pcap](pcaps/1packet.pcap) & [lb-1packet.pcap](pcaps/lb-1packet.pcap): A single packet.
 * [unirand.pcap](pcaps/unirand.pcap) & [lb-unirand.pcap](pcaps/lb-unirand.pcap): Packets following a uniform random distribution, like traditional adversarial traffic.
 * [zipf.pcap](pcaps/zipf.pcap) & [lb-zipf.pcap](pcaps/lb-zipf.pcap): Packets forming a Zipfian distribution, like typical Internet traffic.


### NF Specific Workloads:

| NF | CASTAN Workload | Manual Workload | Uniform Random with the same number of flows as CASTAN |
| --- | --- | --- | --- |
| NAT / Hash Table | [dpdk-nat-basichash-castan.pcap](pcaps/dpdk-nat-basichash-castan.pcap) | -- | [dpdk-nat-basichash-unirand-castan.pcap](pcaps/dpdk-nat-basichash-unirand-castan.pcap) |
| NAT / Hash Ring | [dpdk-nat-hashring-castan.pcap](pcaps/dpdk-nat-hashring-castan.pcap) | -- | [dpdk-nat-hashring-unirand-castan.pcap](pcaps/dpdk-nat-hashring-unirand-castan.pcap) |
| NAT / Red-Black Tree | [dpdk-nat-stlmap-castan.pcap](pcaps/dpdk-nat-stlmap-castan.pcap) | -- | [dpdk-nat-stlmap-unirand-castan.pcap](pcaps/dpdk-nat-stlmap-unirand-castan.pcap) |
| NAT / Unbalanced Tree | [dpdk-nat-tree-castan.pcap](pcaps/dpdk-nat-tree-castan.pcap) | [dpdk-nat-tree-manual.pcap](pcaps/dpdk-nat-tree-manual.pcap) | [dpdk-nat-tree-unirand-castan.pcap](pcaps/dpdk-nat-tree-unirand-castan.pcap) |
| LB  / Hash Table | [dpdk-lb-basichash-castan.pcap](pcaps/dpdk-lb-basichash-castan.pcap) | -- | [dpdk-lb-basichash-unirand-castan.pcap](pcaps/dpdk-lb-basichash-unirand-castan.pcap) |
| LB / Hash Ring | [dpdk-lb-hashring-castan.pcap](pcaps/dpdk-lb-hashring-castan.pcap) | -- | [dpdk-lb-hashring-unirand-castan.pcap](pcaps/dpdk-lb-hashring-unirand-castan.pcap) |
| LB / Red-Black Tree | [dpdk-lb-stlmap-castan.pcap](pcaps/dpdk-lb-stlmap-castan.pcap) | -- | [dpdk-lb-stlmap-unirand-castan.pcap](pcaps/dpdk-lb-stlmap-unirand-castan.pcap) |
| LB / Unbalanced Tree | [dpdk-lb-tree-castan.pcap](pcaps/dpdk-lb-tree-castan.pcap) | [dpdk-lb-tree-manual.pcap](pcaps/dpdk-lb-tree-manual.pcap) | [dpdk-lb-tree-unirand-castan.pcap](pcaps/dpdk-lb-tree-unirand-castan.pcap) |
| LPM / PATRICIA Trie | [dpdk-lpm-btrie-castan.pcap](pcaps/dpdk-lpm-btrie-castan.pcap) | [dpdk-lpm-btrie-manual.pcap](pcaps/dpdk-lpm-btrie-manual.pcap) | [dpdk-lpm-btrie-unirand-castan.pcap](pcaps/dpdk-lpm-btrie-unirand-castan.pcap) |
| LPM / 1-Stage Lookup | [dpdk-lpm-da-castan.pcap](pcaps/dpdk-lpm-da-castan.pcap) | -- | [dpdk-lpm-da-unirand-castan.pcap](pcaps/dpdk-lpm-da-unirand-castan.pcap) |
| LPM / 2-Stage Lookup (DPDK) | [dpdk-lpm-dpdklpm-castan.pcap](pcaps/dpdk-lpm-dpdklpm-castan.pcap) | -- | [dpdk-lpm-dpdklpm-unirand-castan.pcap](pcaps/dpdk-lpm-dpdklpm-unirand-castan.pcap) |
