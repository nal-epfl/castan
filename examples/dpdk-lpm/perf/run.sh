#!/bin/bash
. ./config.sh

MIDDLEBOX=$1
SCENARIO=$2
RESULTS_FILE=$3

if [ -z $MIDDLEBOX ]; then
    echo "[bench] No app specified" 1>&2
    exit 1
fi

if [ -z $SCENARIO ]; then
    echo "[bench] No scenario specified" 1>&2
    exit 2
fi

if [ -z $RESULTS_FILE ]; then
    echo "[bench] No result file specified" 1>&2
    exit 3
fi

if [ -f "$RESULTS_FILE" ]; then
    echo "[run] The result file $RESULTS_FILE exists! exiting" 1>&2
    exit 4
fi


case $SCENARIO in
    "thru-1p")
        LUA_SCRIPT="l3-load-find-1p.lua"
        echo "[bench] Benchmarking throughput..."
        ssh $TESTER_HOST "sudo ~/moon-gen/build/MoonGen ~/scripts/moongen/$LUA_SCRIPT -r 3000 -u 5 -t 20 1 0"
        scp $TESTER_HOST:mg-find-mg-1p.txt "./$RESULTS_FILE"
        ssh $TESTER_HOST "sudo rm mg-find-mg-1p.txt"
        ;;
    "latency")
        LUA_SCRIPT="l3-latency-light.lua"
        echo "[bench] Benchmarking throughput..."
        ssh $TESTER_HOST "sudo ~/moon-gen/build/MoonGen ~/scripts/moongen/$LUA_SCRIPT -r 100 -u 5 -t 20 1 0"
        scp $TESTER_HOST:mf-lat.txt "./$RESULTS_FILE"
        ssh $TESTER_HOST "sudo rm mf-lat.txt"
        ;;
    *)
        echo "[bench] Unknown scenario: $MIDDLEBOX" 1>&2
        exit 10
        ;;
esac
