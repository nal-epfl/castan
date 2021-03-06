
# Config for 10G:
# [mb]{p802p2 (90:e2:ba:55:14:11)/dev6} -- {p802p2 (90:e2:ba:55:12:25)/dev8}[tester]
# [mb]{p802p1 (90:e2:ba:55:14:10)/dev5} -- {p802p1 (90:e2:ba:55:12:24)/dev7}[tester]

# direct link on tester:
# p802p2 90:e2:ba:55:12:25 -- p801p1 90:e2:ba:55:14:38

KERN_NIC_DRIVER=ixgbe
DPDK_NIC_DRIVER=igb_uio

# Middlebox

MB_HOST=icnalsp3s2.epfl.ch

MB_MAC_INTERNAL=90:e2:ba:55:14:11
MB_MAC_EXTERNAL=90:e2:ba:55:14:10

MB_IP_INTERNAL=192.168.34.1
MB_IP_EXTERNAL=192.168.41.1

MB_DEVICE_INTERNAL=p802p2
MB_DEVICE_EXTERNAL=p802p1

MB_PCI_INTERNAL=0000:83:00.1
MB_PCI_EXTERNAL=0000:83:00.0


# Tester

TESTER_HOST=icnalsp3s1.epfl.ch

TESTER_DEVICE_INTERNAL=p802p2
TESTER_DEVICE_EXTERNAL=p802p1

TESTER_PCI_INTERNAL=0000:83:00.1 #dev8
TESTER_PCI_EXTERNAL=0000:83:00.0 #dev7

TESTER_MAC_INTERNAL=90:e2:ba:55:12:25
TESTER_MAC_EXTERNAL=90:e2:ba:55:12:24

TESTER_IP_INTERNAL=192.168.34.5
TESTER_IP_EXTERNAL=192.168.41.10

# Other

DEFAULT_PCAP=test.pcap

export RTE_SDK=$HOME/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc

