#!/bin/bash
set -e

while [[ $# -gt 0 ]]; do
    case $1 in
        --type)
                type=$2
                shift
                ;;
        --rootdir)
                rootdir=$2
                shift
                ;;
        *)
                shift
                ;;
    esac
done

rootdir="$(realpath -m "$rootdir")"

filelist="pkglist"

read -r line < $filelist
ifs=' ' read -r -a files <<< "$line"

for file in "${files[@]}"; do
	target="$(realpath -m $rootdir/$file)"
	if [[ "$target" != "$rootdir"* ]]; then
		echo "Path escape detected: $file"
		exit 1
	fi
	rm -rf -- "$target"
done 

