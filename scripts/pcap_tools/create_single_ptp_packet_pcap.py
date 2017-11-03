#!/usr/bin/python

import argparse
import sys

from scapy.all import *
from scapy.utils import PcapWriter

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="create a pcap file with a single PTP packet")
    parser.add_argument('--output',  help='name of output pcap file', required=True)
    args = parser.parse_args()

    pktdump = PcapWriter(args.output, append=False)
    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47", type=0x88f7)
    pkt = pkt/Raw(['\x00\x02\x00\x22\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xE4\xAF\xA1\x30\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00'])
    pktdump.write(pkt)
