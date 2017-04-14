#!/usr/bin/python

import argparse
import csv
import numpy as np
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

def zipfCdfApprox(k, s, N):
    a = (pow(k, 1 - s) - 1)/(1 - s) + 0.5 + pow(k, -s)/2 + \
        s/12 - pow(k, -1 - s)*s/12
    b = (pow(N, 1 - s) - 1)/(1 - s) + 0.5 + pow(N, -s)/2 + \
        s/12 - pow(N, -1 - s)*s/12
    return a/b

def zipfInversedCdfFast(p, s, N):
    if (1.0 < p or p < 0.0):
        raise Error("probability must be within [0,1]")
    tolerance = 0.01
    x = N/2

    D = p*(12*(pow(N, 1 - s) - 1)/(1 - s) + \
           6 - 6*pow(N, -s) + \
           s - pow(N, -1 - s)*s);

    while (True):
        m    = pow(x, -2 - s)
        mx   = m   * x
        mxx  = mx  * x
        mxxx = mxx * x

        a = 12 * (mxxx - 1) / (1 - s) + 6 * (1 - mxx) + (s - (mx * s)) - D
        b = 12 * mxx + 6 * (s * mx) + (m * s * (s + 1))
        newx = max(1, x - a / b)
        if (abs(newx - x) <= tolerance):
            return int (newx)
        x = newx

def genZipf(s, N, size):
    ps = np.random.uniform(0, 1, size)
    zipfs = {}
    for i in xrange(size):
        zipfs[i] = zipfInversedCdfFast(ps[i], s, N)
    return zipfs


def create_flows(num_packets, bytes_per_packet, output_file, zipf_param, n_flows):
    pktdump = PcapWriter(output_file, append=False)
    #fs = np.random.zipf(zipf_param, num_packets)
    fs = genZipf(zipf_param, n_flows, num_packets)
    flows = {}

    ip_table = IPTable()

    for i in xrange(num_packets): 
        f = fs[i] # generate a single sample from zipfian distribution
        if f not in flows:
            flows[f] = 1
        else:
            flows[f] += 1

        src_ip, dst_ip, prot, sport, dport = ip_table.lookup_flow(f)

        pkt = IP(src=src_ip, dst=dst_ip)
        pkt = pkt/UDP(sport=sport,dport=dport)
        pkt.len = bytes_per_packet

        pktdump.write(pkt)

    flows_file = open("result-flws.txt", 'w')
    flows_file.write("flw_id n_pkts\n")

    for k,v in flows.iteritems():
        flows_file.write('{} {}\n'.format(k, v))
    flows_file.close()


if __name__ == "__main__":
    # parse params
    parser = argparse.ArgumentParser(description="create a pcap file with n flows with equal distribution of b bytes")
    parser.add_argument('--zipf_param', type=float, help='Parameter for Zipf distribution ("s")', required=True)
    parser.add_argument('--nflows', type=float, help='Parameter for Zipf distribution ("N")', required=True)
    parser.add_argument('--npackets', type=int, help='number of packets total (across all flows)', required=True)
    parser.add_argument('--bytes_per_packet',  type=int, help='number of bytes per packet', required=True)
    parser.add_argument('--output',  help='name of output pcap file', required=True)

    args = parser.parse_args()

    create_flows(args.npackets, args.bytes_per_packet,
                 args.output, args.zipf_param,
                 args.nflows)
