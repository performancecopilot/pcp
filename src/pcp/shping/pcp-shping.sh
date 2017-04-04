#! /bin/sh
# 
# Copyright (c) 2015 Red Hat.
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
# Displays stats from the shell ping Performance Co-Pilot domain agent.
#

. $PCP_DIR/etc/pcp.env

sts=2
progname=`basename $0`
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$sts" 0 1 2 3 15

_usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2
    pmgetopt --progname=$progname --usage --config=$tmp/usage
    exit 1
}

cat > $tmp/usage << EOF
# Usage: [options] tag...

Options:
   -l,--tags	report the list of shell-ping command tags
   -c,--status	report shell-ping service check control variables
   --help
# end
EOF

cflag=false
lflag=false
ARGS=`pmgetopt --progname=$progname --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1" in
      -c)
	cflag=true
	;;
      -l)
	lflag=true
	;;
      -\?)
	_usage ""
	;;
      --)	# end of options, start of arguments
	shift
	while [ $# -gt 0 ]
	do
	    echo \"$1\" >> $tmp/tags
	    shift
	done
	break
	;;
    esac
    shift	# finished with this option now, next!
done

pmprobe -I shping.cmd | grep -v 'Note: timezone' > $tmp/cmd 2> $tmp/err
if grep "^pmprobe:" $tmp/err > /dev/null 2>&1
then
    $PCP_ECHO_PROG $PCP_ECHO_N "$progname: ""$PCP_ECHO_C"
    sed < $tmp/err -e 's/^pmprobe: //g'
    sts=1
    exit
fi

set `cat $tmp/cmd`
if [ "$2" -eq "-12357" ]	# Unknown metric name
then
    $PCP_ECHO_PROG "$progname: pmdashping(1) is not installed"
    sts=1
    exit
elif [ "$2" -lt 0 ]		# Some other PMAPI error
then
    shift && shift
    $PCP_ECHO_PROG "$progname: $*"
    sts=1
    exit
elif [ "$2" -eq 0 ]		# No instances?
then
    $PCP_ECHO_PROG "$progname: no shell ping command instances"
    sts=1
    exit
fi
shift && shift		# skip over name and error/count

if $lflag
then
    $PCP_ECHO_PROG "$progname: $*"
    sts=0
    exit
elif $cflag
then
    $PCP_ECHO_PROG $PCP_ECHO_N "Duty cycles:       ""$PCP_ECHO_C"
    pmprobe -v shping.control.cycles | $PCP_AWK_PROG '{ print $3 }'
    $PCP_ECHO_PROG $PCP_ECHO_N "Refresh interval:  ""$PCP_ECHO_C"
    pmprobe -v shping.control.cycletime | $PCP_AWK_PROG '{ print $3, "secs" }'
    $PCP_ECHO_PROG $PCP_ECHO_N "Check timeout:     ""$PCP_ECHO_C"
    pmprobe -v shping.control.timeout | $PCP_AWK_PROG '{ print $3, "secs" }'
    $PCP_ECHO_PROG
    $PCP_ECHO_PROG "Tags and command lines:"
    pminfo -f shping.cmd | \
	sed -e '/^$/d' -e '/^shping/d' \
	    -e 's/"] value/]/' -e 's/^ *inst//' \
	    -e 's/[0-9][0-9]* or "//g'
    sts=0
    exit
fi

# positional args now hold shping metric instance names
while [ $# -gt 0 ]
do
    if [ -s $tmp/tags ]
    then
	if ! grep -q "^$1$" $tmp/tags
	then
	    shift
	    continue
	fi
    fi
    # build up a pmie configuration file
    cmd=`echo $1 | tr -d '"'`
    echo "'sts_$cmd' = shping.status #'$cmd';" >> $tmp/pmie
    echo "'time_$cmd' = shping.time.real #'$cmd';" >> $tmp/pmie
    shift
done

if [ ! -s $tmp/pmie ]
then
    $PCP_ECHO_PROG "$progname: no matching command tags found"
    sts=1
    exit
fi

cat $tmp/pmie | pmie -v -e -q | pmie2col -p 3 -w 10
sts=0
exit
