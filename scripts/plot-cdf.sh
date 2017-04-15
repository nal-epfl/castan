#!/bin/bash

set -e



OUTPUT="$1"
shift

HISTOGRAM=$(mktemp)
declare -a CDFS

while (("$#")); do
  cat $1 | sort -n | uniq -c > $HISTOGRAM

  TOTAL=$(awk '{sum += $1} END {print sum;}' $HISTOGRAM)

  CDFS+=($(mktemp))
  cat $HISTOGRAM | awk "
    BEGIN {
      print \"0,0\";
    }

    {
      print \$2 \",\" (acc / $TOTAL);
      acc += \$1;
      print \$2 \",\" (acc / $TOTAL);
    }" > ${CDFS[-1]}

  shift
done

PLOT_LINES="plot"
for i in $(seq 0 $((${#CDFS[@]} - 1))); do
  PLOT_LINES+=" '${CDFS[$i]}' using 1:2 with line ls $i lw 5,"
done

gnuplot > $1.eps <<EOF
set ylabel 'CDF'
set xlabel 'Latency (ns)'
set grid
set key off
set term epscairo
set output '$OUTPUT'
set datafile separator ","
$PLOT_LINES
EOF

rm $HISTOGRAM ${CDFS[@]}
