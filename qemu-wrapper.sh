#!/bin/sh
export LD_LIBRARY_PATH=/home/shay/a/chu244/ECE46900/jos-ece469/lib:$LD_LIBRARY_PATH
exec /home/shay/a/ee469/labs_2022/qemu/build/bin/qemu-system-i386 "$@"
