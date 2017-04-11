#!/bin/sh

export PATH=$PATH:$(cd $(dirname ${BASH_SOURCE[0]} ) && pwd)/build/Debug+Asserts/bin:$(cd $(dirname ${BASH_SOURCE[0]} ) && pwd)/build/Release+Debug+Asserts/bin:$(cd $(dirname ${BASH_SOURCE[0]} ) && pwd)/scripts:$(cd $(dirname ${BASH_SOURCE[0]} ) && pwd)/scripts/perf
