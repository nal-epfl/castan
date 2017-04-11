#!/bin/bash

set -e

echo "[init] Setting up middlebox..."

sudo apt-get -qq update

sudo apt-get install -yqq \
    tcpdump git libpcap-dev \
    linux-headers-3.13.0-93 \
    libglib2.0-dev daemon iperf3 netperf tmux

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

$DIR/install-dpdk.sh
