#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/../config.sh

echo "[clean] Unbinding middlebox interfaces from Linux..."
sudo ifconfig $MB_DEVICE_INTERNAL down
sudo ifconfig $MB_DEVICE_EXTERNAL down

echo "[clean] Unbinding middlebox interfaces from DPDK..."
sudo $RTE_SDK/tools/dpdk-devbind.py -b $KERN_NIC_DRIVER $MB_PCI_INTERNAL $MB_PCI_EXTERNAL || true

echo "[clean] Killing the NF on middlebox..."
sudo pkill -9 nf | true
