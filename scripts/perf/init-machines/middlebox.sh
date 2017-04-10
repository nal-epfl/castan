#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/../config.sh

echo "[init] Initializing DPDK on middlebox..."
. $DIR/../util/dpdk-functions.sh
set_numa_pages
load_igb_uio_module

echo "[init] Binding middlebox interfaces to DPDK..."
bind_nics_to_igb_uio $MB_PCI_INTERNAL
bind_nics_to_igb_uio $MB_PCI_EXTERNAL
