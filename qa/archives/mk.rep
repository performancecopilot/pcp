#!/bin/sh
# 
# Recipe for creating the rep archive, with the following matrix
# of metric type / semantics / units ...
#
#    string / instant  / none    - kernel.uname.sysname
#    string / discrete / none    - hinv.machine
#
#    int / instant / none        - kernel.all.lastpid
#    int / instant / byte        - mem.util.used
#    int / instant / count       - network.tcpconn.close
#    int / instant / sec         - kernel.all.uptime
#
#    int / discrete / none       - hinv.ncpu
#    int / discrete / byte       - mem.physmem
#    int / discrete / count      - hinv.nfilesys
#    int / discrete / sec        - proc.psinfo.start_time
#    int / discrete / b/s        - network.interface.baudrate
#    int / discrete / c/s        - kernel.all.hz
#
#    int / counter / byte        - disk.all.read_bytes
#    int / counter / count       - kernel.all.sysfork
#    int / counter / sec         - kernel.percpu.cpu.user
#
#    float / instant / none      - kernel.all.load
#
#    float / discrete / none     - hinv.cpu.bogomips
#    float / discrete / count    - hinv.cpu.clock
#    float / discrete / b/s      - network.interface.speed
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$

if [ "$PCP_PLATFORM" != "linux" ]
then
    echo "$0: Error: requires Linux kernel metrics"
    exit 1
fi

rm -rf $tmp $here/rep.{0,meta,index}
trap "cd $here; rm -fr $tmp; exit" 0 1 2 3 15

cat <<End-of-File >> $tmp.rep.config
log mandatory on 1 sec {
	kernel.uname.sysname
	hinv.machine

	kernel.all.lastpid
	mem.util.used
	network.tcpconn.close
	kernel.all.uptime

	hinv.ncpu
	mem.physmem
	hinv.nfilesys
	proc.psinfo.start_time
	network.interface.baudrate
	kernel.all.hz

	disk.all.read_bytes
	kernel.all.sysfork
	kernel.percpu.cpu.user

	kernel.all.load

	hinv.cpu.bogomips
	hinv.cpu.clock
	network.interface.speed
}
End-of-File

${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.rep.config $here/rep

exit
