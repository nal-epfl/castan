default: all
clean: clean-bc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = nf

# C++ compiler
CC = g++

# sources
SRCS-y := dpdk-nat-dpdkhash.c

# g++ flags
CFLAGS += -O3
CFLAGS += -I..
CFLAGS += -std=gnu++11
CFLAGS += -I../../../include
CFLAGS += -I/castan/include

CFLAGS += -DNDEBUG
# CFLAGS += -DLATENCY
# CFLAGS += -DPTP
# VV always forward a packet, even when it should be dropped.
# works only if nf has exactly 2 interfaces.
# CFLAGS += -DNODROP

LDFLAGS += -lstdc++

include $(RTE_SDK)/mk/rte.extapp.mk

$(APP).bc: $(SRCS-y:.c=.bc) $(RTE_SDK)/build-llvm/librte_eal.a $(RTE_SDK)/build-llvm/librte_hash.a $(RTE_SDK)/build-llvm/librte_ring.a
	llvm-link -o $@ $^

%.bc: %.c
	clang -g -emit-llvm -mno-sse \
	    -I../../../include/castan -I../../../include \
	    -Ibuild/include -I$(RTE_SDK_BIN)/include \
	    -include $(RTE_SDK_BIN)/include/rte_config.h -I. \
	    -include ../../../include/castan/castan-dpdk.h \
	    -Wno-deprecated-register \
	    -o $@ -c $<

make-rainbow-table: make-rainbow-table.c
	gcc -Wall -O3 -I$(RTE_SDK_BIN)/include $< -o $@

havoc.rt: make-rainbow-table
	./make-rainbow-table 1000000 | sort -s -k 1,1 > havoc.rt

clean-bc:
	@rm -f *.bc
