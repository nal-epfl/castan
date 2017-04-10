# Functions that tweak the kernal to make it possible to run DPDK apps:
# enable NUMA, reserver hugepages, bind/unbind network interfaces

# Taken from dpdk/tools/setup.sh

create_mnt_huge()
{
	  echo "Creating /mnt/huge and mounting as hugetlbfs"
	  sudo mkdir -p /mnt/huge

	  grep -s '/mnt/huge' /proc/mounts > /dev/null \
        || sudo mount -t hugetlbfs nodev /mnt/huge
}

remove_mnt_huge()
{
	  echo "Unmounting /mnt/huge and removing directory"
	  grep -s '/mnt/huge' /proc/mounts > /dev/null \
        && sudo umount /mnt/huge

	  if [ -d /mnt/huge ] ; then
		    sudo rm -R /mnt/huge
	  fi
}

clear_huge_pages()
{
	  echo "Removing currently reserved hugepages"
	  for d in /sys/devices/system/node/node? ; do
      echo 0 > sudo tee $d/hugepages/hugepages-2048kB/nr_hugepages > /dev/null
	  done

	  remove_mnt_huge
}

set_numa_pages()
{
	  clear_huge_pages

	  echo "Reserving hugepages"
	  for d in /sys/devices/system/node/node? ; do
		    node=$(basename $d)
                    Pages=1600
		    echo $Pages > sudo tee $d/hugepages/hugepages-2048kB/nr_hugepages > /dev/null
	  done

	  create_mnt_huge
}

remove_igb_uio_module()
{
	  echo "Unloading any existing DPDK UIO module"
	  /sbin/lsmod | grep -s igb_uio > /dev/null \
        && sudo /sbin/rmmod igb_uio
}

load_igb_uio_module()
{
	  if [ ! -f $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko ];then
		    echo "## ERROR: Target does not have the DPDK UIO Kernel Module."
		    echo "       To fix, please try to rebuild target."
		    return
	  fi

	  remove_igb_uio_module

	  
	  if ! /sbin/lsmod | grep -s uio > /dev/null; then
		    modinfo uio > /dev/null
		    if [ $? -eq 0 ]; then
			      echo "Loading uio module"
			      sudo /sbin/modprobe uio
		    fi
	  fi

	  # UIO may be compiled into kernel, so it may not be an error if it can't
	  # be loaded.

	  echo "Loading DPDK UIO module"
	  if ! sudo /sbin/insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko; then
		    echo "## ERROR: Could not load kmod/igb_uio.ko."
		    quit
	  fi
}

bind_nics_to_igb_uio()
{
	  if  /sbin/lsmod  | grep -q igb_uio ; then
		    PCI_PATH=$1
                    echo "Binding PCI device: $PCI_PATH ..."
		    sudo ${RTE_SDK}/tools/dpdk-devbind.py -b $DPDK_NIC_DRIVER $PCI_PATH && echo "OK"
	  else
		    echo "# Please load the 'igb_uio' kernel module before querying or "
		    echo "# adjusting NIC device bindings"
	  fi
}
