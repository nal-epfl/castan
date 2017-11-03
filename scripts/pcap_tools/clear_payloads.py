#!/usr/bin/python

import argparse
import sys

from scapy.all import *
from scapy.utils import PcapWriter

if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="Remove the L5 payloads from a pcap file")
    parser.add_argument('--input',  help='name of input pcap file', required=True)
    parser.add_argument('--output',  help='name of output pcap file', required=True)
    args = parser.parse_args()

    in_pkts = PcapReader(args.input)
    pktdump = PcapWriter(args.output, append=False)

    for in_pkt in in_pkts:
        if Ether in in_pkt and IP in in_pkt:
          out_pkt = Ether(src=in_pkt[Ether].src, dst=in_pkt[Ether].dst)
          out_pkt = out_pkt/IP(src=in_pkt[IP].src, dst=in_pkt[IP].dst)

          if UDP in in_pkt:
              out_pkt = out_pkt/UDP(sport=in_pkt[UDP].sport,dport=in_pkt[UDP].dport)

          if TCP in in_pkt:
              out_pkt = out_pkt/TCP(sport=in_pkt[TCP].sport,dport=in_pkt[TCP].dport)

          pktdump.write(out_pkt)
