#!/usr/bin/python

import argparse
import sys
import socket

from scapy.all import *
from scapy.utils import PcapWriter

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="Generates a pcap file with a dummy packet")
    parser.add_argument('--output',  help='name of output pcap file', required=True)
    args = parser.parse_args()

    pktdump = PcapWriter(args.output, append=False)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pktdump.write(pkt)
