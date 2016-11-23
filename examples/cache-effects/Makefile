TARGETS := local nonlocal
CFLAGS := -I../../include -Wall -g
LDFLAGS :=

default: $(TARGETS) $(addsuffix .bc,$(TARGETS))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%: %.o
	gcc $(LDFLAGS) -o $@ $<

%.bc: %.c %
	clang $(CFLAGS) -emit-llvm -c -o $@ $<

time: $(TARGETS)
	  parallel -k -j 1 "time ./{1} 2>&1 | grep user | sed -e 's/\([0-9.]*\)user.*/{1} {2}: \1/'" ::: $^ ::: `seq 1 10`

clean:
	rm -f $(TARGETS) $(addsuffix .o,$(TARGETS)) $(addsuffix .bc,$(TARGETS)) deps.mk

deps.mk: $(wildcard *.c)
	gcc $(CFLAGS) -M $^ > $@

-include deps.mk
