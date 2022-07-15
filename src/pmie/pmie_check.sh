#! /bin/sh
#
# Copyright (c) 2013-2016,2018,2020 Red Hat.
# Copyright (c) 1998-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# Administrative script to check pmie processes are alive, and restart
# them as required.
#

# Get standard environment
. $PCP_DIR/etc/pcp.env
. $PCP_SHARE_DIR/lib/rc-proc.sh
. $PCP_SHARE_DIR/lib/utilproc.sh

PMIE="$PCP_BIN_DIR/pmie"
PMIECONF="$PCP_BIN_DIR/pmieconf"

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# ensure mere mortals cannot write any configuration files,
# but that the unprivileged PCP_USER account has read access
#
umask 022

# added to handle problem when /var/log/pcp is a symlink, as first
# reported by Micah_Altman@harvard.edu in Nov 2001
#
_unsymlink_path()
{
    [ -z "$1" ] && return
    __d=`dirname $1`
    __real_d=`cd $__d 2>/dev/null && $PWDCMND`
    if [ -z "$__real_d" ]
    then
	echo $1
    else
	echo $__real_d/`basename $1`
    fi
}

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
    if [ -n "$tmp" ]
    then
	lockfile=`cat $tmp/lock 2>/dev/null`
	[ -n "$lockfile" ] && rm -f "$lockfile"
	[ -d "$tmp" ] && rm -rf $tmp
    fi
    $VERY_VERBOSE && echo "End: `date '+%F %T.%N'`"
}

# constant setup
#
tmp=''
status=0
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmie/$prog.log
MYPROGLOG=$PROGLOG.$$
USE_SYSLOG=true

trap "_cleanup; exit \$status" 0 1 2 3 15
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmie_check.XXXXXXXXX"` || exit 1
echo >$tmp/lock

# control files for pmie administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration;
# see also -c option below.
#
CONTROL=$PCP_PMIECONTROL_PATH
CONTROLDIR=$PCP_PMIECONTROL_PATH.d

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

# option parsing
#
SHOWME=false
MV=mv
RM=rm
CP=cp
KILL=pmsignal
TERSE=false
VERBOSE=false
VERY_VERBOSE=false
CHECK_RUNLEVEL=false
START_PMIE=true
STOP_PMIE=false
SKIP_PRIMARY=false
ONLY_PRIMARY=false

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -c=FILE,--control=FILE  configuration of pmie instances to manage
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -C                      query system service runlevel information
  -N,--showme             perform a dry run, showing what would be done
  -p,--skip-primary	  do not start or stop the primary pmie instance
  -P,--only-primary	  only start or stop the primary pmie instance, no others
  -s,--stop               stop pmie processes instead of starting them
  -T,--terse              produce a terser form of output
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
	-c)	CONTROL="$2"
		CONTROLDIR="$2.d"
		shift
		;;
	-C)	CHECK_RUNLEVEL=true
		;;
	-l)	PROGLOG="$2"
		MYPROGLOG="$PROGLOG".$$
		USE_SYSLOG=false
		shift
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		MV="echo + mv"
		RM="echo + rm"
		CP="echo + cp"
		KILL="echo + kill"
		;;
	-p)     SKIP_PRIMARY=true
		;;
	-P)     ONLY_PRIMARY=true
		;;

	-s)	START_PMIE=false
		STOP_PMIE=true
		;;
	-T)	TERSE=true
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
_error()
{
    echo "$prog: [$controlfile:$line]"
    echo "Error: $@"
    echo "... automated performance reasoning for host \"$host\" unchanged"
    touch $tmp/err
}

_warning()
{
    echo "$prog [$controlfile:$line]"
    echo "Warning: $@"
}

_restarting()
{
    $PCP_ECHO_PROG $PCP_ECHO_N "Restarting pmie for host \"$host\" ...""$PCP_ECHO_C"
}

_lock()
{
    # demand mutual exclusion
    #
    rm -f $tmp/stamp $tmp/out
    delay=200		# tenths of a second
    while [ $delay -ne 0 ]
    do
	if pmlock -v $logfile.lock >$tmp/out
	then
	    echo $logfile.lock >$tmp/lock
	    break
	else
	    if [ ! -f $tmp/stamp ]
	    then
		touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
	    fi
	    if [ -n "`find $logfile.lock ! -newer $tmp/stamp -print 2>/dev/null`" ]
	    then
		_warning "removing lock file older than 30 minutes"
		ls -l $logfile.lock
		rm -f $logfile.lock
	    fi
	fi
	pmsleep 0.1
	delay=`expr $delay - 1`
    done

    if [ $delay -eq 0 ]
    then
	# failed to gain mutex lock
	#
	if [ -f $logfile.lock ]
	then
	    _warning "is another PCP cron job running concurrently?"
	    ls -l $logfile.lock
	else
	    echo "$prog: `cat $tmp/out`"
	fi
	_warning "failed to acquire exclusive lock ($logfile.lock) ..."
	return 1
    fi

    return 0
}

_unlock()
{
    rm -f $logfile.lock
    echo >$tmp/lock
}

_check_logfile()
{
    if [ ! -f $logfile ]
    then
	echo "$prog: Error: cannot find pmie output file at \"$logfile\""
	if $TERSE
	then
	    :
	else
	    logdir=`dirname $logfile`
	    echo "Directory (`cd $logdir; $PWDCMND`) contents:"
	    LC_TIME=POSIX ls -la $logdir
	fi
    else
	echo "Contents of pmie output file \"$logfile\" ..."
	cat $logfile
    fi
}

# check and wait for launched pmie daemon process.
# $1 is the pid and $2 is the pmie process stdout/stderr output file
_check_pmie()
{
    $VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N " [process $1] ""$PCP_ECHO_C"

    # wait until pmie process starts, or exits
    #
    delay=5
    [ ! -z "$PMCD_CONNECT_TIMEOUT" ] && delay=$PMCD_CONNECT_TIMEOUT
    x=5
    [ ! -z "$PMCD_REQUEST_TIMEOUT" ] && x=$PMCD_REQUEST_TIMEOUT

    # wait for maximum time of a connection and 20 requests
    #
    delay=`expr \( $delay + 20 \* $x \) \* 10`	# tenths of a second
    while [ $delay -ne 0 ]
    do
	if [ -s "$2" ]; then
	    if grep -q -s "Log finished" "$2"; then
		echo "Error: pmie daemon failed to start, see $PCP_LOG_DIR/pmie/pmie_check.log"
		cat "$2"
		return 1
	    fi
	fi

	if [ -f $logfile ]
	then
	    # $logfile was previously removed, if it has appeared again then
	    # we know pmie has started ... if not just sleep and try again
	    #
	    if ls "$PCP_TMP_DIR/pmie/$1" >$tmp/out 2>&1
	    then
		if grep "No such file or directory" $tmp/out >/dev/null
		then
		    :
		else
		    $VERBOSE && echo " done"
		    return 0
		fi
	    fi

	    _plist=`_get_pids_by_name pmie`
	    _found=false
	    for _p in `echo $_plist`
	    do
		[ $_p -eq $1 ] && _found=true
	    done

	    if $_found
	    then
		# process still here, just hasn't created its status file
		# yet, try again
		:
	    else
		$VERBOSE || _restarting
		echo " process exited!"
		if $TERSE
		then
		    :
		else
		    echo "$prog: Error: failed to restart pmie"
		    echo "Current pmie processes:"
		    $PCP_PS_PROG $PCP_PS_ALL_FLAGS | tee $tmp/tmp | sed -n -e 1p
		    for _p in `echo $_plist`
		    do
			sed -n -e "/^[ ]*[^ ]* [ ]*$_p /p" < $tmp/tmp
		    done
		    echo
		fi
		_check_logfile
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
    _check_logfile
    return 1
}

_get_configfile()
{
    # extract the pmie configuration file (-c) from a list of arguments
    #
    echo $@ | sed -n \
	-e 's/^/ /' \
	-e 's/[ 	][ 	]*/ /g' \
	-e 's/-c /-c/' \
	-e 's/.* -c\([^ ]*\).*/\1/p'
}

_configure_pmie()
{
    # update a pmie configuration file if it should be created/modified
    #
    tmpconfig="$1.tmp"
    configfile="$1"
    isprimary="$3"

    if [ -f "$configfile" ]
    then
	# look for "magic" string at start of file, and ensure we created it
	sed 1q "$configfile" | grep '^// pmieconf-pmie [0-9]' >/dev/null
	magic=$?
	grep '^// Auto-generated by pmieconf' "$configfile" >/dev/null
	owned=$?
	if [ $magic -eq 0 -a $owned -eq 0 ]
	then
	    # pmieconf file, see if re-generation is needed
	    cp "$configfile" "$tmpconfig"
	    if $PMIECONF -f "$tmpconfig" -cF >$tmp/diag 2>&1
	    then
		[ $isprimary = y ] && $PMIECONF -f "$tmpconfig" -c modify primary enabled yes
		grep -v "generated by pmieconf" "$configfile" >$tmp/old
		grep -v "generated by pmieconf" "$tmpconfig" >$tmp/new
		if ! diff $tmp/old $tmp/new >/dev/null
		then
		    if [ -w "$configfile" ]
		    then
			$VERBOSE && echo "Reconfigured: \"$configfile\" (pmieconf)"
			eval $CP "$tmpconfig" "$configfile"
		    else
			_warning "no write access to pmieconf file \"$configfile\", skip reconfiguration"
			ls -l "$configfile"
		    fi
		fi
	    else
		_warning "pmieconf failed to reconfigure \"$configfile\""
		sed -e "s;$tmpconfig;$configfile;g" $tmp/diag
		echo "=== start pmieconf file ==="
		cat "$tmpconfig"
		echo "=== end pmieconf file ==="
	    fi
	    rm -f "$tmpconfig"
	fi
    elif [ ! -e "$configfile" ]
    then
	# file does not exist, generate it, if possible
	if $SHOWME
	then
	    echo "+ $PMIECONF -f $configfile -cF"
	elif ! $PMIECONF -f "$configfile" -cF >$tmp/diag 2>&1
	then
	    _warning "pmieconf failed to generate \"$configfile\""
	    cat $tmp/diag
	    echo "=== start pmieconf file ==="
	    cat "$configfile"
	    echo "=== end pmieconf file ==="
	else
	    [ $isprimary = y ] && $PMIECONF -f "$configfile" -c modify primary enabled yes
            chown $PCP_USER:$PCP_GROUP "$configfile" >/dev/null 2>&1
	fi
    fi
}

QUIETLY=false
if [ $CHECK_RUNLEVEL = true ]
then
    # determine whether to start pmie based on runlevel settings - we
    # need to do this when running unilaterally from cron, else we'll
    # always start pmie up (even when we shouldn't).
    #
    QUIETLY=true
    if is_chkconfig_on pmie
    then
	START_PMIE=true
    else
	START_PMIE=false
    fi
fi

if [ $STOP_PMIE = true ]
then
    # if pmie has never been started, there's no work to do to stop it
    [ ! -d "$PCP_TMP_DIR/pmie" ] && exit
    $QUIETLY || $PCP_BINADM_DIR/pmpost "stop pmie from $prog"
elif [ $START_PMIE = false ]
then
    exit
fi

if [ ! -f "$CONTROL" ]
then
    echo "$prog: Error: cannot find control file ($CONTROL)"
    status=1
    exit
fi

# note on control file format version
#   1.0 was the first release, and did not include the primary field
#        [this is the default for backwards compatibility]
#   1.1 adds the primary field (ala pmlogger control file) indicating
#        localhost-specific rules should be enabled
#
version=''

# if this file exists at the end, we encountered a serious error
#
rm -f $tmp/err

rm -f $tmp/pmies

_parse_control()
{
    controlfile="$1"
    line=0

    if echo "$controlfile" | grep -q -e '\.rpmsave' -e '\.rpmnew'
    then
	_warning "ignored backup control file \"$controlfile\""
	return
    fi

    sed -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" $controlfile | \
    while read host primary socks logfile args
    do
	# start in one place for each iteration (beware relative paths)
	cd "$here"
	line=`expr $line + 1`

	case "$host"
	in
	    \#*|'')	# comment or empty
		continue
		;;
	    \$*)	# in-line variable assignment
		$SHOWME && echo "# $host $primary $socks $logfile $args"
		cmd=`echo "$host $primary $socks $logfile $args" \
		     | sed -n \
			 -e "/='/s/\(='[^']*'\).*/\1/" \
			 -e '/="/s/\(="[^"]*"\).*/\1/' \
			 -e '/=[^"'"'"']/s/[;&<>|].*$//' \
			 -e '/^\\$[A-Za-z][A-Za-z0-9_]*=/{
s/^\\$//
/^\([A-Za-z][A-Za-z0-9_]*\)=/s//export \1; \1=/p
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
	if [ -f $tmp/cmd ]
	then
	    . $tmp/cmd
	    if grep 'version=' $tmp/cmd >/dev/null
	    then
		case "$version"
		in
		    1.0|1.1)
			;;
		    *)
			_error "bad version ($version) in control file"
			return
			;;
		esac
	    fi
	fi

	if [ -z "$version" ]
	then
	    _warning "missing \$version, assuming version 1.0 control format"
	    version=1.0
	fi
	if [ "$version" = "1.0" ]
	then
	    # handle backwards compatibility
	    # one less field and primary defaults to "n" for version 1.0
	    #
	    if [ -n "$logfile" ]
	    then
		args="$logfile $args"
	    else
		# missing "args" ... this is bad, but will be reported below
		# ... guard avoids setting "args" to " " which would defeat
		# the test below
		#
		args=""
	    fi
	    logfile="$socks"
	    socks="$primary"
	    primary=n
	fi

	if [ -z "$primary" -o -z "$socks" -o -z "$logfile" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi
	if [ "$primary" != y -a "$primary" != n ]
	then
	    _error "primary field in control file record must be y or n, not \"$primary\""
	    continue
	fi
	if [ "$socks" != y -a "$socks" != n ]
	then
	    _error "socks field in control file record must be y or n, not \"$socks\""
	    continue
	fi

	# args should begin with a hyphen
	#
	case "$args"
	in
	    -*)
		;;
	    *)
		_error "args field in control file must begin with a hyphen not \"$args\""
		continue
		;;
	esac

	# if -p/--skip-primary on the command line, do not process
	# a control file line for the primary pmie
	#
	if $SKIP_PRIMARY && [ $primary = y ]
	then
	    $VERY_VERBOSE && echo "Skip, -p/--skip-primary on command line"
	    continue
	fi

	# if -P/--only-primary on the command line, only process
	# the control file line for the primary pmie
	#
	if $ONLY_PRIMARY && [ $primary != y ]
	then
	    $VERY_VERBOSE && echo "Skip non-primary, -P/--only-primary on command line"
	    continue
	fi

	# substitute LOCALHOSTNAME marker in this config line
	# (differently for logfile and pcp -h HOST arguments)
	#
	logfilehost=`hostname || echo localhost`
	logfile=`echo $logfile | sed -e "s;LOCALHOSTNAME;$logfilehost;"`
	logfile=`_unsymlink_path $logfile`
	[ $primary = y -o "x$host" = xLOCALHOSTNAME ] && host=local:

	dir=`dirname $logfile`
	$VERY_VERBOSE && echo "Check pmie -h $host -l $logfile ..."

	# make sure output directory exists
	#
	if [ ! -d "$dir" ]
	then
	    mkdir_and_chown "$dir" 755 $PCP_USER:$PCP_GROUP >$tmp/tmp 2>&1
	    if [ ! -d "$dir" ]
	    then
		cat $tmp/tmp
		_error "cannot create directory ($dir) for pmie log file"
		continue
	    fi
	fi

	# and the user pcp can write there
	#
	chown $PCP_USER:$PCP_GROUP "$dir" >/dev/null 2>&1

	# and the logfile is writeable, if it exists
	#
	[ -f "$logfile" ] && chown $PCP_USER:$PCP_GROUP "$logfile" >/dev/null 2>&1

	if cd "$dir" >/dev/null 2>&1
	then
	    :
	else
	    _error "cannot chdir to directory ($dir) for pmie log file"
	    continue
	fi
	dir=`$PWDCMND`
	$SHOWME && echo "+ cd $dir"

	if [ ! -w "$dir" ]
	then
	    _warning "no write access in $dir, skip lock file processing"
	    ls -ld "$dir"
	else
	    _lock || continue
	fi

	# match $logfile from control file to running pmies
	pid=""
	for pidfile in $PCP_TMP_DIR/pmie/[0-9]*
	do
	    [ "$pidfile" = "$PCP_TMP_DIR/pmie/[0-9]*" ] && continue
	    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "... try $pidfile: ""$PCP_ECHO_C"

	    p_id=`echo $pidfile | sed -e 's,.*/,,'`
	    p_logfile=""
	    p_pmcd_host=""

	    # throw away stderr in case $pidfile has been removed by now
	    eval `$PCP_BINADM_DIR/pmiestatus $pidfile 2>/dev/null | $PCP_AWK_PROG '
NR == 2	{ printf "p_logfile=\"%s\"\n", $0; next }
NR == 3	{ printf "p_pmcd_host=\"%s\"\n", $0; next }
	{ next }'`

	    p_logfile=`_unsymlink_path $p_logfile`
	    if [ "$p_logfile" != $logfile ]
	    then
		$VERY_VERBOSE && echo "different logfile, skip"
		$VERY_VERBOSE && echo "  $p_logfile differs to $logfile"
	    elif _get_pids_by_name pmie | grep "^$p_id\$" >/dev/null
	    then
		$VERY_VERBOSE && echo "pmie process $p_id identified, OK"
		pid=$p_id
		break
	    else
		$VERY_VERBOSE && echo "pmie process $p_id not running, skip"
		$VERY_VERBOSE && _get_pids_by_name pmie
	    fi
	done

	if $VERY_VERBOSE
	then
	    if [ -z "$pid" ]
	    then
		echo "No current pmie process exists for:"
	    else
		echo "Found pmie process $pid monitoring:"
	    fi
	    echo "    host = $host"
	    echo "    log file = $logfile"
	fi

	if [ -z "$pid" -a $START_PMIE = true ]
	then
	    configfile=`_get_configfile $args`
	    if [ ! -z "$configfile" ]
	    then
		# if this is a relative path and not relative to cwd,
		# substitute in the default pmie search location.
		#
		if [ ! -f "$configfile" -a "`basename $configfile`" = "$configfile" ]
		then
		    configfile="$PCP_VAR_DIR/config/pmie/$configfile"
		fi

		# check configuration file exists and is up to date
		_configure_pmie "$configfile" "$host" "$primary"
	    fi

	    if [ "$primary" = y ]
	    then
		args="-F -P -l $logfile $args"
	    else
		args="-h $host -l $logfile $args"
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
		    echo "$prog: Warning: no pmsocks available, would run without"
		    sock_me=""
		fi
	    fi

	    [ -f "$logfile" ] && eval $MV -f "$logfile" "$logfile.prior"

	    if $SHOWME
	    then
		$VERBOSE && echo
		echo "+ ${sock_me}$PMIE -b $args"
		_unlock
		continue
	    else
		# since this is launched as a daemon, any output should
		# go on pmie's stderr, i.e. $logfile ... use -b for this
		#
		$VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ ${sock_me}$PMIE -b $args""$PCP_ECHO_C"; echo "..." )
		$PCP_BINADM_DIR/pmpost "start pmie from $prog for host $host"
		err=`mktemp "$tmp/pmie.errXXXXXXXXX"`
		pid=`${sock_me} $PMIE -b $args >$err 2>&1 & echo $!`
	    fi

	    # wait for pmie to get started, and check on its health
	    _check_pmie $pid $err

	elif [ ! -z "$pid" -a $STOP_PMIE = true ]
	then
	    # Send pmie a SIGTERM, which is noted as a pending shutdown.
	    # Add pid to list of pmies sent SIGTERM - may need SIGKILL later.
	    #
	    $VERY_VERBOSE && echo "+ $KILL -s TERM $pid"
	    eval $KILL -s TERM $pid
	    $PCP_ECHO_PROG $PCP_ECHO_N "$pid ""$PCP_ECHO_C" >> $tmp/pmies
	fi

	_unlock
    done
}

_parse_control $CONTROL
append=`ls $CONTROLDIR 2>/dev/null | LC_COLLATE=POSIX sort`
for controlfile in $append
do
    _parse_control $CONTROLDIR/$controlfile
done

# check all the SIGTERM'd pmies really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
elif [ $STOP_PMIE = true -a -s $tmp/pmies ]
then
    pmielist=`cat $tmp/pmies`
    if $PCP_PS_PROG -p "$pmielist" >/dev/null 2>&1
    then
	$VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ $KILL -KILL `cat $tmp/pmies` ...""$PCP_ECHO_C" )
	eval $KILL -s KILL $pmielist >/dev/null 2>&1
	delay=30	# tenths of a second
	while $PCP_PS_PROG -f -p "$pmielist" >$tmp/alive 2>&1
	do
	    if [ $delay -gt 0 ]
	    then
	        pmsleep 0.1
		delay=`expr $delay - 1`
		continue
	    fi
	    echo "$prog: Error: pmie process(es) will not die"
	    cat $tmp/alive
	    status=1
	    break
	done
    fi
fi

[ -f $tmp/err ] && status=1
exit
