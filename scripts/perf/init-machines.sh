#!/bin/bash
# Initializes the machines on the test network.

. ./config.sh

ssh $TESTER_HOST "bash ~/scripts/init-machines/tester.sh"

. ./init-machines/middlebox.sh
