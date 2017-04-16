#!/bin/bash

set -e

NF=$1
PCAP=$2
shift 2

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
. $DIR/config.sh

CLEAN_PCAP="$(echo $PCAP | sed -e 's#.*/\(.*\)\.pcap#\1#')"

cd $DIR/../../examples/$NF

echo "Building $NF ..."
sudo rm -rf build
make clean
make

sudo ./build/nf --vdev "eth_pcap0,rx_pcap=$PCAP,tx_pcap=/dev/null" --vdev 'eth_pcap1,rx_pcap=/home/lpedrosa/pcap/empty.pcap,tx_pcap=/dev/null' -- --eth-dest 0,90:e2:ba:55:12:25 --eth-dest 1,90:e2:ba:55:12:24 $@ 2>&1 > ~/results/$NF-$CLEAN_PCAP.log &
PID=$!

wait 2

kill -2 $PID
wait
