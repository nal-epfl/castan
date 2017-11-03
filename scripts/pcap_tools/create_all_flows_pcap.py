#!/usr/bin/python

import argparse
import sys
import socket

from scapy.all import *
from scapy.utils import PcapWriter

from random import shuffle

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="Generates a pcap file with one packet destined to each IP within a given number of mask bits")
    parser.add_argument('--output',  help='name of output pcap file', required=True)
    parser.add_argument('--mask',  help='number of mask bits', required=True)
    args = parser.parse_args()

    pktdump = PcapWriter(args.output, append=False)

    ips = []
    ip = 0
    while ip < 2**(32 - int(args.mask)):
      ips.append(ip)
      ip += 1
    shuffle(ips)

    for ip in ips:
      pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
      pkt = pkt/IP(src="192.168.0.1",
                   dst=socket.inet_ntoa(struct.pack('!L', ip)))
      pkt = pkt/UDP(sport=1234,dport=80)
      pktdump.write(pkt)
