#! /bin/sh
# Copyright (c) 2019,2022 Red Hat.
#
# Capture scsi and cpu fake sysfs and per-cpu proc stuff to a fake root directory.
# See example tarballs in qa/linux/{blk*,scsi*}
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
for f in stat partitions diskstats interrupts softirqs cpuinfo net/softnet_stat; do
    cat /proc/$f >$R/proc/$f
    echo FILE $R/proc/$f
done

find /sys/devices/* -type f | while read f; do
  mkdir -p `dirname $R$f` 2>/dev/null
  cat $f >$R$f
  echo FILE $R$f
done

mkdir -p $R/sys/block
find /sys/block/*/device/wwid /sys/block/*/wwid -type f | while read f; do
  mkdir -p `dirname $R$f` 2>/dev/null
  cat $f >$R$f
  echo FILE $R$f
done

find /sys/block/* -type l | while read f; do
  L=`ls -l $f | awk '{print $NF}'`
  ln -s $L $R$f
  echo "SYMLINK $L $R$f"
done

exit 0
