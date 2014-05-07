#!/bin/sh
#
# Copyright (c) 2014 Red Hat.
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

# Get standard environment
. $PCP_DIR/etc/pcp.env

tmp=`mktemp -d /var/tmp/pcp.XXXXXXXXX` || exit 1
status=1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15
prog=`basename $0`

cat > $tmp/usage << EOF
# Usage: [options] archive1 [archive2]

Options:
  -d,--keep            debug, keep intermediate files
  -p=N,--precision=N   number of digits to display after the decimal point
  -q=N,--threshold=N   change interesting threshold to be > N or < 1/N [N=2]
  --start
  --finish
  -B=TIME,--begin=TIME  start time for second archive (optional)
  -E=TIME,--end=TIME    end time for second archive (optional)
  -x=REGEX              egrep(1) pattern of metric(s) to be excluded
  -X=FILE               file containing egrep(1) patterns to exclude
  --timezone
  --hostzone
  --help
EOF

_usage()
{
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    exit $status
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

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-d)	trap "exit \$status" 0 1 2 3 15
		otmp="$tmp"
		tmp=`pwd`/tmp
		[ -d "$tmp" ] || mkdir "$tmp" || exit 1
		mv $otmp/exclude $tmp/exclude
		rmdir $otmp
		;;
	-p)	precision="$2"
		shift
		opts="$opts -p $precision"
		;;
	-q)	thres="$2"
		shift
		;;
	-S)	start1="$2"
		shift
		;;
	-T)	finish1="$2"
		shift
		;;
	-B)	start2="$2"
		shift
		;;
	-E)	finish2="$2"
		shift
		;;
	-x)	echo "$2" >>$tmp/exclude
		shift
		;;
	-X)	cat "$2" >>$tmp/exclude
		shift
		;;
	-z)	opts="$opts -z"
		;;
	-Z)	opts="$opts -Z $2"
		shift
		;;
	--)	shift
		break
		;;
	-\?)	_usage
		# NOTREACHED
		;;
    esac
    shift
done

if [ $# -lt 1 -o $# -gt 2 ]
then
    _usage
    # NOTREACHED
elif [ $# -eq 2 ]
then
    arch1="$1"
    arch2="$2"
else
    arch1="$1"
    arch2="$1"
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
pmlogsummary -N $options $arch1 2>$tmp/err | _fix >$tmp/1
if [ -s $tmp/err ]
then
    echo "Warnings from pmlogsummary ... $arch1"
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
pmlogsummary -N $options $arch2 2>$tmp/err | _fix >$tmp/2
if [ -s $tmp/err ]
then
    echo "Warnings from pmlogsummary ... $arch2"
    cat $tmp/err
    echo
fi

if [ -z "$start1" ] 
then
    window1="start"
else
    window1="$start1"
fi
if [ -z "$finish1" ] 
then
    window1="$window1-end"
else
    window1="$window1-$finish1"
fi
if [ -z "$start2" ] 
then
    window2="start"
else
    window2="$start2"
fi
if [ -z "$finish2" ] 
then
    window2="$window2-end"
else
    window2="$window2-$finish2"
fi

join -t\| -v 2 $tmp/1 $tmp/2 >$tmp/tmp
if [ -s $tmp/tmp ]
then
    echo "Missing from $arch1 $window1 (not compared) ..."
    sed <$tmp/tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

join -t\| -v 1 $tmp/1 $tmp/2 >$tmp/tmp
if [ -s $tmp/tmp ]
then
    echo "Missing from $arch2 $window2 (not compared) ..."
    sed <$tmp/tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

a1=`basename "$arch1"`
a2=`basename "$arch2"`
echo "$thres" | awk '
    { printf "Ratio Threshold: > %.2f or < %.3f\n",'"$thres"',1/'"$thres"'
      printf "%15s %15s   Ratio  Metric-Instance\n","'"$a1"'","'"$a2"'" }'
if [ -z "$start1" ] 
then
    window1="start"
else
    window1="$start1"
fi
if [ -z "$finish1" ] 
then
    window1="$window1-end"
else
    window1="$window1-$finish1"
fi
if [ -z "$start2" ] 
then
    window2="start"
else
    window2="$start2"
fi
if [ -z "$finish2" ] 
then
    window2="$window2-end"
else
    window2="$window2-$finish2"
fi
printf '%15s %15s\n' "$window1" "$window2"
join -t\| $tmp/1 $tmp/2 \
| awk -F\| '
function doval(v)
{
    precision='"$precision"'
    if (precision < 3 || precision > 15)
	precision=3
    extra=precision-3
    if (v > 99999999)
	printf "%*.*f%*s",15+extra,0,v,1," "
    else if (v > 999)
	printf "%*.*f%*s",11,0,v,2+precision," "
    else if (v > 99)
	printf "%*.*f%*s",13+extra,1+extra,v,3," "
    else if (v > 9)
	printf "%*.*f%*s",14+extra,2+extra,v,2," "
    else
	printf "%*.*f%*s",15+extra,precision,v,1," "
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
		    printf " 0.001-"
		else if (r < 0.01)
		    printf "%6.3f ",r
		else if (r > 100)
		    printf " 100+  "
		else
		    printf "%5.2f  ",r
		printf " %s\n",$1
	}' \
| sort -nr -k 3,3 \
| sed -e 's/100+/>100/' -e 's/ 0.001-/<0.001 /'

status=0
exit
