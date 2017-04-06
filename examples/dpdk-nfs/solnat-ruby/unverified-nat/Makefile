include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = nat

# C++ compiler
CC = g++

# sources
SRCS-y := ruby/st.c \
          nat_forward_nat.c nat_map_ruby.c ../nat_main.c ../nat_config.c ../nat_time.c ../nat_util.c

# g++ flags
CFLAGS += -O3
CFLAGS += -I..
CFLAGS += -std=gnu++11

# Ruby needs that if it's, well, not Ruby
CFLAGS += -DNOT_RUBY

# Debug only!
#CFLAGS += -DENABLE_LOG -O0 -g -rdynamic

LDFLAGS += -lstdc++

include $(RTE_SDK)/mk/rte.extapp.mk
