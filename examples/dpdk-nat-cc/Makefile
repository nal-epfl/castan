ifeq ($(NOT_FIRST_CALL),)
default: all
else
ifeq ($(shell which clang),)
default: all
else
default: all nf.bc
endif
endif
clean: clean-bc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = nf

# sources
SRCS-y := nat_forward_vignat.c nf_main.c nat_config.c \
          nf_time.c nf_util.c \
          flow.c flow-log.c flowmanager.c flowtable.c \
          expirator.c \
          double-chain.c double-chain-impl.c \
          map.c double-map.c

# gcc flags
CFLAGS += -O3
#CFLAGS += -O0 -g -rdynamic -DENABLE_LOG -Wfatal-errors
CFLAGS += -I.. -I../../../include
CFLAGS += -std=gnu99

CFLAGS += -DNDEBUG
# CFLAGS += -DLATENCY
# CFLAGS += -DPTP
# VV always forward a packet, even when it should be dropped.
# works only if nf has exactly 2 interfaces.
# CFLAGS += -DNODROP

# disable warnings triggered by DPDK
CFLAGS += -Wno-implicit-function-declaration
CFLAGS += -Wno-nested-externs

include $(RTE_SDK)/mk/rte.extapp.mk

$(APP).bc: $(SRCS-y:.c=.bc) $(RTE_SDK)/build-llvm/librte_eal.a
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
	@rm -f *.bc
