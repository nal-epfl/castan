#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

echo "[init] Cloning scripts..."
rsync -q -a -r --exclude '*.log' --exclude '*.results' $DIR/ $TESTER_HOST:scripts

echo "[init] Setting up all machines..."
ssh $TESTER_HOST 'bash ~/scripts/setup-machines/tester.sh'
$DIR/setup-machines/middlebox.sh
