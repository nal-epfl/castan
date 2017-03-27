//===-- ktest2pcap.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <assert.h>
#include <string>

#include "klee/Internal/ADT/KTest.h"

#include <pcap/pcap.h>

int main(int argc, char **argv) {
  assert(argc == 3 && "Usage: ktest2pcap <ktest-file> <pcap-file>");

  KTest *input = kTest_fromFile(argv[1]);
  assert(input && "Error loading ktest file.");

  pcap_t *pcap = pcap_open_dead(DLT_EN10MB, 65536);
  pcap_dumper_t *out = pcap_dump_open(pcap, argv[2]);
  assert(out && "Error opening pcap file.");

  for (unsigned i = 0; i < input->numObjects; i++) {
    KTestObject *o = &input->objects[i];

    if (std::string(o->name) == "castan_packet") {
      struct pcap_pkthdr pkthdr = {
          .ts = {.tv_sec = 0, .tv_usec = 0},
          .caplen = o->numBytes,
          .len = o->numBytes,
      };
      pcap_dump((u_char *)out, &pkthdr, o->bytes);
    }
  }

  pcap_close(pcap);
  pcap_dump_close(out);

  return 0;
}
