#!/bin/sh

tcpdump -nvr $1 | awk '
  />/ {
    flow = $1 ":" $4 ":" $3;
    flow_cnt[flow]++;
    packet_cnt++;
  }

  END {
    print packet_cnt, "packets."
    print length(flow_cnt), "flows."
  }
'
