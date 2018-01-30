#!/bin/bash

set -e

OUTPUT="$1"
XLABEL="$2"
RANGE="$3"
shift 3

HISTOGRAM=$(mktemp)

GLOBAL_MIN=9223372036854775807 # INT_MAX
GLOBAL_MAX=-9223372036854775808 # INT_MIN


# Line style parameters
declare -A LINE_STYLES
LINE_STYLES["nop"]='dt 1 lw 2 lc rgb "#000000"'
LINE_STYLES["1packet"]='dt 2 lw 2 lc rgb "#E69F00"'
LINE_STYLES["zipf"]='dt 3 lw 3 lc rgb "#56B4E9"'
LINE_STYLES["unirand-all"]='dt 4 lw 4 lc rgb "#009E73"'
LINE_STYLES["unirand-castan"]='dt 5 lw 5 lc rgb "#F0E442"'
LINE_STYLES["castan"]='dt 6 lw 6 lc rgb "#0072B2"'
LINE_STYLES["manual50"]='dt 7 lw 6 lc rgb "#D55E00"'
LINE_STYLES["manual"]='dt 7 lw 6 lc rgb "#D55E00"'
LINE_STYLES["manual64k"]='dt 8 lw 6 lc rgb "#CC79A7"'

declare -A TITLES
TITLES["unirand-all"]="UniRand"
TITLES["unirand-castan"]="UniRand CASTAN"
TITLES["1packet"]="1 Packet"
TITLES["zipf"]="Zipfian"
TITLES["nop"]="NOP"
TITLES["castan"]="CASTAN"
TITLES["manual50"]="Manual CASTAN"
TITLES["manual64k"]="Manual 64k"
TITLES["manual"]="Manual"

PLOT_LINES="plot"
DASH_TYPE=1
while (("$#")); do
  CSV="$1"
  NAME="$2"
  TITLE="${TITLES[$NAME]}"
  CDF="$CSV.cdf"

  if [ -s "$CSV" ]; then
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

    MIN=$(sed '2q;d' $CDF | cut -d , -f 1)
    MAX=$(tail -n 1 $CDF | cut -d , -f 1)

    if [ "$MIN" -lt "$GLOBAL_MIN" ]; then
      GLOBAL_MIN="$MIN"
    fi
    if [ "$MAX" -gt "$GLOBAL_MAX" ]; then
      GLOBAL_MAX="$MAX"
    fi

    PLOT_LINES+=" '$CDF' using 1:2 title '$TITLE' with line ${LINE_STYLES[$NAME]},"
  else
    echo "No data in $CSV. Skipping."
  fi

  shift 2
  DASH_TYPE=$(($DASH_TYPE + 1))
done

if [ "$RANGE" == "auto" ]; then
  RANGE="$GLOBAL_MIN:$GLOBAL_MAX"
  echo "Auto range: $RANGE"
fi

echo "Plotting CDF into $OUTPUT."

gnuplot <<EOF
  set ylabel 'CDF'
  set xlabel '$XLABEL'
  set style line 12 lc rgb '#b0b0b0' lt 0 lw 1
  set grid back ls 12
  set xr [$RANGE]
  set yr [0:1]
  set term epscairo
  set output '$OUTPUT'
  set datafile separator ","
  $PLOT_LINES
EOF

rm $HISTOGRAM
