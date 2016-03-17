#!/bin/sh
#
# Recreate vmstat archive for pmrep and pcp-vmstat testing
#

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit 0" 0 1 2 3 15

cat <<End-of-File >$tmp.config
log mandatory on 1 sec {
	mem.util.free
	mem.util.bufmem
	mem.util.cached
	mem.util.slab
	swap.used
	swap.pagesin
	swap.pagesout
	mem.vmstat.pgpgin
	mem.vmstat.pgpgout
	disk.all.blkread
	disk.all.blkwrite
	kernel.all.load
	kernel.all.intr
	kernel.all.sysfork
	kernel.all.pswitch
	kernel.all.running
	kernel.all.blocked
	kernel.all.cpu
	hinv.ncpu
}
End-of-File

rm -f pcp-vmstat.index pcp-vmstat.meta pcp-vmstat.0
pmlogger -c $tmp.config -s 5 pcp-vmstat
