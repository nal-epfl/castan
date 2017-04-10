#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

# Parameters:
# $1: The full path to the dpdk-enabled middlebox source file
# $2: The scenario, one of the following:
#     "thru-1p": Measure throughput -- find the rate at which the
#           middlebox starts loosing 1% of packets.
#     "latency": Measure the forwarding latency.
# $3: PCAP file containing the sample trace to run looped

MIDDLEBOX=$1
SCENARIO=$2
PCAP_FILE=$3

if [ -z $MIDDLEBOX ]; then
    echo "[bench] No app specified" 1>&2
    exit 1
fi

if [ -z $SCENARIO ]; then
    echo "[bench] No scenario specified" 1>&2
    exit 2
fi

if [ -z $PCAP_FILE ]; then
    echo "[bench] No pcap file specified, using default: $DEFAULT_PCAP"
    PCAP_FILE=$DEFAULT_PCAP
fi

CLEAN_APP_NAME=`echo "$MIDDLEBOX" | tr '/' '_'`
RESULTS_FILE="bench-$CLEAN_APP_NAME-$SCENARIO.results"

if [ -f "$RESULTS_FILE" ]; then
    rm "$RESULTS_FILE"
fi

$DIR/setup-machines.sh
$DIR/clean.sh
$DIR/init-machines.sh
$DIR/start-middlebox.sh $MIDDLEBOX
$DIR/run.sh $MIDDLEBOX $SCENARIO $RESULTS_FILE $PCAP_FILE
$DIR/stop-middlebox.sh
$DIR/clean.sh

