#!/bin/bash
set -e

if [ -f "/vmlinuz" ]; then
  cp -v /vmlinuz ./bzImage
else
  cp -v /boot/vmlinuz-$(uname -r)* ./bzImage
fi

if [ -f ./initramfs.img ]; then
  echo "OK"
else
  echo "Initramfs not found"
  exit 1
fi

qemu-system-x86_64 -m 2G -kernel bzImage -initrd ./initramfs.img -append "console=ttyS0 rdinit=/init" -net user -net nic $@

rm ./bzImage
