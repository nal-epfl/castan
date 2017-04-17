TARGETS := local nonlocal variable-array
CFLAGS := -I../../include -Wall -g
LDFLAGS :=

default: $(TARGETS) $(addsuffix .bc,$(TARGETS))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%: %.o
	gcc $(LDFLAGS) -o $@ $<

%.bc: %.c %
	clang $(CFLAGS) -emit-llvm -c -o $@ $<

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
	gcc $(CFLAGS) -M $^ > $@

-include deps.mk
