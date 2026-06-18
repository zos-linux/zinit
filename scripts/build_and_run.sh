#!/bin/sh
set -e

# duh

source /etc/os-release

if [[ $NAME = "Alpine Linux" ]]; then
    ./build_initramfs.sh
fi
./run_initramfs.sh
