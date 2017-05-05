#!/bin/bash

set -e

OUTPUT="$1"
shift 1

PLOT_LINES="plot"
while (("$#")); do
  CSV="$1"

  PLOT_LINES+=" '$CSV' using 1:2 title '$(echo $CSV | sed s/\.csv//)' with line lw 5,"

  shift 1
done

echo "Plotting CSV into $OUTPUT."

gnuplot <<EOF
  set grid
  set term epscairo
  set output '$OUTPUT'
  set datafile separator ","
  $PLOT_LINES
EOF
