#!/usr/bin/python
import numpy
from scapy.all import PcapReader,IP,UDP,TCP
from flow import Flow

reader = PcapReader("caida/equinix-chicago.dirA.20160121-125911.UTC.anon.pcap")
flows = {}
i = 0
for p in reader:
    Flow.pkt_handler(p, flows)
    i += 1

i = 0
print "flow n_pkts n_bytes"
for k,f in flows.iteritems():
    print i, f.packet_count, f.data_transferred
    i += 1
