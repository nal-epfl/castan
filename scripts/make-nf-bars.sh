#!/bin/bash

set -e

OUTPUT="$1"
shift 1

TEMP=$(mktemp)

while (("$#")); do
  INPUT_FILE="$1"
  TITLE="$2"

  echo "Processing $INPUT_FILE."

  DATA="$(echo $(sed '2q;d' $INPUT_FILE | cut -d ' ' -f 3) / 1000000 | bc -l 2>/dev/null)"

  if [ "$DATA" ]; then
    echo "$TITLE,$DATA" >> $TEMP
  else
    echo "No data in $INPUT_FILE. Skipping."
  fi

  shift 2
done

if [ -s $TEMP ]; then
  plot-bars.sh $OUTPUT $TEMP "Mpps" auto
else
  echo "No data to plot."
fi

rm $TEMP
