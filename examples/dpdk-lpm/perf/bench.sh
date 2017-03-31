#!/bin/bash
. ./config.sh

# Parameters:
# $1: The full path to the dpdk-enabled middlebox source file
# $2: The scenario, one of the following:
#     "thru-1p": Measure throughput -- find the rate at which the
#           middlebox starts loosing 1% of packets.
#     "latency": Measure the forwarding latency.

$MIDDLEBOX = $1
$SCENARIO = $2

if [ -z $MIDDLEBOX ]; then
    echo "[bench] No app specified" 1>&2
    exit 1
fi

if [ -z $SCENARIO ]; then
    echo "[bench] No scenario specified" 1>&2
    exit 2
fi

CLEAN_APP_NAME=`echo "$MIDDLEBOX" | tr '/' '_'`
RESULTS_FILE="bench-$CLEAN_APP_NAME-$SCENARIO.results"

if [ -f "$RESULTS_FILE" ]; then
    rm "$RESULTS_FILE"
fi

. ./setup-machines.sh

. ./clean.sh

. ./init-machines.sh

. ./start-middlebox.sh $MIDDLEBOX

. ./run.sh $MIDDLEBOX $SCENARIO $RESULT_FILE

. ./stop-middlebox.sh

. ./clean.sh

