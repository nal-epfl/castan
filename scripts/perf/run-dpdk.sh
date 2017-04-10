#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

# Runs a DPDK app for the specified scenario

# Parameters:
# $1: Folder of the app; the app must be named "nat"
#     and take the usual arguments e.g. "--extip"
# $2..: Additional arguments for the app

APP=$1
shift
ARGS=$@

pushd $APP >> /dev/null

echo "[bench] Building $APP ..."
sudo rm build -rf
make clean
make

echo "[bench] Running $APP ..."
sudo ./build/nf -- \
  --eth-dest 0,$TESTER_MAC_INTERNAL \
  --eth-dest 1,$TESTER_MAC_EXTERNAL \
  $ARGS

popd >> /dev/null

