#!/usr/bin/python

import argparse
import csv
import sys
from random import randint

from scapy.all import *
from scapy.utils import PcapWriter

class IPTable():
    _flow_table = {}



    def lookup_flow(self, flow):
        if flow in self._flow_table:
            return self._flow_table[flow]
            

        src_ip = socket.inet_ntoa(struct.pack('!L', random.randint(0,0xFFFFFFFF)))
        dst_ip = socket.inet_ntoa(struct.pack('!L', random.randint(0,0xFFFFFFFF)))
        protocol = 6 if random.randint(0,1) == 0 else 17
        sport = random.randint(1024,65535)
        dport = random.randint(1,10000)

        self._flow_table[flow] = (src_ip, dst_ip, protocol, sport, dport)

        return self._flow_table[flow]



def create_flows(num_flows, num_bytes, output_file):
    pktdump = PcapWriter(output_file, append=False)

    first_iteration = True
    cur_seq = 1002
    ip_table = IPTable()
    while num_bytes > 0:
        for f in xrange(num_flows): 
            src_ip, dst_ip, prot, sport, dport = ip_table.lookup_flow(f)

            pkt = IP(src=src_ip, dst=dst_ip)

            if prot == 6:
                # TCP
                if first_iteration:
                    # SYN
                    pkt = pkt/TCP(sport=sport,dport=dport,flags='S', seq=1000)
                    pktdump.write(pkt)
                    pkt = IP(src=dst_ip, dst=src_ip)/TCP(sport=dport,dport=sport,flags='SA',seq=1000, ack=1000)
                    pktdump.write(pkt)
                    pkt = IP(src=src_ip, dst=dst_ip)/TCP(sport=sport,dport=dport,flags='A',seq=1001,ack=1001)
                    pktdump.write(pkt)
                    pkt = IP(src=src_ip, dst=dst_ip)
                    
                pkt = pkt/TCP(sport=sport,dport=dport,seq=cur_seq)

            elif prot == 17:
                # UDP
                pkt = pkt/UDP(sport=sport,dport=dport)
            else:
                continue

            pkt.len = 1500 if num_bytes > 1500 else num_bytes
            pktdump.write(pkt)

        num_bytes -= 1500
        cur_seq += 1500


if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="create a pcap file with n flows with equal distribution of b bytes")
    parser.add_argument('--nflows', nargs=1, type=int, help='number of flows')
    parser.add_argument('--npackets', nargs=1, type=int, help='number of packets total (across all flows)')
    parser.add_argument('--bytes_per_packet', nargs=1, type=int, help='number of bytes per packet')

    parser.add_argument('--output', nargs=1, help='name of output pcap file')

    args = parser.parse_args()

    create_flows(args.n[0], args.b[0], args.output[0])
