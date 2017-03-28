#!/bin/bash

vagrant up
vagrant ssh -c 'wget -O /dev/null -T 1 -t 1 192.168.34.10' client
echo ""
echo ""
if [ $? -eq 0 ]; then
    echo "==========="
    echo "NAT works!"
    echo "==========="
else
    echo "==============="
    echo "Something wrong"
    echo "==============="
fi
