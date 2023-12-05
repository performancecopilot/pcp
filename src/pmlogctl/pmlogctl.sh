#!/bin/sh
#
# Control program for managing pmlogger and pmie instances.
#
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2021 Red Hat.
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
# TODO
# - more than 1 -c option ... what does it mean?  the current code simply
#   and silently uses the last -c option from the command line (this
#   warrants at least a warning) ... if supported the likely semantics
#   are the union of the named classes ... unless this is allowed, a regex
#   pattern for the -c arg (classname) makes no sense
# - regex expansion for <class>
# - other sections in the "policy" files, especially with pmfind to
#   (a) at destroy, decide not to or wait some time before destroying
#       (the latter is really hard)
#

. "$PCP_DIR/etc/pcp.env"
. "$PCP_SHARE_DIR/lib/rc-proc.sh"
. "$PCP_SHARE_DIR/lib/utilproc.sh"

prog=`basename "$0"`
case "$prog"
in
    pmlogctl*)	IAM=pmlogger
		CONTROLFILE=$PCP_PMLOGGERCONTROL_PATH
    		;;
    pmiectl*)	IAM=pmie
		CONTROLFILE=$PCP_PMIECONTROL_PATH
    		;;
    *)		echo >&2 "$0: who the hell are you, bozo?"
    		exit 1
		;;
esac
CONTROLDIR=${CONTROLFILE}.d

tmp=`mktemp -d "$PCP_TMPFILE_DIR/$prog.XXXXXXXXX"` || exit 1
status=0

_cleanup()
{
    [ -n "$ACTION" -a "$ACTION" != status ] && _unlock
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

cat >$tmp/usage <<End-of-File
# Usage: [options] command [host ...]

Options:
  -a,--all                      apply action to all matching hosts
  -c=NAME,--class=NAME    	${IAM} instances belong to the NAME class [default: default]
  -C=ARGS,--checkargs=ARGS	pass command line ARGS to ${IAM}_check
  -f,--force                    force action if possible
  -i=IDENT,--ident=IDENT        over-ride instance id (only for create and cond-create)
  -m,--migrate			migrate matching processes to farm services (for create and check)
  -N,--showme             	perform a dry run, showing what would be done
  -p=POLICY,--policy=POLICY	use POLICY as the class policy file [default: $PCP_ETC_DIR/pcp/${IAM}/class.d/<class>]
  -V,--verbose            	increase verbosity
  --help
End-of-File

_warning()
{
    echo >&2 "Warning: $1"
}

_error()
{
    echo >&2 "Error: $1"
    status=1
    exit
}

_lock()
{
    $SHOWME && return

    # can assume $__dir is writeable ... if we get this far we're running
    # as root ...
    #
    __dir="$PCP_ETC_DIR/pcp/${IAM}"
    # demand mutual exclusion
    #
    rm -f $tmp/stamp $tmp/out
    __delay=200		# 1/10 of a second, so max wait is 20 sec
    while [ $__delay -gt 0 ]
    do
	if pmlock -v "$__dir/lock" >>$tmp/out 2>&1
	then
	    echo "$$" >"$__dir/lock"
	    break
	else
	    [ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
	    find $tmp/stamp -newer "$__dir/lock" -print 2>/dev/null >$tmp/tmp
	    if [ -s $tmp/tmp ]
	    then
		if [ -f "$__dir/lock" ]
		then
		    _warning "removing lock file older than 30 minutes (PID `cat $__dir/lock`)"
		    LC_TIME=POSIX ls -l "$__dir/lock"
		    rm -f "$__dir/lock"
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
	__delay=`expr $__delay - 1`
    done

    if [ $__delay -eq 0 ]
    then
	# failed to gain mutex lock
	#
	if [ -f "$__dir/lock" ]
	then
	    _warning "is another $prog job running concurrently?"
	    LC_TIME=POSIX ls -l "$__dir/lock"
	else
	    _error "`cat $tmp/out`"
	fi
	_error "failed to acquire exclusive lock ($__dir/lock) ..."
	return 1
    else
	if $VERY_VERBOSE
	then
	    echo "Lock acquired `cat $__dir/lock` `ls -l $__dir/lock`"
	fi
    fi

    return 0
}

_unlock()
{
    $SHOWME && return
    __dir="$PCP_ETC_DIR/pcp/${IAM}"
    if [ -f "$__dir/lock" ]
    then
	rm -f "$__dir/lock"
	$VERY_VERBOSE && echo "Lock released"
    fi
}

# FreeBSD's egrep does not support -r nor -Z
#
# This variant accepts -r or -rl as the first argument ...
# -r always outputs the filename at the start of the line (no matter
# how may filenames are processed, followed by a | (using a : causes
# all manner of problems with the hostname local:) followed by line
# of text from the file that matches the pattern
#
_egrep()
{
    if [ "$1" = "-rl" ]
    then
	__text=false
    elif [ "$1" = "-r" ]
    then
	__text=true
    else
	echo >&2 "Botch: _egrep() requires -r or -rl, not $1"
	return
    fi
    shift
    __pat="$1"
    shift

    # skip errors from find(1) and egrep(1), only interested in matches for
    # real, existing files
    #
    find "$@" -type f 2>/dev/null \
    | while read __f
    do
	if echo "$__f" | grep -q -e '\.rpmsave$' -e '\.rpmnew$' -e '\.rpmorig$' \
	    -e '\.dpkg-dist$' -e '\.dpkg-old$' -e '\.dpkg-new$' >/dev/null 2>&1
	then
	    # ignore backup packaging files (daily and check scripts warn).
	    continue
	fi
	# possible race here with async execution of ${IAM}_check removing
	# the file after find saw it ... so check again for existance
	#
	[ -f "$__f" ] && grep -E "$__pat" "$__f" 2>/dev/null >$tmp/_egrep
	if [ -s $tmp/_egrep ]
	then
	    if $__text
	    then
		sed -e "s;^;$__f|;" $tmp/_egrep
	    else
		echo "$__f"
	    fi
	fi
    done
    rm -f $tmp/_egrep
}

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage 2>&1 \
    | sed >&2 -e 's/ \[default/\
                        [default/'
    cat >&2 <<End-of-File

Avaliable commands:
   [-c classname] create  host ...
   {-c classname|-i ident} cond-create host ...
   [-c classname] {start|stop|restart|destroy|check|status} [host ...]

   and host may be a valid hostname or an egrep(1) pattern that matches
   the start of a hostname
End-of-File
    status=1
    exit
}

# find matching hosts from command line args ...
# 1. find control lines that contain each named host (or all hosts in the
#    case of no hosts on the command line)
# 2. if --class is specified, then restrict the hosts from 1. to those that
#    are in the named class
#
# Output file $tmp/args has this format
# <controlfile> <class> <host> <primary> <socks> <dir> <args> ...
#
_get_matching_hosts()
{
    rm -f $tmp/args
    if [ $# -eq 0 ]
    then
	# this regexp matches the start of all possible lines that
	# could be ${IAM} control lines, e.g
	# somehostname n ...
	#
	set -- '[^#$]'
    fi
    for _host
    do
	$VERY_VERBOSE && echo "Looking for host $_host in class $CLASS ..."
	rm -f $tmp/primary_seen
	if [ "$_host" = "$LOCALHOST" ]
	then
	    pat="($_host|LOCALHOSTNAME)"
	else
	    pat="$_host"
	fi
	_egrep -r "^($pat|#!#$pat)" $CONTROLFILE $CONTROLDIR \
	| sed -e 's/|/ /' \
	| while read ctl_file host primary socks dir args
	do
	    # do shell expansion of $dir if needed ... may juggle
	    # $dir and $args to ensure correct white space separation
	    # after shell expansion
	    #
	    # small problem exposed by qa/1216 ... if $pat starts .*
	    # we may pick up comments and _do_dir_and_args tries to
	    # parse something that is not a real control line
	    #
	    case "$host"
	    in
		\#!#*)
			_do_dir_and_args
			;;

		\#*)	# other non-control comment line
			;;
		*)
			_do_dir_and_args
			;;
	    esac
	    ctl_line="$host $primary $socks $dir $args"

	    # the pattern above returns all possible control lines, but
	    # may need some further culling
	    #
	    ctl_host="`echo "$ctl_line" | sed -e 's/[ 	].*//'`"
	    if echo "$_host" | grep '^[a-zA-Z0-9][a-zA-Z0-9.-]*$' >/dev/null
	    then
		# $_host is a syntactically correct hostname so we need
		# an exact match on the first field (up to the first white
		# space)
		#
		if [ "$ctl_host" = "$_host" -o "$ctl_host" = "#!#$_host" ]
		then
		    :
		elif [ "$_host" = "$LOCALHOST" ]
		then
		    if [ "$ctl_host" = "LOCALHOSTNAME" -o "$ctl_host" = "#!#LOCALHOSTNAME" ]
		    then
			:
		    else
			# false match
			continue
		    fi
		else
		    # false match
		    continue
		fi
	    else
		# otherwise assume $_host is a regexp and this could match
		# all manner of lines, including comments (consider .*pat)
		#
		if echo "$ctl_host" | grep -E "^($pat|#!#$pat)" >/dev/null
		then
		    # so far so good (matches first field, not just whole
		    # line ... still some false matches to weed out
		    #
		    ok=false
		    case "$ctl_host"
		    in
			\#!\#*)
			    ok=true
			    ;;
			\#*)
			    ;;
			*)
			    ok=true
			    ;;
		    esac
		    $ok || continue;
		else
		    # false match
		    continue
		fi
	    fi
	    ctl_line=`echo "$ctl_line" | _expand_control | sed -e 's/^#!#//'`
	    check=`echo "$ctl_line" | wc -w | sed -e 's/ //g'`
	    if [ "$check" -lt 4 ]
	    then
		# bad control line ... missing at least directory, so warn and
		# ignore
		#
		_warning "$ctl_file: insufficient fields in control line for host `echo "$ctl_line" | sed -e 's/ .*//'`"
		continue
	    fi
	    primary=`echo "$ctl_line" | $PCP_AWK_PROG '{ print $2 }'`
	    if [ "$primary" = y ]
	    then
		touch $tmp/primary_seen
		if $EXPLICIT_CLASS || [ "$ACTION" = status ]
		then
		    # primary is not a concern here
		    #
		    :
		else
		    # don't dink with the primary ... systemctl (or the
		    # "rc" script) must be used to control the primary ${IAM}
		    #
		    if [ "$ACTION" != "check" ]; then
			_warning "$ctl_file: cannot $ACTION the primary ${IAM} from $prog"
		    fi
		    continue
		fi
	    fi
	    echo "$ctl_file" "$ctl_line"
	done >$tmp/tmp
	if $VERY_VERBOSE
	then
	    echo "Candidate control files:"
	    sed -e 's/ .*//' <$tmp/tmp \
	    | LC_COLLATE=POSIX sort \
	    | uniq
	fi
	if $EXPLICIT_CLASS
	then
	    cat $tmp/tmp \
	    | while read control host primary socks dir args
	    do
		if [ "$primary" = y ]
		then
		    class="primary"
		else
		    class=`_get_class "$control" "$host" "$dir"`
		fi
		if [ "$class" = "$CLASS" ]
		then
		    echo "$control" "$class" "$host" "$primary" "$socks" "$dir" "$args" >>$tmp/tmp2
		elif [ -z "$class" -a "$CLASS" = default ]
		then
		    echo "$control" default "$host" "$primary" "$socks" "$dir" "$args" >>$tmp/tmp2
		else
		    $VERY_VERBOSE && echo "No match for control $control host $host directory $dir class $class"
		fi
	    done
	    if [ -s $tmp/tmp2 ]
	    then
		mv $tmp/tmp2 $tmp/tmp
	    else
		rm $tmp/tmp
		touch $tmp/tmp
	    fi
	    if $VERY_VERBOSE
	    then
		echo "Matching control files:"
		sed -e 's/ .*//' $tmp/tmp
	    fi
	else
	    # add "class" of "-" to make $tmp/tmp format the same in
	    # both cases
	    #
	    sed <$tmp/tmp >$tmp/tmp2 -e 's/ / - /'
	    mv $tmp/tmp2 $tmp/tmp
	fi
	ninst=`wc -l <$tmp/tmp | sed -e 's/ //g'`
	if [ "$ninst" -eq 0 ]
	then
	    if [ "$ACTION" = create ]
	    then
		# that's good ...
		:
	    elif $FIND_ALL_HOSTS
	    then
		if $EXPLICIT_CLASS
		then
		    _warning "no host defined in class $CLASS for any ${IAM} control file"
		elif [ -f $tmp/primary_seen ]
		then
		    # Warning reported above, don't add chatter here
		    #
		    :
		else
		    _warning "no host defined in any ${IAM} control file"
		fi
	    else
		if $EXPLICIT_CLASS
		then
		    _warning "host $_host not defined in class $CLASS for any ${IAM} control file"
		elif [ -f $tmp/primary_seen ]
		then
		    # Warning reported above, don't add chatter here
		    #
		    :
		else
		    _warning "host $_host not defined in any ${IAM} control file"
		fi
	    fi
	    continue
	fi
	if [ "$ACTION" != status ]
	then
	    $PCP_AWK_PROG <$tmp/tmp '$4 != "?" { print $3 }' \
	    | LC_COLLATE=POSIX sort \
	    | uniq -c \
	    | grep -v ' 1 ' >$tmp/tmp2
	    if [ -s $tmp/tmp2 ] && ! $DOALL
	    then
		dups=`$PCP_AWK_PROG <$tmp/tmp2 '{ print $2 }' | tr '\012' ' ' | sed -e 's/  *$//'`
		if $EXPLICIT_CLASS
		then
		    _error "host(s) ($dups) defined in class $CLASS multiple times, don't know which instance to $ACTION"
		else
		    _error "host(s) ($dups) defined multiple times, don't know which instance to $ACTION"
		fi
	    fi
	fi

	cat $tmp/tmp >>$tmp/args
    done
    if [ -f $tmp/args ]
    then
	if $VERY_VERBOSE
	then
	    echo "_get_matching_hosts results:"
	    echo "# control class host dir"
	    cat $tmp/args \
	    | while read control class host primary socks dir other
	    do
		echo "$control $class $host $dir"
	    done
	    echo "# end"
	fi
    fi
}

# get class for a specific ${IAM} instance
# $1 = control
# $2 = host (expanded) [need to match either expanded or unexpanded names]
# $3 = directory (expanded) [need to match either expanded or unexpanded name
#      or name with just $PCP_* vars unexpanded]
#
_get_class()
{
    control="$1"
    # need space at end so hostname looks like it does in a control line
    host="`echo "$2 " | _unexpand_control | sed -e 's/ $//'`"
    dir="`echo "$3" | _unexpand_control`"
    alt_dir="`echo "$3" | _unexpand_pcp_control`"
    class=`$PCP_AWK_PROG <"$control" '
BEGIN			{ class = "" }
/^[$]class=/		{ class = $1; sub(/[$]class=/,"",class) }
$4 == "'"$dir"'" || $4 == "'"$alt_dir"'" || $4 == "'"$3"'" 	{
			  if ($1 == "'"$host"'" || $1 == "#!#'"$host"'" ||
			      $1 == "'"$2"'" || $1 == "#!#'"$2"'") {
				  print class
				  exit
			  }
			}'`
    [ -z "$class" ] && class=default
    echo "$class"
}

# $1 is policy file (known to already exist)
# $2 is section name (expect alphabetic(s): at start of line)
#
_get_policy_section()
{
    $PCP_AWK_PROG <"$1" '
NF == 0			{ next }
$1 == "['"$2"']"	{ want = 1; next }
$1 ~ /^\[[a-z]*]$/	{ want = 0; next }
want == 1		{ print }'
}

# find the PID for the ${IAM} that is dinking in the $1 directory
#
_get_pid()
{
    if [ ${IAM} = pmlogger ]
    then
	_egrep -rl "^$1/[^/]*$" $PCP_TMP_DIR/${IAM} \
	| sed -e 's;.*/;;' \
	| grep -f $tmp/pids
    else
	$PCP_BINADM_DIR/pmie_dump_stats $PCP_TMP_DIR/${IAM}/* 2>&1 \
	| grep ":logfile=$1" \
	| sed -e 's/:.*//' \
	| grep -f $tmp/pids
    fi
}

# do what ${IAM}_check does to a control line in terms of variable
# expansion
# 
_expand_control()
{
    sed \
	-e 's/[ 	][ 	]*/ /g' \
	-e "s;PCP_ARCHIVE_DIR/;$PCP_ARCHIVE_DIR/;g" \
	-e "s;PCP_LOG_DIR/;$PCP_LOG_DIR/;g" \
	-e "s/^LOCALHOSTNAME /$LOCALHOST /g" \
	-e "s/\\([^a-zA-Z0-9]\\)LOCALHOSTNAME/\\1$LOCALHOST/g" \
    # end
}

# reverse the changes from _expand_control()
#
_unexpand_control()
{
    sed \
	-e "s;$PCP_ARCHIVE_DIR/;PCP_ARCHIVE_DIR/;g" \
	-e "s;$PCP_LOG_DIR/;PCP_LOG_DIR/;g" \
	-e "s/^$LOCALHOST /LOCALHOSTNAME /g" \
	-e "s/\\([^a-zA-Z0-9]\\)$LOCALHOST/\\1LOCALHOSTNAME/g" \
    # end
}

# reverse the changes for just the critical $PCP_* vars from
# _expand_control()
#
_unexpand_pcp_control()
{
    sed \
	-e "s;$PCP_ARCHIVE_DIR/;PCP_ARCHIVE_DIR/;g" \
	-e "s;$PCP_LOG_DIR/;PCP_LOG_DIR/;g" \
    # end
}

# verbose diagosis of failed state
# $1 = host
# $2 = dir (pmlogger) or logfile (pmie)
#
_diagnose()
{
    if [ ${IAM} = pmlogger ]
    then
	if [ -f "$2/pmlogger.log" ]
	then
	    sed <"$2/pmlogger.log" \
		-e '/^[ 	]*$/d' \
		-e '/^preprocessor cmd:/d' \
		-e '/^Config parsed/d' \
		-e '/^Group \[/,/^} logged/d' \
		-e 's/^/   + /' \
	    # end
	else
	    echo "   + pmlogger.log not available"
	fi
    else
	# TODO ... need some filtering here for pmie logs
	if [ -f "$2" ]
	then
	    sed <"$2" \
		-e '/^[ 	]*$/d' \
		-e 's/^/   + /' \
	    # end
	else
	    echo "   + pmie.log not available"
	fi
    fi
}

# check ${IAM} really started
#
# $1 = dir as it appears on the $PCP_TMP_DIR/${IAM} files (so a real path,
#      not a possibly sybolic path from a control file)
#
_check_started()
{
    $SHOWME && return 0
    dir="$1"
    max=600		# 1/10 of a second, so 1 minute max
    i=0
    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "Started? ""$PCP_ECHO_C"
    while [ $i -lt $max ]
    do
	$VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	# rebuild active pids list, then check for our $dir
	_get_pids_by_name ${IAM} | sed -e 's/.*/^&$/' >$tmp/pids
	pid=`_get_pid "$dir"`
	[ -n "$pid" ] && break
	i=`expr $i + 1`
	pmsleep 0.1
    done
    if [ -z "$pid" ]
    then
	$VERY_VERBOSE && $PCP_ECHO_PROG " no"
	_warning "${IAM} failed to start for host $host and directory $dir"
	sts=1
    else
	$VERY_VERBOSE && $PCP_ECHO_PROG " yes"
	if $MIGRATE
	then
	    # Add new process to the farm service (pmlogger_farm or pmie_farm).
	    # It will be removed automatically if/when it exits.
	    $VERBOSE && vflag="-v"
	    migrate_pid_service $vflag "$pid" ${IAM}_farm.service
	fi
	sts=0
    fi
    return $sts
}

# check ${IAM} really stopped
#
# $1 = dir as it appears on the $PCP_TMP_DIR/${IAM} files (so a real path,
#      not a possibly symbolic path from a control file)
#
_check_stopped()
{
    $SHOWME && return 0
    dir="$1"
    max=50		# 1/10 of a second, so 5 secs max
    i=0
    $VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N "Stopped? ""$PCP_ECHO_C"
    while [ $i -lt $max ]
    do
	$VERY_VERBOSE && $PCP_ECHO_PROG $PCP_ECHO_N ".""$PCP_ECHO_C"
	# rebuild active pids list, then check for our $dir
	_get_pids_by_name ${IAM} | sed -e 's/.*/^&$/' >$tmp/pids
	pid=`_get_pid "$dir"`
	[ -z "$pid" ] && break
	i=`expr $i + 1`
	pmsleep 0.1
    done
    if [ -n "$pid" ]
    then
	$VERY_VERBOSE && $PCP_ECHO_PROG " no"
	_warning "${IAM} failed to stop for host $host and directory $dir (PID=$pid)"
	sts=1
    else
	$VERY_VERBOSE && $PCP_ECHO_PROG " yes"
	sts=0
    fi
    return $sts
}

# status command
#
_do_status()
{
    if [ ${IAM} = pmlogger ]
    then
	if $VERBOSE
	then
	    fmt="%-20s %-17s %-8s %7s %-8s %s\n"
	else
	    fmt="%-20s %-17s %-8s %7s %-8s\n"
	fi
    else
	if $VERBOSE
	then
	    fmt="%-20s %5s %11s %-8s %7s %-8s %s\n"
	else
	    fmt="%-20s %5s %11s %-8s %7s %-8s\n"
	fi
    fi

    PICK_HOSTS=false
    [ -s $tmp/args ] && PICK_HOSTS=true

    # see if system-level controls have stopped (all) ${IAM} processes
    #
    systemctl_state=''
    if which systemctl >/dev/null 2>&1
    then
	if [ -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/${IAM}.service" ]
	then
	    # systemctl is handling this
	    #
	    if [ "`systemctl is-enabled ${IAM}.service`" = enabled ]
	    then
		if [ "`systemctl is-active ${IAM}.service`" = inactive ]
		then
		    systemctl_state='stopped by systemctl'
		fi
	    else
		systemctl_state='disabled by systemctl'
	    fi
	fi
    fi

    if [ ${IAM} = pmlogger ]
    then
	# for pmlogger the entry here is the full pathname of
	# the current archive
	#
	find $PCP_TMP_DIR/${IAM} -type f -a ! -name primary \
	| while read f
	do
	    # skip entries if the process is no longer running
	    #
	    _pid=`echo "$f" \
	          | sed -e "s;^$PCP_TMP_DIR/${IAM}/;;" \
		  | grep -f $tmp/pids`
	    [ -z "$_pid" ] && continue
	    sed -n -e 3p $f \
	    | _expand_control
	done >>$tmp/archive
    else
	# for pmie, the entry here is ...
	# pid:logfile:eval_actual
	#
	$PCP_BINADM_DIR/pmie_dump_stats $PCP_TMP_DIR/pmie/* 2>&1 \
	| $PCP_AWK_PROG -F':' '
BEGIN			{ OFS = ":" }
$2 ~ /logfile=/		{ logfile = $2
			  sub(/^logfile=/,"",logfile)
			}
$2 ~ /numrules=/	{ rules = $2
			  sub(/^numrules=/,"",rules)
			}
$2 ~ /eval_actual=/	{ evals = $2
			  sub(/^eval_actual=/,"",evals)
			  print $1,logfile,rules,evals
			}' \
	| _expand_control >$tmp/pmiestats
    fi

    find $CONTROLFILE $CONTROLDIR -type f 2>/dev/null \
    | while read control
    do
	class=''
	sed <"$control" -n \
	    -e '/^[^#]/p' \
	    -e '/^#!#/p ' \
	| _expand_control \
	| while read host primary socks dir args
	do
	    # do shell expansion of $dir if needed ... may juggle
	    # $dir and $args to ensure correct white space separation
	    # after shell expansion
	    #
	    _do_dir_and_args

	    state=running
	    case "$host"
	    in
		\$class=*)	class=`echo "$host" | sed -e 's/.*=//'`
				continue
				;;
		\$*)		continue
				;;
		\#!\#*)		host=`echo "$host" | sed -e 's/^#!#//'`
				state="stopped by $prog"
				;;
	    esac
	    if [ -z "$dir" ]
	    then
		# bad control line ... already reported in
		# _get_matching_hosts() before _do_status() was called,
		# so silently ignore it here
		#
		continue
	    fi
	    if $PICK_HOSTS
	    then
		# remove this one from $tmp/args ... so at the end we can
		# see if any have been missed
		#
		rm -f $tmp/match
		$PCP_AWK_PROG <$tmp/args >$tmp/tmp '
BEGIN				{ found = 0 }
found == 0 && $3 == "'"$host"'" && $6 == "'"$dir"'"	{ print NR >>"'$tmp/match'"; found = 1; next }
				{ print }'
		if [ -f $tmp/match ]
		then
		    mv $tmp/tmp $tmp/args
		else
		    continue
		fi
	    fi
	    archive=''
	    evals=''
	    if [ ${IAM} = pmlogger ]
	    then
		archive=`grep "^$dir/[^/]*$" $tmp/archive \
			 | sed -e 's;.*/;;'`
		check=`echo "$archive" | wc -l | sed -e 's/ //g'`
		if [ "$check" -gt 1 ]
		then
		    cat >&2 $tmp/archive
		    ls >&2 -l $PCP_TMP_DIR/${IAM}
		    _error "Botch: more than one archive matches directory $dir"
		fi
		pid=`_egrep -rl "^$dir/[^/]*$" $PCP_TMP_DIR/${IAM} \
		     | sed -e 's;.*/;;' \
		     | grep -f $tmp/pids`
		[ -z "$archive" ] && archive='?'
		[ -z "$pid" ] && pid='?'
	    else
		pid=''
		rules=''
		evals=''
		eval `$PCP_AWK_PROG -F':' <$tmp/pmiestats '$2 == "'"$dir"'" { print "pid=" $1 " rules=" $3 " evals=" $4 }'`
		[ -z "$pid" ] && pid='?'
		[ -z "$rules" ] && rules='?'
		[ -z "$evals" ] && evals='?'
	    fi
	    [ -z "$class" ] && class=default
	    if [ "$archive" = '?' -o "$evals" = '?' ]
	    then
		if [ "$state" = running ]
		then
		    if [ -n "$systemctl_state" ]
		    then
			state="$systemctl_state"
		    else
			state="dead"
		    fi
		fi
	    fi
	    $VERBOSE && state="$state|$dir"
	    if [ "$primary" = y ]
	    then
		# "primary" is a pseudo-class and in particular don't set
		# $class as this may screw up the next pmlogger/pmie line
		# (if any) in this control file
		#
		if [ ${IAM} = pmlogger ]
		then
		    printf "$fmt" "$host" "$archive" "primary" "$pid" "$state"
		else
		    printf "$fmt" "$host" "$rules" "$evals" "primary" "$pid" "$state"
		fi
	    else
		if [ ${IAM} = pmlogger ]
		then
		    printf "$fmt" "$host" "$archive" "$class" "$pid" "$state"
		else
		    printf "$fmt" "$host" "$rules" "$evals" "$class" "$pid" "$state"
		fi
	    fi
	done
    done \
    | LC_COLLATE=POSIX sort >$tmp/out

    if [ -s $tmp/out ]
    then
	if [ ${IAM} = pmlogger ]
	then
	    if $VERBOSE
	    then
		printf "$fmt" "pmcd Host" Archive Class PID State "Instance Id"
	    else
		printf "$fmt" "pmcd Host" Archive Class PID State
	    fi
	else
	    if $VERBOSE
	    then
		printf "$fmt" "pmcd Host" Rules Evaluations Class PID State "Instance Id"
	    else
		printf "$fmt" "pmcd Host" Rules Evaluations Class PID State
	    fi
	fi
	if $VERBOSE
	then
	    if [ ${IAM} = pmlogger ]
	    then
		cat $tmp/out \
		| while read host archive class pid state
		do
		    dir=`echo "$state" | sed -e 's/.*|//'`
		    state=`echo "$state" | sed -e 's/|.*//'`
		    if [ ${IAM} = pmlogger ]
		    then
			ident=`echo "$dir" | sed -e 's;.*/;;'`
		    else
			ident=`echo "$dir" | sed -e 's;/pmie.log;;' -e 's;.*/;;'`
		    fi
		    printf "$fmt" "$host" "$archive" "$class" "$pid" "$state" "$ident"
		    if [ "$state" = dead ]
		    then
			_diagnose "$host" "$dir" 
		    fi
		done
	    else
		cat $tmp/out \
		| while read host rules evals class pid state
		do
		    dir=`echo "$state" | sed -e 's/.*|//'`
		    state=`echo "$state" | sed -e 's/|.*//'`
		    ident=`echo "$dir" | sed -e 's;/[^/]*$;;' -e 's;.*/;;'`
		    printf "$fmt" "$host" "$rules" "$evals" "$class" "$pid" "$state" "$ident"
		    if [ "$state" = dead ]
		    then
			_diagnose "$host" "$dir" 
		    fi
		done
	    fi
	else
	    cat $tmp/out
	fi
    fi
    if [ -s $tmp/args ]
    then
	echo "No ${IAM} configuration found for:"
	cat $tmp/args \
	| while read control class args_host primary socks args_dir args
	do
	    if [ X"$class" != X- ]
	    then
		echo "  host $args_host directory $args_dir class $class"
	    else
		echo "  host $args_host directory $args_dir"
	    fi
	done
    fi
}

# build aggregated ${IAM} config file from multiple selected control
# files
#
# $1 = the remote host
# $2 ... = the control files
#
_resolve_configs()
{
    _host="$1"
    shift
    rm -f $tmp/config $tmp/done_conf
    rm -f $tmp/config.0 $tmp/config.1 $tmp/config.2 $tmp/config.3
    for c
    do
	sed -n <$c \
	    -e 's/[ 	][ 	]*/ /g' \
	    -e '/^#/d' \
	    -e '/^\$/d' \
	    -e '/^ *$/d' \
	    -e '/ -c/{
s/.*-c *\([^ ]*\).*/\1/p
}' \
	| while read config
	do
	    if [ ! -f "$config" ]
	    then
		# config does not exist, would normally expect it to be
		# created at the first use in ${IAM}_check ... so do that
		# now, unless it has already been done
		#
		[ -f $tmp/done_conf ] && continue
		rm -f $tmp/tmp
		if [ ${IAM} = pmlogger ]
		then
		    if ! pmlogconf -c -q -h "$_host" $tmp/tmp </dev/null >$tmp/err 2>&1
		    then
			_warning "pmlogconf failed"
			cat $tmp/diag
			echo "=== start pmlogconf file ==="
			cat $tmp/tmp
			echo "=== end pmlogconf file ==="
			continue
		    fi
		else
		    if ! pmieconf -cF -f $tmp/tmp </dev/null 2>$tmp/err 2>&1
		    then
			_warning "pmieconf failed"
			cat $tmp/diag
			echo "=== start pmieconf file ==="
			cat $tmp/tmp
			echo "=== end pmieconf file ==="
			continue
		    fi
		fi
		config=$tmp/tmp
		touch $tmp/done_conf
	    fi
	    # now have the desired config file for this class ... split
	    # it into parts:
	    # 0 - any #! and preamble before the first config or conf lines
	    # 1 - any pm{log,ie}conf lines
	    # 2 - any config lines
	    # 3 - any [access] section
	    #
	    rm -f $tmp/[0-3]
	    $PCP_AWK_PROG <"$config" '
BEGIN		{ part = 2; state = 0 }
NR == 1 && /^#pmlogconf /		{ part = 0 }
NR == 1 && /^\/\/ pmieconf-pmie/	{ part = 0 }
state == 1 && $1 == "#+"		{ state = 2; part = 1 }
state == 3 && $1 !~ /^#/		{ state = 4; part = 2 }
/^\/\/ --- START GENERATED SECTION (do not change this section) ---/ \
					{ part = 1 }
/^\[access]/				{ part = 3 }
					{ print >"'$tmp/'" part }
/^# DO NOT UPDATE THE INITIAL SECTION OF THIS FILE/ \
					{ state = 1 }
/^# DO NOT UPDATE THE FILE ABOVE THIS LINE/ \
					{ state = 3 }
/^\/\/ --- END GENERATED SECTION (changes below will be preserved) ---/ \
					{ part = 2 }'
	    if $VERY_VERY_VERBOSE
	    then
		echo "$config split ->"
		for p in 0 1 2 3
		do
		    echo "--- part $p ---"
		    [ -f $tmp/$p ] && cat $tmp/$p
		done
		echo "--- end parts ---"
	    fi
	    if [ -f $tmp/0 ]
	    then
		if [ -f $tmp/config.0 ]
		then
		    : TODO, may be different?
		else
		    mv $tmp/0 $tmp/config.0
		fi
	    fi
	    # we concat these blocks of pm{log,ie}conf controls and
	    # config fragments ... pm{log,ie}conf will cull any
	    # duplicates when the config is regenerated in pm${IAM}_check
	    #
	    [ -f $tmp/1 ] && cat $tmp/1 >>$tmp/config.1
	    # concat these explicit config fragments together
	    [ -f $tmp/2 ] && cat $tmp/2 >>$tmp/config.2
	    if [ -f $tmp/3 ]
	    then
		if [ -f $tmp/config.3 ]
		then
		    : TODO, may be different?
		else
		    mv $tmp/3 $tmp/config.3
		fi
	    fi
	done
    done

    # assemble to final config file ...
    #
    for p in 0 1 2 3
    do
	[ -f $tmp/config.$p ] && cat $tmp/config.$p >>$tmp/config
    done
    touch $tmp/config
}

# cond-create command
#
_do_cond_create()
{
    sts=0
    FROM_COND_CREATE=true
    __POLICY="$POLICY"		# value on entry, POLICY gets reset below

    for host
    do
	echo 0 >$tmp/condition-true
	# if no -p, then we're going to use all the class policy files,
	# unless none exist in which case we'll use the default policy.
	#
	if [ "$__POLICY" = $tmp/policy ]
	then
	    find "$PCP_ETC_DIR/pcp/${IAM}/class.d" -type f \
	    | sed -e '/class.d\/pmfind$/d' >$tmp/class
	    if [ -s $tmp/class ]
	    then
		# we have user-defined classes, use 'em first, then
		# marker, then the default pmfind class
		#
		cat $tmp/class
		echo "End-of-User-Classes"
		echo "$PCP_ETC_DIR/pcp/${IAM}/class.d/pmfind"
	    else
		# fallback to the default pmfind class
		#
		echo "$PCP_ETC_DIR/pcp/${IAM}/class.d/pmfind"
	    fi
	else
	    # explicit policy file from command line -p or implicit policy
	    # file from command line -c ... use that
	    #
	    echo "$__POLICY"
	fi \
	| while read policy
	do
	    if [ "$policy" = "End-of-User-Classes" ]
	    then
		if [ "`cat $tmp/condition-true`" -gt 0 ]
		then
		    $VERY_VERBOSE && echo "host: $host condition true for some class, skip pmfind class"
		    break
		fi
		continue
	    fi
	    _get_policy_section "$policy" create >$tmp/cond
	    if [ -s $tmp/cond ]
	    then
		# expect func(args...)
		#
		sed -e '/^#/d' <$tmp/cond \
		| grep -v '[a-z][^(]*(.*)[ 	]*$' >$tmp/tmp
		if [ -s $tmp/tmp ]
		then
		    _warning "$policy: bad create clause(s) will be ignored"
		    cat >&2 $tmp/tmp
		fi
		rm -f $tmp/match
		grep '[a-z][^(]*(.*)[ 	]*$' <$tmp/cond \
		| sed -e 's/(/ /' -e 's/)[ 	]*$//' \
		| while read func args
		do
		    case "$func"
		    in
			exists)
			    if pminfo -h "$host" "$args" >/dev/null 2>&1
			    then
				touch $tmp/match
				$VERBOSE && echo "$policy: host $host exists($args) true"
				break
			    else
				$VERY_VERBOSE && echo "$policy: host $host exists($args) false"
			    fi
			    ;;

			values)
			    if pmprobe -h "$host" "$args" 2>/dev/null \
			       | $PCP_AWK_PROG '
BEGIN	{ sts=1 }
$2 > 0	{ sts=0; exit }
END	{ exit(sts) }'
			    then
				touch $tmp/match
				$VERBOSE && echo "$policy: host $host values($args) true"
				break
			    else
				$VERY_VERBOSE && echo "$policy: host $host values($args) false"
			    fi
			    ;;

			condition)
			    echo "pm_ctl.check = $args" >$tmp/derived
			    PCP_DERIVED_CONFIG=$tmp/derived pmprobe -v -h "$host" pm_ctl.check >$tmp/tmp
			    numval=`cut -d ' ' -f 2 <$tmp/tmp`
			    val=`cut -d ' ' -f 3 <$tmp/tmp`
			    if [ "$numval" -gt 1 ]
			    then
				_warning "$policy: condition($args) has $numval values, not 1 as expected, using first value ($val)"
			    fi
			    if [ "$numval" -gt 0 ]
			    then
				if [ "$val" -gt 0 ]
				then
				    touch $tmp/match
				    $VERBOSE && echo "$policy: host $host condition($args) true, value $val"
				    break
				else
				    $VERY_VERBOSE && echo "$policy: host $host condition($args) false, value $val"
				fi
			    else
				$VERY_VERBOSE && echo "$policy: host $host condition($args) false, numval $numval"

			    fi
			    ;;

			hostname)
			    if echo "$host" | grep -E "$args" >/dev/null
			    then
				touch $tmp/match
				$VERBOSE && echo "$policy: host $host hostname($args) true"
			    else
				$VERY_VERBOSE && echo "$policy: host $host hostname($args) false"
				break
			    fi
			    ;;
		    esac
		done
		if [ -f $tmp/match ]
		then
		    POLICY="$policy"
		    if _do_create "$host"
		    then
			# on success $tmp/control is the control file for
			# this class
			#
			n=`cat $tmp/condition-true`
			n=`expr $n + 1`
			mv $tmp/control $tmp/control.$n
			echo "$policy" >$tmp/policy.$n
			echo $n >$tmp/condition-true
		    else
			_error "$policy: create failed for host $host"
		    fi
		fi
	    else
		$VERY_VERBOSE && echo "$policy: no [create] section, skip class"
	    fi
	done
	n=`cat $tmp/condition-true`
	if [ "$n" -eq 0 ]
	then
	    $VERBOSE && _warning "no instance created for host $host"
	    continue
	elif [ "$n" -eq 1 ]
	then
	    # just one class "matches", use the control file from do_create()
	    #
	    mv $tmp/control.$n $tmp/control
	    POLICY="`cat $tmp/policy.1`"
	else
	    # some work to be done ...
	    #
	    _resolve_configs "$host" $tmp/control.*
	    if $VERBOSE
	    then
		echo "--- start combined config file ---"
		cat $tmp/config
		echo "--- end combined config file ---"
	    fi
	    [ -z "$IDENT" ] && IDENT=pmfind-$host
	    # build a pmfind-like control file
	    #
	    if [ ${IAM} = pmlogger ]
	    then
		target_dir=$PCP_ARCHIVE_DIR/$IDENT
		cat <<End-of-File >$tmp/control
# DO NOT REMOVE OR EDIT THE FOLLOWING LINE
\$version=1.1

\$class=pmfind
$host n n PCP_ARCHIVE_DIR/$IDENT -c ./$IDENT.config -r
End-of-File
	    else
		target_dir=$PCP_LOG_DIR/pmie/$IDENT
		cat <<End-of-File >$tmp/control
# DO NOT REMOVE OR EDIT THE FOLLOWING LINE
\$version=1.1

\$class=pmfind
$host n n PCP_LOG_DIR/pmie/$IDENT/pmie.log -c ./$IDENT.config
End-of-File
	    fi
	    if $SHOWME
	    then
		echo + mkdir_and_chown "$target_dir" 755 $PCP_USER:$PCP_GROUP
	    else
		mkdir_and_chown "$target_dir" 755 $PCP_USER:$PCP_GROUP >$tmp/tmp 2>&1
		if [ ! -d "$target_dir" ]
		then
		    cat $tmp/tmp
		    _error "cannot create directory ($target_dir)"
		fi
	    fi
	    $CP $tmp/config $target_dir/$IDENT.config
	fi
	# this bit is more or less replicated from do_create(),
	# but we don't need to replicate error checking that's
	# already been done
	#
	if [ -n "$IDENT" ]
	then
	    ident="$IDENT"
	else
	    _get_policy_section "$POLICY" ident >$tmp/tmp
	    if [ -s $tmp/tmp ]
	    then
		ident=`sed -e "s;%h;$host;g" <$tmp/tmp`
	    else
		ident="$host"
	    fi
	fi
	dir=`$PCP_AWK_PROG <$tmp/control '
$1 == "'"$host"'"	{ print $4 }'`
	if $VERBOSE
	then
	    echo "--- start control file ---"
	    cat $tmp/control
	    echo "--- end control file ---"
	fi
	$VERBOSE && echo "Installing control file: $CONTROLDIR/$ident"

	$CP $tmp/control "$CONTROLDIR/$ident"
	$CHECK $CHECKARGS -c "$CONTROLDIR/$ident"
	dir_args="`echo "$dir" | _expand_control`"
	_check_started "$dir_args" || sts=1
    done

    return $sts
}

# create command
#
# if FROM_COND_CREATE is true, we're doing work on behalf of the cond-create
# command, and nothing is installed, but the control file is left in
# $tmp/control to be used back in cond_create()
#
_do_create()
{
    sts=0
    for host
    do
	if [ -n "$IDENT" ]
	then
	    # -i from command line ... 
	    #
	    ident="$IDENT"
	else
	    # -c from command line ... 
	    #
	    _get_policy_section "$POLICY" ident >$tmp/tmp
	    if [ -s $tmp/tmp ]
	    then
		check=`wc -w <$tmp/tmp | sed -e 's/ //g'`
		[ "$check" -ne 1 ] &&
		    _error "[ident] section is invalid in $POLICY policy file (expect a single word, not $check words)"
		ident=`sed -e "s;%h;$host;g" <$tmp/tmp`
	    else
		ident="$host"
	    fi
	fi
	[ -f $CONTROLDIR/"$ident" ] && _error "control file $CONTROLDIR/$ident already exists"
	if $EXPLICIT_CLASS
	then
	    # use classname from -c
	    :
	else
	    # try to extract from [class] section, else fallback to basename
	    # of the policy file (this was the scheme before the [class]
	    # section was introduced)
	    #
	    CLASS=`_get_policy_section "$POLICY" class`
	    [ -z "$CLASS" ] && CLASS=`echo "$POLICY" | sed -e 's;.*/;;'`
	fi
	cat <<End-of-File >$tmp/control
# created by $prog on `date`
End-of-File
	_get_policy_section "$POLICY" control >$tmp/tmp
	[ ! -s $tmp/tmp ] && _error "[control] section is missing from $POLICY policy file"
	if grep '^\$class=' $tmp/tmp >/dev/null
	then
	    :
	else
	    echo "\$class=$CLASS" >>$tmp/control
	fi
	if grep '^\$version=1.1$' $tmp/tmp >/dev/null
	then
	    :
	else
	    $VERBOSE && echo "Adding \$version=1.1 to control file"
	    echo '#DO NOT REMOVE OR EDIT THE FOLLOWING LINE' >>$tmp/control
	    echo '$version=1.1' >>$tmp/control
	fi
	sed -e "s;%h;$host;g" -e "s;%i;$ident;g" <$tmp/tmp >>$tmp/control
	primary=`$PCP_AWK_PROG <$tmp/control '
$1 == "'"$host"'"	{ print $2 }'`
	if [ -z "$primary" ]
	then
	    echo "control file ..."
	    cat $tmp/control
	    _error "cannot find primary field from control file"
	fi
	if [ "$primary" = y ]
	then
	    # don't dink with the primary ... systemctl (or the "rc" script)
	    # must be used to control the primary ${IAM}
	    #
	    _error "primary ${IAM} cannot be created from $prog"
	fi
	dir=`$PCP_AWK_PROG <$tmp/control '
$1 == "'"$host"'"	{ print $4 }'`
	if [ -z "$dir" ]
	then
	    echo "control file ..."
	    cat $tmp/control
	    _error "cannot find directory field from control file"
	fi
	if [ "$host" = "$LOCALHOST" ]
	then
	    pat_host="($host|LOCALHOSTNAME)"
	    pat_dir="($dir|`echo "$dir" | sed -e "s;$host;LOCALHOSTNAME;"`)"
	else
	    pat_host="$host"
	    pat_dir="$dir"
	fi
	_egrep -rl "^($pat_host|#!#$pat_host)[ 	].*[ 	]$pat_dir([ 	]|$)" $CONTROLFILE $CONTROLDIR >$tmp/out
	[ -s $tmp/out ] && _error "host $host and directory $dir already defined in `cat $tmp/out`"
	if $FROM_COND_CREATE
	then
	    # skip this part (the real create and start) ...
	    :
	else
	    if $VERBOSE
	    then
		echo "--- start control file ---"
		cat $tmp/control
		echo "--- end control file ---"
	    fi
	    $VERBOSE && echo "Installing control file: $CONTROLDIR/$ident"
	    $CP $tmp/control "$CONTROLDIR/$ident"
	    $CHECK $CHECKARGS -c "$CONTROLDIR/$ident"
	    dir_args="`echo "$dir" | _expand_control`"
	    _check_started "$dir_args" || sts=1
	fi
    done

    return $sts
}

# destroy command
#
_do_destroy()
{
    mv $tmp/args $tmp/destroy
    cat $tmp/destroy \
    | while read control class args_host primary socks args_dir args
    do
	echo "$control" "$class" "$args_host" "$primary" "$socks" "$args_dir" "$args" >$tmp/args
	if _do_stop -q
	then
	    :
	else
	    _error "control file changes skipped because ${IAM} could not be stopped"
	fi
	dir=`echo "$args_dir" | _unexpand_control`
	alt_dir="`echo "$args_dir" | _unexpand_pcp_control`"
	host=`echo "$args_host " | _unexpand_control | sed -e 's/ $//'`
	# need to match either expanded or unexpanded host name, with
	# or without #!# prefix
	#
	$PCP_AWK_PROG <"$control" >$tmp/control '
$1 == "'"$args_host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")	{ next }
$1 == "'"#!#$args_host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")	{ next }
$1 == "'"$host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")		{ next }
$1 == "'"#!#$host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")	{ next }
									{ print }'
	if cmp -s "$control" $tmp/control
	then
	    $VERBOSE && echo "${IAM} for host $host and directory $dir already removed from control file $control"
	else
	    if $VERY_VERBOSE
	    then
		echo "Diffs for control file $control after removing host $host and directory $dir ..."
		diff "$control" $tmp/control
	    elif $VERBOSE
	    then
		echo "Remove ${IAM} for host $host and directory $dir in control file $control"
	    fi
	fi
	sed -n <$tmp/control >$tmp/tmp -e '/^[^$# 	]/p'
	if [ -s $tmp/tmp ]
	then
	    # at least one active control line left in $tmp/control ...
	    # cannot remove it
	    #
	    $CP $tmp/control "$control"
	else
	    $VERBOSE && echo "Remove control file $control"
	    $RM "$control"
	fi
    done

    return 0
}

# start command
#
_do_start()
{
    restart=false
    [ "$1" = '-r' ] && restart=true
    sts=0
    cat $tmp/args \
    | while read control class args_host primary socks args_dir args
    do
	$VERBOSE && echo "Looking for ${IAM} using directory $args_dir ..."
	pid=`_get_pid "$args_dir"`
	if [ -n "$pid" ]
	then
	    $VERBOSE && echo "${IAM} PID $pid already running for host $args_host, nothing to do"
	    $VERBOSE && $restart && echo "Not expected for restart!"
	    if $MIGRATE
	    then
		$VERBOSE && vflag="-v"
		migrate_pid_service $vflag "$pid" ${IAM}_farm.service
	    fi
	    continue
	fi
	if $VERBOSE
	then
	    if $restart
	    then
		echo "Not found as expected, launching new ${IAM}"
	    else
		echo "Not found, launching new ${IAM}"
	    fi
	fi
	if [ ! -f "$control" ]
	then
	    _warning "control file $control for host $args_host ${IAM} has vanished"
	    sts=1
	    continue
	fi
	dir=`echo "$args_dir" | _unexpand_control`
	alt_dir="`echo "$args_dir" | _unexpand_pcp_control`"
	host=`echo "$args_host " | _unexpand_control | sed -e 's/ $//'`
	$PCP_AWK_PROG <"$control" >$tmp/control '
$1 == "'"#!#$host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")	{ sub(/^#!#/,"",$1) }
									{ print }'
	if cmp -s "$control" $tmp/control
	then
	    if $restart
	    then
		:
	    else
		$VERBOSE && echo "${IAM} for host $host and directory $dir already enabled in control file $control"
	    fi
	else
	    if $VERY_VERBOSE
	    then
		echo "Diffs for control file $control after enabling host $host and directory $dir ..."
		diff "$control" $tmp/control
	    elif $VERBOSE
	    then
		echo "Enable ${IAM} for host $host and directory $dir in control file $control"
	    fi
	    $CP $tmp/control "$control"
	fi
	$CHECK $CHECKARGS -c "$control"

	_check_started "$args_dir" || sts=1
    done

    return $sts
}

# check command - start dead hosts, if any
#
_do_check()
{
    _do_start $*
}

# stop command
#
_do_stop()
{
    skip_control_update=false
    [ "$1" = '-q' ] && skip_control_update=true
    sts=0
    rm -f $tmp/sts
    cat $tmp/args \
    | while read control class args_host primary socks args_dir args
    do
	host=`echo "$args_host " | _unexpand_control | sed -e 's/ $//'`
	if grep "^#!#$host[ 	]" $control >/dev/null
	then
	    _warning "${IAM} for host $host already stopped, nothing to do"
	    continue
	fi
	$VERBOSE && echo "Looking for ${IAM} using directory $args_dir ..."
	pid=`_get_pid "$args_dir"`
	if [ -z "$pid" ]
	then
	    _warning "cannot find PID for host $args_host ${IAM}, already exited?"
	else
	    # $PCPQA_KILL_SIGNAL is only intended for QA tests
	    #
	    $VERBOSE && echo "Found PID $pid to stop using signal ${PCPQA_KILL_SIGNAL-TERM}"
	    $KILL ${PCPQA_KILL_SIGNAL-TERM} $pid
	    if _check_stopped "$args_dir"
	    then
		:
	    else
		echo 1 >$tmp/sts
		continue
	    fi
	fi
	$skip_control_update && continue
	if [ ! -f "$control" ]
	then
	    _warning "control file $control for host $args_host ${IAM} has vanished"
	    echo 1 >$tmp/sts
	    continue
	fi
	dir=`echo "$args_dir" | _unexpand_control`
	alt_dir="`echo "$args_dir" | _unexpand_pcp_control`"
	$PCP_AWK_PROG <"$control" >$tmp/control '
$1 == "'"$host"'" && ($4 == "'"$dir"'" || $4 == "'"$alt_dir"'")	{ $1 = "#!#" $1 }
								{ print }'
	if cmp -s "$control" $tmp/control
	then
	    $VERBOSE && echo "${IAM} for host $host and directory $dir already disabled in control file $control"
	else
	    if $VERY_VERBOSE
	    then
		echo "Diffs for control file $control after disabling host $host and directory $dir ..."
		diff "$control" $tmp/control
	    elif $VERBOSE
	    then
		echo "Disable ${IAM} for host $host and directory $dir in control file $control"
	    fi
	    $CP $tmp/control "$control"
	fi
    done

    [ -f $tmp/sts ] && sts="`cat $tmp/sts`"

    return $sts
}

# restart command
#
_do_restart()
{
    sts=0
    mv $tmp/args $tmp/restart
    cat $tmp/restart \
    | while read control class host primary socks dir args
    do
	echo "$control" "$class" "$host" "$primary" "$socks" "$dir" "$args" >$tmp/args
	if _do_stop -q
	then
	    if _do_start -r
	    then
		:
	    else
		_error "restart failed to start host $host in class $class"
		sts=1
	    fi
	else
	    _error "restart failed to stop host $host in class $class"
	    sts=1
	fi
    done

    return $sts
}

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"

DOALL=false
FORCE=false
IDENT=''
SHOWME=false
CP=cp
RM=rm
CHECK="sudo -u $PCP_USER -g $PCP_GROUP $PCP_BINADM_DIR/${IAM}_check"
CHECKARGS=''
KILL="$PCP_BINADM_DIR/pmsignal -s"
MIGRATE=false
VERBOSE=false
VERY_VERBOSE=false
VERY_VERY_VERBOSE=false
CLASS=default
POLICY=''
EXPLICIT_CLASS=false
ARGS=''
while [ $# -gt 0 ]
do
    case "$1"
    in
	-a)	DOALL=true
		;;
	-c)	CLASS="$2"
		EXPLICIT_CLASS=true
		shift
		;;
	-C)	CHECKARGS="$CHECKARGS $2"
		shift
		;;
	-f)	FORCE=true
		;;
	-i)	IDENT="$2"
		shift
		;;
	-m)	MIGRATE=true
		;;
	-N)	SHOWME=true
		CP="echo + $CP"
		RM="echo + $RM"
		CHECK="echo + $CHECK"
		KILL="echo + $KILL"
		;;
	-p)	POLICY="$2"
		shift
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
		;;
	--)	# we're not being POSIX conformant, want to allow -x options after command
		# so skip this one
		;;
	-*)	_usage
		# NOTREACHED
		;;
	*)	# this is a non-option arg, gather them up for later
		if [ -z "$ARGS" ]
		then
		    ARGS="\"$1\""
		else
		    ARGS="$ARGS \"$1\""
		fi
		;;
    esac
    shift
done

eval set -- $ARGS
if [ $# -lt 1 ]
then
    _usage
    # NOTREACHED
fi

LOCALHOST=`hostname`

ACTION="$1"
shift

if [ -n "$IDENT" ]
then
    if [ "$ACTION" != create -a "$ACTION" != cond-create ]
    then
	_error "-i option may only be used with create or cond-create commands"
    fi
fi

if $VERY_VERBOSE
then
    if $EXPLICIT_CLASS
    then
	echo "Using class: $CLASS"
    else
	echo "Using default class"
    fi
fi
[ -z "$POLICY" ] && POLICY="$PCP_ETC_DIR/pcp/${IAM}/class.d/$CLASS"
if [ "$CLASS" = default ]
then
    if [ ! -f "$POLICY" ]
    then
	# This is the _real_ default policy, when there is no
	# $PCP_ETC_DIR/pcp/${IAM}/class.d/default
	#
	cat <<'End-of-File' >$tmp/policy
[class]
default

[ident]
%h

[destroy]
condition(1)

[create]
hostname(.*)

[control]
#DO NOT REMOVE OR EDIT THE FOLLOWING LINE
$version=1.1
End-of-File
	if [ ${IAM} = pmlogger ]
	then
	    echo '%h n n PCP_ARCHIVE_DIR/%i -c ./%i.config' >>$tmp/policy
	else
	    echo '%h n n PCP_LOG_DIR/pmie/%i/pmie.log -c ./%i.config' >>$tmp/policy
	fi
	POLICY=$tmp/policy
	$VERY_VERBOSE && echo "Using default policy"
    fi
else
    if [ ! -f "$POLICY" ]
    then
	if [ "$ACTION" = create ]
	then

	    _error "policy file $POLICY not found, class $CLASS is not defined so cannot create"
	elif [ "$ACTION" = destroy ] && ! $FORCE
	then
	    _error "policy file $POLICY not found, class $CLASS is not defined so cannot destroy"
	fi
    fi
    $VERY_VERBOSE && echo "Using policy: $POLICY"
fi

FIND_ALL_HOSTS=false
FROM_COND_CREATE=false

# don't get confused by processes that exited, but did not cleanup ...
# build a list of runing ${IAM} processes
#
_get_pids_by_name ${IAM} | sed -e 's/.*/^&$/' >$tmp/pids

case "$ACTION"
in
    check|create|cond-create|start|stop|restart|destroy)
	    if [ `id -u` != 0 -a "$SHOWME" = false ]
	    then
		_error "you must be root (uid 0) to change the Performance Co-Pilot logger setup"
	    fi
	    # need --class and/or hostname
	    #
	    if [ "$ACTION" = "check" ]
	    then
		FIND_ALL_HOSTS=true
	    elif [ $# -eq 0 ]
	    then
		$EXPLICIT_CLASS || _error "\"$ACTION\" command requres hostname(s) and/or a --class"
		FIND_ALL_HOSTS=true
	    fi

	    _lock
	    if [ "$ACTION" != create -a "$ACTION" != cond-create ]
	    then
		_get_matching_hosts "$@"
		if [ ! -f $tmp/args ]
		then
		    if [ "$ACTION" = check ]
		    then
			# special case: successfully check nothing
			status=0
			exit
		    else
			_error "no matching host(s) to $ACTION"
			exit
		    fi
		fi
	    fi
	    # small wrinkle: map - to _ in action, e.g.
	    # cond-create -> cond_create, so it is a valid shell
	    # function name
	    #
	    eval "_do_`echo "$ACTION" | sed -e 's/-/_/g'`" $*
	    cmd_sts=$?
	    if [ $cmd_sts -ne 0 ]
	    then
		_error "could not complete $ACTION operation"
	    fi
	    ;;

    status)
	    [ $# -eq 0 ] && FIND_ALL_HOSTS=true
	    _get_matching_hosts "$@"
	    _do_status
	    ;;

    *)	    _error "command \"$ACTION\" not known"
	    exit
	    ;;
esac

exit
