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

cp -a -r -f package/* $rootdir/  
exit 0
