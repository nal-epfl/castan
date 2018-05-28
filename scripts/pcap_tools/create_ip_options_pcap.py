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
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x04\x05\x00'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x08\x05\x00\x00\x00\x00\x00'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x08\x09\x00\xFF\xFF\xFF\xFF'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x08\x09\xF0\xFF\xFF\xFF\xFF'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x0C\x05\x01\x00\x00\x00\x00\x00\x00\x00\x00'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x0C\x05\x03\x01\x02\x03\x04\x00\x00\x00\x00'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x44\x08\x05\x00\x00\x00\x00\x00\x44\x08\x09\x00\xFF\xFF\xFF\xFF\x44\x08\x09\xF0\xFF\xFF\xFF\xFF\x44\x0C\x05\x01\x00\x00\x00\x00\x00\x00\x00\x00'))
    pktdump.write(pkt)

    pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
    pkt = pkt/IP(src="192.168.0.1", dst="192.168.0.2", options=IPOption('\x83\x03\x10'))
    pktdump.write(pkt)
