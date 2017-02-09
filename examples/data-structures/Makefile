TARGETS := lpm-trie lpm-da hash-table
CFLAGS := -D_GNU_SOURCE -I../../include -Wall -g
LDFLAGS :=

default: $(TARGETS) $(addsuffix .bc,$(TARGETS))

.SECONDEXPANSION:
.SUFFIXES:

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

castan: $(addsuffix .bc,$(TARGETS))
	parallel "time castan {} 2>&1 | tee {}.log" ::: $^

clean:
	rm -f $(TARGETS) $(addsuffix .o,$(TARGETS)) $(addsuffix .bc,$(TARGETS)) deps.mk

deps.mk: $(wildcard *.c)
	gcc $(CFLAGS) -M $^ > $@

-include deps.mk
