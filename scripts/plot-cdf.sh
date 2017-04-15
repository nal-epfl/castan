#!/bin/bash

set -e

OUTPUT="$1"
RANGE="$2"
shift 2

HISTOGRAM=$(mktemp)

PLOT_LINES="plot"
while (("$#")); do
  CSV="$1"
  TITLE="$2"
  CDF="$CSV.cdf"

  if [ "$CSV" -nt "$CDF" ]; then
    echo "Processing $CSV."

    sort -n --parallel=$(nproc) $CSV \
        | uniq -c > $HISTOGRAM

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
      }" > $CDF
  fi

  PLOT_LINES+=" '$CDF' using 1:2 title '$TITLE' with line lw 5,"

  shift 2
done

echo "Plotting CDF into $OUTPUT."

gnuplot <<EOF
  set ylabel 'CDF'
  set xlabel 'Latency (ns)'
  set grid
  set xr [$RANGE]
  set term epscairo
  set output '$OUTPUT'
  set datafile separator ","
  $PLOT_LINES
EOF

rm $HISTOGRAM
