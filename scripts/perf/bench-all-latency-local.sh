#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

for NF in dpdk-lpm-btrie \
          dpdk-lpm-dpdklpm; do
  [ -f ~/pcap/castan/$NF-castan.pcap ] \
      && [ ! -f ~/results/$NF-$NF-castan.log ] \
      && $DIR/bench-latency-local.sh $NF ~/pcap/castan/$NF-castan.pcap \
             --pfx2as ~/castan/scripts/perf/routing-table.pfx2as

  [ -f ~/pcap/random/$NF-unirand.pcap ] \
      && [ ! -f ~/results/$NF-$NF-unirand.log ] \
      && $DIR/bench-latency-local.sh $NF ~/pcap/random/$NF-unirand.pcap \
             --pfx2as ~/castan/scripts/perf/routing-table.pfx2as

  for PCAP in ~/pcap/facebook/cluster_A_1.pcap \
              ~/pcap/facebook/cluster_B_1.pcap \
              ~/pcap/facebook/cluster_C_1.pcap \
              ~/pcap/imc10/univ1_pt1.pcap \
              ~/pcap/imc10/univ2_pt8.pcap \
              ~/pcap/random/imc10-2pt8-zipf.pcap \
              ~/pcap/random/unirand1000.pcap; do
    [ -f "$PCAP" ] \
        && [ ! -f ~/results/$NF-$(echo $PCAP | sed -e 's#.*/\(.*\)\.pcap#\1#').log ] \
        && $DIR/bench-latency-local.sh $NF $PCAP \
               --pfx2as ~/castan/scripts/perf/routing-table.pfx2as
  done
done

for NF in dpdk-nat-basichash \
          dpdk-nat-dpdkhash \
          dpdk-nat-ruby \
          dpdk-nat-stlmap \
          dpdk-nat-stlumap; do
  [ -f ~/pcap/castan/$NF-castan.pcap ] \
      && [ ! -f ~/results/$NF-$NF-castan.log ] \
      && $DIR/bench-latency-local.sh $NF ~/pcap/castan/$NF-castan.pcap \
             --nat-ip 192.168.0.1

  [ -f ~/pcap/random/$NF-unirand.pcap ] \
      && [ ! -f ~/results/$NF-$NF-unirand.log ] \
      && $DIR/bench-latency-local.sh $NF ~/pcap/random/$NF-unirand.pcap \
             --nat-ip 192.168.0.1

  for PCAP in ~/pcap/facebook/cluster_A_1.pcap \
              ~/pcap/facebook/cluster_B_1.pcap \
              ~/pcap/facebook/cluster_C_1.pcap \
              ~/pcap/imc10/univ1_pt1.pcap \
              ~/pcap/imc10/univ2_pt8.pcap \
              ~/pcap/random/imc10-2pt8-zipf.pcap \
              ~/pcap/random/unirand1000.pcap; do
    [ -f "$PCAP" ] \
        && [ ! -f ~/results/$NF-$(echo $PCAP | sed -e 's#.*/\(.*\)\.pcap#\1#').log ] \
        && $DIR/bench-latency-local.sh $NF $PCAP \
               --nat-ip 192.168.0.1
  done
done

for PCAP in ~/pcap/facebook/cluster_A_1.pcap \
            ~/pcap/facebook/cluster_B_1.pcap \
            ~/pcap/facebook/cluster_C_1.pcap \
            ~/pcap/imc10/univ1_pt1.pcap \
            ~/pcap/imc10/univ2_pt8.pcap \
            ~/pcap/random/imc10-2pt8-zipf.pcap \
            ~/pcap/random/unirand1000.pcap; do
  [ -f "$PCAP" ] \
      && [ ! -f ~/results/dpdk-nat-cc-$(echo $PCAP | sed -e 's#.*/\(.*\)\.pcap#\1#').log ] \
      && $DIR/bench-latency-local.sh dpdk-nat-cc $PCAP \
             --extip 192.168.0.1 --expire 120 --max-flows 65536 --wan 1
done

parallel \
    "awk '/^Latency:/ {print \$2}' {} > \$(echo {} | sed -e 's/.log/.csv/')" \
    ::: ~/results/*.log
