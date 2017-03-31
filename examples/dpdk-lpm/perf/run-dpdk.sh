#!/bin/bash
. ./config.sh

# Runs a DPDK app for the specified scenario

# Parameters:
# $1: Folder of the app; the app must be named "nat"
#     and take the usual arguments e.g. "--extip"
# $2..: Additional arguments for the app

APP=$1
shift
ARGS=$@

pushd $1 >> /dev/null

echo "[bench] Building $1..."
sudo rm build -rf
make clean
make

echo "[bench] Running $1..."
if [ $1 = "loopback" ]; then
    sudo ./build/nf -- \
         --eth-dest 0,$TESTER_MAC_INTERNAL \
         --eth-dest 1,$TESTER_MAC_EXTERNAL \
         $ARGS

popd >> /dev/null
