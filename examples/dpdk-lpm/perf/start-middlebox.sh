#!/bin/bash
. ./config.sh

# Master script to initialize VigNAT-related programs benchmarks.
# Can work with different implementations, including non-NATs,
# using different scenarios.
# Parameters:
# $1: the path to the dpdk-enabled middlebox source (must contain a Makefile)

MIDDLEBOX=$1

NOW=$(date +"%d.%m.%Y_%H_%M")
CLEAN_APP_NAME=`echo "$MIDDLEBOX" | tr '/' '_'`
LOG_FILE="logs/bench-$CLEAN_APP_NAME-$NOW.log"

mkdir -p logs
if [ -f "$LOG_FILE" ]; then
    rm "$LOG_FILE"
fi

(bash ./run-dpdk.sh $MIDDLEBOX "--pfx2as $MIDDLEBOX/perf/routing-table.pfx2as" \
  0<&- &>"$LOG_FILE" ) &

sleep 10

