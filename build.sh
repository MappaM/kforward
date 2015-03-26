#!/bin/sh
rm -rf *o
make -C /usr/src/linux-headers-`uname -r` M=$PWD
