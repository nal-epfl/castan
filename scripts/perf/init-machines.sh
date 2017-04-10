#!/bin/bash
# Initializes the machines on the test network.

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

ssh $TESTER_HOST "bash ~/scripts/init-machines/tester.sh"

. $DIR/init-machines/middlebox.sh
