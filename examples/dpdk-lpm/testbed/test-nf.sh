#!/bin/bash

vagrant up
vagrant ssh -c 'wget -O /dev/null -T 1 -t 1 192.168.34.10' client
if [ $? -eq 0 ]; then
    echo ""
    echo ""
    echo "==========="
    echo "NF works!"
    echo "==========="
else
    echo ""
    echo ""
    echo "==============="
    echo "Something wrong"
    echo "==============="
fi
