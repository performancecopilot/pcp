#!/bin/sh
#
# Copyright (c) 2014 Red Hat.
# Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

# This script uses pmgsys to layout gadgets for a series of hosts and then
# combines the resulting config files into one big one and invokes pmgadgets
# on the result.

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -fr $tmp; exit \$status" 0 1 2 3 15
verbose="false"
prog=`basename $0`
nhosts=0
hosts=""

# usage - print out the usage of program
#
_usage()
{
    echo >$tmp/msg "Usage: $prog [options] [pmgadgets options] [host ...]"
    echo >>$tmp/msg '

Default hosts are specified in /etc/nodes (or /etc/ace/nodes).

options:
  -H nodes               file specifying nodes in cluster
		         [default $PCP_CLUSTER_CONFIG or /etc/nodes]
  -r rows                number of rows for layout
  -l                     suppress host name labels
  -L label               title label for pmgagdets layout
  -V                     verbose (print pmgadgets configuration)

pmgadgets(1) options:'
    _pmgadgets_usage | sed -e '/.*-h host.*/d' >>$tmp/msg
    _pmgadgets_info -f $tmp/msg
}

# keep the original command line for restart after logout/login
#
echo -n "pmgadgets 1 \"pmgcluster\"" >$tmp/conf
for arg
do
    echo -n " \"$arg\"" >>$tmp/conf
done
echo >>$tmp/conf

# get arguments
#
. $PCP_SHARE_DIR/lib/pmgadgets-args


# Have to pre-parse the options for -L coz getopts can't handle multiword
# strings when using $otherArgs
#
if [ -z "$PCP_CLUSTER_CONFIG" ]
then
    nodesfile=/etc/nodes
    [ ! -f "$nodesfile" ] && nodesfile=/etc/ace/nodes
else
    nodesfile="$PCP_CLUSTER_CONFIG"
    if [ ! -f "$nodesfile" ]
    then
    	echo "Error: \"$nodesfile\" specified in \$PCP_CLUSTER_CONFIG: file not found" 
	_usage
	status=1
	exit
    fi
fi

while [ $# -gt 0 ]
do
    case $1
    in
        -h) hosts="$2"
	    nhosts=1
            shift
	    ;;
        -L) titlelabel="$2"
            shift
	    ;;
        *)  pmgargs="$pmgargs $1"
            ;;
    esac
    shift
done
set -- $pmgargs

_pmgadgets_args "$@"

if [ -n "$otherArgs" ]
then
    while getopts "H:r:lV?" c $otherArgs
    do
	case $c
	in
	    H)  nodesfile=$OPTARG
		if [ ! -f "$nodesfile" ]
		then
		    echo "$prog Error: \"$nodesfile\" for -H file not found"
		    _usage
		    status=1
		    exit
		fi
		;;
	    r)  rows=$OPTARG
		;;
	    l)  pmgsysargs="$pmgsysargs -l"
		;;
	    V)  verbose="true"
		;;
	    ?)  _usage
		status=1
		exit
		;;
	esac
    done

    set -- $otherArgs
    shift `expr $OPTIND - 1`
else
    set --
fi

if [ "$interval" != "0" ]
then
    args="$args -t $interval"
fi

if [ ! -z "$titleArg" ]
then
    args="$args -title $titleArg"
fi

ytitle=0
if [ ! -z "$titlelabel" ]
then
    ytitle=16
    echo "_label 6 13 \"$titlelabel\" \"7x13bold\"" >> $tmp/conf
fi

if [ "$nhosts" -eq 0 ]
then
    nhosts=$#
    if [ $nhosts -eq 0 ]
    then
	if [ -f "$nodesfile" ]
	then
	    hosts=`sed -e 's/[# ].*$//' $nodesfile`
	    nhosts=`echo $hosts | wc -w`
	fi
    else
	hosts=$*
    fi
fi

if [ $nhosts -eq 0 ]
then
    _usage
    status=1
    exit
fi


ox=0
oy=$ytitle
row=0
col=0
ygap=20
xgap=50
xmax=0
ymax=0
hostnum=0

if [ -z "$rows" ]
then
    rows=`echo "sqrt($nhosts)" | bc`
fi

cols=`expr $nhosts / $rows`

for host in $hosts
do
    if ! pmgsys $pmgsysargs -C -V -h $host > $tmp/pmgsys 2> $tmp/pmgsys.err
    then
	sed -e "s/pmgsys/$prog/g" $tmp/pmgsys.err >&2
	continue
    fi
    
    sed -e 's/^pmgadgets/# pmgadgets/' \
	-e "/.*_update.*/d" \
	-e "s/-C//g" \
    	-e "s/cpuActions/"$hostnum"_cpuActions/g" \
	-e "s/loadActions/"$hostnum"_loadActions/g" \
    	-e "s/netActions/"$hostnum"_netActions/g" \
    	-e "s/diskActions/"$hostnum"_diskActions/g" \
    	-e "s/diskLegend/"$hostnum"_diskLegend/g" \
	-e "s/cpuColours/"$hostnum"_cpuColours/g" \
	-e "s/netColours/"$hostnum"_netColours/g" \
	-e "s/kernel\./"$host":kernel./g" \
	-e "s/disk\./"$host":disk./g" \
	-e "s/mem\./"$host":mem./g" \
	-e "s/swap\./"$host":swap./g" \
	-e "s/network\./"$host":network./g" \
	$tmp/pmgsys \
    | $PCP_AWK_PROG '
    /^_/ && $2 ~ /^[0-9]+$/ && $3 ~ /^[0-9]+$/ {
	printf "%s %d %d ", $1, $2 + '$ox', $3 + '$oy'
	for (i=4; i <= NF; i++)
	    printf "%s ", $i
	printf "\n"
	next
    }
    {
	print
    }' | tee $tmp/$host >> $tmp/conf

    #
    # set xmax and ymax for this host
    #
    eval `$PCP_AWK_PROG '
    /^_/ && $2 ~ /^[0-9]+$/ && $3 ~ /^[0-9]+$/ {
	if (xmax < $2)
	    xmax = $2
	if (ymax < $3)
	    ymax = $3
    }
    END {
	printf "host_xmax=%d; ymax=%d", xmax, ymax
    }' $tmp/$host`

    [ $host_xmax -gt $xmax ] && xmax=$host_xmax
    	
    row=`expr $row + 1`
    if [ $row -eq $rows ]
    then
    	row=0
	oy=$ytitle
	ox=`expr $xmax + $xgap`
	col=`expr $col + 1`
    else
	oy=`expr $ymax + $ygap`
    fi

    rm -f $tmp/$host
    hostnum=`expr $hostnum + 1`
done

rm -f $tmp/pmgsys $tmp/pmgsys.err

if $verbose
then
    cat $tmp/conf
fi

if [ $hostnum -le 0 ]
then
    echo "$prog: unable to monitor any hosts" >&2
    status=1
    exit
fi

eval pmgadgets $args <$tmp/conf 

status=$?
exit
