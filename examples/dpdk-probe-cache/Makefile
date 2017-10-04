default: all

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = nf

# C++ compiler
CC = g++

# sources
SRCS-y := dpdk-probe-cache.c

# g++ flags
CFLAGS += -O3
CFLAGS += -I..
CFLAGS += -std=gnu++11

CFLAGS += -DNDEBUG
# CFLAGS += -DLATENCY
# CFLAGS += -DPTP
# VV always forward a packet, even when it should be dropped.
# works only if nf has exactly 2 interfaces.
# CFLAGS += -DNODROP

LDFLAGS += -lstdc++

include $(RTE_SDK)/mk/rte.extapp.mk
