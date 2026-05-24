#!/bin/sh
set -e
mkdir /sys /proc
mount -t proc proc /proc
mount -t sysfs sysfs /sys