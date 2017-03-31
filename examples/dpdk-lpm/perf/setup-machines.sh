#!/bin/bash
. ./config.sh

echo "[init] Cloning scripts..."
rsync -q -t -r --exclude '*.log' --exclude '*.results' ./ $TESTER_HOST:scripts

echo "[init] Initializing all machines..."
ssh $TESTER_HOST 'bash ~/scripts/setup-machines/tester.sh'
. ./setup-machines/middlebox.sh
