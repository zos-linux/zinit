#!/bin/sh
set -e

echo "Busybox build script"
rm -rf busybox/{busybox-1.37.0,busybox-1.37.0,busybox}
URL="https://busybox.net/downloads/busybox-1.37.0.tar.bz2"
if [ ! -e "busybox" ]; then
	 mkdir busybox 
fi
cd busybox
echo "--> Fetching Busybox tarball..."
wget --continue $URL

tar -xf busybox-1.37.0.tar.bz2
cd busybox-1.37.0
cp ../../busybox-config .config
echo "--> Building..."
make -j$(nproc)
cp busybox ..
echo "Created Busybox binary (busybox/busybox)"
