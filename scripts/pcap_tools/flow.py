import sys
from scapy.all import *
from operator import methodcaller

class Flow(object):
	"""docstring for Flow"""

	@staticmethod
	def flowid(pkt):
		IP_layer = IP if IP in pkt else IPv6

                if TCP in pkt:
                    sport = pkt[TCP].sport
                    dport = pkt[TCP].dport
                    protocol = 'TCP'
		elif UDP in pkt:
                    print dir(pkt)
                    sport = pkt[UDP].sport
                    dport = pkt[UDP].dport
		    protocol = 'UDP'
                else:
                    sport = -1
                    dport = -1
                    protocol = 'unknown'

                fid = "flowid--%s-%s--%s-%s---%s" % (pkt[IP_layer].src, sport, pkt[IP_layer].dst, dport, protocol)

		return fid.replace('.','-')

	@staticmethod
	def pkt_handler(pkt, flows):
		if IP not in pkt:
			return

		flowid = Flow.flowid(pkt)
		if flowid not in flows:
			flows[flowid] = Flow(pkt)
		else:
			flows[flowid].add_pkt(pkt)

	def __init__(self, pkt):
		self.packets = []
                self.last_packet = None
		self.tls = False # until proven otherwise
		self.cleartext_payload = ""

		# set initial timestamp
		self.timestamp = pkt.time

		# addresses
		IP_layer = IP if IP in pkt else IPv6

		self.src_addr = pkt[IP_layer].src
		self.dst_addr = pkt[IP_layer].dst
            
                if TCP in pkt:
                    self.src_port = pkt[TCP].sport
                    self.dst_port = pkt[TCP].dport
                    self.protocol = 'TCP'
                    self.buffer = [] # buffer for out-of-order packets
		elif UDP in pkt:
                    self.src_port = pkt[UDP].sport
                    self.dst_port = pkt[UDP].dport
		    self.protocol = 'UDP'
		else:
		    self.protocol = "???"

		# see if we need to reconstruct flow (i.e. check SEQ numbers)
		self.payload = ""
		self.decoded_flow = None
		self.data_transferred = 0
		self.packet_count = 0
		self.fid = Flow.flowid(pkt)

		self.add_pkt(pkt)



	def extract_elements(self):
		if self.decoded_flow and self.decoded_flow['flow_type'] == 'http_request':
			return {'url': self.decoded_flow['url'], 'host': self.decoded_flow['host'], 'method': self.decoded_flow['method']}
		else:
			return None

	def add_pkt(self, pkt):
		self.packet_count += 1
                self.data_transferred += int(pkt.sprintf("%IP.len%"))


	def reconstruct_flow(self, pkt):
		assert TCP in pkt

		# deal with all packets or only new connections ?
                self.data_transferred += int(pkt.sprintf("%IP.len%"))
                print self.data_transferred
                #self.packets += pkt


	def check_buffer(self):
		for i, pkt in enumerate(self.buffer):
			last = self.packets[-1:][0]

			# calculate expected seq
			if Raw in last:
				next_seq = self.seq + len(last[Raw].load)
			else:
				next_seq = self.seq

			# the packet's sequence number matches
			if next_seq == pkt[TCP].seq:

				# pop from buffer
				self.packets += self.buffer.pop(i)
				self.seq = pkt[TCP].seq

				if Raw in pkt:
					self.payload += str(pkt[Raw].load)
					self.data_transferred += len(pkt[Raw].load)

				return True

		return False

	def get_statistics(self):

		update = {
				'timestamp': self.timestamp,
				'fid' : self.fid,
				'src_addr': self.src_addr,
				'src_port': self.src_port,
				'dst_addr': self.dst_addr,
				'dst_port': self.dst_port,
				'protocol': self.protocol,
				'packet_count': self.packet_count,
				'data_transferred': self.data_transferred,
				'tls': self.tls,
				}

		# we'll use the type and info fields
		self.decoded_flow = Decoder.decode_flow(self)
		update['decoded_flow'] = self.decoded_flow

		return update

	def get_payload(self, encoding='web'):

		if self.tls:
			payload = self.cleartext_payload
		else:
			payload = self.payload

		if encoding == 'web':
			return unicode(payload, errors='replace')
		if encoding == 'raw':
			return payload


	def print_statistics(self):
            print "%s:%s  ->  %s:%s (%s, %s packets, %s bytes)" % (self.src_addr, self.src_port, self.dst_addr, self.dst_port, self.protocol, self.packet_count, self.data_transferred)

        def flow_stats(self):
            return "%s: %s packets, %s bytes, %s duration" % (self.fid,
                    self.packet_count, self.data_transferred,
                    self.last_packet.time - self.timestamp)

        def bytes_transferred_since(self, time):
            self.packets = [pkt for pkt in self.packets if pkt.time > time]
            return reduce(lambda x,y: x+y, [len(pkt) for pkt in  self.packets if pkt.time > time] or [0])



def clean_flows(flows, time):
    new_flows = {}
    for k,v  in flows.iteritems():
        add = False
        for p in v.packets:
            if p.time > time:
                add = True

        if add:
            new_flows[k] = v
    flows = new_flows

if __name__ == '__main__':
    filename = sys.argv[1]
    flows = {}

    reader = PcapReader(filename)
    i = 0
    for p in reader:
        Flow.pkt_handler(p, flows)
        i += 1
        if i%500000 == 0:
            print i
            with open("interim_%s.txt" % (i), "w") as file:
                for k,f in flows.iteritems():
                    file.write(f.flow_stats())
                    file.write("\n")

    for k,f in flows.iteritems():
        print f.flow_stats()

 # x.weight(p.time))
