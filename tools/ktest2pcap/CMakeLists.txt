#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(ktest2pcap ktest2pcap.cpp)

target_link_libraries(ktest2pcap PRIVATE kleeBasic)

install(TARGETS ktest2pcap RUNTIME DESTINATION bin)
