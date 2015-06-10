#! /bin/sh
# 
# Copyright (c) 2013-2015 Red Hat.
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

. $PCP_DIR/etc/pcp.env

sts=2
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$sts" 0 1 2 3 15

progname=`basename $0`
for var in Aflag aflag Dflag gflag hflag Lflag nflag Oflag pflag Sflag sflag Tflag tflag Zflag zflag
do
    eval $var=false
done

# usage spec for pmgetopt, note posix flag (commands mean no reordering)
cat > $tmp/usage << EOF
# getopts: A:a:D:gh:Ln:O:p:PS:s:T:t:Z:z?
# Usage: [options] [[...] command [...]]

Summary Options:
   --archive
   --host
   --origin
   -P,--pmie     display pmie evaluation statistics
   --help

Command Options:
   --align
   --archive
   --debug
   --guimode
   --host
   --namespace
   --origin
   --guiport
   --start
   --samples
   --finish
   --interval
   --timezone
   --hostzone
# end
EOF

_usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2

    ls $PCP_BINADM_DIR/pcp-* $HOME/.pcp/bin/pcp-* 2>/dev/null | \
    while read command
    do
	[ -x "$command" ] || continue
	basename "$command" | sed -e 's/^pcp-//g' >> $tmp/cmds
    done

    if [ ! -z "$@" -a -s "$tmp/cmds" ]
    then
	echo >> $tmp/usage
	echo "Please install pcp system tools package"
    else
	echo >> $tmp/usage
	( $PCP_ECHO_PROG $PCP_ECHO_N "Available Commands:     ""$PCP_ECHO_C" \
		&& sort -u < $tmp/cmds ) | _fmt >> $tmp/usage
	pmgetopt --progname=$progname --usage --config=$tmp/usage
    fi
    exit 1
}

_fmt()
{
    if [ "$PCP_PLATFORM" = netbsd ]
    then
	fmt -g 64
    else
	fmt -w 64
    fi \
    | tr -d '\r' | tr -s '\n' | $PCP_AWK_PROG '
NR > 1	{ printf "           %s\n", $0; next }
	{ print }'
}

opts=""
ARGS=`pmgetopt --progname=$progname --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1" in
      -A)
	Aflag=true
	pcp_align_time="$2"
	shift
	;;
      -a)
	aflag=true
	pcp_archive="$2"
	shift
	;;
      -D)
	Dflag=true
	pcp_debug="$2"
	shift
	;;
      -g)
	gflag=true
	;;
      -h)
	hflag=true
	pcp_host="$2"
	shift
	;;
      -L)
	Lflag=true
	;;
      -n)
	nflag=true
	pcp_namespace="$2"
	shift
	;;
      -O)
	Oflag=true
	pcp_origin_time="$2"
	shift
	;;
      -P)
	opts="$opts -P"
	;;
      -p)
	pflag=true
	pcp_guiport="$2"
	shift
	;;
      -S)
	Sflag=true
	pcp_start_time="$2"
	shift
	;;
      -s)
	sflag=true
	pcp_samples="$2"
	shift
	;;
      -T)
	Tflag=true
	pcp_finish_time="$2"
	shift
	;;
      -t)
	tflag=true
	pcp_interval="$2"
	shift
	;;
      -Z)
	Zflag=true
	pcp_timezone="$2"
	shift
	;;
      -z)
	zflag=true
	;;
      -\?)
	_usage ""
	;;
      --)	# end of options, start of arguments
	shift
	break
	;;
    esac
    shift	# finished with this option now, next!
done

# Note - in all cases, pmGetOptions(3) will discover all of the standard
# arguments we've set above, automagically.
#
if [ $# -eq 0 ]
then
    # default to generating a PCP collector installation summmary
    command="summary"
else
    # seek out matching command and execute it with remaining arguments
    command=$1
    shift
fi

if [ -x "$HOME/.pcp/bin/pcp-$command" ]
then
    command="$HOME/.pcp/bin/pcp-$command"
elif [ -x "$PCP_BINADM_DIR/pcp-$command" ]
then
    command="$PCP_BINADM_DIR/pcp-$command"
else
    _usage "Cannot find a pcp-$command command to execute"
fi

$Aflag && export PCP_ALIGN_TIME="$pcp_align_time"
$aflag && export PCP_ARCHIVE="$pcp_archive"
$Dflag && export PCP_DEBUG="$pcp_debug"
$gflag && export PCP_GUIMODE=true
$hflag && export PCP_HOST="$pcp_host"
$Lflag && export PCP_LOCALMODE=true
$nflag && export PCP_NAMESPACE="$pcp_namespace"
$Oflag && export PCP_ORIGIN_TIME="$pcp_origin_time"
$pflag && export PCP_GUIPORT="$pcp_guiport"
$Sflag && export PCP_START_TIME="$pcp_start_time"
$sflag && export PCP_SAMPLES="$pcp_samples"
$Tflag && export PCP_FINISH_TIME="$pcp_finish_time"
$tflag && export PCP_INTERVAL="$pcp_interval"
$Zflag && export PCP_TIMEZONE="$pcp_timezone"
$zflag && export PCP_HOSTZONE=true

rm -rf $tmp	# cleanup now, no trap handler post-exec
exec $command $opts $@
