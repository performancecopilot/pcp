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

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -fr $tmp; exit \$status" 0 1 2 3 15

# usage - print out the usage of program
#
_usage()
{
    echo >$tmp/msg "Usage: $prog [options] [pmgadgets options]"
    echo >>$tmp/msg '
options:
  -e                  use espping metrics, instead of shping
  -G gap              set distance between columns (pixels)
  -V                  verbose/diagnostic output

pmgadgets(1) options:'
    _pmgadgets_usage >>$tmp/msg
    echo >>$tmp/msg
    _pmgadgets_info -f $tmp/msg
}

# keep the original command line for restart after logout/login
# 
echo -n "pmgadgets 1 \"pmgshping\"" >$tmp/conf
for arg
do
    echo -n " \"$arg\"" >>$tmp/conf
done
echo "" >>$tmp/conf

# get arguments
#
. $PCP_SHARE_DIR/lib/pmgadgets-args

# default variables
#
gap=""
ping=shping
verbose=false

_pmgadgets_args "$@"

if [ -n "$otherArgs" ]
then
    while getopts "eG:V?" c $otherArgs
    do
	case $c
	in
	    e)
		ping=espping
		;;
	    G)
		gap=$OPTARG
		;;
	    V)
		verbose=true
		;;
	    ?)
		_usage
		status=1
		exit
		;;
	esac
    done

    set -- $otherArgs
    shift `expr $OPTIND - 1`

    if [ $# -gt 0 ]
    then
        _usage
	status=1
	exit
    fi
fi

# check on metric availability
#
if _pmgadgets_fetch_indom ${ping}.time.real
then
    if [ ! -s $tmp/pmgadgets_result -o "$number" -lt 1 ]
    then
	_pmgadgets_fetch_fail "get $ping metrics"
	#NOTREACHED
    fi
    # save number of instances of ${ping}.time.real to be used below
    #
    mynumber=$number
else
    _pmgadgets_fetch_fail "get $ping metrics"
    #NOTREACHED
fi

cp $tmp/pmgadgets_result $tmp/info

# default is update at the same frequency as the $ping PMDA
# runs the commands
#
if [ $interval = 0 ]
then
    result=''
    if _pmgadgets_fetch_values ${ping}.control.cycletime
    then
     	if [ ! -s $tmp/pmgadgets_result -o "$number" -lt 1 ]
        then
	    _pmgadgets_fetch_fail "get $ping cycle time"
	    #NOTREACHED
     	fi
     else
    	_pmgadgets_fetch_fail "get $ping cycle time"
    	#NOTREACHED
     fi

    interval=`cat $tmp/pmgadgets_result`
    if [ -z "$interval" ]
    then
	# if you can't get this, probably doomed, but use 10sec as a fallback
	#
	interval=10sec
    else
	interval="${interval}sec"
    fi
fi
args="$args -t $interval"

# $ping PMDA may not be installed here ... fake out $ping.RealTime
# config file for pmchart and use stacked bar instead of line plot
# because the time interval may be quite long
pmchart_config=$tmp/config
cat >$tmp/config <<End-of-File
#pmchart
Version 2.0 host dynamic
Chart Title "Elapsed Time for $ping Commands" Style stacking
	Plot Color #-cycle Host * Metric $ping.time.real Matching .*
End-of-File

# output the config file
#
start_x=8	# starting x co-ord
start_y=8	# starting y co-ord
diam=10		# LED diameter
if [ -z "$gap" ]
then
    gap=50	# column width
    [ "$ping" = "espping" ] && gap=150	# pmdaespping instance names are long
fi

pmchart_opts="-t $interval -c $pmchart_config"
[ ! -z "$host" ] && pmchart_opts="$pmchart_opts -h $host"
[ ! -z "$namespace" ] && pmchart_opts="$pmchart_opts $namespace"

cat <<End-of-File >>$tmp/conf
_actions Actions (
"pmchart"           "/usr/bin/X11/xconfirm -icon info -t 'Values plotted by pmchart will not appear' -t 'until after the first time interval ($interval)' -B Dismiss >/dev/null & pmchart $pmchart_opts"
)

_legend Status (
    _default	"#608080"
    1		yellow
    2		orange
    3		red
    4		purple
)

_legend Response (
    _default	"#608080"
    0		green3
    1000	yellow
    5000	orange
    10000	red
)
End-of-File

# get instance information from metrics
#
for tag in `cat $tmp/info`
do
    echo "$tag"
done \
| $PCP_AWK_PROG -v ping=$ping >>$tmp/conf '
BEGIN	{ x = '$start_x'; y = '$start_y'; diam = '$diam'; gap = '$gap'; left = 1
	  printf "_label %d %d \"%s\"\n",x,y+8,"'$host'"
	  y += 12
	  printf "_label %d %d \"%s\"\n",x,y+8,"S"
	  x = x + diam + 2
	  printf "_label %d %d \"%s\"\n",x,y+8,"Response"
	  if ('$mynumber' > 1) {
	      x = x + diam + 2
	      x = x + gap
	      printf "_label %d %d \"%s\"\n",x,y+8,"S"
	      x = x + diam + 2
	      printf "_label %d %d \"%s\"\n",x,y+8,"Response"
	  }
	  y += 12
	}
	{
	  if (left) {
	    print ""
	    x = '$start_x'
	  }
	  else
	    x = x + gap
  	  print "_led " x " " y " " diam " " diam
	  print "  _metric " ping".status[\"" $1 "\"]"
	  print "  _legend Status"
	  x = x + diam + 2
	  print "_led " x " " y " " diam " " diam
	  print "  _metric " ping ".time.real[\"" $1 "\"]"
	  print "  _legend Response"
	  print "  _actions Actions"
	  x = x + diam + 2
	  printf "_label %d %d \"%s\"\n",x,y+diam-2,$1
	  if (!left) {
	      y = y + diam + 2
	  }
	  left = 1-left
	}'

$verbose && cat $tmp/conf
eval pmgadgets <$tmp/conf $args

status=$?
exit
