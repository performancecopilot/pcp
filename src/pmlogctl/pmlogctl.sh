#! /bin/sh
#
# Control program for managing pmlogger and pmie instances.
#
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
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
# - man page
# - glob expansion for <class> and <host> names
# - create => create + start ... is there a case for create only?  
# - destroy => stop + destroy ... is there a case for destroy iff already
#   stopped?
# - multiple pmloggers in the one control file ... create will not do
#   this and destroy checks for it before removing the control file,
#   but I'd like to avoid a "migration" path 'cause splitting a
#   control file is messy and potentially dangerous, and subsequent manual
#   editing could bring the situation back at any time
# - need to either ban or have alternate action for stop/start primary,
#   and ban destroy for primary and don't allow policy with primary == y
#   (to stop create)
# - pmfind integration
# - other sections in the "policy" files, especially with pmfind to
#   (a) at create, pick a class by probing the host
#   (b) at create, decide not to by probing the host or by hostname
#       pattern
#   (c) at destroy, decide not to or wait some time before destroying
#       (the latter is really hard)
# - IAM=pmie is completely untested
#

. $PCP_DIR/etc/pcp.env

prog=`basename $0`
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

tmp=`mktemp -d $PCP_TMPFILE_DIR/$prog.XXXXXXXXX` || exit 1
status=0

_cleanup()
{
    _unlock
    rm -rf $tmp
}
trap "_cleanup; exit \$status" 0 1 2 3 15

cat >$tmp/usage <<End-of-File
# Usage: [options] command [arg ...]

Options:
  -a,--all                      apply action to all matching hosts
  -c=NAME,--class=NAME    	${IAM} instances belong to the NAME class \
[default: default]
  -f,--force                    force action if possible
  -n,--showme             	perform a dry run, showing what would be done
  -p=POLICY,--policy=POLICY	use POLICY as the policy file \
[default: $PCP_ETC_DIR/pcp/${IAM}/policy.d/<class>]
  -v,--verbose            	increase verbosity
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
    _dir="$PCP_ETC_DIR/pcp/${IAM}"
    if [ ! -w "$_dir" ]
    then
	_warning "no write access in \"$_dir\" skip lock file processing"
    else
	# demand mutual exclusion
	#
	rm -f $tmp/stamp $tmp/out
	_delay=200		# tenths of a second, so max wait is 20sec
	while [ $_delay -gt 0 ]
	do
	    if pmlock -v "$_dir/lock" >>$tmp/out 2>&1
	    then
		echo "$$" >"$_dir/lock"
		break
	    else
		[ -f $tmp/stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/stamp
		find $tmp/stamp -newer "$_dir/lock" -print 2>/dev/null >$tmp/tmp
		if [ -s $tmp/tmp ]
		then
		    if [ -f "$_dir/lock" ]
		    then
			_warning "removing lock file older than 30 minutes (PID `cat $_dir/lock`)"
			LC_TIME=POSIX ls -l "$_dir/lock"
			rm -f "$_dir/lock"
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
	    _delay=`expr $_delay - 1`
	done

	if [ $_delay -eq 0 ]
	then
	    # failed to gain mutex lock
	    #
	    if [ -f "$_dir/lock" ]
	    then
		_warning "is another ${IAM}ctl job running concurrently?"
		LC_TIME=POSIX ls -l "$_dir/lock"
	    else
		_error "`cat $tmp/out`"
	    fi
	    _error "failed to acquire exclusive lock ($_dir/lock) ..."
	    return 1
	else
	    if $VERY_VERBOSE
	    then
		echo "Lock acquired `cat $_dir/lock` `ls -l $_dir/lock`"
	    fi
	fi
    fi

    return 0
}

_unlock()
{
    _dir="$PCP_ETC_DIR/pcp/${IAM}"
    if [ -f "$_dir/lock" ]
    then
	rm -f "$_dir/lock"
	$VERY_VERBOSE && echo "Lock released"
    fi
}

_usage()
{
    pmgetopt >&2 --progname=$prog --config=$tmp/usage --usage
    cat >&2 <<End-of-File

Avaliable commands:
   create [-c class] host ...
   status [host ...]
   {start|stop|restart|destroy} [-c class] [host ...]
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
	# this regexp matches all possible hostname lines
	#
	set -- '[^#$][^ 	]*'
    fi
    for host
    do
	$VERY_VERBOSE && echo "Looking for host=$host and class=$CLASS ..."
	if [ "$host" = "$localhost" ]
	then
	    pat="($host|LOCALHOSTNAME)"
	else
	    pat="$host"
	fi
	egrep -r "^($pat|#!#$pat)[ 	]" $CONTROLFILE $CONTROLDIR \
	| sed -e 's/:/ /' \
	| while read controlfile controlline
	do
	    controlline=`echo "$controlline" | _expand_control | sed -e 's/^#!#//'`
	    echo "$controlfile" "$controlline"
	done >$tmp/tmp
	if $VERY_VERBOSE
	then
	    echo "Candidate control files:"
	    sed -e 's/ .*//' $tmp/tmp
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
		    $VERY_VERBOSE && echo "No match for control=$control host=$host dir=$dir class=$class"
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
	    if [ "$action" = create ]
	    then
		# that's good ...
		:
	    elif $FIND_ALL_HOSTS
	    then
		if $EXPLICIT_CLASS
		then
		    _warning "no host defined in class \"$CLASS\" for any ${IAM} control file"
		else
		    _warning "no host defined in any ${IAM} control file"
		fi
	    else
		if $EXPLICIT_CLASS
		then
		    _warning "host \"$host\" not defined in class \"$CLASS\" for any ${IAM} control file"
		else
		    _warning "host \"$host\" not defined in any ${IAM} control file"
		fi
	    fi
	    continue
	fi
	if [ "$action" != status ]
	then
	    $PCP_AWK_PROG <$tmp/tmp '{ print $3 }' \
	    | sort \
	    | uniq -c \
	    | grep -v ' 1 ' >$tmp/tmp2
	    if [ -s $tmp/tmp2 ] && ! $DOALL
	    then
		$VERBOSE && cat $tmp/tmp
		if $EXPLICIT_CLASS
		then
		    _error "host \"$host\" defined in class \"$CLASS\" multiple times, don't know which instance to $action"
		else
		    _error "host \"$host\" is defined multiple times, don't know which instance to $action"
		fi
	    fi
	fi

	cat $tmp/tmp >>$tmp/args
    done
    if [ -f $tmp/args ]
    then
	$VERY_VERBOSE && ( echo "\$tmp/args:"; cat $tmp/args )
    fi
}

# get class for a specific ${IAM} instance
# $1 = control
# $2 = host (expanded)
# $3 = dir (expanded)
#
_get_class()
{
    control="$1"
    # need space at end so hostname looks like it does in a control line
    host="`echo "$2 " | _unexpand_control | sed -e 's/ $//'`"
    dir="`echo "$3" | _unexpand_control`"
    class=`$PCP_AWK_PROG <"$control" '
BEGIN			{ class = "" }
/^[$]class=/		{ class = $1; sub(/[$]class=/,"",class) }
$4 == "'"$dir"'" 	{ if ($1 == "'"$host"'" || $1 == "#!#'"$host"'") {
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
$1 == "'"$2"':"		{ want = 1; next }
$1 ~ /^[a-z]*:$/	{ want = 0; next }
want == 1		{ print }'
}

# do what ${IAM}_check does to a control line in terms of variable
# expansion
# 
_expand_control()
{
    sed \
	-e 's/[ 	][ 	]*/ /g' \
	-e "s;PCP_ARCHIVE_DIR/;$PCP_ARCHIVE_DIR/;g" \
	-e "s/^LOCALHOSTNAME /$localhost /g" \
	-e "s/\\([^a-zA-Z0-9]\\)LOCALHOSTNAME/\\1$localhost/g" \
    # end
}

# reverse the changes from _expand_control()
#
_unexpand_control()
{
    sed \
	-e "s;$PCP_ARCHIVE_DIR/;PCP_ARCHIVE_DIR/;g" \
	-e "s/^$localhost /LOCALHOSTNAME /g" \
	-e "s/\\([^a-zA-Z0-9]\\)$localhost/\\1LOCALHOSTNAME/g" \
    # end
}

# verbose diagosis of failed state
# $1 = host
# $2 = directory
#
_diagnose()
{
    echo "  $1 $2"
}

# status command
#
_do_status()
{
    fmt="%-20s %-17s %-8s %7s %-8s\n"

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
		if [ "`systemctl is-active ${IAM}.service`" = active ]
		then
		    # all healthy
		    :
		else
		    systemctl_state='stopped by systemctl'
		fi
	    else
		systemctl_state='disabled by systemctl'
	    fi
	fi
    fi
    find $PCP_TMP_DIR/${IAM} -type f -a ! -name primary \
    | while read f
    do
	sed -n -e 3p $f \
	| _expand_control
    done >>$tmp/archive
    find $CONTROLFILE $CONTROLDIR -type f \
    | while read control
    do
	class=''
	sed <"$control" -n \
	    -e '/^[^#]/p' \
	    -e '/^#!#/p ' \
	| _expand_control \
	| while read host primary socks dir args
	do
	    state=running
	    case "$host"
	    in
		\$class=*)	class=`echo "$host" | sed -e 's/.*=//'`
				continue
				;;
		\$*)		continue
				;;
		\#!\#*)		host=`echo "$host" | sed -e 's/^#!#//'`
				state='stopped by pmlogctl'
				;;
	    esac
	    if $PICK_HOSTS
	    then
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
	    [ "$primary" = y ] && class=primary
	    archive=`grep "^$dir/" $tmp/archive \
		     | sed -e 's;.*/;;'`
	    [ -z "$archive" ] && archive='?'
	    if [ "$archive" = '?' ]
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
	    pid=`grep -rl "^$dir/" $PCP_TMP_DIR/${IAM} \
		 | sed -e 's;.*/;;'`
	    [ -z "$pid" ] && pid='?'
	    printf "$fmt" "$host" "$archive" "$class" "$pid" "$state"
	    if $VERBOSE
	    then
		case "$state"
		in
		    dead)
			_diagnose "$host" "$dir"
			;;
		esac
	    fi
	    class=''
	done
    done \
    | sort >$tmp/out

    if [ -s $tmp/out ]
    then
	printf "$fmt" "pmcd Host" Archive Class PID State
	cat $tmp/out
    fi
    if [ -s $tmp/args ]
    then
	echo "No ${IAM} configuration found for: `tr '\012' ' ' <$tmp/args`"
    fi
}

# create command
#
_do_create()
{
    sts=0
    for host
    do
	_get_policy_section "$POLICY" name >$tmp/tmp
	if [ -s $tmp/tmp ]
	then
	    name=`sed -e "s/%h/$host/g" <$tmp/tmp`
	else
	    name="$host"
	fi
	[ -f $CONTROLDIR/$name ] && _error "control file $CONTROLDIR/$name already exists"
	cat <<End-of-File >$tmp/control
# created by pmlogctl on `date`
\$class=$CLASS
End-of-File
	_get_policy_section "$POLICY" control >$tmp/tmp
	[ ! -s $tmp/tmp ] && _error "\"control:\" section is missing from $POLICY policy file"
	if grep '^\$version=1.1$' $tmp/tmp >/dev/null
	then
	    :
	else
	    $VERBOSE && echo "Adding \$version=1.1 to control file"
	    echo '#DO NOT REMOVE OR EDIT THE FOLLOWING LINE' >>$tmp/control
	    echo '$version=1.1' >>$tmp/control
	fi
	sed -e "s/%h/$host/g" <$tmp/tmp >>$tmp/control
	dir=`$PCP_AWK_PROG <$tmp/control '
$1 == "'"$host"'"	{ print $4 }'`
	if [ -z "$dir" ]
	then
	    echo "control file ..."
	    cat $tmp/control
	    _error "cannot find directory from control file"
	fi
	egrep -lr "^($host|#!#$host)[ 	].*[ 	]$dir([ 	]|$)" $CONTROLFILE $CONTROLDIR >$tmp/out
	[ -s $tmp/out ] && _error "host \"$host\" and directory $dir already defined in `cat $tmp/out`"
	if $SHOWME
	then
	    echo "--- start control file ---"
	    cat $tmp/control
	    echo "--- end control file ---"
	fi
	$VERBOSE && echo "Installing control file: $CONTROLDIR/$name"
	$CP $tmp/control $CONTROLDIR/$name
	$CHECK -c $CONTROLDIR/$name
	# TODO ... check really started && set sts=1
    done

    return $sts
}

# destroy command
#
_do_destroy()
{
    mv $tmp/args $tmp/destroy
    cat $tmp/destroy \
    | while read control class host primary socks dir args
    do
	echo "$control" "$class" "$host" "$primary" "$socks" "$dir" "$args" >$tmp/args
	if _do_stop
	then
	    :
	else
	    _error "failed to stop host \"$host\" and class \"$class\""
	fi
	dir=`echo "$dir" | _unexpand_control`
	host=`echo "$host " | _unexpand_control | sed -e 's/ $//'`
	$PCP_AWK_PROG <"$control" >$tmp/control '
$1 == "'"$host"'" && $4 == "'"$dir"'"	{ next }
					{ print }'
	if $VERY_VERBOSE
	then
	    echo "Diffs for control file $control ..."
	    diff "$control" $tmp/control
	elif $VERBOSE
	then
	    echo "Remove ${IAM} for host \"$host\" and directory $dir in control file $control"
	fi
	sed -n <$tmp/control >$tmp/tmp -e '/^[^$# 	]/p'
	if [ -s $tmp/tmp ]
	then
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
    cat $tmp/args \
    | while read control class host primary socks dir args
    do
	$VERBOSE && echo "Looking for ${IAM} using directory $dir ..."
	pid=`grep -rl "^$dir/" $PCP_TMP_DIR/${IAM} \
	     | sed -e 's;.*/;;'`
	if [ -n "$pid" ]
	then
	    $VERBOSE && echo "${IAM} PID $pid already running for host \"$host\", nothing to do"
	    return 0
	else
	    $VERBOSE && echo "Not found, launching new ${IAM}"
	fi
	if [ ! -f "$control" ]
	then
	    _warning "control file $control for host \"$host\" ${IAM} has vanished"
	    return 1
	fi
	if grep "^#!#$host[ 	]" $control >/dev/null
	then
	    # stopped by pmlogctl, remove the stopped prefix (#!#)
	    #
	    if $SHOWME
	    then
		echo "+ unmark host as stopped in $control"
	    else
		sed -e "/^#!#$host[ 	]/s/^#!#//" <"$control" >$tmp/control
		if $VERY_VERBOSE
		then
		    echo "Diffs for control file $control ..."
		    diff "$control" $tmp/control
		elif $VERBOSE
		then
		    echo "Enable ${IAM} in control file $control"
		fi
		$CP $tmp/control "$control"
	    fi
	fi
	$CHECK -c $control
	# TODO ... check really started
    done
    return 0
}

# stop command
#
_do_stop()
{
    cat $tmp/args \
    | while read control class host primary socks dir args
    do
	if grep "^[^:]*:#!#$host[ 	]" $control >/dev/null
	then
	    _warning "${IAM} for host \"$host\" already stopped, nothing to do"
	    return 0
	fi
	$VERBOSE && echo "Looking for ${IAM} using directory $dir ..."
	pid=`grep -rl "^$dir/" $PCP_TMP_DIR/${IAM} \
	     | sed -e 's;.*/;;'`
	if [ -z "$pid" ]
	then
	    _warning "cannot find PID for host \"$host\" ${IAM}, already exited?"
	    return 1
	fi
	$VERBOSE && echo "Found PID $pid to stop"
	if [ ! -f "$control" ]
	then
	    _warning "control file $control for host \"$host\" ${IAM} has vanished"
	    return 1
	fi
	$KILL TERM $pid
	# TODO ... check really stopped
	if $SHOWME
	then
	    echo "+ mark host as stopped in $control"
	else
	    sed -e "/^$host[ 	]/s/^/#!#/" <"$control" >$tmp/control
	    if $VERY_VERBOSE
	    then
		echo "Diffs for control file $control ..."
		diff "$control" $tmp/control
	    elif $VERBOSE
	    then
		echo "Disable ${IAM} in control file $control"
	    fi
	    $CP $tmp/control "$control"
	fi
    done

    return 0
}

# restart command
#
_do_restart()
{
    mv $tmp/args $tmp/restart
    cat $tmp/restart \
    | while read control class host primary socks dir args
    do
	echo "$control" "$class" "$host" "$primary" "$socks" "$dir" "$args" >$tmp/args
	if _do_stop
	then
	    if _do_start
	    then
		:
	    else
		_error "failed to stop host \"$host\" and class \"$class\""
	    fi
	else
	    _error "failed to start host \"$host\" and class \"$class\""
	fi
    done

    return 0
}

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"

DOALL=false
FORCE=false
SHOWME=false
CP=cp
RM=rm
CHECK="sudo -u $PCP_USER -g $PCP_GROUP $PCP_BINADM_DIR/${IAM}_check"
KILL="$PCP_BINADM_DIR/pmsignal -s"
VERBOSE=false
VERY_VERBOSE=false
CLASS=default
POLICY=''
EXPLICIT_CLASS=false
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
	-f)	FORCE=true
		;;
	-n)	SHOWME=true
		CP="echo + $CP"
		RM="echo + $RM"
		CHECK="echo + $CHECK"
		KILL="echo + $KILL"
		;;
	-p)	POLICY="$2"
		shift
		;;
	-v)	if $VERBOSE
		then
		    VERY_VERBOSE=true
		else
		    VERBOSE=true
		fi
		;;
	--)	shift
		break
		;;
	-\?)	_usage
		# NOTREACHED
		;;
    esac
    shift
done

if [ $# -lt 1 ]
then
    _usage
    # NOTREACHED
fi

if [ `id -u` != 0 -a "$1" != "status" -a "$SHOWME" = false ]
then
    _error "you must be root (uid 0) to change the Performance Co-Pilot logger setup"
    status=1
    exit
fi

localhost=`hostname`

action="$1"
shift

[ "$action" != "status" ] && _lock

if $VERY_VERBOSE
then
    if $EXPLICIT_CLASS
    then
	echo "Using class: $CLASS"
    else
	echo "Using default class"
    fi
fi
[ -z "$POLICY" ] && POLICY="$PCP_ETC_DIR/pcp/${IAM}/policy.d/$CLASS"
if [ "$CLASS" = default ]
then
    if [ ! -f "$POLICY" ]
    then
	# This is the _real_ default policy, when there is no
	# $PCP_ETC_DIR/pcp/${IAM}/policy.d/default
	#
	cat <<'End-of-File' >$tmp/policy
name:
%h

control:
#DO NOT REMOVE OR EDIT THE FOLLOWING LINE
$version=1.1
%h n n PCP_ARCHIVE_DIR/%h -c config.default

destroy:
manual
End-of-File
	POLICY=$tmp/policy
	$VERY_VERBOSE && echo "Using default policy"
    fi
else
    if [ ! -f "$POLICY" ]
    then
	if [ "$action" = create ]
	then

	    _error "policy file $POLICY not found, class \"$CLASS\" is not defined so cannot create"
	elif [ "$action" = destroy ] && ! $FORCE
	then
	    _error "policy file $POLICY not found, class \"$CLASS\" is not defined so cannot destroy"
	fi
    fi
    $VERY_VERBOSE && echo "Using policy: $POLICY"
fi

# need --class and/or hostname, except for status command
#
FIND_ALL_HOSTS=false
if [ $# -eq 0 ]
then
    if [ "$action" != status ]
    then
	$CLASSS_EXPLICIT || _error "\"$action\" command requres hostname(s) and/or a --class"
    fi
    FIND_ALL_HOSTS=true
fi

case "$action"
in
    create|start|stop|restart|destroy)
	    if [ "$action" != create ]
	    then
		_get_matching_hosts $*
		if [ ! -f $tmp/args ]
		then
		    _warning "nothing to be done!"
		    exit
		fi
	    fi
	    eval "_do_$action" $*
	    cmd_sts=$?
	    if [ $cmd_sts -ne 0 ]
	    then
		_warning "action not completed"
	    fi
	    ;;

    status)
	    _get_matching_hosts $*
	    _do_status
	    ;;

    *)	    _error "action \"$action\" not known"
	    exit
	    ;;
esac

exit
