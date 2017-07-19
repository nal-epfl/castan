#!/bin/bash

set -e

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

NOW=$(date +"%d.%m.%Y_%H_%M")
CLEAN_APP_NAME=`echo "$APP" | tr '/' '_'`
LOG_FILE=~/"logs/bench-$CLEAN_APP_NAME-$NOW.log"

mkdir -p ~/logs

pushd ~/castan/examples/$APP >> /dev/null

echo "[bench] Building $APP ..."
sudo rm build -rf
make clean
make

echo "[bench] Running $APP ..."
sudo taskset -c 8 hugectl --no-preload --heap=1073741824 ./build/nf -- \
  --eth-dest 0,$TESTER_MAC_INTERNAL \
  --eth-dest 1,$TESTER_MAC_EXTERNAL \
  $ARGS > $LOG_FILE &

popd >> /dev/null
