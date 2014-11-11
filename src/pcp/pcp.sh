#! /bin/sh
# 
# Copyright (c) 2013-2014 Red Hat.
# Copyright (c) 1997,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# Displays the Performance Co-Pilot configuration for a host running the
# pmcd(1) daemon or from an archive created by pmlogger(1).
#

. $PCP_DIR/etc/pcp.env

sts=2
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$sts" 0 1 2 3 15

errors=0
progname=`basename $0`
pcp_host=`hostname` # may match pmcd.hostname
for var in unknown version build numagents numclients ncpu ndisk nnode nrouter nxbow ncell mem cputype uname timezone hostname services status
do
    eval $var="unknown?"
done
for var in Aflag aflag Dflag gflag hflag Lflag nflag Oflag Pflag pflag Sflag sflag Tflag tflag Zflag zflag
do
    eval $var=false
done

# metrics
metrics="pmcd.numagents pmcd.numclients pmcd.version pmcd.build pmcd.timezone pmcd.hostname pmcd.services pmcd.agent.status pmcd.pmlogger.archive pmcd.pmlogger.pmcd_host hinv.ncpu hinv.ndisk hinv.nnode hinv.nrouter hinv.nxbow hinv.ncell hinv.physmem hinv.cputype pmda.uname pmcd.pmie.pmcd_host pmcd.pmie.configfile pmcd.pmie.numrules pmcd.pmie.logfile"
pmiemetrics="pmcd.pmie.actions pmcd.pmie.eval.true pmcd.pmie.eval.false pmcd.pmie.eval.unknown pmcd.pmie.eval.expected"

# usage spec for pmgetopt, note posix flag (commands mean no reordering)
cat > $tmp/usage << EOF
# getopts: A:a:D:gh:Ln:O:p:PS:s:T:t:Z:z?
# Usage: [options] [[...] command [...]]

Summary Options:
   --archive
   --host
   --namespace
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

    echo >> $tmp/usage
    ( $PCP_ECHO_PROG $PCP_ECHO_N "Available Commands:     ""$PCP_ECHO_C" && \
    sort -u < $tmp/cmds ) | _fmt >> $tmp/usage
    pmgetopt --progname=$progname --usage --config=$tmp/usage
    exit 1
}

_plural()
{
    if [ "$1" = $unknown -o "$1" = 0 ]
    then
        echo ""
    elif [ "$1" = 1 ]
    then
    	echo " $1 $2,"
    else
        echo " $1 ${2}s,"
    fi
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
	opts="$opts -a $2"
	shift
	;;
      -D)
	Dflag=true
	pcp_debug="$2"
	opts="$opts -D $2"
	shift
	;;
      -g)
	gflag=true
	;;
      -h)
	hflag=true
	pcp_host="$2"
	opts="$opts -h $2"
	shift
	;;
      -L)
	Lflag=true
	;;
      -n)
	nflag=true
	pcp_namespace="$2"
	opts="$opts -n $2"
	shift
	;;
      -O)
	Oflag=true
	pcp_origin_time="$2"
	shift
	;;
      -P)
	Pflag=true
	metrics="$metrics $pmiemetrics"
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

if [ $# -gt 0 ]
then
    # pcp-command mode - seek out a matching command and execute it
    # with the remaining arguments - pmGetOptions(3) will discover
    # all of the standard arguments we've set above, automagically.
    #
    command=$1
    shift

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
    exec $command $@
fi

$hflag && $aflag && _usage "$progname: -a and -h mutually exclusive"

if $aflag
then
    eval `pmdumplog -Lz "$pcp_archive" 2>/dev/null | $PCP_AWK_PROG '
/^Performance metrics from host/	{  printf "pcp_host=%s\n", $5  }
/^Archive timezone: /			{  printf "timezone=%s\n", $3  }
/^  commencing/				{  tmp = substr($5, 7, 6)
					   sub(tmp, tmp+0.001, $5)
					   sub("commencing", "@")
					   printf "offset=\"%s\"\n", $0
					}'`
    [ "X$pcp_host" = X ] && pcp_host="unknown host"
    [ "X$offset" != X ] && opts="$opts -O '$offset' -z"
fi


if eval pminfo $opts -f $metrics > $tmp/metrics 2>$tmp/err
then
    :
else
    if grep "^pminfo:" $tmp/err > /dev/null 2>&1
    then
	$PCP_ECHO_PROG $PCP_ECHO_N "$progname: ""$PCP_ECHO_C"
	sed < $tmp/err -e 's/^pminfo: //g'
	sts=1
	exit
    fi
fi

[ -s $tmp/err ] && sed -e '/Unknown metric name/d' <$tmp/err >&2

eval `$PCP_AWK_PROG < $tmp/metrics -v out=$tmp '
BEGIN			{ mode = 0; count = 0; errors = 0; quote="" }

function quoted()
{
    if ($1 == "value") {
	printf "%s=", quote
	for (i = 2; i < NF; i++)
	    printf "%s ", $i
	printf "%s\n", $NF
    }
    else
	errors++
}

function inst()
{
    if (count == 0)
	file=sprintf("%s/%s", out, quote)
    if (NF == 0) {
	mode = 0
	printf "%s=%d\n", quote, count
    }
    else if ($1 == "inst") {
	count++
	id=substr($2, 2, length($2) - 1)
	value=$6
	if (mode == 2) {
	    agent=substr($4, 2, length($4) - 3)
	    printf "%s %s %s\n", id, agent, value > file
	}
	else
	    printf "%s %s\n", id, value > file
    }
    else {
	printf "%s=%d\n", quote, 0
	mode = 0
	errors++
    }
}

mode == 1		{ quoted(); mode = 0; next }
mode == 2		{ inst(); next }
mode == 3		{ inst(); next }
/pmcd.version/		{ mode = 1; quote="version"; next }
/pmcd.build/		{ mode = 1; quote="build"; next }
/pmcd.numagents/	{ mode = 1; quote="numagents"; next }
/pmcd.numclients/	{ mode = 1; quote="numclients"; next }
/pmcd.timezone/		{ mode = 1; quote="timezone"; next }
/pmcd.hostname/		{ mode = 1; quote="hostname"; next }
/pmcd.services/		{ mode = 1; quote="services"; next }
/pmcd.agent.status/	{ mode = 2; count = 0; quote="status"; next }
/pmcd.pmlogger.archive/	{ mode = 3; count = 0; quote="log_archive"; next }
/pmcd.pmlogger.pmcd_host/ { mode = 3; count = 0; quote="log_host"; next }
/pmcd.pmie.pmcd_host/	{ mode = 3; count = 0; quote="ie_host"; next }
/pmcd.pmie.logfile/	{ mode = 3; count = 0; quote="ie_log"; next }
/pmcd.pmie.configfile/	{ mode = 3; count = 0; quote="ie_config"; next }
/pmcd.pmie.numrules/	{ mode = 3; count = 0; quote="ie_numrules"; next }
/pmcd.pmie.actions/	{ mode = 3; count = 0; quote="ie_actions"; next }
/pmcd.pmie.eval.true/	{ mode = 3; count = 0; quote="ie_true"; next }
/pmcd.pmie.eval.false/	{ mode = 3; count = 0; quote="ie_false"; next }
/pmcd.pmie.eval.unknown/  { mode = 3; count = 0; quote="ie_unknown"; next }
/pmcd.pmie.eval.expected/ { mode = 3; count = 0; quote="ie_expected"; next }
/hinv.ncpu/		{ mode = 1; quote="ncpu"; next }
/hinv.ndisk/		{ mode = 1; quote="ndisk"; next }
/hinv.nnode/		{ mode = 1; quote="nnode"; next }
/hinv.nrouter/		{ mode = 1; quote="nrouter"; next }
/hinv.nxbow/		{ mode = 1; quote="nxbow"; next }
/hinv.ncell/		{ mode = 1; quote="ncell"; next }
/hinv.physmem/		{ mode = 1; quote="mem"; next }
/hinv.cputype/		{ mode = 3; count = 0; quote="cputype"; next }
/pmda.uname/	{ mode = 1; quote="uname"; next }
END			{ printf "errors=%d\n", errors }'`

numagents=`_plural $numagents agent`
ndisk=`_plural $ndisk disk`
nnode=`_plural $nnode node`
nrouter=`_plural $nrouter router`
nxbow=`_plural $nxbow xbow`
ncell=`_plural $ncell cell`

if [ -f $tmp/status ]
then
    agents=`$PCP_AWK_PROG < $tmp/status '
$3 == 0		{ printf "%s ",$2 }
$3 != 0		{ printf "%s[%d] ",$2,$3 }' | _fmt`
fi

if [ "$numclients" = $unknown ]
then
    numclients=""
else
    numclients=`expr $numclients - 1`
    numclients=`_plural $numclients client | tr -d ','`
    [ "$numclients" = "" ] && numagents=`echo "$numagents" | tr -d ','`
fi

if [ "$version" = $unknown ]
then
    version="Version unknown"
else
    version="Version $version"
    [ "$build" != $unknown ] && version="$version-$build"
fi

if [ "$mem" = $unknown -o "$mem" = 0 ]
then
    mem=""
else
    mem=" ${mem}MB RAM"
fi

if [ "$uname" = $unknown ]
then
    uname=""
else
    uname="$uname"
fi

[ "$services" = $unknown ] && services=""
[ "$timezone" = $unknown ] && timezone="Unknown"
[ "$hostname" = $unknown ] || pcp_host="$hostname"

if [ "$cputype" = $unknown ]
then
    cputype=""
elif [ -f $tmp/cputype ]
then
    cputype=`head -1 $tmp/cputype | sed -e 's/^.*"R/R/' -e 's/"$//g'`
else
    cputype=""
fi

ncpu=`_plural $ncpu "$cputype cpu"`

hardware="${ncpu}${ndisk}${nnode}${nrouter}${nxbow}${ncell}$mem"

if [ -f $tmp/log_archive -a -f $tmp/log_host ]
then
    sort $tmp/log_archive -o $tmp/log_archive
    sort $tmp/log_host -o $tmp/log_host

    # need \n\n here to force line breaks when piped into fmt later
    #
    numloggers=`join $tmp/log_host $tmp/log_archive | sort \
	| sed -e 's/"//g' | tee $tmp/log | $PCP_AWK_PROG '
BEGIN		{ count = 0 }
$1 == "0"	{ next }
$1 == "1"	{ next }
		{ count++ }
END		{ print count }'`

    $PCP_AWK_PROG < $tmp/log > $tmp/loggers '
BEGIN		{ primary=0 }
$1 == "0"	{ primary=$3; next }
$3 == primary	{ printf "primary logger: %s\n\n",$3; exit }'

    $PCP_AWK_PROG < $tmp/log >> $tmp/loggers '
BEGIN		{ primary=0 }
$1 == "0"	{ primary=$3; next }
$1 == "1"	{ next }
$3 == primary	{ next }
		{ printf "%s: %s\n\n",$2,$3 }'
else
    numloggers=0
fi

if [ -f $tmp/ie_host -a -f $tmp/ie_config -a -f $tmp/ie_log -a -f $tmp/ie_numrules ]
then
    sort $tmp/ie_log -o $tmp/ie_log
    sort $tmp/ie_host -o $tmp/ie_host
    sort $tmp/ie_config -o $tmp/ie_config
    sort $tmp/ie_numrules -o $tmp/ie_numrules
    if [ $Pflag = "true" ]; then
	numpmies=`join $tmp/ie_host $tmp/ie_config | join - $tmp/ie_numrules \
	    | sort -n | sed -e 's/"//g' | tee $tmp/pmie | wc -l | tr -d ' '`
    else
	numpmies=`join $tmp/ie_host $tmp/ie_log \
	    | sort -n | sed -e 's/"//g' | tee $tmp/pmie | wc -l | tr -d ' '`
    fi

    if [ $Pflag = "true" -a -f $tmp/ie_actions -a -f $tmp/ie_true -a \
	-f $tmp/ie_false -a -f $tmp/ie_unknown -a -f $tmp/ie_expected ]
    then
	sort $tmp/ie_actions -o $tmp/ie_actions
	sort $tmp/ie_true -o $tmp/ie_true
	sort $tmp/ie_false -o $tmp/ie_false
	sort $tmp/ie_unknown -o $tmp/ie_unknown
	sort $tmp/ie_expected -o $tmp/ie_expected
	sort $tmp/pmie -o $tmp/pmie
	join $tmp/pmie $tmp/ie_true | join - $tmp/ie_false \
		| join - $tmp/ie_unknown | join - $tmp/ie_actions \
		| join - $tmp/ie_expected > $tmp/tmp
	mv $tmp/tmp $tmp/pmie
    fi

    $PCP_AWK_PROG -v Pflag=$Pflag < $tmp/pmie '{
	if (Pflag == "true") {
	    printf "%s: %s (%u rules)\n\n",$2,$3,$4
	    printf "evaluations true=%u false=%u unknown=%u (actions=%u)\n\n",$5,$6,$7,$8
	    printf "expected evaluation rate=%.2f rules/sec\n\n",$9
	} else {
	    printf "%s: %s\n\n",$2,$3
	}
    }' > $tmp/pmies
else
    numpmies=0
fi

# finally, display everything we've found...
# 
echo "Performance Co-Pilot configuration on ${pcp_host}:"
echo
[ -n "$pcp_archive" ] && echo "  archive: $pcp_archive"
echo " platform: ${uname}"
echo " hardware: "`echo $hardware | _fmt`
echo " timezone: $timezone"
[ -n "$services" ] && echo " services: $services"

echo "     pmcd: ${version},${numagents}$numclients"

[ -n "$agents" ] && echo "     pmda: $agents"

if [ "$numloggers" != 0 ]
then
    $PCP_ECHO_PROG $PCP_ECHO_N " pmlogger: ""$PCP_ECHO_C"
    LC_COLLATE=POSIX sort < $tmp/loggers \
    | sed -e '/^$/d' | sed -e '1!s/^/           /'
fi

if [ "$numpmies" != 0 ]
then
    $PCP_ECHO_PROG $PCP_ECHO_N "     pmie: ""$PCP_ECHO_C"
    if [ $Pflag = "true" ]; then
	_fmt < $tmp/pmies
    else
	LC_COLLATE=POSIX sort < $tmp/pmies  \
	| sed -e '/^$/d' | sed -e '1!s/^/           /'
    fi
fi

sts=0
exit
