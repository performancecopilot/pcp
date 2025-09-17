#! /bin/sh
#
# Copyright (c) 2013-2016,2018,2020-2022 Red Hat.
# Copyright (c) 1995-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# Administrative script to check pmlogger processes are alive, and restart
# them as required.
#

. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh
. $PCP_SHARE_DIR/lib/utilproc.sh

PMLOGGER="$PCP_BINADM_DIR/pmlogger"
PMLOGCONF="$PCP_BINADM_DIR/pmlogconf"
PMLOGGERENVS="$PCP_SYSCONFIG_DIR/pmlogger"
PMLOGGERFARMENVS="$PCP_SYSCONFIG_DIR/pmlogger_farm"
PMLOGGERZEROCONFENVS="$PCP_SHARE_DIR/zeroconf/pmlogger"

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# ensure mere mortals cannot write any configuration files,
# but that the unprivileged PCP_USER account has read access
#
umask 022

# constant setup
#
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmlogger_check.XXXXXXXXX"` || exit 1
status=0
echo >$tmp/lock
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmlogger/$prog.log
MYPROGLOG=$PROGLOG.$$
USE_SYSLOG=true

# optional begin logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    logmsg="begin pid:$$ $prog args:$*"
    if which pstree >/dev/null 2>&1
    then
	logmsg="$logmsg [`_pstree_oneline $$`]"
    fi
    $PCP_BINADM_DIR/pmpost "$logmsg"
fi

_cleanup()
{
    if [ "$PROGLOG" != "/dev/tty" ]
    then
	if [ -s "$MYPROGLOG" ]
	then
	    rm -f "$PROGLOG"
	    mv "$MYPROGLOG" "$PROGLOG"
	else
	    rm -f "$MYPROGLOG"
	fi
    fi
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    lockfile=`cat $tmp/lock 2>/dev/null`
    [ -n "$lockfile" ] && rm -f "$lockfile"
    rm -rf $tmp
    $VERBOSE && echo >&2 "End [check]: `_datestamp` status=$status"
}
trap "_cleanup; exit \$status" 0 1 2 3 15

# control files for pmlogger administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration; see
# also -c option below.
#
CONTROL=$PCP_PMLOGGERCONTROL_PATH
CONTROLDIR=$PCP_PMLOGGERCONTROL_PATH.d

# determine path for pwd command to override shell built-in
PWDCMND=`which pwd 2>/dev/null | $PCP_AWK_PROG '
BEGIN	    	{ i = 0 }
/ not in /  	{ i = 1 }
/ aliased to /  { i = 1 }
 	    	{ if ( i == 0 ) print }
'`
[ -z "$PWDCMND" ] && PWDCMND=/bin/pwd
eval $PWDCMND -P >/dev/null 2>&1
[ $? -eq 0 ] && PWDCMND="$PWDCMND -P"
here=`$PWDCMND`

# default location
#
logfile=pmlogger.log


# option parsing
#
SHOWME=false
MV=mv
CP=cp
LN=ln
RM=rm
KILL=pmsignal
TERSE=false
VERBOSE=false
VERY_VERBOSE=false
VERY_VERY_VERBOSE=false
CHECK_RUNLEVEL=false
START_PMLOGGER=true
STOP_PMLOGGER=false
QUICKSTART=false
SKIP_PRIMARY=false
ONLY_PRIMARY=false
NOERROR=false

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -c=FILE,--control=FILE  configuration of pmlogger instances to manage
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -C                      query system service runlevel information
  -n,--noerror            always exit with status 0 (for systemd services)
  -N,--showme             perform a dry run, showing what would be done
  -p,--skip-primary       do not start or stop the primary pmlogger instance
  -P,--only-primary       only start or stop the primary pmlogger, no others
  -q,--quick              quick start, no compression
  -s,--stop               stop pmlogger processes instead of starting them
  -T,--terse              produce a terser form of output
  -V,--verbose            increase diagnostic verbosity
  --help
EOF

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

daily_args=""
eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-c)	CONTROL="$2"
		CONTROLDIR="$2.d"
		daily_args="${daily_args} -c $2"
		shift
		;;
	-C)	CHECK_RUNLEVEL=true
		;;
	-l)	PROGLOG="$2"
		MYPROGLOG="$PROGLOG".$$
		USE_SYSLOG=false
		if [ "$PROGLOG" = "/dev/tty" ]
		then
		    daily_args="${daily_args} -l /dev/tty"
		else
		    daily_args="${daily_args} -l $2.from.check"
		fi
		shift
		;;
	-n)	NOERROR=true
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		MV="echo + mv"
		CP="echo + cp"
		LN="echo + ln"
		KILL="echo + kill"
		daily_args="${daily_args} -N"
		;;
	-p)	SKIP_PRIMARY=true
		;;
	-P)	ONLY_PRIMARY=true
		;;
	-q)	QUICKSTART=true
		;;
	-s)	START_PMLOGGER=false
		STOP_PMLOGGER=true
		;;
	-T)	TERSE=true
		;;
	-V)	if $VERY_VERBOSE
		then
		    VERY_VERY_VERBOSE=true
		elif $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		daily_args="${daily_args} -V"
		;;
	--)	shift
		break
		;;
	-\?)	pmgetopt --usage --progname=$prog --config=$tmp/usage
		$NOERROR || status=1
		exit
		;;
    esac
    shift
done

if [ $# -ne 0 ]
then
    pmgetopt --usage --progname=$prog --config=$tmp/usage
    $NOERROR || status=1
    exit
fi

_compress_now()
{
    if $QUICKSTART
    then
	$VERY_VERBOSE && echo "Skip compression, -q/--quick on command line"
    else
	# If $PCP_COMPRESSAFTER=0 in the control file(s), compress archives now.
	# Invoked just before exit when this script has finished successfully.
	$VERY_VERBOSE && echo "Doing compression ..."
	$PCP_BINADM_DIR/pmlogger_daily -K $daily_args
    fi
}

# after argument checking, everything must be logged to ensure no mail is
# accidentally sent from cron.  Close stdout and stderr, then open stdout
# as our logfile and redirect stderr there too.
#
if $SHOWME
then
    # Exception is for -N where we want to see the output.
    #
    :
elif [ "$PROGLOG" = "/dev/tty" ]
then
    # special case for debugging ... no salt away previous
    #
    :
else
    # Salt away previous log, if any ...
    #
    _save_prev_file "$PROGLOG"
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.
    #
    exec 1>"$MYPROGLOG" 2>&1
fi

$VERBOSE && echo >&2 "Start [check]: `_datestamp`"
$VERY_VERBOSE && _pstree_all $$

# if SaveLogs exists in the $PCP_LOG_DIR/pmlogger directory and is writeable
# then save $MYPROGLOG there as well with a unique name that contains the date
# and time when we're run ... skip if -N (showme)
if [ "$PROGLOG" != "/dev/tty" ]
then
    if [ -d $PCP_LOG_DIR/pmlogger/SaveLogs -a -w $PCP_LOG_DIR/pmlogger/SaveLogs ]
    then
	if [ `date +%N` = N ]
	then
	    # no %N, %S is the best we can do
	    now="`date '+%Y%m%d.%H:%M:%S'`"
	else
	    now="`date '+%Y%m%d.%H:%M:%S.%N'`"
	fi
	link=`echo $MYPROGLOG | sed -e "s@.*$prog@$PCP_LOG_DIR/pmlogger/SaveLogs/$now-$prog@"`
	if [ ! -f "$link" ]
	then
	    if $SHOWME
	    then
		echo "+ ln $MYPROGLOG $link"
	    else
		ln $MYPROGLOG $link
		if [ -w $link ]
		then
		    echo "--- Added by $prog when SaveLogs dir found ---" >>$link
		    echo "Start [check]: `_datestamp`" >>$link
		    echo "Args: $ARGS" >>$link
		    _pstree_all $$
		fi
	    fi
	fi
    fi
fi

QUIETLY=false
if [ $CHECK_RUNLEVEL = true ]
then
    # determine whether to start pmlogger based on runlevel settings -
    # need to do this when running unilaterally from cron, else we'll
    # always start pmlogger up (even when we shouldn't).
    #
    QUIETLY=true
    if is_chkconfig_on pmlogger
    then
	START_PMLOGGER=true
    else
	START_PMLOGGER=false
    fi
fi

if [ $STOP_PMLOGGER = true ]
then
    # if pmlogger hasn't been started, there's no work to do to stop it
    # but we still want to compress existing logs, if any
    if [ ! -d "$PCP_TMP_DIR/pmlogger" ]
    then
    	_compress_now
	exit
    fi
    $QUIETLY || $PCP_BINADM_DIR/pmpost "stop pmlogger from $prog"
elif [ $START_PMLOGGER = false ]
then
    # if we're not going to start pmlogger, there is no work to do other
    # than compress existing logs, if any.
    _compress_now
    exit
fi

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    $NOERROR || status=1
    exit
fi

_restarting()
{
    $PCP_ECHO_PROG $PCP_ECHO_N "Restarting$iam pmlogger for host \"$host\" ...""$PCP_ECHO_C"
}

_get_configfile()
{
    # extract the pmlogger configuration file (-c) from a list of arguments
    #
    echo $@ | sed -n \
        -e 's/^/ /' \
        -e 's/[         ][      ]*/ /g' \
        -e 's/-c /-c/' \
        -e 's/.* -c\([^ ]*\).*/\1/p'
}

_configure_pmlogger()
{
    # update a pmlogger configuration file if it should be created/modified
    #
    tmpconfig="$1.tmp"
    configfile="$1"
    hostname="$2"

    # clear any zero-length configuration file, that is never helpful.
    [ -f "$configfile" -a ! -s "$configfile" ] && unlink "$configfile"

    if [ -f "$configfile" ]
    then
	# look for "magic" string at start of file, and ensure we created it
	sed 1q "$configfile" | grep '^#pmlogconf [0-9]' >/dev/null
	magic=$?
	grep '^# Auto-generated by pmlogconf' "$configfile" >/dev/null
	owned=$?
	skip=0
	# assign PMLOGGER_CHECK_SKIP_LOGCONF (if present) from $envs
	[ -n "$envs" -a -z "$PMLOGGER_CHECK_SKIP_LOGCONF" ] && eval $envs
	if [ -n "$PMLOGGER_CHECK_SKIP_LOGCONF" ]
	then
	    # $PMLOGGER_CHECK_SKIP_LOGCONF is primarily for QA.
            # If the PMDA configuration is stable we really don't need
            # to keep running pmlogconf here to discover there are
	    # "No changes".
	    #
	    if [ "$PMLOGGER_CHECK_SKIP_LOGCONF" = yes ]
	    then
		$VERBOSE && echo "PMLOGGER_CHECK_SKIP_LOGCONF=$PMLOGGER_CHECK_SKIP_LOGCONF: skip: \"$configfile\" reconfigure"
		skip=1
	    fi
	fi
	if [ $magic -eq 0 -a $owned -eq 0 -a $skip = 0 ]
	then
	    # pmlogconf file that we own, see if re-generation is needed
	    eval $CP "$configfile" "$tmpconfig"
	    if $SHOWME
	    then
		echo + $PMLOGCONF -r -c -q -h $hostname "$tmpconfig"
	    else
		if $PMLOGCONF -r -c -q -h $hostname "$tmpconfig" </dev/null >$tmp/diag 2>&1
		then
		    if grep 'No changes' $tmp/diag >/dev/null 2>&1
		    then
			$VERBOSE && echo "No change: \"$configfile\" (pmlogconf)"
		    elif [ -w "$configfile" ]
		    then
			$VERBOSE && echo "Reconfigured: \"$configfile\" (pmlogconf)"
			eval $MV "$tmpconfig" "$configfile"
			echo "=== pmlogconf changes @ `date` ==="
			cat $tmp/diag
		    else
			# transition problem we're trying to resolve ... configfile may have
			# been owned by root but in a dir owned by $PCP_USER ... if so we'd
			# like configfile to end up ownded by $PCP_USER
			#
			_warning "no write access to pmlogconf file \"$configfile\""
			ls -l "$configfile"
			echo "Trying to remove and replace \"$configfile\" ..."
			$RM -f "$configfile"
			if [ -e "$configfile" ]
			then
			    echo "Failed, parent directory is ..."
			    ls -l `dirname "$configfile"`
			    _warning "skip reconfiguration"
			else
			    eval $MV "$tmpconfig" "$configfile"
			fi
		    fi
		else
		    _warning "pmlogconf failed to reconfigure \"$configfile\""
		    sed -e "s;$tmpconfig;$configfile;g" $tmp/diag
		    echo "=== start pmlogconf file ==="
		    cat "$tmpconfig"
		    echo "=== end pmlogconf file ==="
		fi
	    fi
	    rm -f "$tmpconfig"
	else
	    $VERBOSE && echo "No reconfigure: magic=$magic auto-generated=$owned skip=$skip: \"$configfile\" (pmlogconf)"
	fi
    elif [ ! -e "$configfile" ]
    then
	# file does not exist, generate it, if possible
	if $SHOWME
	then
	    echo "+ $PMLOGCONF -c -q -h $hostname $configfile"
	elif ! $PMLOGCONF -c -q -h $hostname "$configfile" </dev/null >$tmp/diag 2>&1
	then
	    _warning "pmlogconf failed to generate \"$configfile\""
	    cat $tmp/diag
	    echo "=== start pmlogconf file ==="
	    cat "$configfile"
	    echo "=== end pmlogconf file ==="
	else
	    $VERBOSE && echo "Created: \"$configfile\" (pmlogconf)"
	fi
    else
	$VERBOSE && echo "Botched: \"$configfile\" (pmlogconf)"
    fi
}

_get_logfile()
{
    # looking for -lLOGFILE or -l LOGFILE in args
    #
    want=false
    for a in $args
    do
	if $want
	then
	    logfile="$a"
	    want=false
	    break
	fi
	case "$a"
	in
	    -l)
		want=true
		;;
	    -l*)
		logfile=`echo "$a" | sed -e 's/-l//'`
		break
		;;
	esac
    done
}

_get_primary_logger_pid()
{
    pid=`cat "$PCP_RUN_DIR/pmlogger.pid" 2>/dev/null`
    if [ -z "$pid" ]
    then
	# No PID file, try the pmcd.pmlogger.* info files where "primary"
	# is a symlink to a <pid> file
	#
	pid=`ls -l $PCP_TMP_DIR/pmlogger/primary 2>/dev/null | sed -e 's;.*/\([0-9][0-9]*\)$;\1;'`
    fi
    echo "$pid"
}

# wait for the local pmcd to get going for a primary pmlogger
# (borrowed from qa/common.check)
#
# wait_for_pmcd [maxdelay]
#
_wait_for_pmcd()
{
    # 5 seconds default seems like a reasonable max time to get going
    _can_wait=${1-5}
    _limit=`expr $_can_wait \* 10`
    _i=0
    _dead=true
    while [ $_i -lt $_limit ]
    do
	_sts=`pmprobe pmcd.numclients 2>/dev/null | $PCP_AWK_PROG '{print $2}'`
	if [ "${_sts:-0}" -gt 0 ]
	then
	    # numval really > 0, we're done
	    #
	    _dead=false
	    break
	fi
	pmsleep -w 'waiting for pmcd start' 0.1
	_i=`expr $_i + 1`
    done
    if $_dead
    then
	date
	echo "Arrgghhh ... pmcd at localhost failed to start after $_can_wait seconds"
	echo "=== failing pmprobes ==="
	pmprobe pmcd.numclients
	$NOERROR || status=1
    fi
}

_check_archive()
{
    if [ ! -e "$logfile" ]
    then
	echo "$prog: Error: cannot find pmlogger output file at \"$logfile\""
	if $TERSE
	then
	    :
	else
	    logdir=`dirname "$logfile"`
	    echo "Directory (`cd "$logdir"; $PWDCMND`) contents:"
	    LC_TIME=POSIX ls -la "$logdir"
	fi
    elif [ -f "$logfile" ]
    then
	echo "Contents of pmlogger output file \"$logfile\" ..."
	cat "$logfile"
    fi
}

_check_logger()
{
    $VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N " [process $1] ""$PCP_ECHO_C"

    # wait until pmlogger process starts, or exits
    #
    delay=5
    [ -n "$PMCD_CONNECT_TIMEOUT" ] && delay=$PMCD_CONNECT_TIMEOUT
    x=5
    [ -n "$PMCD_REQUEST_TIMEOUT" ] && x=$PMCD_REQUEST_TIMEOUT

    # max delay (secs) before timing out our pmlc request ... if this
    # pmlogger is just started, we may need to be a little patient
    #
    [ -z "$PMLOGGER_REQUEST_TIMEOUT" ] && export PMLOGGER_REQUEST_TIMEOUT=10

    # wait for maximum time of a connection and 20 requests
    #
    delay=`expr \( $delay + 20 \* $x \) \* 10`	# tenths of a second
    #debug# $PCP_ECHO_PROG $PCP_ECHO_N " delay=$delay/10 sec ""$PCP_ECHO_C"
    while [ $delay -gt 0 ]
    do
	if [ -f $logfile ]
	then
	    # $logfile was previously removed, if it has appeared again
	    # then we know pmlogger has started ... if not just sleep and
	    # try again
	    #
	    # may need to wait for pmlogger to get going ... logic here
	    # is based on _wait_for_pmlogger() in qa/common.check
	    #
	    if pmlc "$1" </dev/null 2>&1 | tee $tmp/tmp \
		    | grep "^Connected to .*pmlogger" >/dev/null
	    then
		# pmlogger socket has been set up ...
		$VERBOSE && echo " done"
		return 0
	    else
		if $VERY_VERY_VERBOSE
		then
		    echo "delay=$delay pid=$1 pmlc not connecting yet"
		    cat $tmp/tmp
		fi
	    fi

	    _plist=`_get_pids_by_name pmlogger`
	    _found=false
	    for _p in `echo $_plist`
	    do
		[ $_p -eq $1 ] && _found=true
	    done

	    if $_found
	    then
		# process still here, just not accepting pmlc connections
		# yet, try again
		:
	    else
		$VERBOSE || _restarting
		echo " process exited!"
		if $TERSE
		then
		    :
		else
		    echo "$prog: Error: failed to restart pmlogger"
		    echo "Current pmlogger processes:"
		    $PCP_PS_PROG $PCP_PS_ALL_FLAGS | tee $tmp/tmp | sed -n -e 1p
		    for _p in `echo $_plist`
		    do
			sed -n -e "/^[ ]*[^ ]* [ ]*$_p /p" < $tmp/tmp
		    done 
		    echo
		fi
		_check_archive
		return 1
	    fi
	fi
	pmsleep -w 'waiting for pmlogger start' 0.1
	delay=`expr $delay - 1`
	$VERBOSE && [ `expr $delay % 10` -eq 0 ] && \
			$PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
    done
    $VERBOSE || _restarting
    echo " timed out waiting!"
    if $TERSE
    then
	:
    else
	sed -e 's/^/	/' $tmp/out
    fi
    _check_archive
    return 1
}

# note on control file format version
#  1.0 was shipped as part of PCPWEB beta, and did not include the
#	socks field [this is the default for backwards compatibility]
#  1.1 is the first production release, and the version is set in
#	the control file with a $version=1.1 line (see below)
#
version=''

echo >$tmp/dir

# if this file exists at the end, we encountered a serious error
#
rm -f $tmp/err

rm -f $tmp/pmloggers

# come here from _parse_log_control() once per valid line in a control
# file ... see utilproc.sh for interface definitions
#
_callback_log_control()
{
    if ! $logpush
    then
	# set -d to unexpanded directory from _do_dir_and_args() ...
	# map spaces to CTL-A and tabs to CTL-B to avoid breaking
	# the argument in the eval used to launch pmlogger
	#
	args="-d \"`echo "$orig_dir" | sed -e 's/ //g' -e 's/	//'`\" $args"
    fi

    # if -p/--skip-primary on the command line, do not process
    # a control file line for the primary pmlogger
    #
    if $SKIP_PRIMARY && [ $primary = y ]
    then
	$VERY_VERBOSE && echo "Skip, -p/--skip-primary on command line"
	return
    fi

    # if -P/--only-primary on the command line, only process
    # the control file line for the primary pmlogger
    #
    if $ONLY_PRIMARY && [ $primary != y ]
    then
	$VERY_VERBOSE && echo "Skip non-primary, -P/--only-primary on command line"
	return
    fi

    if $VERY_VERBOSE
    then
	pflag=''
	[ $primary = y ] && pflag=' -P'
	echo "Checking for: pmlogger$pflag -h $host ... in $dir ..."
    fi

    pid=`_find_matching_pmlogger`

    if [ -z "$pid" -a $START_PMLOGGER = true ]
    then
	if [ "X$primary" = Xy ]
	then
	    # User configuration takes precedence over pcp-zeroconf
	    envs=`grep -h ^PMLOGGER "$PMLOGGERZEROCONFENVS" "$PMLOGGERENVS" 2>/dev/null`
	    args="-P $args"
	    iam=" primary"
	    # clean up port-map, just in case
	    #
	    PM_LOG_PORT_DIR="$PCP_TMP_DIR/pmlogger"
	    rm -f "$PM_LOG_PORT_DIR/primary"
	    # We really expect the primary pmlogger to work, especially
	    # in the systemd world, so make sure pmcd is ready to accept
	    # connections.
	    #
	    _wait_for_pmcd
	    if [ "$status" = 1 ]
	    then
		$VERY_VERBOSE && echo "pmcd not running, skip primary pmlogger"
		return
	    fi
	else
	    envs=`grep -h ^PMLOGGER "$PMLOGGERFARMENVS" 2>/dev/null`
	    args="-h $host $args"
	    iam=""
	fi

	# each new log started locally is named yyyymmdd.hh.mm
	#
	LOGNAME=%Y%m%d.%H.%M

	# We used to handle duplicates/aliases here (happens when
	# pmlogger is restarted within a minute and LOGNAME expands
	# to the same string ... this is now magically handled by
	# pmlogger, so do nothing.
	#

	configfile=`_get_configfile $args`
	if [ ! -z "$configfile" ]
	then
	    # if this is a relative path and not relative to cwd,
	    # substitute in the default pmlogger search location.
	    #
	    if [ ! -f "$configfile" -a "`basename $configfile`" = "$configfile" ]
	    then
		configfile="$PCP_VAR_DIR/config/pmlogger/$configfile"
	    fi
	    # check configuration file exists and is up to date
	    _configure_pmlogger "$configfile" "$host"
	fi

	sock_me=''
	if [ "$socks" = y ]
	then
	    # only check for pmsocks if it's specified in the control file
	    have_pmsocks=false
	    if which pmsocks >/dev/null 2>&1
	    then
		# check if pmsocks has been set up correctly
		if pmsocks ls >/dev/null 2>&1
		then
		    have_pmsocks=true
		fi
	    fi
	    if $have_pmsocks
	    then
		sock_me="pmsocks "
	    else
		_warning "no pmsocks available, would run without"
		sock_me=""
	    fi
	fi

	_get_logfile

	# Notify service manager (if any) for the primary logger ONLY.
	[ "$primary" = y ] && args="-N $args"

	args="$args -m pmlogger_check"
	if $logpush
	then
	    # don't need $LOGNAME as last argument, since we assume
	    # command line $args ends with http://... and $dir
	    # prefixed by '+' in control file
	    #
	    :
	else
	    args="$args $LOGNAME"
	fi

	if $SHOWME
	then
	    $VERBOSE && _restarting
	    echo
	    echo "+ ${sock_me}$PMLOGGER $args" | sed -e 's// /g' -e 's//	/g'
	    return
	else
	    $PCP_BINADM_DIR/pmpost "start pmlogger from $prog for host $host"
	    # The pmlogger child will be re-parented to init (aka systemd)
	    $VERY_VERBOSE && echo >&2 "Command: $envs ${sock_me}$PMLOGGER $args"
	    $VERBOSE && _restarting
	    pid=`eval $envs '${sock_me}$PMLOGGER $args >$tmp/out 2>&1 & echo $!'`
	fi

	# wait for pmlogger to get started, and check on its health
	_check_logger $pid
	if [ -s $tmp/out ]
	then
	    _warning "early diagnostics from pmlogger ..."
	    cat $tmp/out
	fi

	# if SaveLogs exists in the same directory that the archive
	# is being created, save pmlogger log file there as well
	#
	if [ -d ./SaveLogs ]
	then
	    # get archive basename, which is the expanded version
	    # of $LOGNAME, possibly with duplicate resolution ...
	    # the $PCP_TMP_DIR/pmlogger file has the answer
	    #
	    mylogname=`sed -n -e 3p $PCP_TMP_DIR/pmlogger/$pid 2>/dev/null \
		       | sed -e 's;.*/;;'`
	    if [ -n "$mylogname" ]
	    then
		if [ ! -f ./SaveLogs/$mylogname.log ]
		then
		    $LN $logfile ./SaveLogs/$mylogname.log
		else
		    $VERBOSE && echo "Failed to link $logfile, SaveLogs/$mylogname.log already exists"
		fi
	    else
		$VERBOSE && echo "Failed to get archive basename from $PCP_TMP_DIR/pmlogger/$pid"
	    fi
	fi

    elif [ -n "$pid" -a $STOP_PMLOGGER = true ]
    then
	# Send pmlogger a SIGTERM, which is noted as a pending shutdown.
	# Add pid to list of loggers sent SIGTERM - may need SIGKILL later.
	#
	$VERY_VERBOSE && echo "+ $KILL -s TERM $pid"
	eval $KILL -s TERM $pid
	$PCP_ECHO_PROG $PCP_ECHO_N "$pid ""$PCP_ECHO_C" >> $tmp/pmloggers
    fi
}

# parse and process the control file(s)
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort | sed -e "s;^;$CONTROLDIR/;g"`
for c in $CONTROL $append
do
    _parse_log_control "$c"
done

# check all the SIGTERM'd loggers really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
elif [ $STOP_PMLOGGER = true -a -s $tmp/pmloggers ]
then
    pmloggerlist=`cat $tmp/pmloggers`
    if $PCP_PS_PROG -p "$pmloggerlist" >/dev/null 2>&1
    then
        $VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ $KILL -KILL $pmloggerlist ...""$PCP_ECHO_C" )
        eval $KILL -s KILL $pmloggerlist >/dev/null 2>&1
        delay=30        # tenths of a second
        while $PCP_PS_PROG -f -p "$pmloggerlist" >$tmp/alive 2>&1
        do
            if [ $delay -gt 0 ]
            then
                pmsleep -w 'waiting for pmlogger exit' 0.1
                delay=`expr $delay - 1`
                continue
            fi
            echo "$prog: Error: pmlogger process(es) will not die"
            cat $tmp/alive
            $NOERROR || status=1
            break
        done
    fi
fi

# Prior to exiting we compress existing logs, if any. See pmlogger_daily -K
# Do not compress on a virgin install - there is nothing to compress anyway.
# See RHBZ#1721223.
[ -f "$PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp" ] && _compress_now

# Run the janitor script to clean up any badness ... but only
# do this if we're using the system-installed control files (not
# something else via -c on the command line) and
# $PMLOGGER_CHECK_SKIP_JANITOR is not "yes".
#
# QA uses -c and we don't want the janitor to be run in that case,
# because the legitimate pmloggers, like the primary pmlogger, may
# not be included in the "test" control file(s).
#
if [ "$CONTROL" = "$PCP_PMLOGGERCONTROL_PATH" -a "$PMLOGGER_CHECK_SKIP_JANITOR" != "yes" ]
then
    $VERY_VERBOSE && echo "Running: pmlogger_janitor $daily_args"
    $PCP_BINADM_DIR/pmlogger_janitor $daily_args
fi

if [ -f $tmp/err ]
then
    $NOERROR || status=1
fi

# optional end logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    $PCP_BINADM_DIR/pmpost "end pid:$$ $prog status=$status"
fi

exit
