#!/bin/sh

tcpdump -nvr $1 | awk '
  />/ {
    flow = $1 ":" $4 ":" $3;
    flow_cnt[flow]++;
    packet_cnt++;
  }

  /length [0-9]*)/ {
    sum_length += $15;
  }

  END {
    print packet_cnt, "packets."
    print length(flow_cnt), "flows."
    print sum_length / packet_cnt, "avg bytes / pkt."
  }
'
