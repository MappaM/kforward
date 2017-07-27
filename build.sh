#!/bin/sh
rm -rf *.o

if [ "$KERNEL_DIR" = "" ] ; then
    KERNEL_DIR=/usr/src/linux-headers-`uname -r`
fi

make -C $KERNEL_DIR M=$PWD $@
