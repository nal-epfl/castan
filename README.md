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

 * examples/ - NF code to be analyzed.
 * include/ - Header files.
 * lib/ - KLEE and CASTAN libraries.
 * tools/ - Main files for the final executables.

The core components of CASTAN are:

 * The CPU cache model (lib/CASTAN/ContentionSetCacheModel.cpp).
 * The directed symbolic execution heuristic (lib/CASTAN/CastanSearcher.cpp).
 * Havoc reconciliation (tools/castan/castan.cpp, within the KleeHandler::processTestCase function).

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
The model is built using standard documented cache parameters (include/castan/Internal/ContentionSetCacheModel.h), and learned contention sets which are loaded from a file.
The contention set file for the Intel(R) Xeon(R) CPU E5-2667v2 is included in this repo for convenience (examples/XeonE52667v2.dat.bz2), and can be used once decompressed with bunzip2.

Generating new models is done with the dpdk-probe-cache (examples/dpdk-probe-cache/), process-contention-sets (examples/cache-effects/process-contention-sets.cpp), and dpdk-check-cache (examples/dpdk-check-cache) tools.
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
