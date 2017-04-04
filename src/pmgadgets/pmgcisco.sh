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
  -t interval	sample interval [default 120.0 seconds]
  -V            verbose/diagnostic output
  -x pixels	width of each bar graph [default 31]
  -y pixels	height of each bar graph [default 31]

pmgadgets(1) options:'
    _pmgadgets_usage >>$tmp/msg
    echo >>$tmp/msg
    _pmgadgets_info -f $tmp/msg
}

# default variables
#
verbose=false
plot_y=2
bar_y=31
bar_x=31

# keep the original command line for restart after logout/login
#
echo -n "pmgadgets 1 \"pmgcisco\"" >$tmp/conf
for arg
do
    echo -n " \"$arg\"" >>$tmp/conf
done
echo "" >> $tmp/conf

# get arguments
#
. $PCP_SHARE_DIR/lib/pmgadgets-args

_pmgadgets_args "$@"
[ "$interval" = 0 ] && interval=120sec
args="$args -t $interval"

if [ -n "$otherArgs" ]
then
    while getopts "Vx:y:?" c $otherArgs
    do
	case $c
	in
	    V)
		verbose=true
		;;
	    x)	# -x pixels (width of bar graph)
		bar_x=$OPTARG
		;;
	    y)	# -y pixels (height of bar graph)
		bar_y=$OPTARG
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
if _pmgadgets_fetch_values cisco.bandwidth
then
    if [ ! -s $tmp/pmgadgets_result -o "$number" -lt 1 ]
    then
	_pmgadgets_fetch_fail "get Cisco metrics"
	#NOTREACHED
    fi
else
    _pmgadgets_fetch_fail "get Cisco metrics"
    #NOTREACHED
fi

# output the config file
#
pminfo $msource -f cisco.bandwidth \
| sed -n \
    -e 's/:/ /' \
    -e '/ value /{
s/"] value / /
s/.*"//
p
}' \
| sort >$tmp/data

# $tmp/data has lines of the form <hostname> <interface> <bandwidth>
# try to truncate the <hostname> by dropping components from the
# right hand end, but retain uniqueness ... the truncated name is used
# for the label gadget
#
sed -e 's/ .*//' <$tmp/data | sort -u >$tmp/hostnames
trim=1
rm -f $tmp/done
while true
do
    $PCP_AWK_PROG -F . <$tmp/hostnames >$tmp/tmp '
NR == 1		{ last_part=NF-'$trim'+1
		  if (last_part < 2) { fail=1; exit }
		  want=$last_part
		}
NF >= last_part	{ if ($last_part != want) { fail=2; exit }
		  next
		}
		{ fail=3; exit }
END		{ # print "fail=" fail
		  if (fail) print "" >"'$tmp/done'"
		}'
    [ -f $tmp/done ] && break
    trim=`expr $trim + 1`
done

$PCP_AWK_PROG -F . <$tmp/hostnames >$tmp/map '
    { printf "%s",$0
      for (i=1; i<=NF-'$trim'+1; i++) {
	if (i == 1) printf "\t%s",$i
	else printf ".%s",$i
      }
      print ""
    }'

# this produces <hostname> <shortname> <interface> <bandwidth>
#
join $tmp/map $tmp/data \
| $PCP_AWK_PROG >>$tmp/conf '

BEGIN	{ y = 15; step_y = '$bar_y'+31; step_x = '$bar_x'+16 }
	{ if (!seen[$4]) {
	    print "_legend link" $4 " ("
	    print " _default \"#608080\""
	    print " " int(0.10*$4) " green"
	    print " " int(0.70*$4) " yellow"
	    print " " int(0.80*$4) " orange"
	    print " " int(0.90*$4) " red"
	    print ")"
	    print ""
	    seen[$4] = 1
	  }

	  print "_label",5,y,"\"" $2 ":" $3 "\" \"7x13bold\""

	  print "_label",5,y+11,"\"in\" \"7x13\""
	  print "_led",5+'$bar_x'-6,y+4,8,8
	  print "_metric cisco.bytes_in[\"" $1 ":" $3 "\"]"
	  print "_legend link" $4

	  print "_bargraph",5,y+14,'$bar_x,$bar_y'
	  print "_metric cisco.bytes_in[\"" $1 ":" $3 "\"]"
	  print "_max " $4

	  print "_label",step_x+5,y+11,"\"out\" \"7x13\""
	  print "_led",step_x+5+'$bar_x'-6,y+4,8,8
	  print "_metric cisco.bytes_out[\"" $1 ":" $3 "\"]"
	  print "_legend link" $4

	  print "_bargraph",step_x+5,y+14,'$bar_x,$bar_y'
	  print "_metric cisco.bytes_out[\"" $1 ":" $3 "\"]"
	  print "_max " $4

	  print ""
	  y += step_y
	}
END	{ value='"`echo $interval | sed -e 's/[^0-9.].*/; scale=\"&\"/' | tr A-Z a-z`"'
	  # tt is total time for plot in seconds, each bar is 1 pixel wide
	  tt = value * ('$bar_x' - 1)
	  if (scale ~ /^min/ || scale == "m") tt *= 60
	  else if (scale ~ /^hour/ || scale == "h") tt *= 3600
	  else if (scale ~ /^day/ || scale == "d") tt *= 24*3600
	  printf "_label 5 %d \"History: ",y
	  if (tt > 24*3600) {
	    xxx = int(tt/24*3600)
	    printf "%dday",xxx
	    if (xxx > 1) printf "s"
	    rem = int(0.5+(tt%(24*3600)/3600))
	    if (rem > 1)
		printf " %dhours",rem
	    else if (rem == 1)
		printf " 1hour"
	  }
	  else if (tt > 3600) {
	    xxx = int(tt/3600)
	    printf "%dhour",xxx
	    if (xxx > 1) printf "s"
	    rem = int(0.5+(tt%(3600)/60))
	    if (rem > 1)
		printf " %dmin",rem
	    else if (rem == 1)
		printf " 1min"
	  }
	  else if (tt > 60) {
	    xxx = int(tt/60)
	    printf "%dmin",xxx
	    if (xxx > 1) printf "s"
	    rem = int(0.5+tt%60)
	    if (rem > 1)
		printf " %dsecs",rem
	    else if (rem == 1)
		printf " 1sec"
	  }
	  else {
	    if (tt > 1)
		printf "%dsecs",tt
	    else if (tt == 1)
		printf "1sec"
	  }
	  print "\""
	}'

$verbose && cat $tmp/conf
eval pmgadgets <$tmp/conf $args

status=$?
exit
