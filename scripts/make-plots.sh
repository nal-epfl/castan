#!/bin/bash

set -e

function make_cdf {
  NF=$1
  RANGE=$2
  echo "Making $NF-latcdf.eps."
  plot-cdf.sh $NF-latcdf.eps 0:$RANGE \
      castan/$NF-castan-lat.csv "CASTAN" \
      facebook/$NF-fbA1-lat.csv "Facebook A1" \
      facebook/$NF-fbB1-lat.csv "Facebook B1" \
      facebook/$NF-fbC1-lat.csv "Facebook C1" \
      imc10/$NF-imc10u1p1-lat.csv "IMC10 U1P1" \
      imc10/$NF-imc10u2p8-lat.csv "IMC10 U2P8" \
      random/$NF-unirand-lat.csv "Uniform Random" \
      random/$NF-zipfrand-lat.csv "Zipf Random"
}

make_cdf dpdk-lpm-dpdklpm 400
make_cdf dpdk-lpm-btrie 400
make_cdf dpdk-nat-stlmap 2000

echo "Making dpdk-thru1p-p.eps and dpdk-thru1p-b.eps"

BARS_P=$(mktemp)
BARS_B=$(mktemp)
echo "NF,CASTAN,Facebook A1,Facebook B1,Facebook C1,IMC10U1P1,IMC10U2P8,Uniform Random,Zipf Random" \
    | tee $BARS_P > $BARS_B

ADD_PLOT=true
COLUMN=2
for NF in dpdk-lpm-dpdklpm \
          dpdk-lpm-btrie \
          dpdk-nat-stlmap \
          dpdk-nat-basichash \
          dpdk-nat-cc \
          dpdk-nat-dpdkhash \
          dpdk-nat-ruby \
          dpdk-nat-stlumap; do
  echo -n "$NF" | tee -a $BARS_P >> $BARS_B
  for RESULT in castan/$NF-castan-thru1p.results \
                facebook/$NF-fbA1-thru1p.results \
                facebook/$NF-fbB1-thru1p.results \
                facebook/$NF-fbC1-thru1p.results \
                imc10/$NF-imc10u1p1-thru1p.results \
                imc10/$NF-imc10u2p8-thru1p.results \
                random/$NF-unirand-thru1p.results \
                random/$NF-zipfrand-thru1p.results; do
    echo "Loading $RESULT."
    if [ -f "$RESULT" ]; then
      echo -n ",$(tail -n 1 $RESULT | awk '{print $3 / 1000000.0}')" >> $BARS_P
      echo -n ",$(tail -n 1 $RESULT | awk '{print $1 / 1000.0}')" >> $BARS_B
    else
      echo "  Missing result file."
      echo -n ",-1" | tee -a $BARS_P >> $BARS_B
    fi
    $ADD_PLOT && PLOT_LINE_P+=" '$BARS_P' using $COLUMN:xtic(1) title column,"
    $ADD_PLOT && PLOT_LINE_B+=" '$BARS_B' using $((COLUMN++)):xtic(1) title column,"
  done
  echo | tee -a $BARS_P >> $BARS_B
  ADD_PLOT=false
done

gnuplot <<EOF
  set boxwidth 0.9 absolute
  set style fill solid 1.00 border lt -1
  set key horizontal
  set style histogram clustered gap 1 title textcolor lt -1
  set style data histograms
  set xtics norangelimit
  set xtics ()
  set xr [-.5:7.5]

  set ylabel 'Mpps'
  set grid ytics

  set term epscairo size 10,2.5
  set output 'dpdk-thru1p-p.eps'
  set datafile separator ","
  plot $PLOT_LINE_P
EOF

gnuplot <<EOF
  set boxwidth 0.9 absolute
  set style fill solid 1.00 border lt -1
  set key horizontal
  set style histogram clustered gap 1 title textcolor lt -1
  set style data histograms
  set xtics norangelimit
  set xtics ()
  set xr [-.5:7.5]
  set yr [-1:11]

  set ylabel 'Gbps'
  set grid ytics

  set term epscairo size 10,2.5
  set output 'dpdk-thru1p-b.eps'
  set datafile separator ","
  plot $PLOT_LINE_B
EOF

rm $BARS_P $BARS_B
