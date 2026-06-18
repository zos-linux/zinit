#!/bin/bash
set -e
 
rm -rf initramfs-tmp
mkdir initramfs-tmp
cd initramfs-tmp
mkdir -p usr/{lib,bin,sbin} etc
for link in lib bin sbin; do
    ln -s usr/$link $link 
done

cp -rp /lib/ld-musl-$(uname -m).so.* lib/
cp -rp /lib/libc.musl-$(uname -m).so.* lib/ || cp -rp /lib/libc.so.* lib/
cp -rp $(realpath /usr/lib/libstdc++.so.6) lib/libstdc++.so.6
cp -rp $(realpath /usr/lib/libgcc_s.so.1) lib/

cp -rp ../../buildDir/src/{zinit{,-serviced},systemctl} sbin/ || cp -rp ../../build/src/{zinit{,-serviced},systemctl} sbin/ 
ln -s zinit sbin/init

cp -rp ../busybox/busybox bin/
bin/busybox --install -s usr/bin

echo "test" >> etc/hostname
echo PRETTY_NAME="Test initramfs" >> etc/os-release

cp -rp ../initramfs-passwd etc/passwd
PASSWORD=$(openssl passwd -6)                                                                                                                                       echo "root:$PASSWORD:0:0:99999:7:::" > etc/shadow                                                                                                                   
mkdir -p etc/zinit.d                                                                                                                                                
chmod +x ../initscripts/scripts/*
cp -ra ../initscripts/* etc/zinit.d/                                                                                                                                
cp -ra ../../etc/zinit.conf etc/
mkdir -p lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/                                                                                           

E1000="lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000" 
if [ -e "/$E1000" ]; then
    cp /$E1000/e1000.ko.* $E1000/
fi

find . -print0 | \
    cpio --null -ov --format=newc | \
    zstd -19 -T0 > ../initramfs.img
echo "Successfully created ../initramfs.img"
echo "Cleaning..."
rm -rf initramfs-tmp  
