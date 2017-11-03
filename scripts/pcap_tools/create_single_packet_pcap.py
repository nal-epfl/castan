#!/usr/bin/python

import argparse
import sys

from scapy.all import *
from scapy.utils import PcapWriter

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="create a pcap file with a single simple packet")
    parser.add_argument('--output',  help='name of output pcap file', required=True)
    args = parser.parse_args()

    pktdump = PcapWriter(args.output, append=False)
    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2")
    pkt = pkt/UDP(sport=1234,dport=80)
    pktdump.write(pkt)
