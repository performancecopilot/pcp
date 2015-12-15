#!/bin/sh
#
# Recreate vmstat archive for pmrep testing
#

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit 0" 0 1 2 3 15

cat <<End-of-File >$tmp.config
log mandatory on 1 sec {
	mem.util.cached
	mem.util.slab
	proc.runq.runnable
	proc.runq.blocked
	mem.util.free
	mem.util.bufmem
	swap.used
	swap.pagesin
	swap.pagesout
	mem.vmstat.pgpgin
	mem.vmstat.pgpgout
	kernel.all.intr
	kernel.all.pswitch
	kernel.all.cpu
}
End-of-File

rm -f pcp-vmstat.index pcp-vmstat.meta pcp-vmstat.0
pmlogger -c $tmp.config -s 5 pcp-vmstat
