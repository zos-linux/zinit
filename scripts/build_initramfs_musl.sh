#!/bin/bash
set -e

rm -rf initramfs-tmp
cd initramfs-tmp
mkdir -p usr/{lib,bin,sbin} etc
for link in lib bin sbin; do
    ln -s usr/$lib $lib 
done

cp -raf /lib/ld-musl-$(uname -m).so.* lib/
cp -raf /lib/libc.musl-$(uname -m).so.* lib/ || cp -raf /lib/libc.so.* lib/
cp -raf $(realpath /usr/lib/libstdc++.so.6) lib/
cp -raf $(realpath /usr/lib/libgcc_s.so.1) lib/

cp -raf ../../buildDir/{zinit{,-serviced},systemctl} sbin/ || cp -raf ../../build/{zinit{,-serviced},systemctl} sbin/ 
ln -s zinit sbin/init

cp -raf ../busybox/busybox bin/
bin/busybox --install -s usr/bin

echo "test" >> etc/hostname
echo PRETTY_NAME="Test initramfs" >> etc/os-release

cat > etc/passwd << "EOF"                                                                                                                                           
root:x:0:0:root:/root:/bin/sh                                                                                                                                        bin:x:1:1:bin:/bin:/bin/false
daemon:x:2:2:daemon:/sbin:/bin/false                                                                                                                                 
EOF                                                                                                                                                                  
                                                                                                                                                                     
PASSWORD=$(openssl passwd -6)                                                                                                                                        
echo "root:$PASSWORD:0:0:99999:7:::" > etc/shadow                                                                                                                   
mkdir -pv etc/zinit.d                                                                                                                                                chmod +x ../initscripts/scripts/*
cp -Rav ../initscripts/* etc/zinit.d/                                                                                                                                
mkdir -p lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/                                                                                            
cp /lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/e1000.ko lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/                        
find . -print0 | \                                                                                                                                                   
cpio --null -ov --format=newc | \                                                                                                                                    
zstd -19 -T0 > ../initramfs.img                                                                                                                                      
echo "Successfully created ../initramfs.img"                                                                                                                         
echo "Cleaning..."                                                                                                                                                   
rm -rf initramfs-tmp  
