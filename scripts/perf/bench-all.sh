#!/bin/bash

set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
. $DIR/config.sh

SCENARIO=$1

if [ -z $SCENARIO ]; then
    echo "[bench-all] No scenario specified" 1>&2
    exit 1
fi

# Initialize the machines, i.e. software+scripts
$DIR/setup-machines.sh
# Clean first, just in case
$DIR/clean.sh
$DIR/init-machines.sh

NOW=$(date +"%d.%m.%Y_%H_%M")

MIDDLEBOXES=("dpdk-lpm-dpdklpm" "dpdk-lpm-btrie" "dpdk-nat-stlmap")

mkdir -p $NOW

for MIDDLEBOX in ${MIDDLEBOXES[@]}; do
    echo "[bench-all] testing $MIDDLEBOX"
    while read -u 10 PCAP_FILE; do
        $DIR/start-middlebox.sh $MIDDLEBOX
        echo "[bench-all] running pcap file: $PCAP_FILE"
        CLEAN_APP_NAME=`echo "$MIDDLEBOX" | tr '/' '_'`
        CLEAN_PCAP_NAME=`echo "$PCAP_FILE" | tr '/' '_'`
        RESULTS_FILE="bench-$CLEAN_APP_NAME-$SCENARIO-$CLEAN_PCAP_NAME.results"
        $DIR/run.sh $MIDDLEBOX $SCENARIO $RESULTS_FILE $PCAP_FILE
        $DIR/stop-middlebox.sh
    done 10<$DIR/pcaplist/$MIDDLEBOX.txt
done

$DIR/clean.sh
