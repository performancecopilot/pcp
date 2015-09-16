#! /bin/sh
#
# Copyright (c) 2013-2015 Red Hat.
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

PMLOGGER="$PCP_BINADM_DIR/pmlogger"
PMLOGCONF="$PCP_BINADM_DIR/pmlogconf"
PMLOGGERENVS="$PCP_SYSCONFIG_DIR/pmlogger"

# error messages should go to stderr, not the GUI notifiers
#
unset PCP_STDERR

# constant setup
#
tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=0
echo >$tmp/lock
prog=`basename $0`
PROGLOG=$PCP_LOG_DIR/pmlogger/$prog.log
USE_SYSLOG=true

_cleanup()
{
    $USE_SYSLOG && [ $status -ne 0 ] && \
    $PCP_SYSLOG_PROG -p daemon.error "$prog failed - see $PROGLOG"
    [ -s "$PROGLOG" ] || rm -f "$PROGLOG"
    lockfile=`cat $tmp/lock 2>/dev/null`
    rm -f "$lockfile"
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

# control files for pmlogger administration ... edit the entries in this
# file (and optional directory) to reflect your local configuration; see
# also -c option below.
#
CONTROL=$PCP_PMLOGGERCONTROL_PATH
CONTROLDIR=$PCP_PMLOGGERCONTROL_PATH.d

# NB: FQDN cleanup; don't guess a 'real name for localhost', and
# definitely don't truncate it a la `hostname -s`.  Instead now
# we use such a string only for the default log subdirectory, ie.
# for substituting LOCALHOSTNAME in the fourth column of $CONTROL.

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
KILL=pmsignal
TERSE=false
VERBOSE=false
VERY_VERBOSE=false
CHECK_RUNLEVEL=false
START_PMLOGGER=true

echo > $tmp/usage
cat >> $tmp/usage << EOF
Options:
  -c=FILE,--control=FILE  configuration of pmlogger instances to manage
  -l=FILE,--logfile=FILE  send important diagnostic messages to FILE
  -C                      query system service runlevel information
  -N,--showme             perform a dry run, showing what would be done
  -s,--stop               stop pmlogger processes instead of starting them
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
		USE_SYSLOG=false
		shift
		;;
	-N)	SHOWME=true
		USE_SYSLOG=false
		MV="echo + mv"
		CP="echo + cp"
		KILL="echo + kill"
		;;
        -s)	START_PMLOGGER=false
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
[ -d "$PROGLOGDIR" ] || mkdir -p -m 775 "$PROGLOGDIR" 2>/dev/null
[ -f "$PROGLOG" ] && mv "$PROGLOG" "$PROGLOG.prev"
exec 1>"$PROGLOG"
exec 2>&1

QUIETLY=false
if [ $CHECK_RUNLEVEL = true ]
then
    # determine whether to start/stop based on runlevel settings - we
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

if [ $START_PMLOGGER = false ]
then
    # if pmlogger has never been started, there's no work to do to stop it
    [ ! -d "$PCP_TMP_DIR/pmlogger" ] && exit
    $QUIETLY || $PCP_BINADM_DIR/pmpost "stop pmlogger from $prog"
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
    rm -f lock
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
    configfile="$1"
    hostname="$2"

    if [ -f "$configfile" ]
    then
        # look for "magic" string at start of file, and ensure we created it
        sed 1q "$configfile" | grep '^#pmlogconf [0-9]' >/dev/null
        magic=$?
        grep '^# Auto-generated by pmlogconf' "$configfile" >/dev/null
        owned=$?
        if [ $magic -eq 0 -a $owned -eq 0 ]
        then
            # pmlogconf file that we own, see if re-generation is needed
            cp "$configfile" $tmp/pmlogger
            if $PMLOGCONF -r -c -q -h $hostname $tmp/pmlogger >$tmp/diag 2>&1
            then
		if grep 'No changes' $tmp/diag >/dev/null 2>&1
                then
		    :
		elif [ -w $configfile ]
                then
                    $VERBOSE && echo "Reconfigured: \"$configfile\" (pmlogconf)"
                    eval $MV $tmp/pmlogger "$configfile"
                else
                    _warning "no write access to pmlogconf file \"$configfile\", skip reconfiguration"
                    ls -l "$configfile"
                fi
            else
                _warning "pmlogconf failed to reconfigure \"$configfile\""
                cat "s;$tmp/pmlogger;$configfile;g" $tmp/diag
                echo "=== start pmlogconf file ==="
                cat $tmp/pmlogger
                echo "=== end pmlogconf file ==="
            fi
        fi
    elif [ ! -e "$configfile" ]
    then
        # file does not exist, generate it, if possible
        if $SHOWME
        then
            echo "+ $PMLOGCONF -c -q -h $hostname $configfile"
        elif ! $PMLOGCONF -c -q -h $hostname "$configfile" >$tmp/diag 2>&1
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
    pidfile="$PCP_TMP_DIR/pmlogger/primary"
    if [ ! -L "$pidfile" ]
    then
	pid=''
    elif which realpath >/dev/null 2>&1
    then
	pri=`readlink $pidfile`
	pid=`basename "$pri"`
    else
	pri=`ls -l "$pidfile" | sed -e 's/.*-> //'`
	pid=`basename "$pri"`
    fi
    echo "$pid"
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
	    if echo "connect $1" | pmlc 2>&1 | grep "Unable to connect" >/dev/null
	    then
		:
	    else
		$VERBOSE && echo " done"
		return 0
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
rm -f $tmp/err $tmp/pmloggers

_parse_control()
{
    controlfile="$1"
    line=0

    sed -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" $controlfile | \
    while read host primary socks dir args
    do
	# start in one place for each iteration (beware relative paths)
	cd "$here"
	line=`expr $line + 1`

	# NB: FQDN cleanup: substitute the LOCALHOSTNAME marker in the config
	# line differently for the directory and the pcp -h HOST arguments.
	dir_hostname=`hostname || echo localhost`
	dir=`echo $dir | sed -e "s;LOCALHOSTNAME;$dir_hostname;"`
	[ "x$host" = "xLOCALHOSTNAME" ] && host=local:

	$VERY_VERBOSE && echo "[$controlfile:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""

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

	[ -f $tmp/cmd ] && . $tmp/cmd
	if [ -z "$version" -o "$version" = "1.0" ]
	then
	    if [ -z "$version" ]
	    then
		_warning "processing version 1.0 control format"
		version=1.0
	    fi
	    args="$dir $args"
	    dir="$socks"
	    socks=n
	fi

	if [ -z "$primary" -o -z "$socks" -o -z "$dir" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi

	if $VERY_VERBOSE
	then
	    pflag=''
	    [ $primary = y ] && pflag=' -P'
	    echo "Check pmlogger$pflag -h $host ... in $dir ..."
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

	# make sure output directory exists
	#
	if [ ! -d "$dir" ]
	then
	    mkdir -p -m 755 "$dir" >$tmp/err 2>&1
	    if [ ! -d "$dir" ]
	    then
		cat $tmp/err
		_error "cannot create directory ($dir) for PCP archive files"
		continue
	    else
		_warning "creating directory ($dir) for PCP archive files"
	    fi
	    chown $PCP_USER:$PCP_GROUP "$dir" >/dev/null 2>&1
	fi

	cd "$dir"
	dir=`$PWDCMND`
	$SHOWME && echo "+ cd $dir"

	# ensure pcp user will be able to write there
	#
	chown -R $PCP_USER:$PCP_GROUP "$dir" >/dev/null 2>&1
	if [ ! -w "$dir" ]
	then
	    _warning "no write access in $dir, skip lock file processing"
	else
	    # demand mutual exclusion
	    #
	    rm -f $tmp/stamp $tmp/out
	    delay=200	# tenths of a second
	    while [ $delay -gt 0 ]
	    do
		if pmlock -v lock >$tmp/out 2>&1
		then
		    echo $dir/lock >$tmp/lock
		    break
		else
		    [ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		    if [ -z "`find lock -newer $tmp/stamp -print 2>/dev/null`" ]
		    then
			if [ -f lock ]
			then
			    echo "$prog: Warning: removing lock file older than 30 minutes"
			    LC_TIME=POSIX ls -l $dir/lock
			    rm -f lock
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
		if [ -f lock ]
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
	# NB: FQDN cleanup: previously, we used to quietly accept several
	# putative-aliases in the first (hostname) slot for a primary logger,
	# which were all supposed to refer to the local host.  So now we
	# squash them all to the officially pcp-preferred way to access it.
	# This does not get used by pmlogger in the end (gets -P and not -h
	# in the primary logger case), but it *does* matter for pmlogconf.
	if [ "X$primary" = Xy ]
	then
	    host=local:

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
			ls -l "$PCP_TMP_DIR/pmlogger"
		    fi
		elif _get_pids_by_name pmlogger | grep "^$pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo "primary pmlogger process $pid identified, OK"
		else
		    $VERY_VERBOSE && echo "primary pmlogger process $pid not running"
		    pid=''
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
		match=`sed -e '3s/\/[0-9][0-9][0-9][0-9][0-9.]*$//' $log 2>/dev/null | \
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
	    rm -f Latest

	    if [ "X$primary" = Xy ]
	    then
		envs=`grep ^PMLOGGER "$PMLOGGERENVS" 2>/dev/null`
		args="-P $args"
		iam=" primary"
		# clean up port-map, just in case
		#
		PM_LOG_PORT_DIR="$PCP_TMP_DIR/pmlogger"
		rm -f "$PM_LOG_PORT_DIR/primary"
	    else
		args="-h $host $args"
		envs=""
		iam=""
	    fi

	    # each new log started is named yyyymmdd.hh.mm
	    #
	    LOGNAME=`date "+%Y%m%d.%H.%M"`

	    # handle duplicates/aliases (happens when pmlogger is restarted
	    # within a minute and LOGNAME is the same)
	    #
	    suff=''
	    for file in $LOGNAME.*
	    do
		[ "$file" = "$LOGNAME"'.*' ] && continue
		# we have a clash! ... find a new -number suffix for the
		# existing files ... we are going to keep $LOGNAME for the
		# new pmlogger below
		#
		if [ -z "$suff" ]
		then
		    for xx in 0 1 2 3 4 5 6 7 8 9
		    do
			for yy in 0 1 2 3 4 5 6 7 8 9
			do
			    [ "`echo $LOGNAME-${xx}${yy}.*`" != "$LOGNAME-${xx}${yy}.*" ] && continue
			    suff=${xx}${yy}
			    break
			done
			[ ! -z "$suff" ] && break
		    done
		    if [ -z "$suff" ]
		    then
	    		_error "unable to break duplicate clash for archive basename $LOGNAME"
		    fi
		    $VERBOSE && echo "Duplicate archive basename ... rename $LOGNAME.* files to $LOGNAME-$suff.*"
		fi
		eval $MV -f $file `echo $file | sed -e "s/$LOGNAME/&-$suff/"`
	    done

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

	    args="$args -m pmlogger_check"
	    if $SHOWME
	    then
		echo
		echo "+ ${sock_me}$PMLOGGER $args $LOGNAME"
		_unlock
		continue
	    else
		$PCP_BINADM_DIR/pmpost "start pmlogger from $prog for host $host"
		eval $envs '${sock_me}$PMLOGGER $args $LOGNAME >$tmp/out 2>&1 &'
		pid=$!
	    fi

	    # wait for pmlogger to get started, and check on its health
	    _check_logger $pid

	    # the archive folio Latest is for the most recent archive in
	    # this directory
	    #
	    if [ -f $LOGNAME.0 ] 
	    then
		$VERBOSE && echo "Latest folio created for $LOGNAME"
		mkaf $LOGNAME.0 >Latest
		chown $PCP_USER:$PCP_GROUP Latest >/dev/null 2>&1
	    else
		touch $tmp/err
		logdir=`dirname $LOGNAME`
		if $TERSE
		then
		    echo "$prog: Error: archive file `cd $logdir; $PWDCMND`/$LOGNAME.0 missing"
		else
		    echo "$prog: Error: archive file $LOGNAME.0 missing"
		    echo "Directory (`cd $logdir; $PWDCMND`) contents:"
		    LC_TIME=POSIX ls -la $logdir
		fi
	    fi

	elif [ ! -z "$pid" -a $START_PMLOGGER = false ]
	then
	    # Send pmlogger a SIGTERM, which is noted as a pending shutdown.
            # Add pid to list of loggers sent SIGTERM - may need SIGKILL later.
	    #
	    $VERY_VERBOSE && echo "+ $KILL -s TERM $pid"
	    eval $KILL -s TERM $pid
	    $PCP_ECHO_PROG $PCP_ECHO_N "$pid ""$PCP_ECHO_C" >> $tmp/pmloggers
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

# check all the SIGTERM'd loggers really died - if not, use a bigger hammer.
# 
if $SHOWME
then
    :
elif [ $START_PMLOGGER = false -a -s $tmp/pmloggers ]
then
    pmloggerlist=`cat $tmp/pmloggers`
    if $PCP_PS_PROG -p "$pmloggerlist" >/dev/null 2>&1
    then
        $VERY_VERBOSE && ( echo; $PCP_ECHO_PROG $PCP_ECHO_N "+ $KILL -KILL `cat $tmp/pmies` ...""$PCP_ECHO_C" )
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

[ -f $tmp/err ] && status=1
exit
