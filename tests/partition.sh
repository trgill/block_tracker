#!/bin/bash

if [ -z "$1" ]
then
    echo "Usage: $0 <test block device>"
    exit 1
fi

parted --script $1 \
    mklabel gpt \
    mkpart primary 1GiB 2GiB \
    mkpart primary 2GiB 3GiB \
    mkpart primary 3GiB 4GiB \
    mkpart primary 4GiB 5GiB \
    mkpart primary 5GiB 6GiB 
