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
	logmsg="$logmsg [`pstree -lps $$`]"
	logmsg="`echo "$logmsg" | sed -e 's/---pstree([^)]*)//'`"
    fi
    $PCP_BINADM_DIR/pmpost "$logmsg"
fi

_cleanup()
{
    if [ -s "$MYPROGLOG" ]
    then
	rm -f "$PROGLOG"
	mv "$MYPROGLOG" "$PROGLOG"
    else
	rm -f "$MYPROGLOG"
    fi
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    lockfile=`cat $tmp/lock 2>/dev/null`
    [ -n "$lockfile" ] && rm -f "$lockfile"
    rm -rf $tmp
    $VERY_VERBOSE && echo "End: `date '+%F %T.%N'`"
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

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -c=FILE,--control=FILE  configuration of pmlogger instances to manage
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -C                      query system service runlevel information
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
		daily_args="${daily_args} -l $2.from.check"
		shift
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
PROGLOGDIR=`dirname "$PROGLOG"`
[ -d "$PROGLOGDIR" ] || mkdir_and_chown "$PROGLOGDIR" 755 $PCP_USER:$PCP_GROUP 2>/dev/null
if $SHOWME
then
    :
else
    # Salt away previous log, if any ...
    #
    _save_prev_file "$PROGLOG"
    # After argument checking, everything must be logged to ensure no mail is
    # accidentally sent from cron.  Close stdout and stderr, then open stdout
    # as our logfile and redirect stderr there too.  Create the log file with
    # correct ownership first.
    #
    # Exception ($SHOWME, above) is for -N where we want to see the output.
    #
    touch "$MYPROGLOG"
    chown $PCP_USER:$PCP_GROUP "$MYPROGLOG" >/dev/null 2>&1
    exec 1>"$MYPROGLOG" 2>&1
fi

if $VERY_VERBOSE
then
    echo "Start: `date '+%F %T.%N'`"
    if which pstree >/dev/null 2>&1
    then
	if pstree -spa $$ >$tmp/tmp 2>&1
	then
	    echo "Called from:"
	    cat $tmp/tmp
	    echo "--- end of pstree output ---"
	else
	    # pstree not functional for us ... -s not supported in older
	    # versions
	    :
	fi
    fi
fi

# if SaveLogs exists in the $PCP_LOG_DIR/pmlogger directory and is writeable
# then save $MYPROGLOG there as well with a unique name that contains the date
# and time when we're run ... skip if -N (showme)
#
if [ -d $PCP_LOG_DIR/pmlogger/SaveLogs -a -w $PCP_LOG_DIR/pmlogger/SaveLogs ]
then
    now="`date '+%Y%m%d.%H:%M:%S.%N'`"
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
		echo "Start: `date '+%F %T.%N'`" >>$link
		echo "Args: $ARGS" >>$link
		if which pstree >/dev/null 2>&1
		then
		    if pstree -spa $$ >$tmp/tmp 2>&1
		    then
			echo "Called from:" >>$link
			cat $tmp/tmp >>$link
		    else
			# pstree not functional for us ... -s not supported
			# in older versions
			:
		    fi
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
    status=1
    exit
fi

_error()
{
    echo "$prog: [$controlfile:$line]"
    echo "Error: $@"
    echo "... logging for host \"$host\" unchanged"
    touch $tmp/err
}

_warning()
{
    echo "$prog [$controlfile:$line]"
    echo "Warning: $@"
}

_restarting()
{
    $PCP_ECHO_PROG $PCP_ECHO_N "Restarting$iam pmlogger for host \"$host\" ...""$PCP_ECHO_C"
}

_unlock()
{
    rm -f "$1/lock"
    echo >$tmp/lock
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
	    [ "$PMLOGGER_CHECK_SKIP_LOGCONF" = yes ] && skip=1
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
			:
		    elif [ -w "$configfile" ]
		    then
			$VERBOSE && echo "Reconfigured: \"$configfile\" (pmlogconf)"
			chown $PCP_USER:$PCP_GROUP "$tmpconfig" >/dev/null 2>&1
			eval $MV "$tmpconfig" "$configfile"
		    else
			_warning "no write access to pmlogconf file \"$configfile\", skip reconfiguration"
			ls -l "$configfile"
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
	    chown $PCP_USER:$PCP_GROUP "$configfile" >/dev/null 2>&1
	fi
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
	pmsleep 0.1
	_i=`expr $_i + 1`
    done
    if $_dead
    then
	date
	echo "Arrgghhh ... pmcd at localhost failed to start after $_can_wait seconds"
	echo "=== failing pmprobes ==="
	pmprobe pmcd.numclients
	status=1
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
    [ ! -z "$PMCD_CONNECT_TIMEOUT" ] && delay=$PMCD_CONNECT_TIMEOUT
    x=5
    [ ! -z "$PMCD_REQUEST_TIMEOUT" ] && x=$PMCD_REQUEST_TIMEOUT

    # max delay (secs) before timing out our pmlc request ... if this
    # pmlogger is just started, we may need to be a little patient
    #
    [ -z "$PMLOGGER_REQUEST_TIMEOUT" ] && export PMLOGGER_REQUEST_TIMEOUT=10

    # wait for maximum time of a connection and 20 requests
    #
    delay=`expr \( $delay + 20 \* $x \) \* 10`	# tenths of a second
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
	pmsleep 0.1
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

_parse_control()
{
    controlfile="$1"
    line=0

    if echo "$controlfile" | grep -q -e '\.rpmsave$' -e '\.rpmnew$' -e '\.rpmorig$' \
	-e '\.dpkg-dist$' -e '\.dpkg-old$' -e '\.dpkg-new$' >/dev/null 2>&1
    then
	_warning "ignored backup control file \"$controlfile\""
	return
    fi

    sed \
	-e "s;PCP_ARCHIVE_DIR;$PCP_ARCHIVE_DIR;g" \
	-e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
	$controlfile | \
    while read host primary socks dir args
    do
	# start in one place for each iteration (beware relative paths)
	cd "$here"
	line=`expr $line + 1`


	if $VERY_VERBOSE 
	then
	    case "$host"
	    in
	    \#*|'')	# comment or empty
			;;
	    *)		echo "[$controlfile:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
	    		;;
	    esac
	fi

	case "$host"
	in
	    \#*|'')	# comment or empty
		continue
		;;
	    \$*)	# in-line variable assignment
		$SHOWME && echo "# $host $primary $socks $dir $args"
		cmd=`echo "$host $primary $socks $dir $args" \
		     | sed -n \
			 -e "/='/s/\(='[^']*'\).*/\1/" \
			 -e '/="/s/\(="[^"]*"\).*/\1/' \
			 -e '/=[^"'"'"']/s/[;&<>|].*$//' \
			 -e '/^\\$[A-Za-z][A-Za-z0-9_]*=/{
s/^\\$//
s/^\([A-Za-z][A-Za-z0-9_]*\)=/export \1; \1=/p
}'`
		if [ -z "$cmd" ]
		then
		    # in-line command, not a variable assignment
		    _warning "in-line command is not a variable assignment, line ignored"
		else
		    case "$cmd"
		    in
			'export PATH;'*)
			    _warning "cannot change \$PATH, line ignored"
			    ;;
			'export IFS;'*)
			    _warning "cannot change \$IFS, line ignored"
			    ;;
			*)
			    $SHOWME && echo "+ $cmd"
			    echo eval $cmd >>$tmp/cmd
			    eval $cmd
			    ;;
		    esac
		fi
		continue
		;;
	esac

	# set the version and other variables
	#
	[ -f $tmp/cmd ] && . $tmp/cmd

	if [ -z "$version" -o "$version" = "1.0" ]
	then
	    if [ -z "$version" ]
	    then
		_warning "processing default version 1.0 control format"
		version=1.0
	    fi
	    args="$dir $args"
	    dir="$socks"
	    socks=n
	fi

	# do shell expansion of $dir if needed
	#
	_do_dir_and_args

	if [ -z "$primary" -o -z "$socks" -o -z "$dir" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi

	# if -p/--skip-primary on the command line, do not process
	# a control file line for the primary pmlogger
	#
	if $SKIP_PRIMARY && [ $primary = y ]
	then
	    $VERY_VERBOSE && echo "Skip, -p/--skip-primary on command line"
	    continue
	fi

	# if -P/--only-primary on the command line, only process
	# the control file line for the primary pmlogger
	#
	if $ONLY_PRIMARY && [ $primary != y ]
	then
	    $VERY_VERBOSE && echo "Skip non-primary, -P/--only-primary on command line"
	    continue
	fi

	# substitute LOCALHOSTNAME marker in this config line
	# (differently for directory and pcp -h HOST arguments)
	#
	dirhostname=`hostname || echo localhost`
	dir=`echo $dir | sed -e "s;LOCALHOSTNAME;$dirhostname;"`
	[ $primary = y -o "x$host" = xLOCALHOSTNAME ] && host=local:

	if $VERY_VERBOSE
	then
	    pflag=''
	    [ $primary = y ] && pflag=' -P'
	    echo "Checking for: pmlogger$pflag -h $host ... in $dir ..."
	fi

	# check for directory duplicate entries
	#
	if [ "`grep $dir $tmp/dir`" = "$dir" ]
	then
	    _error "Cannot start more than one pmlogger instance for archive directory \"$dir\""
	    continue
	else
	    echo "$dir" >>$tmp/dir
	fi

	# make sure output directory hierarchy exists and $PCP_USER
	# user can write there
	#
	if [ ! -d "$dir" ]
	then
	    mkdir_and_chown "$dir" 755 $PCP_USER:$PCP_GROUP >$tmp/tmp 2>&1
	    if [ ! -d "$dir" ]
	    then
		cat $tmp/tmp
		_error "cannot create directory ($dir) for PCP archive files"
		continue
	    else
		_warning "creating directory ($dir) for PCP archive files"
	    fi
	fi

	# and the logfile is writeable, if it exists
	#
	[ -f "$logfile" ] && chown $PCP_USER:$PCP_GROUP "$logfile" >/dev/null 2>&1

	if cd "$dir"
	then
	    :
	else
	    _error "cannot chdir to directory ($dir) for PCP archive files"
	    continue
	fi
	dir=`$PWDCMND`
	$SHOWME && echo "+ cd $dir"

	if [ ! -w "$dir" ]
	then
	    _warning "no write access in $dir, skip lock file processing"
	    ls -ld "$dir"
	else
	    # demand mutual exclusion
	    #
	    rm -f $tmp/stamp $tmp/out
	    delay=200	# tenths of a second
	    while [ $delay -gt 0 ]
	    do
		if pmlock -v "$dir/lock" >$tmp/out 2>&1
		then
		    echo "$dir/lock" >$tmp/lock
		    if $VERY_VERBOSE
		    then
			echo "Acquired lock:"
			ls -l $dir/lock
		    fi
		    break
		else
		    [ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		    find $tmp/stamp -newer "$dir/lock" -print 2>/dev/null >$tmp/tmp
		    if [ -s $tmp/tmp ]
		    then
			if [ -f "$dir/lock" ]
			then
			    echo "$prog: Warning: removing lock file older than 30 minutes"
			    LC_TIME=POSIX ls -l $dir/lock
			    rm -f "$dir/lock"
			else
			    # there is a small timing window here where pmlock
			    # might fail, but the lock file has been removed by
			    # the time we get here, so just keep trying
			    #
			    :
			fi
		    fi
		fi
		pmsleep 0.1
		delay=`expr $delay - 1`
	    done

	    if [ $delay -eq 0 ]
	    then
		# failed to gain mutex lock
		#
		# maybe pmlogger_daily is running ... check it, and silently
		# move on if this is the case
		#
		# Note: $PCP_RUN_DIR may not exist (see pmlogger_daily note),
		#       but only if pmlogger_daily has not run, so no chance
		#       of a collision
		#
		if [ -f "$PCP_RUN_DIR"/pmlogger_daily.pid ]
		then
		    # maybe, check pid matches a running /bin/sh
		    #
		    pid=`cat "$PCP_RUN_DIR"/pmlogger_daily.pid`
		    if _get_pids_by_name sh | grep "^$pid\$" >/dev/null
		    then
			# seems to be still running ... nothing for us to see
			# or do here
			#
			continue
		    fi
		fi
		if [ -f "$dir/lock" ]
		then
		    echo "$prog: Warning: is another PCP cron job running concurrently?"
		    LC_TIME=POSIX ls -l $dir/lock
		else
		    echo "$prog: `cat $tmp/out`"
		fi
		_warning "failed to acquire exclusive lock ($dir/lock) ..."
		continue
	    fi
	fi

	pid=''
	if [ "$primary" = y ]
	then
	    if test -e "$PCP_TMP_DIR/pmlogger/primary"
	    then
		if $VERY_VERBOSE
		then 
		    _host=`sed -n 2p <"$PCP_TMP_DIR/pmlogger/primary"`
		    _arch=`sed -n 3p <"$PCP_TMP_DIR/pmlogger/primary"`
		    echo "... try $PCP_TMP_DIR/pmlogger/primary: host=$_host arch=$_arch"
		fi
		pid=`_get_primary_logger_pid`
		if [ -z "$pid" ]
		then
		    if $VERY_VERBOSE
		    then
			echo "primary pmlogger process pid not found"
			ls -l "$PCP_RUN_DIR/pmlogger.pid"
			ls -l "$PCP_TMP_DIR/pmlogger"
		    fi
		elif _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo "primary pmlogger process $pid identified, OK"
		else
		    $VERY_VERBOSE && echo "primary pmlogger process $pid not running"
		    pid=''
		fi
	    else
		if $VERY_VERBOSE
		then
		    echo "$PCP_TMP_DIR/pmlogger/primary: missing?"
		    echo "Contents of $PCP_TMP_DIR/pmlogger"
		    ls -l $PCP_TMP_DIR/pmlogger
		    echo "--- end of ls output ---"
		fi
	    fi
	else
	    for log in $PCP_TMP_DIR/pmlogger/[0-9]*
	    do
		[ "$log" = "$PCP_TMP_DIR/pmlogger/[0-9]*" ] && continue
		if $VERY_VERBOSE
		then
		    _host=`sed -n 2p <$log`
		    _arch=`sed -n 3p <$log`
		    $PCP_ECHO_PROG $PCP_ECHO_N "... try $log host=$_host arch=$_arch: ""$PCP_ECHO_C"
		fi
		# throw away stderr in case $log has been removed by now
		match=`sed -e '3s@/[^/]*$@@' $log 2>/dev/null | \
		$PCP_AWK_PROG '
BEGIN				{ m = 0 }
NR == 3 && $0 == "'$dir'"	{ m = 2; next }
END				{ print m }'`
		$VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "match=$match ""$PCP_ECHO_C"
		if [ "$match" = 2 ]
		then
		    pid=`echo $log | sed -e 's,.*/,,'`
		    if _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		    then
			$VERY_VERBOSE && echo "pmlogger process $pid identified, OK"
			break
		    fi
		    $VERY_VERBOSE && echo "pmlogger process $pid not running, skip"
		    pid=''
		else
		    $VERY_VERBOSE && echo "different directory, skip"
		fi
	    done
	fi

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
		    _unlock "$dir"
		    continue
		fi
	    else
		envs=`grep -h ^PMLOGGER "$PMLOGGERFARMENVS" 2>/dev/null`
		args="-h $host $args"
		iam=""
	    fi

	    # each new log started is named yyyymmdd.hh.mm
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

	    $VERBOSE && _restarting

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
	    if [ -f $logfile ]
	    then
		$VERBOSE && $SHOWME && echo
		eval $MV -f $logfile $logfile.prior
	    fi

	    # Notify service manager (if any) for the primary logger ONLY.
	    [ "$primary" = y ] && args="-N $args"

	    args="$args -m pmlogger_check"
	    if $SHOWME
	    then
		echo
		echo "+ ${sock_me}$PMLOGGER $args $LOGNAME"
		_unlock "$dir"
		continue
	    else
		$PCP_BINADM_DIR/pmpost "start pmlogger from $prog for host $host"
		# The pmlogger child will be re-parented to init (aka systemd)
		pid=`eval $envs '${sock_me}$PMLOGGER $args $LOGNAME >$tmp/out 2>&1 & echo $!'`
	    fi

	    # wait for pmlogger to get started, and check on its health
	    _check_logger $pid

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
	elif [ ! -z "$pid" -a $STOP_PMLOGGER = true ]
	then
	    # Send pmlogger a SIGTERM, which is noted as a pending shutdown.
            # Add pid to list of loggers sent SIGTERM - may need SIGKILL later.
	    #
	    $VERY_VERBOSE && echo "+ $KILL -s TERM $pid"
	    eval $KILL -s TERM $pid
	    $PCP_ECHO_PROG $PCP_ECHO_N "$pid ""$PCP_ECHO_C" >> $tmp/pmloggers
	fi

	_unlock "$dir"
    done
}

# parse and process the control file(s)
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort | sed -e "s;^;$CONTROLDIR/;g"`
for c in $CONTROL $append
do
    _parse_control "$c"
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
                pmsleep 0.1
                delay=`expr $delay - 1`
                continue
            fi
            echo "$prog: Error: pmlogger process(es) will not die"
            cat $tmp/alive
            status=1
            break
        done
    fi
fi

# Prior to exiting we compress existing logs, if any. See pmlogger_daily -K
# Do not compress on a virgin install - there is nothing to compress anyway.
# See RHBZ#1721223.
[ -f "$PCP_LOG_DIR/pmlogger/pmlogger_daily.stamp" ] && _compress_now

[ -f $tmp/err ] && status=1

# optional end logging to $PCP_LOG_DIR/NOTICES
#
if $PCP_LOG_RC_SCRIPTS
then
    $PCP_BINADM_DIR/pmpost "end pid:$$ $prog status=$status"
fi

exit
