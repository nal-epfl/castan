TARGETS := ip-asn-lookup-trie ip-asn-lookup-da nat-hash-table
CFLAGS := -D_GNU_SOURCE -I../../include -Wall -g
LDFLAGS := -lpcap

SHELL=/bin/bash -o pipefail

default: $(TARGETS) $(addsuffix .bc,$(TARGETS)) make-rainbow-table

.SUFFIXES:
.DELETE_ON_ERROR:
.PRECIOUS: $(addsuffix .log,$(TARGETS))
.PHONY: $(addsuffix .log,$(TARGETS))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%: %.o
	gcc $(LDFLAGS) -o $@ $<

%.bc: %.c
	clang $(CFLAGS) -emit-llvm -c -o $@ $<

make-rainbow-table: make-rainbow-table.c
	gcc -Wall -O3 $< -o $@

havoc.rt: make-rainbow-table
	./make-rainbow-table 10000 | sort -s -k 1,1 > havoc.rt

run-times: $(TARGETS)
	for p in $^; do \
	  for r in `seq 1 100`; do \
	    ./$$p | sed -e 's/^Run Time: \([0-9]*\) us$$/\1/'; \
	  done \
	    | $ awk '{ total += $$1; count++ } END { print total/count }'; \
	done

%.log: %.bc
	time castan --mem-model=wss --concretize-sym-indices $< | tee $@

castan: $(addsuffix .log,$(TARGETS))

clean:
	rm -f $(TARGETS) $(addsuffix .o,$(TARGETS)) $(addsuffix .bc,$(TARGETS)) deps.mk

deps.mk: $(wildcard *.c)
	gcc $(CFLAGS) -M $^ > $@

-include deps.mk
