#!/usr/bin/python
import sys
import numpy
from scapy.all import PcapReader,IP,UDP,TCP
from flow import Flow

reader = PcapReader(sys.argv[1])
flows = {}
i = 0
for p in reader:
    Flow.pkt_handler(p, flows)
    i += 1

i = 0
print "flw_id n_pkts n_bytes"
for k,f in flows.iteritems():
    print i, f.packet_count, f.data_transferred
    i += 1
