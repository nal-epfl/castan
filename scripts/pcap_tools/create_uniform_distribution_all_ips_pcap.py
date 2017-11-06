#!/usr/bin/python

import argparse
import csv
import sys
from random import randint

from scapy.all import *
from scapy.utils import PcapWriter

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="create a pcap file with uniformly random flows")
    parser.add_argument('--npackets', type=int, help='number of packets total (across all flows)', required=True)
    parser.add_argument('--output',  help='name of output pcap file', required=True)

    args = parser.parse_args()

    pktdump = PcapWriter(args.output, append=False)

    for i in xrange(args.npackets): 
        src_ip = socket.inet_ntoa(struct.pack('!L', random.randint(0,0xFFFFFFFF)))
        dst_ip = socket.inet_ntoa(struct.pack('!L', random.randint(0,0xFFFFFFFF)))
        sport = random.randint(1024,65535)
        dport = random.randint(1,10000)

        pkt = Ether(src="08:00:27:53:8b:38", dst="08:00:27:c1:13:47")
        pkt = pkt/IP(src=src_ip, dst=dst_ip)
        pkt = pkt/UDP(sport=sport,dport=dport)

        pktdump.write(pkt)
