export RTE_SDK=/home/vagrant/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc

cd /nf && make && sudo ./build/nf -- --eth-dest 0,08:00:27:53:8b:38 --eth-dest 1,08:00:27:c1:13:47 --nat-ip 192.168.34.2
