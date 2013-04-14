#!/bin/sh
#
# Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
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
# Compare two PCP archives and report significant differences
#
tmp=`mktemp -d /var/tmp/pcp.XXXXXXXXX` || exit 1
sts=0
trap "rm -rf $tmp; exit \$sts" 0 1 2 3 15

_usage()
{
    echo "Usage: $0 [options] archive1 [archive2]"
    echo
    echo "Options:"
    echo "  -d         debug, keep temp files"
    echo "  -p digits  number of digits to display after the decimal point"
    echo "  -q thres   change interesting threshold to be > thres or < 1/thres"
    echo "             [default 2]"
    echo "  -S start   start time, see PCPIntro(1)"
    echo "  -T end     end time, see PCPIntro(1)"
    echo "  -B start   start time, second archive (optional)"
    echo "  -E end     end time, second archive (optional)"
    echo "  -x metric  egrep(1) pattern of metric(s) to be excluded"
    echo "  -X file    file containing egrep(1) patterns to exclude"
    echo "  -z         use local timezone, see PCPIntro(1)"
    echo "  -Z zone    set reporting timezone"
    sts=1
    exit $sts
}

_fix()
{
    sed -e 's/  *\([0-9][0-9.]*\)\([^"]*\)$/|\1/' \
    | egrep -v -f $tmp/exclude \
    | sort -t\| -k1,1
}

cat <<'End-of-File' >$tmp/exclude
^pmcd.pmlogger.port 
End-of-File

thres=2
opts=""
start1=""
start2=""
finish1=""
finish2=""
precision=3
while getopts dp:q:S:T:B:E:x:X:zZ:? c
do
    case $c
    in
	d)
	    trap "exit \$sts" 0 1 2 3 15
	    otmp="$tmp"
	    tmp=`pwd`/tmp
	    mkdir $tmp || exit 1
	    mv $otmp/exclude $tmp/exclude
	    rmdir $otmp
	    ;;
	p)
	    precision="$OPTARG"
	    opts="$opts -p $precision"
	    ;;
	q)
	    thres="$OPTARG"
	    ;;
	S)
	    start1="$OPTARG"
	    ;;
	T)
	    finish1="$OPTARG"
	    ;;
	B)
	    start2="$OPTARG"
	    ;;
	E)
	    finish2="$OPTARG"
	    ;;
	x)
	    echo "$OPTARG" >>$tmp/exclude
	    ;;
	X)
	    cat "$OPTARG" >>$tmp/exclude
	    ;;
	z)
	    opts="$opts -z"
	    ;;
	Z)
	    opts="$opts -Z $OPTARG"
	    ;;
	?)
	    _usage
	    # NOTREACHED
	    ;;
    esac
done
shift `expr $OPTIND - 1`

if [ $# -lt 2 ]
then
    _usage
    # NOTREACHED
fi

echo "Directory: `pwd`"
echo "Excluded metrics:"
sed -e 's/^/    /' <$tmp/exclude
echo

options="$opts"
if [ "X$start1" != X ]; then
    options="$options -S $start1"
fi
if [ "X$finish1" != X ]; then
    options="$options -T $finish1"
fi
pmlogsummary -N $options $1 2>$tmp/err | _fix >$tmp/1
if [ -s $tmp/err ]
then
    echo "Warnings from pmlogsummary ... $1"
    cat $tmp/err
    echo
fi

options="$opts"
if [ "X$start2" != X ]; then
    options="$options -S $start2"
elif [ "X$start1" != X ]; then
    options="$options -S $start1"
fi
if [ "X$finish2" != X ]; then
    options="$options -T $finish2"
elif [ "X$finish1" != X ]; then
    options="$options -T $finish1"
fi
pmlogsummary -N $options $2 2>$tmp/err | _fix >$tmp/2
if [ -s $tmp/err ]
then
    echo "Warnings from pmlogsummary ... $2"
    cat $tmp/err
    echo
fi

join -t\| -v 2 $tmp/1 $tmp/2 >$tmp/tmp
if [ -s $tmp/tmp ]
then
    echo "Missing from $1 (not compared) ..."
    sed <$tmp/tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

join -t\| -v 1 $tmp/1 $tmp/2 >$tmp/tmp
if [ -s $tmp/tmp ]
then
    echo "Missing from $2 (not compared) ..."
    sed <$tmp/tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

a1=`basename "$1"`
a2=`basename "$2"`
echo "$thres" | awk '
    { printf "Ratio Threshold: > %.2f or < %.3f\n",'"$thres"',1/'"$thres"'
      printf "%12s %12s   Ratio  Metric-Instance\n","'"$a1"'","'"$a2"'" }'
join -t\| $tmp/1 $tmp/2 \
| awk -F\| '
function doval(v)
{
    precision='"$precision"'
    if (precision < 3 || precision > 12)
	precision=3
    extra=precision-3
    if (v > 99999999)
	printf "%*.*f%*s",12+extra,0,v,1," "
    else if (v > 999)
	printf "%*.*f%*s",8,0,v,2+precision," "
    else if (v > 99)
	printf "%*.*f%*s",10+extra,1+extra,v,3," "
    else if (v > 9)
	printf "%*.*f%*s",11+extra,2+extra,v,2," "
    else
	printf "%*.*f%*s",12+extra,precision,v,1," "
}
$3+0 == 0 || $2+0 == 0 {
		if ($3 == $2)
		    next
		doval($2)
		doval($3)
		printf "   "
		if ($3+0 == 0)
		    printf "|-|   %s\n",$1
		else if ($2+0 == 0)
		    printf "|+|   %s\n",$1
		next
}
$2 / $3 > '"$thres"' || $3 / $2 > '"$thres"'	{
		doval($2)
		doval($3)
		printf " "
		r = $3/$2
		if (r < 0.001)
		    printf " 0.001- "
		else if (r < 0.01)
		    printf "%6.3f ",r
		else if (r > 100)
		    printf " 100+  "
		else
		    printf "%5.2f  ",r
		printf " %s\n",$1
	}' \
| sort -nr -k 3,3

