#!/usr/bin/python

import argparse
import csv
import sys
from random import randint

from scapy.all import *
from scapy.utils import PcapWriter

class IPTable():
    _table = {}
    _cur_number = 1

    def lookup(self, ip_string):
        if ip_string not in self._table:
            self._table[ip_string] = "10.0.%d.%d" % (self._cur_number / 255, self._cur_number % 255)
            self._cur_number += 1

        return self._table[ip_string]

class PortTable():
    _table = {}
    _taken = {}

    def lookup(self, port_string):
        if port_string not in self._table:
            while True:
                new_port = randint(1,65534)
                if new_port not in self._taken:
                    self._taken[new_port] = True
                    break
            
            self._table[port_string]  = new_port
        return self._table[port_string]

def convert_file(input_file, output_file):
    pktdump = PcapWriter(output_file, append=False)

    ip_table = IPTable()
    port_table = PortTable()

    with open(input_file) as tsv:
        for line in csv.reader(tsv, dialect="excel-tab"): #You can also use delimiter="\t" rather than giving a dialect.
            # order in file is: 
            # 0: Timestamp, 1: PktLen, 2: SrcIP, 3: DestIP, 4: SrcPort, 5: DestPort, 6: Prot, 
            # 7: SrcHostPrefix, 8: DestHostPrefix, 9: SrcRack, 10: DestRack, 11: SrcPod, 
            # 12: DestPod, 13: InterCluster, 14: InterDC

            src_ip = ip_table.lookup(line[2])
            dst_ip = ip_table.lookup(line[3])

            pkt = IP(src=src_ip, dst=dst_ip)

            sport = port_table.lookup(line[4])
            dport = port_table.lookup(line[5])
            if line[6] == "6":
                # TCP
                pkt = pkt/TCP(sport=sport,dport=dport)
            elif line[6] == "17":
                # UDP
                pkt = pkt/TCP(sport=sport,dport=dport)
            else:
                continue

            pkt.time = int(line[0])
            pkt.len = int(line[1])

            pktdump.write(pkt)


if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="convert FB style traces to pcap")
    parser.add_argument('--input', nargs='+', help='list of inputs')
    parser.add_argument('--output', nargs='+', help='list of output pcaps')

    args = parser.parse_args()

    if len(args.input) != len(args.output):
        print "different input and output length"
        sys.exit(1)

    for i in xrange(0, len(args.input)):
        convert_file(args.input[i], args.output[i])
