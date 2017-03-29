ifeq ($(NOT_FIRST_CALL),)
default: all
else
default: all dpdk-lpm.bc
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

dpdk-lpm.bc: $(SRCS-y)
	clang -g -emit-llvm -march=native \
	    -Ibuild/include -I$(RTE_SDK_BIN)/include \
	    -include $(RTE_SDK_BIN)/include/rte_config.h -I. \
	    -Wno-deprecated-register \
	    -o $@ -c $<

clean-bc:
	@rm -f dpdk-lpm.bc
