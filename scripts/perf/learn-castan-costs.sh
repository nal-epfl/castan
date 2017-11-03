#!/bin/bash

set -e

function get_data {
  CASTAN_CACHE="$1"
  ACTUAL_CSV="$2"

  awk '
    /Instructions:/ { instructions = $2; }
    /L1 Hits:/ { l1 = $3; }
    /L2 Hits:/ { l2 = $3; }
    /L3 Hits:/ { l3 = $3; }
    /DRAM Accesses:/ { dram = $3; }

    /Estimated Execution Time:/ {
      latency = $4;

      print instructions "," l1 "," l2 "," l3 "," dram "," latency;

      instructions = 0;
      l1 = 0;
      l2 = 0;
      l3 = 0;
      dram = 0;
      latency = 0;
    }' $CASTAN_CACHE \
    | paste -d , - $ACTUAL_CSV
}

DATA_CSV=$(mktemp)

# get_data castan/dpdk-lpm-dpdklpm-castan.cache \
#          castan/dpdk-lpm-dpdklpm-castan-locallat.csv \
#          >> $DATA_CSV
# get_data castan/dpdk-lpm-btrie-castan.cache \
#          castan/dpdk-lpm-btrie-castan-locallat.csv \
#          >> $DATA_CSV
# get_data castan/dpdk-nat-stlmap-castan.cache \
#          castan/dpdk-nat-stlmap-castan-locallat.csv \
#          >> $DATA_CSV
get_data klee-last/test000001.cache \
         actual.csv \
         >> $DATA_CSV

octave -q <<EOF
  pkg load optim;

  data = csvread("$DATA_CSV");
  x = data(:,1:5);
  old_prediction = data(:,6);
  y = data(:,7);

  F = [ones(rows(x),1), x];

  p = LinearRegression(F, y)

  new_prediction = F * p;

  range = 1000;#max([new_prediction; y; old_prediction]);
  plot([0, range], [0, range], "-",
       new_prediction, y, "+;Adjusted;",
       old_prediction, y, "o;Original;");
  axis([0, range, 0, range]);
  xlabel("Prediction");
  ylabel("Reality");
  print -deps -color lr.eps
EOF

rm $DATA_CSV
