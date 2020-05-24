#! /bin/sh
#
# Copyright (c) 2020 Red Hat.
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
# Administrative script to discover remote PCP services and configure
# pmie and pmlogger control files for each as required.
#

# get standard environment
. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh
. $PCP_SHARE_DIR/lib/utilproc.sh

PMFIND=${PMFIND:-"$PCP_BIN_DIR/pmfind"}

PMIE_ARGS=''
PMFIND_ARGS=${PMFIND_ARGS:-'-s pmcd -r -S'}
PMLOGGER_ARGS='-r -T24h10m -v 100Mb'
PMIE_SERVICE_ARGS=${PMIE_SERVICE_PARAMS:-$PMIE_ARGS}
PMLOGGER_SERVICE_ARGS=${PMLOGGER_SERVICE_PARAMS:-$PMLOGGER_ARGS}

# error messages should go to stderr, not the GUI notifiers
unset PCP_STDERR

tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmfind/$prog.log
USE_SYSLOG=true

_cleanup()
{
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    [ -s "$PROGLOG" ] || rm -f "$PROGLOG"
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

# option parsing
CONTAINERS=false
SHOWME=false
MV=mv
RM=rm
CP=cp
VERBOSE=false
VERY_VERBOSE=false

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -C,--containers         probe each discovered host for pmcd containers
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -N,--showme             perform a dry run, showing what would be done
  -V,--verbose            increase diagnostic verbosity
  --help
EOF

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
        -C)     CONTAINERS=true
		;;
	-l)	PROGLOG="$2"
		USE_SYSLOG=false
		shift
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		CP="echo + cp"
		;;
	-V)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	--)	shift
		break
		;;
	-\?)	pmgetopt --usage --progname=$prog --config=$tmp/usage
		status=1
		exit
		;;
    esac
    shift
done

if [ $# -ne 0 ]
then
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    status=1
    exit
fi

# after argument checking, everything must be logged to ensure no mail is
# accidentally sent from cron.  Close stdout and stderr, then open stdout
# as our logfile and redirect stderr there too.
#
PROGLOGDIR=`dirname "$PROGLOG"`
[ -d "$PROGLOGDIR" ] || mkdir_and_chown "$PROGLOGDIR" 755 $PCP_USER:$PCP_GROUP 2>/dev/null

if $SHOWME
then
    :
else
    # Salt away previous log, if any ...
    _save_prev_file "$PROGLOG"

    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.  Create the log file with
    # correct ownership first.
    #
    # Exception ($SHOWME, above) is for -N where we want to see the output.
    #
    touch "$PROGLOG"
    chown $PCP_USER:$PCP_GROUP "$PROGLOG" >/dev/null 2>&1
    exec 1>"$PROGLOG" 2>&1
fi

_error()
{
    echo "$prog: [$SHA1]"
    echo "Error: $@"
    echo "... automated service discovery for host \"$host\" unchanged"
    touch $tmp/err
}

_debug()
{
    if $VERY_VERBOSE
    then
	echo "$prog [$SHA1]"
	echo "Debug: $@"
    fi
}

$CONTAINERS && PMFIND_ARGS="$PMFIND_ARGS -C"
$PMFIND $PMFIND_ARGS > "$tmp/out" 2> "$tmp/err"
[ $? -eq 0 ] && rm -f "$tmp/err"

cat "$tmp/out" | while read SHA1 host
do
    $VERBOSE && echo "Discovered host $host [$SHA1]"

    if [ ! -f "$PCP_PMIECONTROL_PATH.d/$SHA1" ]
    then
	# newly discovered source - create pmie control file for it
	cat > "$tmp/$SHA1" <<EOF_PMIE
\$version=1.1
$host	n   n	PCP_LOG_DIR/pmie/$SHA1/pmie.log	-c config.$SHA1 $PMIE_SERVICE_ARGS
EOF_PMIE
	$CP "$tmp/$SHA1" "$PCP_PMIECONTROL_PATH.d/$SHA1" || \
	    _error "cannot create $PCP_PMIECONTROL_PATH.d/$SHA1"
	_debug "pmie setup for host \"$host\""
    else
	_debug "pmie already setup for host \"$host\""
    fi

    if [ ! -f "$PCP_PMLOGGERCONTROL_PATH.d/$SHA1" ]
    then
	# newly discovered source - create pmlogger control file for it
	cat > "$tmp/$SHA1" <<EOF_PMLOGGER
\$version=1.1
$host	n   n	PCP_ARCHIVE_DIR/$SHA1	-c config.$SHA1 $PMLOGGER_SERVICE_ARGS
EOF_PMLOGGER
	$CP "$tmp/$SHA1" "$PCP_PMLOGGERCONTROL_PATH.d/$SHA1" || \
	    _error "cannot create $PCP_PMLOGGERCONTROL_PATH.d/$SHA1"
	_debug "pmlogger setup for host \"$host\""
    else
	_debug "pmlogger already setup for host \"$host\""
    fi
done

[ -f $tmp/err ] && status=1
exit
