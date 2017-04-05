ifeq ($(NOT_FIRST_CALL),)
default: all
else
default: all nf.bc
endif
clean: clean-bc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = nf

# C++ compiler
CC = g++

# sources
SRCS-y := dpdk-lpm.c

# g++ flags
CFLAGS += -O3
CFLAGS += -I..
CFLAGS += -std=gnu++11

LDFLAGS += -lstdc++

include $(RTE_SDK)/mk/rte.extapp.mk

$(APP).bc: $(SRCS-y:.c=.bc) $(RTE_SDK)/build-llvm/librte_lpm.a $(RTE_SDK)/build-llvm/librte_eal.a
	llvm-link -o $@ $^

%.bc: %.c
	clang -g -emit-llvm -mno-sse \
	    -I../../../include/castan -I../../../include \
	    -Ibuild/include -I$(RTE_SDK_BIN)/include \
	    -include $(RTE_SDK_BIN)/include/rte_config.h -I. \
	    -include ../../../include/castan/castan-dpdk.h \
	    -Wno-deprecated-register \
	    -o $@ -c $<

clean-bc:
	@rm -f dpdk-lpm.bc
