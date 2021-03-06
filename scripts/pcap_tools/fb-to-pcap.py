#!/usr/bin/python

import argparse
import csv
import sys
from random import randint

from scapy.all import *
from scapy.utils import PcapWriter

class IPTable():
    _ip_table = {}
    _rack_table = {}
    _pod_table = {}
    _cur_rack = {}
    _cur_pod = 1
    _cur_ip = {}
    _prefix_table = {}
    _cur_prefix_ip = {}

    max_prefix = 0
    max_ip = 0


    def lookup_prefix_pod(self, ip_string, prefix_string, pod_string):
        if ip_string not in self._ip_table:
            # use pod for A class, prefix for B & C, and enumerate for D
            rp = "%s_%s" % (prefix_string, pod_string)

            cur_ip = self._cur_ip.get(rp, 1)


            if pod_string not in self._pod_table:
                self._pod_table[pod_string] = self._cur_pod
                self._cur_pod += 1

            pod = self._pod_table[pod_string]


            if pod_string not in self._prefix_table:
                self._prefix_table[pod_string] = {}

            if prefix_string not in self._prefix_table[pod_string]:
                if pod_string not in self._cur_prefix:
                    self._cur_prefix[pod_string] = 1

                self._prefix_table[pod_string][prefix_string] = self._cur_prefix[pod_string]
                self._cur_prefix[pod_string] += 1
            prefix = self._prefix_table[pod_string][prefix_string]

            if prefix >= 255 or pod >= 255:
                print "unexpectedly many pods(%d)/prefixs(%d) for rs: %s, ps: %s" % (pod, prefix, prefix_string, pod_string)
                if "overflow" in pod_string:
                    print "overflow also full"
                    return self.lookup_prefix_pod(ip_string, prefix_string, pod_string + "1")


                return self.lookup_prefix_pod(ip_string, prefix_string, "overflow")

            self._ip_table[ip_string] = "10.%d.%d.%d" % (pod, prefix, cur_ip)
            self._cur_ip[rp] = cur_ip + 1

        return self._ip_table[ip_string]

    def lookup_prefix(self, ip_string, prefix_string):
        if ip_string in self._ip_table:
            return self._ip_table[ip_string]
            


        if prefix_string not in self._prefix_table:
            self._prefix_table[prefix_string] = len(self._prefix_table)
            self._cur_prefix_ip[prefix_string] = 1

        cur_prefix = self._prefix_table[prefix_string]
        cur_ip = self._cur_prefix_ip[prefix_string]

        ip = 10 << 24
        ip +=  (cur_prefix << 16)
        ip += cur_ip


        if cur_prefix > self.max_prefix:
            self.max_prefix = cur_prefix

        if cur_ip > self.max_ip:
            self.max_ip = cur_ip

        
        self._ip_table[ip_string] = socket.inet_ntoa(struct.pack('!L', ip))
        self._cur_prefix_ip[prefix_string] = cur_ip + 1

        return self._ip_table[ip_string]


    def lookup_rack_pod(self, ip_string, rack_string, pod_string):
        if ip_string not in self._ip_table:
            # use pod for A class, rack for B & C, and enumerate for D
            rp = "%s_%s" % (rack_string, pod_string)

            cur_ip = self._cur_ip.get(rp, 1)


            if pod_string not in self._pod_table:
                self._pod_table[pod_string] = self._cur_pod
                self._cur_pod += 1

            pod = self._pod_table[pod_string]


            if pod_string not in self._rack_table:
                self._rack_table[pod_string] = {}

            if rack_string not in self._rack_table[pod_string]:
                if pod_string not in self._cur_rack:
                    self._cur_rack[pod_string] = 1

                self._rack_table[pod_string][rack_string] = self._cur_rack[pod_string]
                self._cur_rack[pod_string] += 1
            rack = self._rack_table[pod_string][rack_string]

            if rack >= 255 or pod >= 255:
                print "unexpectedly many pods(%d)/racks(%d) for rs: %s, ps: %s" % (pod, rack, rack_string, pod_string)
                if "overflow" in pod_string:
                    print "overflow also full"
                    return self.lookup_rack_pod(ip_string, rack_string, pod_string + "1")


                return self.lookup_rack_pod(ip_string, rack_string, "overflow")

            self._ip_table[ip_string] = "10.%d.%d.%d" % (pod, rack, cur_ip)
            self._cur_ip[rp] = cur_ip + 1

        return self._ip_table[ip_string]


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

class FlowTable():
    _table = {}

    def get_flags(self, src, dst, sport, dport, prot):
        flow_str = "%s:%d -> %s:%d (%d)" % (src, sport, dst, dport, prot)
        if flow_str in self._table:
            return ""
        else:
            self._table[flow_str] = True
            return "S"

def convert_file(input_file, output_file):
    pktdump = PcapWriter(output_file, append=False)

    ip_table = IPTable()
    port_table = PortTable()
    flow_table = FlowTable()

    with open(input_file) as tsv:
        for line in csv.reader(tsv, dialect="excel-tab"): #You can also use delimiter="\t" rather than giving a dialect.
            # order in file is: 
            # 0: Timestamp, 1: PktLen, 2: SrcIP, 3: DestIP, 4: SrcPort, 5: DestPort, 6: Prot, 
            # 7: SrcHostPrefix, 8: DestHostPrefix, 9: SrcRack, 10: DestRack, 11: SrcPod, 
            # 12: DestPod, 13: InterCluster, 14: InterDC

            #src_ip = ip_table.lookup_rack_pod(line[2], line[9], line[11])
            #dst_ip = ip_table.lookup_rack_pod(line[3], line[10], line[12])
            src_ip = ip_table.lookup_prefix(line[2], line[7])
            dst_ip = ip_table.lookup_prefix(line[3], line[8])

            pkt = IP(src=src_ip, dst=dst_ip)

            sport = port_table.lookup(line[4])
            dport = port_table.lookup(line[5])

            prot = int(line[6])
            if prot == 6:
                # TCP
                pkt = pkt/TCP(sport=sport,dport=dport,flags=flow_table.get_flags(src_ip, dst_ip, sport, dport, prot))
            elif prot == 17:
                # UDP
                pkt = pkt/UDP(sport=sport,dport=dport)
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
