#!/bin/sh
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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
# Compare two PCP archives and report significant differences
#
tmp=/var/tmp/$$
sts=0
trap "rm -f $tmp.tmp; exit \$sts" 0 1 2 3 15
rm -f $tmp.*

_usage()
{
    echo "Usage: $0 [options] archive1 archive2"
    echo
    echo "Options:"
    echo "  -d         debug, keep temp files"
    echo "  -q thres   change interesting threshold to be > thres or < 1/thres"
    echo "             [default 2]"
    echo "  -S start   start time, see PCPIntro(1)"
    echo "  -T end     end time, see PCPIntro(1)"
    echo "  -x metric  egrep(1) pattern of metric(s) to be excluded"
    echo "  -z         use local timezone, see PCPIntro(1)"
    sts=1
    exit $sts
}

_fix()
{
    sed -e 's/  *\([0-9][0-9.]*\)\([^"]*\)$/|\1/' \
    | egrep -v -f $tmp.exclude \
    | sort -t\| -k1,1
}

cat <<'End-of-File' >$tmp.exclude
^pmcd.pmlogger.port 
End-of-File

thres=2
opts=""
while getopts dq:S:T:x:z? c
do
    case $c
    in
	d)
	    trap "exit \$sts" 0 1 2 3 15
	    otmp="$tmp"
	    tmp=`pwd`/tmp
	    mv $otmp.exclude $tmp.exclude
	    ;;
	q)
	    thres="$OPTARG"
	    ;;
	S)
	    opts="$opts -S $OPTARG"
	    ;;
	T)
	    opts="$opts -T $OPTARG"
	    ;;
	x)
	    echo "$OPTARG" >>$tmp.exclude
	    ;;
	z)
	    opts="$opts -z"
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
echo "PCP options:$opts"
echo "Excluded metrics:"
sed -e 's/^/    /' <$tmp.exclude
echo

pmlogsummary $opts $1 2>$tmp.err | _fix >$tmp.1
if [ -s $tmp.err ]
then
    echo "Warnings from pmlogsummary ... $1"
    cat $tmp.err
    echo
fi
pmlogsummary $opts $2 2>$tmp.err | _fix >$tmp.2
if [ -s $tmp.err ]
then
    echo "Warnings from pmlogsummary ... $2"
    cat $tmp.err
    echo
fi

join -t\| -v 2 $tmp.1 $tmp.2 >$tmp.tmp
if [ -s $tmp.tmp ]
then
    echo "Missing from $1 (not compared) ..."
    sed <$tmp.tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

join -t\| -v 1 $tmp.1 $tmp.2 >$tmp.tmp
if [ -s $tmp.tmp ]
then
    echo "Missing from $2 (not compared) ..."
    sed <$tmp.tmp -e 's/|.*//' -e 's/^/    /'
    echo
fi

echo "$thres" | awk '
    { printf "Ratio Threshold: > %.2f or < %.3f\n",'"$thres"',1/'"$thres"'
      printf "%12s %12s   Ratio  Metric-Instance\n",'"$1"','"$2"' }'
join -t\| $tmp.1 $tmp.2 \
| awk -F\| '
function doval(v)
{
    if (v > 99999999)
	printf "%12.0f ",v
    else if (v > 999)
	printf "%8.0f     ",v
    else if (v > 99)
	printf "%10.1f   ",v
    else if (v > 9)
	printf "%11.2f  ",v
    else
	printf "%12.3f ",v
}
$3+0 == 0			{ next }
$2+0 == 0			{ next }
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

