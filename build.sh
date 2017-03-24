#!/bin/bash

set -e

# git status 2>/dev/null >/dev/null || git init .
# 
# [ "$(git remote show origin 2>/dev/null | grep 'Fetch URL:')" == "  Fetch URL: https://github.com/klee/klee.git" ] || git remote add -t \* -f origin https://github.com/klee/klee.git
# 
# [ "$(git status | head -n 2)" == "Your branch is up-to-date with 'origin/master'." ] || git checkout master

[ -d /usr/local/src/llvm-3.4/build ] || { echo "Please build LLVM before proceeding."; exit; }
[ -d /usr/local/src/stp/build ] || { echo "Please build STP before proceeding."; exit; }
[ -d /usr/local/src/klee-uclibc/lib ] || { echo "Please build KLEE-uClibc before proceeding."; exit; }

[ -d build ] || mkdir build
cd build

[ -f Makefile.config ] || CXXFLAGS="-std=c++11" LDFLAGS=-L/usr/local/src/minisat/build ../configure --with-llvmsrc=/usr/local/src/llvm-3.4 --with-llvmobj=/usr/local/src/llvm-3.4/build --with-stp=/usr/local/src/stp/build --with-uclibc=/usr/local/src/klee-uclibc --enable-posix-runtime

# make -skj`grep -c processor /proc/cpuinfo` ENABLE_OPTIMIZED=0
make -skj`grep -c processor /proc/cpuinfo` ENABLE_OPTIMIZED=1
