#!/bin/bash

set -eo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

sudo apt-get install -y \
  autoconf \
  automake \
  bc \
  binutils-dev \
  binutils-gold \
  bison \
  build-essential \
  cmake \
  curl \
  doxygen \
  flex \
  g++ \
  gcc \
  git \
  libboost-all-dev \
  libcap-dev \
  libffi-dev \
  libgoogle-perftools-dev \
  libncurses5-dev \
  libpcap-dev \
  libtcmalloc-minimal4 \
  libtool \
  libz3-dev \
  m4 \
  make \
  python \
  python-minimal \
  python-pip \
  subversion \
  texinfo \
  unzip \
  wget \
  zlib1g \
  zlib1g-dev

cat >> ~/.bash_profile <<EOF

export C_INCLUDE_PATH="/usr/include/x86_64-linux-gnu"
export CPLUS_INCLUDE_PATH="/usr/include/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/src/minisat/build"
export PATH="$PATH:/usr/local/src/llvm-3.4/build/Release+Debug+Asserts/bin:$SCRIPT_DIR/build/Debug+Asserts/bin:$SCRIPT_DIR/build/Release+Debug+Asserts/bin:$SCRIPT_DIR/scripts:$SCRIPT_DIR/scripts/perf:$SCRIPT_DIR/scripts/pcap_tools"

ulimit -s unlimited
EOF

. ~/.bash_profile

sudo svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_34/final/ /usr/local/src/llvm-3.4
sudo svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_34/final/ /usr/local/src/llvm-3.4/tools/clang
sudo chown -R $(id -nu):$(id -ng) /usr/local/src/llvm-3.4
mkdir -p /usr/local/src/llvm-3.4/build
pushd /usr/local/src/llvm-3.4/build
/usr/local/src/llvm-3.4/configure --enable-optimized --enable-assertions --enable-debug-symbols --enable-libffi --enable-doxygen
make -kj$(nproc)
sudo ln -fs ld.gold /usr/bin/ld
popd

sudo git clone https://github.com/stp/minisat.git /usr/local/src/minisat
sudo chown -R $(id -nu):$(id -ng) /usr/local/src/minisat
mkdir /usr/local/src/minisat/build
pushd /usr/local/src/minisat/build
cmake -DSTATICCOMPILE=ON -DCMAKE_INSTALL_PREFIX=/usr/ ..
make -skj$(nproc)
popd

sudo git clone --branch 2.1.2 https://github.com/stp/stp.git /usr/local/src/stp
sudo chown -R $(id -nu):$(id -ng) /usr/local/src/stp
mkdir /usr/local/src/stp/build
pushd /usr/local/src/stp/build
cmake -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_PYTHON_INTERFACE:BOOL=OFF -DMINISAT_LIBRARY=/usr/local/src/minisat/build/libminisat.a -DMINISAT_INCLUDE_DIR=/usr/local/src/minisat ..
make -skj$(nproc)
popd

sudo git clone --branch klee_0_9_29 https://github.com/klee/klee-uclibc.git /usr/local/src/klee-uclibc
sudo chown -R $(id -nu):$(id -ng) /usr/local/src/klee-uclibc
pushd /usr/local/src/klee-uclibc
/usr/local/src/klee-uclibc/configure --make-llvm-lib
make -skj$(nproc)
popd

mkdir $SCRIPT_DIR/build
pushd $SCRIPT_DIR/build
CXXFLAGS="-std=c++11" LDFLAGS=-L/usr/local/src/minisat/build $SCRIPT_DIR/configure --with-llvmsrc=/usr/local/src/llvm-3.4 --with-llvmobj=/usr/local/src/llvm-3.4/build --with-z3=/usr --with-stp=/usr/local/src/stp/build --with-uclibc=/usr/local/src/klee-uclibc --enable-posix-runtime
make -skj$(nproc) ENABLE_OPTIMIZED=1
popd
