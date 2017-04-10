#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/config.sh

echo "[clean] Cleaning machines..."
ssh $TESTER_HOST "bash ~/scripts/clean/tester.sh"
$DIR/clean/middlebox.sh
