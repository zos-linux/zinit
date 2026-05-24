#!/usr/bin/bash
set -e

if [ -e "./initramfs-tmp" ]; then
  rm -rf ./initramfs-tmp
fi
mkdir initramfs-tmp
cd initramfs-tmp

mkdir -pv {etc,usr/{bin,sbin,lib,lib64},var}
for link in bin sbin lib lib64; do
  ln -sv usr/$link $link
done

if [ ! -f ../busybox/busybox ]; then
  echo "You should run ./build_busybox.sh first."
  exit 1
fi
cp -Rav ../busybox/busybox usr/bin
cp -Rav $(realpath /usr/lib64/ld-linux-x86-64.so.2) usr/lib64

source /etc/os-release
echo "Host OS: $NAME"

if [[ $NAME = "Debian GNU/Linux" ]]; then
  LIB="/usr/lib/x86_64-linux-gnu/"
fi
if [[ $NAME = "Devuan GNU/Linux" ]]; then
  LIB="/usr/lib/x86_64-linux-gnu/"
else
  LIB="/usr/lib64"
fi

mkdir -p usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libstdc++.so.6) usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/
ln -sv libstdc++.so.6.0.34 usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libstdc++.so.6
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libstdc++.so.6) usr/lib64
ln -sv libstdc++.so.6.0.34 usr/lib64/libstdc++.so.6
cp -Rav /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libgcc_s.so.1 usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/
cp -Rav /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libgcc_s.so.1 usr/lib64
cp -Rav $LIB/libresolv.so.2 usr/lib64
cp -Rav $LIB/libm.so.6 usr/lib64
cp -Rav $LIB/libc.so.6 usr/lib64
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libasan.so.8) usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libasan.so.8
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libubsan.so.1) usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libubsan.so.1
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libasan.so.8) usr/lib64/libasan.so.8
cp -Rav $(realpath /usr/lib/gcc/$(uname -m)-pc-linux-gnu/$(gcc -dumpversion)/libubsan.so.1) usr/lib64/libubsan.so.1

cp -Rav $(realpath /usr/lib64/libpam.so.0) usr/lib64/libpam.so.0
cp -Rav $(realpath /usr/lib64/libpam_misc.so.0) usr/lib64/libpam_misc.so.0

echo 'PRETTY_NAME="Test Initramfs"' > etc/os-release
echo 'test' > etc/hostname

busybox --install -s usr/bin

echo "Copying zinit... (search paths: ../../buildDir, ../../build)"
if [ -e "../../buildDir" ]; then
  BUILDDIR="../../buildDir"
else
  BUILDDIR="../../build"
fi

cp -av ../../etc/zinit.conf etc/zinit.conf
cp -av $BUILDDIR/src/zinit-serviced ./usr/sbin/zinit-serviced
cp -av $BUILDDIR/src/zinit ./init
cp -av $BUILDDIR/src/systemctl usr/bin/systemctl
chmod +x ./init

cat > etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/sh
bin:x:1:1:bin:/bin:/bin/false
daemon:x:2:2:daemon:/sbin:/bin/false
EOF

PASSWORD=$(openssl passwd -6)
echo "root:$PASSWORD:0:0:99999:7:::" > etc/shadow

mkdir -pv etc/zinit.d
chmod +x ../initscripts/scripts/*
cp -Rav ../initscripts/* etc/zinit.d/

mkdir -p lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/
cp /lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/e1000.ko lib/modules/$(uname -r)/kernel/drivers/net/ethernet/intel/e1000/

find . -print0 | \
cpio --null -ov --format=newc | \
zstd -19 -T0 > ../initramfs.img

echo "Successfully created ../initramfs.img"
echo "Cleaning..."
rm -rf initramfs-tmp