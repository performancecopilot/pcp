#!/bin/sh
#
# pmlogconf - generate/edit a pmlogger configuration file
#
# control lines have this format
# #+ tag:on-off:delta
# where
#	tag	matches [A-Z][0-9a-z] and is unique
#	on-off	y or n to enable or disable this group
#	delta	delta argument for pmlogger "logging ... on delta" clause
#	
#
# Copyright (c) 1998,2003 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com

# Get standard environment
. /etc/pcp.env

prog=`basename $0`

_usage()
{
    echo "Usage: $prog [-q] configfile"
    exit 1
}

quick=false
pat=''
prompt=true

while getopts q? c
do
    case $c
    in
	q)	# "quick" mode, don't change logging intervals
		#
		quick=true
		;;

	?)	# eh?
		_usage
		# NOTREACHED
		;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -ne 1 ]
then
    _usage
    # NOTREACHED
fi

config=$1

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit" 0 1 2 3 15
rm -f $tmp.*

# split $tmp.ctl at the line containing the unprocessed tag
#
_split()
{
    rm -f $tmp.head $tmp.tag $tmp.tail
    $PCP_AWK_PROG <$tmp.ctl '
BEGIN						{ out = "'"$tmp.head"'" }
seen == 0 && /^#\? [A-Z][0-9a-z]:[yn]:/		{ print >"'"$tmp.tag"'"
						  out = "'"$tmp.tail"'"
						  seen = 1
						  next
						}
						{ print >out }'
}

# do all of the real iterative work
#
_update()
{
    # strip the existing pmlogger config and leave the comments
    # and the control lines
    #

    $PCP_AWK_PROG <$tmp.in '
/DO NOT UPDATE THE FILE ABOVE/	{ tail = 1 }
tail == 1			{ print; next }
/^\#\+ [A-Z][0-9a-z]:[yn]:/	{ print; skip = 1; next }
skip == 1 && /^\#----/		{ skip = 0; next }
skip == 1			{ next }
				{ print }' \
    | sed -e '/^#+ [A-Z][0-9a-z]:[yn]:/s/+/?/' >$tmp.ctl

    # now need to be a little smarter ... tags may have appeared or
    # disappeared from the shipped defaults, so need to munge the contents
    # of $tmp.ctl to reflect this
    #
    last_tag=''
    sed -n -e '/^#+ [A-Z][0-9a-z]:[yn]:/p' $tmp.skel \
    | while read line
    do
	tag=`echo "$line" | sed -e 's/^#+ \([A-Z][0-9a-z]\):.*/\1/'`
	if grep "^#? $tag:" $tmp.ctl >/dev/null
	then
	    :
	else
	    #DEBUG# echo "need to add tag=$tag after last_tag=$last_tag"
	    rm -f $tmp.pre $tmp.post
	    if [ -z "$last_tag" ]
	    then
		$PCP_AWK_PROG <$tmp.ctl '
BEGIN						{ out = "'"$tmp.pre"'" }
						{ print >out }
NF == 0						{ out = "'"$tmp.post"'" }'
	    else
		$PCP_AWK_PROG <$tmp.ctl '
BEGIN						{ out = "'"$tmp.pre"'" }
						{ print >out }
/^#\? '"$last_tag"':/				{ out = "'"$tmp.post"'" }'
	    fi
	    mv $tmp.pre $tmp.ctl
	    echo "$line" | sed -e 's/+/?/' >>$tmp.ctl
	    cat $tmp.post >>$tmp.ctl
	fi
	last_tag="$tag"
    done

    while true
    do
	_split
	[ ! -s $tmp.tag ] && break
	eval `sed <$tmp.tag -e 's/^#? /tag="/' -e 's/:/" onoff="/' -e 's/:/" delta="/' -e 's/:.*/"/'`
	[ -z "$delta" ] && delta=default

	case $onoff
	in
	    y|n)	;;
	    *)	echo "Warning: tag=$tag onoff is illegal ($onoff) ... setting to \"n\""
		    onoff=n
		    ;;
	esac

	desc="Unknown group, tag \"$tag\""
	metrics=""
	metrics_a=""
	case $tag
	in

	    I0)	desc="hardware configuration [nodevis, osvis, oview, routervis, pmchart:Overview]"
		    metrics="hinv"
		    ;;

	    D0)	desc="activity (IOPs and bytes for both reads and writes) over all disks [osvis, pmkstat, pmchart:Disk, pmchart:Overview]"
		    metrics="disk.all.read
			    disk.all.write
			    disk.all.total
			    disk.all.read_bytes
			    disk.all.write_bytes
			    disk.all.bytes
			    disk.all.avg_disk.active"
		    ;;

	    D1)	desc="per controller disk activity [pmchart:DiskCntrls]"
		    metrics="disk.ctl.read
			    disk.ctl.write
			    disk.ctl.total
			    disk.ctl.read_bytes
			    disk.ctl.write_bytes
			    disk.ctl.bytes"
		    ;;

	    D2)	desc="per spindle disk activity [dkvis, pmie:per_disk]"
		    metrics="disk.dev.read
			    disk.dev.write
			    disk.dev.total
			    disk.dev.read_bytes
			    disk.dev.write_bytes
			    disk.dev.bytes
			    disk.dev.active"
		    ;;

	    D3)	desc="all available data per disk spindle"
		    metrics="disk.dev"
		    ;;

	    C0)	desc="utilization (usr, sys, idle, ...) over all CPUs [osvis, pmkstat, pmchart:CPU, pmchart:Overview, pmie:cpu]"
		    metrics="kernel.all.cpu.idle
			    kernel.all.cpu.nice
			    kernel.all.cpu.intr
			    kernel.all.cpu.sys
			    kernel.all.cpu.sxbrk
			    kernel.all.cpu.user
			    kernel.all.cpu.wait.total"
		    ;;

	    C2) desc="contributions to CPU wait time"
	            metrics="kernel.all.wait"
		    ;;

	    C1)	desc="utilization per CPU [clustervis, mpvis, nodevis, oview, pmie:cpu, pmie:per_cpu]"
		    metrics="kernel.percpu.cpu.idle
			    kernel.percpu.cpu.nice
			    kernel.percpu.cpu.intr
			    kernel.percpu.cpu.sys
			    kernel.percpu.cpu.sxbrk
			    kernel.percpu.cpu.user
			    kernel.percpu.cpu.wait.total"
		    ;;

	    C3) desc="per CPU contributions to wait time"
	            metrics="kernel.percpu.wait"
		    ;;

	    K0)	desc="load average and number of logins [osvis, pmkstat, pmchart:LoadAvg, pmchart:Overview, pmie:cpu]"
		    metrics="kernel.all.load
			    kernel.all.users"

		    ;;

	    K1)	desc="context switches, total syscalls and counts for selected calls (e.g. read, write, fork, exec, select) over all CPUs [pmkstat, pmchart:Syscalls, pmie:cpu]"
		    metrics="kernel.all.pswitch
			    kernel.all.syscall
			    kernel.all.sysexec
			    kernel.all.sysfork
			    kernel.all.sysread
			    kernel.all.syswrite"
		    metrics_a="kernel.all.kswitch
			    kernel.all.kpreempt
			    kernel.all.sysioctl"
		    ;;

	    K2)	desc="per CPU context switches, total syscalls and counts for selected calls [pmie:per_cpu]"
		    metrics="kernel.percpu.pswitch
			    kernel.percpu.syscall
			    kernel.percpu.sysexec
			    kernel.percpu.sysfork
			    kernel.percpu.sysread
			    kernel.percpu.syswrite"
		    metrics_a="kernel.percpu.kswitch
			    kernel.percpu.kpreempt
			    kernel.percpu.sysioctl"
		    ;;

	    K3)	desc="bytes across the read() and write() syscall interfaces"
		    metrics="kernel.all.readch kernel.all.writech"
		    ;;

	    K4)	desc="interrupts [pmkstat]"
		    metrics="kernel.all.intr.vme
			    kernel.all.intr.non_vme
			    kernel.all.tty.recvintr
			    kernel.all.tty.xmitintr
			    kernel.all.tty.mdmintr"
		    ;;

	    K5)	desc="buffer cache reads, writes, hits and misses [pmchart:BufferCache, pmie:filesys]"
		    metrics="kernel.all.io.bread
			    kernel.all.io.bwrite
			    kernel.all.io.lread
			    kernel.all.io.lwrite
			    kernel.all.io.phread
			    kernel.all.io.phwrite
			    kernel.all.io.wcancel"
		    ;;

	    K6)	desc="all available buffer cache data"
		    metrics="buffer_cache"
		    ;;

	    K7)	desc="vnode activity"
		    metrics="vnodes"
		    ;;

	    K8)	desc="name cache (namei, iget, etc) activity [pmchart:DNLC, pmie:filesys]"
		    metrics="kernel.all.io.iget
			    kernel.all.io.namei
			    kernel.all.io.dirblk
			    name_cache"
		    ;;

	    K9)	desc="asynchronous I/O activity"
		    metrics="kaio"
		    ;;

	    Ka)	desc="run and swap queues [pmkstat]"
		    metrics="kernel.all.runque
			    kernel.all.runocc
			    kernel.all.swap.swpque
			    kernel.all.swap.swpocc"
		    ;;

	    M0)	desc="pages in and out (severe VM demand) [pmkstat, pmchart:Paging]"
		    metrics="swap.pagesin
			    swap.pagesout"
		    ;;

	    M1)	desc="address translation (faults and TLB activity)"
		    metrics="mem.fault mem.tlb"
		    ;;

	    M2)	desc="kernel memory allocation [osvis, pmkstat, pmchart:Memory, pmchart:Overview]"
		    metrics="mem.system
			    mem.util
			    mem.freemem
			    mem.availsmem
			    mem.availrmem
			    mem.bufmem
			    mem.physmem
			    mem.dchunkpages
			    mem.pmapmem
			    mem.strmem
			    mem.chunkpages
			    mem.dpages
			    mem.emptymem
			    mem.freeswap
			    mem.halloc
			    mem.heapmem
			    mem.hfree
			    mem.hovhd
			    mem.hunused
			    mem.zfree
			    mem.zonemem
			    mem.zreq
			    mem.iclean
			    mem.bsdnet
			    mem.palloc
			    mem.unmodfl
			    mem.unmodsw
			    mem.paging.reclaim"
		    ;;

	    M3)	desc="current swap allocation and all swap activity [pmchart:Swap, pmie:memory]"
		    metrics="swap"
		    ;;

	    M4)	desc="swap configuration"
		    metrics="swapdev"
		    ;;

	    M5)	desc="\"large\" page and Origin node-based allocations and activity [nodevis, oview]"
		    metrics="mem.lpage"
		    metrics_a="origin.node"
		    ;;

	    M6)	desc="all NUMA stats"
		    metrics="origin.numa"
		    ;;

	    M7)	desc="NUMA migration stats [nodevis, oview]"
		    metrics="origin.numa.migr.intr.total"
		    ;;

	    N0)	desc="bytes and packets (in and out) and bandwidth per network interface [clustervis, osvis, pmchart:NetBytes, pmchart:Overview, pmie:per_netif]"
		    metrics="network.interface.in.bytes
			    network.interface.in.packets
			    network.interface.in.errors
			    network.interface.out.bytes
			    network.interface.out.packets
			    network.interface.out.errors
			    network.interface.total.bytes
			    network.interface.total.packets
			    network.interface.total.errors
			    network.interface.collisions
			    network.interface.baudrate"
		    ;;

	    N1)	desc="all available data per network interface"
		    metrics="network.interface"
		    ;;

	    N2)	desc="TCP bytes and packets (in and out), connects, accepts, drops and closes [pmchart:NetConnDrop, pmchart:NetPackets, pmie:network]"
		    metrics="network.tcp.accepts
			    network.tcp.connattempt
			    network.tcp.connects
			    network.tcp.drops
			    network.tcp.conndrops
			    network.tcp.timeoutdrop
			    network.tcp.closed
			    network.tcp.sndtotal
			    network.tcp.sndpack
			    network.tcp.sndbyte
			    network.tcp.rcvtotal
			    network.tcp.rcvpack
			    network.tcp.rcvbyte
			    network.tcp.rexmttimeo
			    network.tcp.sndrexmitpack"
		    ;;

	    N3)	desc="all available TCP data [pmchart:NetTCPCongestion]"
		    metrics="network.tcp"
		    ;;

	    N4)	desc="UDP packets in and out [pmchart:NetPackets]"
		    metrics="network.udp.ipackets network.udp.opackets"
		    ;;

	    N5)	desc="all available UDP data"
		    metrics="network.udp"
		    ;;

	    N6)	desc="socket stats (counts by type and state)"
		    metrics="network.socket"
		    ;;

	    N7)	desc="all available data for other protocols (IP, ICMP, IGMP)"
		    metrics="network.ip
			    network.icmp
			    network.igmp"
		    ;;

	    N8)	desc="mbuf stats (alloc, failed, waited, etc) [pmie:network]"
		    metrics="network.mbuf"
		    ;;

	    N9)	desc="multicast routing stats"
		    metrics="network.mcr"
		    ;;

	    Na)	desc="SVR5 streams activity"
		    metrics="resource.nstream_queue
			    resource.nstream_head"
		    ;;

	    S0)	desc="NFS2 stats [nfsvis, pmchart:NFS2]"
		    metrics="nfs"
		    ;;

	    S1)	desc="NFS3 stats [nfsvis, pmchart:NFS3]"
		    metrics="nfs3"
		    ;;

	    S2)	desc="RPC stats [pmie:rpc]"
		    metrics="rpc"
		    ;;

	    F0)	desc="Filesystem fullness [pmchart:FileSystem, pmie:filesys]"
		    metrics="filesys"
		    ;;

	    F1)	desc="XFS data and log traffic"
		    metrics="xfs.log_writes
			    xfs.log_blocks
			    xfs.log_noiclogs
			    xfs.read_bytes
			    xfs.write_bytes"
		    ;;

	    F2)	desc="all available XFS data"
		    metrics="xfs"
		    ;;

	    F3)	desc="XLV operations and bytes per volume [xlv_vis]"
		    metrics="xlv.read
			    xlv.write
			    xlv.read_bytes
			    xlv.write_bytes"
		    ;;

	    F4)	desc="XLV striped volume stats [xlv_vis]"
		    metrics="xlv.stripe_ops
			    xlv.stripe_units
			    xlv.aligned
			    xlv.unaligned
			    xlv.largest_io"
		    ;;

	    F5)	desc="EFS activity"
		    metrics="efs"
		    ;;

	    F6)	desc="XVM operations and bytes per volume"
		    metrics="xvm.ve.read
			    xvm.ve.write
			    xvm.ve.read_bytes
			    xvm.ve.write_bytes"
		    ;;

	    F7)	desc="XVM stripe, mirror and concat volume stats [pmie:xvm]"
		    metrics="xvm.ve.concat
			    xvm.ve.mirror
			    xvm.ve.stripe"
		    ;;

	    F8)	desc="all available XVM data"
		    metrics="xvm"
		    ;;

	    H0)	desc="NUMALink routers [nodevis, oview, routervis, pmchart:NUMALinks, pmie:craylink]"
		    metrics="hw.router"
		    ;;

	    H1)	desc="Origin hubs [pmie:craylink]"
		    metrics="hw.hub"
		    ;;

	    H2)	desc="global MIPS CPU event counters (enable first with ecadmin(1))"
		    metrics="hw.r10kevctr"
		    ;;

	    H3)	desc="XBOW activity [xbowvis]"
		    metrics="xbow"
		    ;;

	esac

	if [ ! -z "$pat" ]
	then
	    if echo "$desc" "$metrics" "$metrics_a" | grep "$pat" >/dev/null
	    then
		pat=''
		prompt=true
	    fi
	fi
	if $prompt
	then
	    # prompt for answers
	    #
	    echo
	    was_onoff=$onoff
	    echo "Group: $desc" | fmt -74 | sed -e '1!s/^/       /'
	    while true
	    do
		$PCP_ECHO_PROG $PCP_ECHO_N "Log this group? [$onoff] ""$PCP_ECHO_C"
		read ans
		if [ "$ans" = "?" ]
		then
		    echo 'Valid responses are:
m         report the names of the metrics in this group
n         do not log this group
q         quit; no change for this or any of the following groups
y         log this group
/pattern  no change for this group and search for a group containing pattern
	  in the description or the metrics associated with the group'
		    continue
		fi
		if [ "$ans" = m ]
		then
		    echo "Metrics in this group ($tag):"
		    echo $metrics $metrics_a \
		    | sed -e 's/[ 	][ 	]*/ /g' \
		    | tr ' ' '\012' \
		    | sed -e 's/^/    /' \
		    | sort
		    continue
		fi
		if [ "$ans" = q ]
		then
		    # quit ...
		    ans="$onoff"
		    prompt=false
		fi
		pat=`echo "$ans" | sed -n 's/^\///p'`
		if [ ! -z "$pat" ]
		then
		    echo "Searching for \"$pat\""
		    ans="$onoff"
		    prompt=false
		fi
		[ -z "$ans" ] && ans="$onoff"
		[ "$ans" = y -o "$ans" = n ] && break
		echo "Error: you must answer \"m\" or \"n\" or \"q\" or \"y\" or \"/pattern\" ... try again"
	    done
	    onoff="$ans"
	    if [ $prompt = true -a "$onoff" = y ]
	    then
		if $quick
		then
		    if [ $was_onoff = y ]
		    then
			# no change, be quiet
			:
		    else
			echo "Logging interval: $delta"
		    fi
		else
		    while true
		    do
			$PCP_ECHO_PROG $PCP_ECHO_N "Logging interval? [$delta] ""$PCP_ECHO_C"
			read ans
			if [ -z "$ans" ]
			then
			    # use suggested value, assume this is good
			    #
			    ans="$delta"
			    break
			else
			    # do some sanity checking ...
			    #
			    ok=`echo "$ans" \
			        | sed -e 's/^every //' \
				| awk '
/^once$/			{ print "true"; exit }
/^default$/			{ print "true"; exit }
/^[0-9][0-9]* *msec$/		{ print "true"; exit }
/^[0-9][0-9]* *msecs$/		{ print "true"; exit }
/^[0-9][0-9]* *millisecond$/	{ print "true"; exit }
/^[0-9][0-9]* *milliseconds$/	{ print "true"; exit }
/^[0-9][0-9]* *sec$/		{ print "true"; exit }
/^[0-9][0-9]* *secs$/		{ print "true"; exit }
/^[0-9][0-9]* *second$/		{ print "true"; exit }
/^[0-9][0-9]* *seconds$/	{ print "true"; exit }
/^[0-9][0-9]* *min$/		{ print "true"; exit }
/^[0-9][0-9]* *mins$/		{ print "true"; exit }
/^[0-9][0-9]* *minute$/		{ print "true"; exit }
/^[0-9][0-9]* *minutes$/	{ print "true"; exit }
/^[0-9][0-9]* *hour$/		{ print "true"; exit }
/^[0-9][0-9]* *hours$/		{ print "true"; exit }
				{ print "false"; exit }'`
			    if $ok
			    then
				delta="$ans"
				break
			    else

				echo "Error: logging interval must be of the form \"once\" or \"default\" or"
				echo "\"<integer> <scale>\", where <scale> is one of \"sec\", \"secs\", \"min\","
				echo "\"mins\", etc ... try again"
			    fi
			fi
		    done
		fi
	    fi
	else
	    $PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	fi

	echo "#+ $tag:$onoff:$delta:" >>$tmp.head
	echo "$desc" | fmt | sed -e 's/^/## /' >>$tmp.head
	if [ "$onoff" = y ]
	then
	    if [ ! -z "$metrics" ]
	    then
		echo "log advisory on $delta {" >>$tmp.head
		for m in $metrics
		do
		    echo "	$m" >>$tmp.head
		done
		echo "}" >>$tmp.head
	    fi
	    if [ ! -z "$metrics_a" ]
	    then
		echo "log advisory on $delta {" >>$tmp.head
		for m in $metrics_a
		do
		    echo "	$m" >>$tmp.head
		done
		echo "}" >>$tmp.head
	    fi
	fi
	echo "#----" >>$tmp.head
	cat $tmp.head $tmp.tail >$tmp.ctl

    done
}

# the current version of the skeletal control file
# see below for tag explanations
#
cat <<End-of-File >>$tmp.skel
#pmlogconf 1.0
# $prog control file version
#
# pmlogger(1) config file created and updated by
# $prog(1).
#
# DO NOT UPDATE THE INTITIAL SECTION OF THIS FILE.
# Any changes may be lost the next time $prog is used
# on this file.
#

# System configuration
#
#+ I0:y:once:
#----

# Disk activity
#
#+ D0:y:default:
#----
#+ D1:n:default:
#----
#+ D2:n:default:
#----
#+ D3:n:default:
#----

# CPU activity
#
#+ C0:y:default:
#----
#+ C2:n:default:
#----
#+ C1:n:default:
#----
#+ C3:n:default:
#----

# Kernel activity
#
#+ K0:y:default:
#----
#+ Ka:n:default:
#----
#+ K1:y:default:
#----
#+ K2:n:default:
#----
#+ K3:n:default:
#----
#+ K4:n:default:
#----
#+ K5:n:default:
#----
#+ K6:n:default:
#----
#+ K7:n:default:
#----
#+ K8:n:default:
#----
#+ K9:n:default:
#----

# Memory
#
#+ M0:y:default:
#----
#+ M1:n:default:
#----
#+ M2:n:default:
#----
#+ M3:n:default:
#----
#+ M4:n:default:
#----
#+ M5:n:default:
#----
#+ M7:n:default:
#----
#+ M6:n:default:
#----

# Network
#
#+ N0:y:default:
#----
#+ N1:n:default:
#----
#+ N2:n:default:
#----
#+ N3:n:default:
#----
#+ N4:n:default:
#----
#+ N5:n:default:
#----
#+ N6:n:default:
#----
#+ N7:n:default:
#----
#+ N8:n:default:
#----
#+ N9:n:default:
#----
#+ Na:n:default:
#----

# Services
#
#+ S0:n:default:
#----
#+ S1:n:default:
#----
#+ S2:n:default:
#----

# Filesystems and Volumes
#
#+ F0:n:default:
#----
#+ F1:y:default:
#----
#+ F2:n:default:
#----
#+ F3:n:default:
#----
#+ F4:n:default:
#----
#+ F6:n:default:
#----
#+ F7:n:default:
#----
#+ F8:n:default:
#----
#+ F5:n:default:
#----

# Hardware event counters
#
#+ H0:n:default:
#----
#+ H1:n:default:
#----
#+ H2:n:default:
#----
#+ H3:n:default:
#----

# DO NOT UPDATE THE FILE ABOVE THIS LINE
# Otherwise any changes may be lost the next time $prog is
# used on this file.
#
# It is safe to make additions from here on ...
#

End-of-File

if [ ! -f $config ]
then
    touch $config
    if [ ! -f $config ]
    then
	echo "$prog: Error: config file \"$config\" does not exist and cannot be created"
	exit 1
    fi

    $PCP_ECHO_PROG $PCP_ECHO_N "Creating config file \"$config\" using default settings ""$PCP_ECHO_C"
    prompt=false
    new=true
    touch $config
    cp $tmp.skel $tmp.in

else
    new=false
    magic=`sed 1q $config`
    if echo "$magic" | grep "^#pmlogconf" >/dev/null
    then
	version=`echo $magic | sed -e "s/^#pmlogconf//" -e 's/^  *//'`
	if [ "$version" = "1.0" ]
	then
	    :
	else
	    echo "$prog: Error: existing config file \"$config\" is wrong version ($version)"
	    exit 1
	fi
    else
	echo "$prog: Error: existing \"$config\" is not a $prog control file"
	exit 1
    fi
    if [ ! -w $config ]
    then
	echo "$prog: Error: existing config file \"$config\" is not writeable"
	exit 1
    fi

    # use as-is, but may have to re-map for some bogus tags that escaped in
    # earlier versions of the tool, e.g. K10 which should have been Ka (as
    # tags are restricted to 2 letters)
    #
    sed <$config >$tmp.in \
	-e '/^#+ K10/s/K10/Ka/'
fi

while true
do
    _update

    [ -z "$pat" ] && break

    echo " not found."
    while true
    do
	$PCP_ECHO_PROG $PCP_ECHO_N "Continue searching from start of the file? [y] ""$PCP_ECHO_C"
	read ans
	[ -z "$ans" ] && ans=y
	[ "$ans" = y -o "$ans" = n ] && break
	echo "Error: you must answer \"y\" or \"n\" ... try again"
    done
    mv $tmp.ctl $tmp.in
    if [ "$ans" = n ]
    then
	pat=''
	prompt=true
    else
	echo "Searching for \"$pat\""
    fi
done


if $new
then
    echo
    cp $tmp.ctl $config
else
    echo
    if diff $config $tmp.ctl >/dev/null
    then
	echo "No changes"
    else
	echo "Differences ..."
	${DIFF-diff} -c $config $tmp.ctl
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Keep changes? [y] ""$PCP_ECHO_C"
	    read ans
	    [ -z "$ans" ] && ans=y
	    [ "$ans" = y -o "$ans" = n ] && break
	    echo "Error: you must answer \"y\" or \"n\" ... try again"
	done
	[ "$ans" = y ] && cp $tmp.ctl $config
    fi
fi

exit 0
