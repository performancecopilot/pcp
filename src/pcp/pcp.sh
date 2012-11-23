#! /bin/sh
# 
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
# Displays the Performance Co-Pilot configuration for a host running pmcd(1)
#

. $PCP_DIR/etc/pcp.env

sts=2
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
trap "rm -rf $tmp; exit \$sts" 0 1 2 3  15

errors=0
prog=`basename $0`
host=`pmhostname`
for var in unknown version build numagents numclients ncpu ndisk nnode nrouter nxbow ncell mem cputype uname timezone status
do
    eval $var="unknown?"
done

# metrics
metrics="pmcd.numagents pmcd.numclients pmcd.version pmcd.build pmcd.timezone pmcd.agent.status pmcd.pmlogger.archive pmcd.pmlogger.pmcd_host hinv.ncpu hinv.ndisk hinv.nnode hinv.nrouter hinv.nxbow hinv.ncell hinv.physmem hinv.cputype pmda.uname pmcd.pmie.pmcd_host pmcd.pmie.configfile pmcd.pmie.numrules pmcd.pmie.logfile"
pmiemetrics="pmcd.pmie.actions pmcd.pmie.eval.true pmcd.pmie.eval.false pmcd.pmie.eval.unknown pmcd.pmie.eval.expected"

_usage()
{
    [ ! -z "$@" ] && echo $@ 1>&2
    echo 1>&2 "Usage: $prog [options]

Options:
  -a archive    metrics source is a PCP log archive
  -h host       metrics source is PMCD on host
  -n pmnsfile   use an alternative PMNS
  -p            display pmie evaluation statistics"
    exit
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
    fmt -64 | tr -d '\r' | tr -s '\n' | $PCP_AWK_PROG '
NR > 1	{ printf "           %s\n", $0; next }
	{ print }'
}

opts=""
hflag=false
aflag=false
pflag=false
while getopts "?a:h:n:p" c
do
    case $c in
      a)
	[ $aflag = true ] && _usage "$prog: only one -a option permitted"
	[ $hflag = true ] && _usage "$prog: -a and -h mutually exclusive"
	opts="$opts -a $OPTARG"
	arch=$OPTARG
	eval `pmdumplog -lz $arch | $PCP_AWK_PROG '
/^Performance metrics from host/	{  printf "host=%s\n", $5  }
/^  commencing/				{  tmp = substr($5, 7, 6)
					   sub(tmp, tmp+0.001, $5)
					   sub("commencing", "@")
					   printf "offset=\"%s\"\n", $0
					}'`
	[ "X$host" = X ] && host="unknown host"
	[ "X$offset" != X ] && opts="$opts -O '$offset' -z"
	aflag=true
	;;
      h)
	[ $hflag = true ] && _usage "$prog: only one -h option permitted"
	[ $aflag = true ] && _usage "$prog: -a and -h mutually exclusive"
	opts="$opts -h $OPTARG"
	host=$OPTARG
	hflag=true
	;;
      n)
	opts="$opts -n $OPTARG" ;;
      p)
	metrics="$metrics $pmiemetrics"
	pflag=true
	;;
      ?)
	_usage "" ;;
    esac
done

shift `expr $OPTIND - 1`
[ $# -ge 1 ] && _usage "$prog: too many arguments"

if eval pminfo $opts -f $metrics > $tmp/metrics 2>&1
then
    :
else
    if grep "^pminfo:" $tmp/metrics > /dev/null 2>&1
    then
	$PCP_ECHO_PROG $PCP_ECHO_N "$prog: ""$PCP_ECHO_C"
	sed < $tmp/metrics -e 's/^pminfo: //g'
	sts=1
	exit
    fi
fi

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

[ "$timezone" = $unknown ] && timezone="Unknown"

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

    numloggers=`join $tmp/log_host $tmp/log_archive | sort -n \
	| sed -e 's/"//g' | tee $tmp/log | $PCP_AWK_PROG '
BEGIN		{ count = 0 }
$1 == "0"	{ next }
$1 == "1"	{ next }
		{ count++ }
END		{ print count }'`

    $PCP_AWK_PROG < $tmp/log > $tmp/loggers '
BEGIN		{ primary=0 }
$1 == "0"	{ primary=$3; next }
$3 == primary	{ offset = match($3, "/pmlogger/")
		  if (offset != 0) {
		    $3=substr($3, offset+10, length($3))
		  } else {
		    offset = match($3, "/pcplog/")
		    if (offset != 0)
		      $3=substr($3, offset+8, length($3))
		  }
		  printf "primary logger: %s\n\n",$3; exit }'

    $PCP_AWK_PROG < $tmp/log >> $tmp/loggers '
BEGIN		{ primary=0 }
$1 == "0"	{ primary=$3; next }
$1 == "1"	{ next }
$3 == primary	{ next }
	{ offset = match($3, "/pmlogger/")
		  if (offset != 0) {
		    $3=substr($3, offset+10, length($3))
		  } else {
		    offset = match($3, "/pcplog/")
		    if (offset != 0)
		      $3=substr($3, offset+8, length($3))
		  }
		  printf "%s: %s\n\n",$2,$3 }'
else
    numloggers=0
fi

if [ -f $tmp/ie_host -a -f $tmp/ie_config -a -f $tmp/ie_log -a -f $tmp/ie_numrules ]
then
    sort $tmp/ie_log -o $tmp/ie_log
    sort $tmp/ie_host -o $tmp/ie_host
    sort $tmp/ie_config -o $tmp/ie_config
    sort $tmp/ie_numrules -o $tmp/ie_numrules
    if [ $pflag = "true" ]; then
	numpmies=`join $tmp/ie_host $tmp/ie_config | join - $tmp/ie_numrules \
	    | sort -n | sed -e 's/"//g' | tee $tmp/pmie | wc -l | tr -d ' '`
    else
	numpmies=`join $tmp/ie_host $tmp/ie_log \
	    | sort -n | sed -e 's/"//g' | tee $tmp/pmie | wc -l | tr -d ' '`
    fi

    if [ $pflag = "true" -a -f $tmp/ie_actions -a -f $tmp/ie_true -a \
	-f $tmp/ie_false -a -f $tmp/ie_unknown -a -f $tmp/ie_expected ]
    then
	sort $tmp/ie_actions -o $tmp/ie_actions
	sort $tmp/ie_true -o $tmp/ie_true
	sort $tmp/ie_false -o $tmp/ie_false
	sort $tmp/ie_unknown -o $tmp/ie_unknown
	sort $tmp/ie_expected -o $tmp/ie_expected
	join $tmp/pmie $tmp/ie_true | join - $tmp/ie_false \
		| join - $tmp/ie_unknown | join - $tmp/ie_actions \
		| join - $tmp/ie_expected > $tmp/pmie
    fi

    $PCP_AWK_PROG -v pflag=$pflag < $tmp/pmie '{
	if (pflag == "true") {
	    offset = match($3, "/pcp/config/")
	    if (offset != 0)
		$3=substr($3, offset+12, length($3))
	    printf "%s: %s [%u]\n\n",$2,$3,$4
	    printf "evaluations true=%u false=%u unknown=%u (actions=%u)",$5,$6,$7,$8
	    printf "\n\nexpected evaluation rate=%f/sec\n\n",$9
	} else {
	    printf "%s: %s\n\n",$2,$3
	}
    }' > $tmp/pmies
else
    numpmies=0
fi

# finally, display everything we've found...
# 
echo "Performance Co-Pilot configuration on ${host}:"
echo
[ -n "$arch" ] && echo "  archive: $arch"
echo " platform: ${uname}"
echo " hardware: "`echo $hardware | _fmt`
echo " timezone: $timezone"

echo "     pmcd: ${version},${numagents}$numclients"

[ -n "$agents" ] && echo "     pmda: $agents"

if [ "$numloggers" != 0 ]
then
    $PCP_ECHO_PROG $PCP_ECHO_N " pmlogger: ""$PCP_ECHO_C"
    _fmt < $tmp/loggers
fi

if [ "$numpmies" != 0 ]
then
    $PCP_ECHO_PROG $PCP_ECHO_N "     pmie: ""$PCP_ECHO_C"
    _fmt < $tmp/pmies
fi

sts=0
