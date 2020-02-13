#! /bin/sh
# Copyright (c) 2019 Red Hat.
#
# Capture fake sysfs and per-cpu proc stuff to a fake root directory.
#
#
[ $# -ne 1 ] && echo "Usage $0 FAKEROOT" && exit 1
R=$1
[ -e $R ] && echo "Error: fakeroot $R already exists" && exit 1

find /sys/devices/system -type d | while read f; do
    [ ! -d $R/$f ] && mkdir -p $R$f && echo DIR $R$f
done

find /sys/devices/system -type f | while read f; do
  cat $f >$R$f
  echo FILE $R$f 
done

find /sys/devices/system -type l | while read f; do
  L=`ls -l $f | awk '{print $NF}'`
  ln -s $L $R$f
  echo "SYMLINK $L $R$f"
done

mkdir -p $R/proc/net
for f in stat interrupts softirqs cpuinfo net/softnet_stat; do
    cat /proc/$f >$R/proc/$f
    echo FILE $R/proc/$f
done

exit 0
