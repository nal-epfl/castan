TARGETS := lpm-da lpm-trie hash-table hash-ring ring-array ring-list fsm-loop
CFLAGS := -D_GNU_SOURCE -I../../include -Wall -O0 -g
LDFLAGS :=

SHELL=/bin/bash -o pipefail

default: $(TARGETS) $(addsuffix .bc,$(TARGETS))

.SECONDEXPANSION:
.SUFFIXES:
.DELETE_ON_ERROR: 

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%: %.o $$(firstword $$(subst -, , $$@))-main.o
	gcc $(LDFLAGS) -o $@ $^

%.bc.o: %.c
	clang $(CFLAGS) -emit-llvm -c -o $@ $<

%.bc: %.bc.o $$(firstword $$(subst -, , $$@))-main.bc.o
	llvm-link $(LDFLAGS) -o $@ $^

run-times: $(TARGETS)
	for p in $^; do \
	  for r in `seq 1 100`; do \
	    ./$$p | sed -e 's/^Run Time: \([0-9]*\) us$$/\1/'; \
	  done \
	    | $ awk '{ total += $$1; count++ } END { print total/count }'; \
	done

%.log: %.bc
	time castan --mem-model=wss $< | tee $@

castan: $(addsuffix .log,$(TARGETS))

clean:
	rm -f $(TARGETS)
	rm -f $(addsuffix .o,$(TARGETS))
	rm -f $(addsuffix .bc,$(TARGETS))
	rm -f $(foreach TARGET, $(TARGETS), $(firstword $(subst -, , $(TARGET)))-main.o)
	rm -f deps.mk

deps.mk: $(wildcard *.c)
	gcc $(CFLAGS) -M $^ > $@

-include deps.mk
