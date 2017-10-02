TARGETS := local nonlocal variable-array process-contention-sets probe-slices check-contention-sets
CFLAGS := -I../../include -Wall -g -O3 -I$(RTE_SDK)/$(RTE_TARGET)/include
LDFLAGS := -pthread $(RTE_SDK)/$(RTE_TARGET)/lib/librte_eal.a -ldl

.SUFFIXES:

default: $(TARGETS) $(addsuffix .bc,$(TARGETS))

%.o: %.c
	gcc -std=c11 $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	g++ -std=c++11 $(CFLAGS) -c -o $@ $<

%: %.o
	g++ -o $@ $^ $(LDFLAGS)

%.bc: %.c %
	clang -std=c11 $(CFLAGS) -emit-llvm -c -o $@ $<

%.bc: %.cpp %
	clang++ -std=c++11 $(CFLAGS) -emit-llvm -c -o $@ $<

run-times: $(TARGETS)
	for p in $^; do \
	  for r in `seq 1 100`; do \
	    ./$$p | sed -e 's/^Run Time: \([0-9]*\) us$$/\1/'; \
	  done \
	    | $ awk '{ total += $$1; count++ } END { print total/count }'; \
	done

castan: $(addsuffix .bc,$(TARGETS))
	parallel "time castan {} 2>&1 | sed 's/^\(.*\)$$/{}: \1/'" ::: $^

clean:
	rm -f $(TARGETS) $(addsuffix .o,$(TARGETS)) $(addsuffix .bc,$(TARGETS)) deps.mk

deps.mk: $(wildcard *.c)
	gcc $(CFLAGS) -M $^ > $@ || true

-include deps.mk
