#!/bin/bash

set -e

OUTPUT="$1"
shift 1

PLOT_LINES="plot"
while (("$#")); do
  CSV="$1"

  if ! (head -n 1 $CSV | grep ',' >/dev/null 2>/dev/null); then
    awk 'BEGIN {c = 0} {print c++ "," $0}' $CSV > $CSV.sequence.csv
    CSV="$CSV.sequence.csv"
  fi

  PLOT_LINES+=" '$CSV' using 1:2 title '$(echo $CSV | sed -e s/\.csv$// -e s/_/\\\\_/g)' with line lw 5,"

  shift 1
done

echo "Plotting CSV into $OUTPUT."

gnuplot <<EOF
  set grid
  set term epscairo
  set output '$OUTPUT'
  set datafile separator ","
#   set logscale y
#   set format y "10^{%L}"
  $PLOT_LINES
EOF
