#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

# Master script to initialize VigNAT-related programs benchmarks.
# Can work with different implementations, including non-NATs,
# using different scenarios.
# Parameters:
# $1: the path to the dpdk-enabled middlebox source (must contain a Makefile)

MIDDLEBOX=$1

$DIR/run-dpdk.sh $MIDDLEBOX \
"--pfx2as $DIR/routing-table.pfx2as"
# "--nat-ip 192.168.0.1"
# "--vip 192.168.0.1 --dip 10.0.0.1 --dip 10.0.0.2"
# "--extip 192.168.0.1 --expire 120 --max-flows 65536 --wan 1"

sleep 10
