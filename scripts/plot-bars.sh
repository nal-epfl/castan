#!/bin/bash

set -e

OUTPUT="$1"
INPUT="$2"
YLABEL="$3"
RANGE="$4"

if [ "$RANGE" == "auto" ]; then
  # MIN=$(cat $INPUT | cut -d, -f2 | sort -n | head -n 1)
  MAX=$(cat $INPUT | cut -d, -f2 | sort -n | tail -n 1)

  RANGE="0:$MAX"
  echo "Auto range: $RANGE"
fi

TEMP_DATA=$(mktemp)
awk '{print NR-1 "," $0}' $INPUT > $TEMP_DATA

echo "Plotting bar plot into $OUTPUT."

TEMP_PLOT=$(mktemp)
gnuplot <<EOF
  set grid
  set term epscairo size 2,5
  set output '$TEMP_PLOT'
  set boxwidth 0.5
  set yr [$RANGE]
  set ylabel '$YLABEL' offset 3
  set lmargin 4
  set style fill solid border -1
  set style line 1 lc rgb "skyblue"
  set key off
  set xtics rotate by 90
  set xtics right offset 0,0
  set ytics 1
  set ytics rotate by 90
  set ytics right offset 0,.25
  set boxwidth .85
  set datafile separator ","
  plot "$TEMP_DATA" using 1:3:xtic(2) with boxes ls 1
EOF

gs -dSAFER -dBATCH -dNOPAUSE -sDEVICE=eps2write \
  -sOutputFile=$OUTPUT \
  -c "<</Orientation 3>> setpagedevice" \
  -f $TEMP_PLOT
# convert -rotate 90 $TEMP_PLOT $OUTPUT

rm $TEMP_DATA
rm $TEMP_PLOT
