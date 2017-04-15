#!/bin/bash

set -e

HISTOGRAM=$(mktemp)
CDF=$(mktemp)

cat $1 | sort -n | uniq -c > $HISTOGRAM

TOTAL=$(awk '{sum += $1} END {print sum;}' $HISTOGRAM)

cat $HISTOGRAM | awk "
  BEGIN {
    print \"0,0\";
  }

  {
    acc += \$1;
    print \$2 \",\" (acc / $TOTAL);
  }" > $CDF

gnuplot > $1.eps <<EOF
set ylabel 'CDF'
set xlabel 'Latency (ns)'
set grid
set term eps
set output '$1.eps'
plot '$CDF' with line lt -1 lw 2
EOF

rm $HISTOGRAM $CDF
