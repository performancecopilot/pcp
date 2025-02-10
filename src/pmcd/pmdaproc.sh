# Common sh(1) procedures to be used in the Performance Co-Pilot
# PMDA Install and Remove scripts
#
# Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
# Portions Copyright (c) 2008 Aconex.  All Rights Reserved.
# Portions Copyright (c) 2013-2016,2018,2020 Red Hat.
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

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

tmp=`mktemp -d $PCP_TMPFILE_DIR/pmdaproc.XXXXXXXXX` || exit 1
__tmp=$tmp	# (preserve backward compatibility)
status=0
trap "rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15
prog=`basename $0`

_setup_platform()
{
    case "$PCP_PLATFORM"
    in
	mingw)
	    uid=0	# no permissions we can usefully test here
	    dso_suffix=dll
	    default_pipe_opt=false
	    default_socket_opt=true
	    CHOWN=": skip chown"
	    CHMOD=chmod
	    ;;
	*)
	    eval `id | sed -e 's/(.*//'`
	    dso_suffix=so
	    [ "$PCP_PLATFORM" = darwin ] && dso_suffix=dylib
	    default_pipe_opt=true
	    default_socket_opt=false
	    CHOWN=chown
	    CHMOD=chmod
	    ;;
    esac
}

_setup_localhost()
{
    # Try to catch some truly evil preconditions.  If you cannot reach
    # localhost, all bets are off!
    #
    if which ping >/dev/null 2>&1
    then
	__opt=''
	# Larry Wall style hunt for the version of ping that is first
	# on our path
	#
	ping --help >$tmp/hlp 2>&1
	if grep '.-c <*count>*' $tmp/hlp >/dev/null 2>&1
	then
	    __opt='-c 1 localhost'
	elif grep '.-n <*count>*' $tmp/hlp >/dev/null 2>&1
	then
	    __opt='-n 1 localhost'
	elif grep 'host .*packetsize .*count' $tmp/hlp >/dev/null 2>&1
	then
	    __opt='localhost 56 1'
	elif grep 'host .*data_size.*npackets' $tmp/hlp >/dev/null 2>&1
	then
	    __opt='localhost 56 1'
	fi
	if [ -z "$__opt" ]
	then
	    echo "Warning: can't find a ping(1) that I understand ... pushing on"
	else
	    if ping $__opt >$tmp/hlp 2>&1
	    then
		:
	    else
		# failing that, try 3 pings ... failure means all 3 were lost,
		# and so there is no hope of continuing
		#
		__opt=`echo "$__opt" | sed -e 's/1/3/'`
		if ping $__opt >/dev/null 2>&1
		then
		    :
		else
		    echo "Error: no route to localhost, pmcd reconfiguration abandoned" 
		    status=1
		    exit
		fi
	    fi
	fi
    fi
}

# wait for pmcd to be alive again
# 	Usage: __wait_for_pmcd [can_wait]
#
__wait_for_pmcd()
{
    # 60 seconds default seems like a reasonable max time to get going
    [ -z "$__can_wait" ] && __can_wait=${1-60}
    if $PCP_BINADM_DIR/pmcd_wait -t $__can_wait
    then
	__pmcd_is_dead=false	# should be set already, force it anyway
    else
	echo "Arrgghhh ... PMCD failed to start after $__can_wait seconds"
	if [ -f $LOGDIR/pmcd.log ]
	then
	    echo "Here is the PMCD logfile ($LOGDIR/pmcd.log):"
	    ls -l $LOGDIR/pmcd.log
	    cat $LOGDIR/pmcd.log
	else
	    echo "No trace of the PMCD logfile ($LOGDIR/pmcd.log)!"
	fi
	__pmcd_is_dead=true
    fi
}

# try and put pmcd back the way it was
#
__restore_pmcd()
{
    if [ -f $tmp/pmcd.conf.save ]
    then
	__pmcd_is_dead=false
	echo
	echo "Save current PMCD control file in $PCP_PMCDCONF_PATH.prev ..."
	rm -f $PCP_PMCDCONF_PATH.prev
	mv $PCP_PMCDCONF_PATH $PCP_PMCDCONF_PATH.prev
	echo "Restoring previous PMCD control file, and trying to restart PMCD ..."
	cp $tmp/pmcd.conf.save $PCP_PMCDCONF_PATH
	eval $CHOWN root $PCP_PMCDCONF_PATH
	eval $CHMOD 644 $PCP_PMCDCONF_PATH
	rm -f $tmp/pmcd.conf.save
	__pmda_restart_pmcd
	__wait_for_pmcd
    fi
    if $__pmcd_is_dead
    then
	echo
	echo "Sorry, failed to restart PMCD."
    fi
}

# send pmcd SIGHUP and reliably check that it received (at least) one
#
__sighup_pmcd()
{
    __sighups_before=-1

    eval `pmprobe -v pmcd.sighups 2>/dev/null \
	  | $PCP_AWK_PROG '{ printf "__sighups_before=%d\n", $3 }'`
    [ $__sighups_before -lt 0 ] && return 1	# pmcd not running

    pmsignal -a -s HUP pmcd >/dev/null 2>&1

    # first make sure pmcd has received SIGHUP
    #
    __sts=1
    for __delay in 0.01 0.05 0.1 0.15 0.25 0.5 1 2
    do
	$PCP_BINADM_DIR/pmsleep $__delay
	__sighups=-1
	eval `pmprobe -v pmcd.sighups 2>/dev/null \
	      | $PCP_AWK_PROG '{ printf "__sighups=%d\n", $3 }'`
        if [ $__sighups -gt $__sighups_before ]
	then
	    __sts=0
	    break
	fi
    done
    # now configurable delay for $signal_delay while pmcd actually
    # does post-SIGHUP work
    #
    $PCP_BINADM_DIR/pmsleep $signal_delay
    return $__sts
}

# __pmda_cull name domain
#
__pmda_cull()
{
    # context and integrity checks
    #
    if [ $# -ne 2 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_cull() called with $# (instead of 2) arguments"
	status=1
	exit
    fi
    [ ! -f $PCP_PMCDCONF_PATH ] && return
    if eval $CHMOD u+w $PCP_PMCDCONF_PATH
    then
	:
    else
	echo "pmdaproc.sh: __pmda_cull: Unable to make $PCP_PMCDCONF_PATH writable"
        status=1
	exit
    fi
    if [ ! -w $PCP_PMCDCONF_PATH ]
    then
	echo "pmdaproc.sh: \"$PCP_PMCDCONF_PATH\" is not writeable"
	status=1
	exit
    fi

    # remove matching entry from $PCP_PMCDCONF_PATH if present
    #
    rm -f $tmp/pmcd.warn
    $PCP_AWK_PROG <$PCP_PMCDCONF_PATH >$tmp/pmcd.conf '
BEGIN					{ status = 0 }
$1 == "'"$1"'" && $2 == "'"$2"'" 	{ status = 1; next }
$1=="'"$1"'" && $2!="'"$2"'"	{ print "Warning: '"$PCP_PMCDCONF_PATH"'[" NF "] culling entry with same PMDA name ('"$1"') but different domain (" $2 " not '"$2"')" >"'$tmp/pmcd.warn'"; status=1; next }
$1!="'"$1"'" && $2=="'"$2"'"	{ print "Warning: '"$PCP_PMCDCONF_PATH"'[" NF "] culling entry with same PMDA domain ('"$2"') but different name (" $1 " not '"$1"')" >"'$tmp/pmcd.warn'"; status=1; next }
					{ print }
END					{ exit status }'
    if [ $? -eq 0 ]
    then
	# no match
	:
    else
	[ -f $tmp/pmcd.warn ] && cat $tmp/pmcd.warn >&2
	
	# log change to the PCP NOTICES file
	#
	$PCP_BINADM_DIR/pmpost "PMDA cull: from $PCP_PMCDCONF_PATH: $1 $2"

	# save pmcd.conf in case we encounter a problem, and then
	# install updated $PCP_PMCDCONF_PATH
	#
	cp $PCP_PMCDCONF_PATH $tmp/pmcd.conf.save
	cp $tmp/pmcd.conf $PCP_PMCDCONF_PATH
	eval $CHOWN root $PCP_PMCDCONF_PATH
	eval $CHMOD 644 $PCP_PMCDCONF_PATH

	# signal pmcd if it is running
	#
	if pminfo -v pmcd.version >/dev/null 2>&1
	then
	    __sighup_pmcd
	    __wait_for_pmcd
	    $__pmcd_is_dead && __restore_pmcd
	fi
    fi
    rm -f $tmp/pmcd.conf

    # stop any matching PMDA that is still running
    #
    for __sig in TERM KILL
    do
	__pids=`_get_pids_by_name -a pmda$1`
	if [ ! -z "$__pids" ]
	then
	    pmsignal -s $__sig $__pids >/dev/null 2>&1
	    # allow signal processing to be done
	    $PCP_BINADM_DIR/pmsleep $signal_delay
	else
	    break
	fi
    done
}

__filter_journalctl_pmcd()
{
    grep " pmcd\\[" | sed -e 's/\.\.*done$//'
}

__journalctl_pmcd()
{
    if `which journalctl >/dev/null 2>&1`
    then
	journalctl -n 10 -u pmcd 2>/dev/null | __filter_journalctl_pmcd
    elif `which systemd-journalctl >/dev/null 2>&1`
    then
	systemd-journalctl -q -n 100 2>/dev/null | __filter_journalctl_pmcd
    fi
}

__pmda_restart_pmcd()
{
    if [ -n "$PCP_SYSTEMDUNIT_DIR" -a -f $PCP_SYSTEMDUNIT_DIR/pmcd.service -a "$PCPQA_SYSTEMD" != no ]
    then
	# smells like systemctl is the go ...
	#
	__journalctl_pmcd > $tmp.journal.pre
	systemctl restart pmcd.service
        sleep 2
	__journalctl_pmcd > $tmp.journal.post

	# diff the pre and post journal lines to find those most recently
	# added:
	#	- lines beginning with a digit are ed(1) commands, ignore
	#	- --- lines are diff fluff
	#	- lines beginnning < are only in journal.pre, ignore
	#	- strip journal datestamp and process id info
	#	- add trailing space for some output lines to match
	#	  non-systemd output and QA *.out files
	#	- bizarro (on vm19) the .post file may contain lines
	#	  _before_ the start of a non-empty .pre file, so any
	#	  > lines after 0a? are nonsense, but need to make sure
	#	  the .pre file is not empty (sigh)
	#
	[ -s $tmp.journal.pre ] || echo "-- added by _pmda_restart_pmcd() --" >$tmp.journal.pre
	diff $tmp.journal.pre $tmp.journal.post \
	| $PCP_AWK_PROG '
BEGIN		{ skip = 0 }
$1 ~ /^0a/	{ skip = 1; next }
skip == 1 && $1 != ">"	{ skip = 0 }
skip == 0	{ print }' \
	| sed \
	    -e '/^[<0-9]/d' \
	    -e '/^---$/d' \
	    -e "s/^.*\\[[0-9]*]: //" \
	    -e '/^Starting p.*[^ ]$/s/$/ /' \
	    -e '/^Stopping p.*[^ ]$/s/$/ /' \
	# end
    else
	$PCP_RC_DIR/pmcd start
    fi
}

# __pmda_add "entry for $PCP_PMCDCONF_PATH"
#
__pmda_add()
{
    # context and integrity checks
    #
    if [ $# -ne 1 ]
    then
	echo "pmdaproc.sh: internal botch: __pmda_add() called with $# (instead of 1) arguments"
	status=1
	exit
    fi
    if eval $CHMOD u+w $PCP_PMCDCONF_PATH
    then
	:
    else
	echo "pmdaproc.sh: __pmda_add: Unable to make $PCP_PMCDCONF_PATH writable"
        status=1
	exit
    fi
    if [ ! -w $PCP_PMCDCONF_PATH ]
    then
	echo "pmdaproc.sh: \"$PCP_PMCDCONF_PATH\" is not writeable"
	status=1
	exit
    fi

    # save pmcd.conf in case we encounter a problem
    #
    cp $PCP_PMCDCONF_PATH $tmp/pmcd.conf.save

    myname=`echo $1 | $PCP_AWK_PROG '{print $1}'`
    mydomain=`echo $1 | $PCP_AWK_PROG '{print $2}'`
    # add entry to $PCP_PMCDCONF_PATH
    #
    echo >$tmp/pmcd.body
    echo >$tmp/pmcd.access
    rm -f $tmp/pmcd.warn
    $PCP_AWK_PROG <$PCP_PMCDCONF_PATH '
NF==0					{ next }
/^[      ]*\[[   ]*access[       ]*\]/	{ state = 2 }
state == 2				{ print >"'$tmp/pmcd.access'"; next }
$1=="'$myname'" && $2=="'$mydomain'"	{ next }
$1=="'$myname'" && $2!="'$mydomain'"	{ print "Warning: '"$PCP_PMCDCONF_PATH"'[" NF "] culling entry with same PMDA name ('$myname') but different domain (" $2 " not '$mydomain')" >"'$tmp/pmcd.warn'"; next }
$1!="'$myname'" && $2=="'$mydomain'"	{ print "Warning: '"$PCP_PMCDCONF_PATH"'[" NF "] culling entry with same PMDA domain ('$mydomain') but different name (" $1 " not '$myname')" >"'$tmp/pmcd.warn'"; next }
					{ print >"'$tmp/pmcd.body'"; next }'
    echo "$1" >> $tmp/pmcd.body 
    ( LC_COLLATE=POSIX sort -n -k2 $tmp/pmcd.body; echo; cat $tmp/pmcd.access )\
    >$PCP_PMCDCONF_PATH
    [ -f $tmp/pmcd.warn ] && cat $tmp/pmcd.warn >&2
    rm -f $tmp/pmcd.access $tmp/pmcd.body $tmp/pmcd.warn
    eval $CHOWN root $PCP_PMCDCONF_PATH
    eval $CHMOD 644 $PCP_PMCDCONF_PATH

    # log change to pcplog/NOTICES
    #
    $PCP_BINADM_DIR/pmpost "PMDA add: to $PCP_PMCDCONF_PATH: $1"

    # signal pmcd if it is running (and ok to do so), else start it
    #
    if ! $forced_restart && pminfo -v pmcd.version >/dev/null 2>&1
    then
	__sighup_pmcd
    else
	log=$LOGDIR/pmcd.log
	rm -f $log
	__pmda_restart_pmcd
    fi
    __wait_for_pmcd
    $__pmcd_is_dead && __restore_pmcd
}

# expect -R root or $ROOT not set in environment
#
__check_root()
{
    if [ "X$root" != X ]
    then
	ROOT="$root"
	export ROOT
    else
	if [ "X$ROOT" != X -a "X$ROOT" != X/ ]
	then
	    echo "$prog: \$ROOT was set to \"$ROOT\""
	    echo "          Use -R rootdir to install somewhere other than /"
	    status=1
	    exit
	fi
    fi
}

# should be able to extract default domain from domain.h
#
__check_domain()
{
    if [ -f domain.h ]
    then
	__infile=domain.h
    elif [ -f domain.h.perl ]
    then
	__infile=domain.h.perl
    elif [ -f domain.h.python ]
    then
	__infile=domain.h.python
    else
	echo "$prog: cannot find domain.h (or similar) to determine the Performance Metrics Domain"
	status=1
	exit
    fi
    # $domain is for backwards compatibility, modern PMDAs
    # have something like
    #	#define FOO 123
    #
    __root="$ROOT"	# saved, so we do not overwrite ROOT
    domain=''
    eval `$PCP_AWK_PROG <$__infile '
/^#define/ && $3 ~ /^[0-9][0-9]*$/	{ print $2 "=" $3
				      if (seen == 0) {
					print "domain=" $3
					sub(/^PMDA/, "", $2)
					print "SYMDOM=" $2
					seen = 1
				      }
				    }'`
    if [ "X$__root" != X ]	# restore ROOT if it was set before
    then
	export ROOT="$__root"
    else
	unset ROOT
    fi
    if [ "X$domain" = X ]
    then
	echo "$prog: cannot determine the Performance Metrics Domain from $__infile"
	status=1
	exit
    fi
}

# handle optional configuration files that maybe already given in an
# $PCP_PMCDCONF_PATH line or user-supplied or some default or sample
# file
#
# before calling _choose_configfile, optionally define the following
# variables
#
# Name		Default			Use
#
# configdir	$PCP_VAR_DIR/config/$iam	directory for config ... assumed
#					name is $iam.conf in this directory
#
# configfile	""			set if have a preferred choice,
#					e.g. from $PCP_PMCDCONF_PATH
#					this will be set on return if we've
#					found an acceptable config file
#
# default_configfile
#		""			if set, this is the default which
#					will be offered
#
# Note: 
#  If the choice is aborted then $configfile will be set to empty.
#  Therefore, there should be a test for an empty $configfile after
#  the call to this function.
#
_choose_configfile()
{
    configdir=${configdir-$PCP_VAR_DIR/config/$iam}

    if [ ! -d $configdir ]
    then
    	mkdir -p $configdir
    fi

    while true
    do
	echo "Possible configuration files to choose from:"
	# List viable alternatives
	__i=0          # menu item number
	__filelist=""  # list of configuration files
	__choice=""    # the choice of configuration file
	__choice1=""   # the menu item for the 1st possible choice
	__choice2=""   # the menu item for the 2nd possible choice
	__choice3=""   # the menu item for the 3rd possible choice

        if [ ! -z "$configfile" ]
	then
	    if [ -f $configfile ]
	    then
		__i=`expr $__i + 1`
		__choice1=$__i
		__filelist="$__filelist $configfile"
		echo "[$__i] $configfile"
	    fi
	fi

        if [ -f $configdir/$iam.conf ]
	then
	    if echo $__filelist | grep "$configdir/$iam.conf" >/dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice2=$__i
		__filelist="$__filelist $configdir/$iam.conf"
		echo "[$__i] $configdir/$iam.conf"
	    fi
	fi

	if [ -f $default_configfile ]
	then
	    if echo $__filelist | grep "$default_configfile" >/dev/null
	    then
		:
	    else
		__i=`expr $__i + 1`
		__choice3=$__i
		__filelist="$__filelist $default_configfile"
		echo "[$__i] $default_configfile"
	    fi
	fi

	__i=`expr $__i + 1`
	__own_choice=$__i
	echo "[$__i] Specify your own configuration file."

	__i=`expr $__i + 1`
	__abort_choice=$__i
	echo "[$__i] None of the above (abandon configuration file selection)."

	$PCP_ECHO_PROG $PCP_ECHO_N "Which configuration file do you want to use ? [1] ""$PCP_ECHO_C"
	read __reply
	$__echo && echo "$__reply"

	# default
	if [ -z "$__reply" ] 
	then
	    __reply=1
	fi

	# Process the reply from the user
	if [ $__reply = $__own_choice ]
	then
	    $PCP_ECHO_PROG $PCP_ECHO_N "Enter the name of the existing configuration file: ""$PCP_ECHO_C"
	    read __choice
	    $__echo && echo "$__choice"
	    if [ ! -f "$__choice" ]
	    then
		echo "Cannot open \"$__choice\"."
		echo ""
		echo "Please choose another configuration file."
		__choice=""
	    fi
	elif [ $__reply = $__abort_choice ]
	then
	    echo "Abandoning configuration file selection."
	    configfile=""
	    return 0
	elif [ "X$__reply" = "X$__choice1" -o "X$__reply" = "X$__choice2" -o "X$__reply" = "X$__choice3" ]
	then
	    # extract nth field as the file
	    __choice=`echo $__filelist | $PCP_AWK_PROG -v n=$__reply '{ print $n }'` 
	else
	    echo "Illegal choice: $__reply"
	    echo ""
	    echo "Please choose number between: 1 and $__i"
	fi

	if [ ! -z "$__choice" ]
	then
	    echo
	    echo "Contents of the selected configuration file:"
	    echo "--------------- start $__choice ---------------"
	    cat $__choice
	    echo "--------------- end $__choice ---------------"
	    echo

	    $PCP_ECHO_PROG $PCP_ECHO_N "Use this configuration file? [y] ""$PCP_ECHO_C"
	    read ans
	    $__echo && echo "$ans"
	    if [ ! -z "$ans" -a "X$ans" != Xy -a "X$ans" != XY ]
	    then
		echo ""
		echo "Please choose another configuration file."
	    else
		break
	    fi
	fi
    done


    __dest=$configdir/$iam.conf
    if [ "$__choice" != "$__dest" ]
    then
	if [ -f $__dest ]
	then
	    echo "Removing old configuration file \"$__dest\""
	    rm -f $__dest
	    if [ -f $__dest ]
	    then
		echo "Error: cannot remove old configuration file \"$__dest\""
		status=1
		exit
	    fi
	fi
	if cp $__choice $__dest
	then
	    :
	else
	    echo "Error: cannot install new configuration file \"$__dest\""
	    status=1
	    exit
	fi
	__choice=$__dest
    fi

    configfile=$__choice
}

# choose an IPC method
#
__choose_ipc()
{
    _dir=$1
    ipc_type=''
    $pipe_opt && ipc_type=pipe
    $socket_opt && ipc_type=socket
    $pipe_opt && $socket_opt && ipc_type=''
    if [ -z "$ipc_type" ]
    then
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "PMCD should communicate with the $iam daemon via a pipe or a socket? [pipe] ""$PCP_ECHO_C"
	    read ipc_type
	    $__echo && echo "$ipc_type"
	    if  [ "X$ipc_type" = Xpipe -o "X$ipc_type" = X ]
	    then
		ipc_type=pipe
		break
	    elif [ "X$ipc_type" = Xsocket ]
	    then
		break
	    else
		echo "Must choose one of \"pipe\" or \"socket\", please try again"
	    fi
	done
    fi

    if [ $ipc_type = pipe ]
    then
	# This defaults to binary unless the Install file
	# specifies ipc_prot="binary notready"   -- See pmcd(1)
	type="pipe	$ipc_prot 		$_dir/$pmda_name"
    else
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Use Internet, IPv6 or Unix domain sockets? [Internet] ""$PCP_ECHO_C"
	    read ans
	    $__echo && echo "$ans"
	    if [ "X$ans" = XInternet -o "X$ans" = XIPv6 -o "X$ans" = X ]
	    then
		$PCP_ECHO_PROG $PCP_ECHO_N "Internet port number or service name? [$socket_inet_def] ""$PCP_ECHO_C"
		read port
		$__echo && echo "$port"
		[ "X$port" = X ] && port=$socket_inet_def
		case $port
		in
		    [0-9]*)
			    ;;
		    *)
			    if grep "^$port[ 	]*[0-9]*/tcp" /etc/services >/dev/null 2>&1
			    then
				:
			    else
				echo "Warning: there is no tcp service for \"$port\" in /etc/services!"
			    fi
			    ;;
		esac
		if [ "X$ans" = XInternet -o "X$ans" = X ]
		then
		    type="socket	inet $port	$_dir/$pmda_name"
		    __args="-i $port $__args"
		else
		    type="socket	ipv6 $port	$_dir/$pmda_name"
		    __args="-6 $port $__args"
		fi
		break
	    elif [ "X$ans" = XUnix ]
	    then
		$PCP_ECHO_PROG $PCP_ECHO_N "Unix FIFO name? ""$PCP_ECHO_C"
		read fifo
		$__echo && echo "$fifo"
		if [ "X$fifo" = X ]
		then
		    echo "Must provide a name, please try again"
		else
		    type="socket	unix $fifo	$_dir/$pmda_name"
		    __args="-u $fifo $__args"
		    break
		fi
	    else
		echo "Must choose one of \"Unix\" or \"Internet\", please try again"
	    fi
	done
    fi
}

# filter pmprobe -i output of the format:
#	postgresql.active.is_in_recovery -12351 Missing metric value(s)
#	postgresql.statio.sys_sequences.blks_hit 0
#	disk.partitions.read 13 ?0 ?1 ?2 ?3 ?4 ?5 ?6 ?7 ?8 ?9 ?10 ?11 ?12
# to produce a summary of metrics, values and warnings
#
__filter()
{
    $PCP_AWK_PROG '
/^'$1'/         { metric++
                  if ($2 < 0)
		    warn++
		  else
		    value += $2
		  next
                }
                { warn++ }
END             { if (warn) printf "%d warnings, ",warn
                  printf "%d metrics and %d values\n",metric,value
                }'
}

_setup()
{
    # some uses (especially in QA) re-define $pmda_dir ... if this
    # happens, need to chdir there first
    if [ "$__here" != "$pmda_dir" ]
    then
	if [ ! -d "$pmda_dir" ]
	then
	    echo "Error: pmda_dir ($pmda_dir) is not a directory"
	    status=1
	    exit
	fi
	cd "$pmda_dir"
	__here=`pwd`
    fi

    # some more configuration controls
    pmns_name="${pmns_name-$iam}"
    pmda_name="${pmda_name-pmda$iam}"
    pmda_dso_name="${PCP_PMDAS_DIR}/${iam}/pmda_${iam}.${dso_suffix}"
    dso_name="${dso_name-$pmda_dso_name}"
    dso_entry="${dso_entry-${iam}_init}"

    _check_userroot
    _check_directory

    # automatically generate files for those lazy Perl programmers
    #
    if $perl_opt
    then
	if [ -z "$perl_name" ]
	then
	    perl_name="${perl_name-${pmda_dir}/pmda${iam}.perl}"
	    [ -f "$perl_name" ] || perl_name="${pmda_dir}/pmda${iam}.pl"
	fi
	if [ -f "$perl_name" ]
	then
	    perl_pmns="${pmda_dir}/pmns.perl"
	    perl_dom="${pmda_dir}/domain.h.perl"
	    perl -e 'use PCP::PMDA' >$tmp/out 2>&1
	    if test $? -eq 0
	    then
		if eval PCP_PERL_DOMAIN=1 perl "$perl_name" >"$perl_dom"
		then
		    :
		else
		    echo "Arrgh! failed to create $perl_dom from $perl_name"
		    status=1
		    exit
		fi
		if eval PCP_PERL_PMNS=1 perl "$perl_name" >"$perl_pmns"
		then
		    :
		else
		    echo "Arrgh! failed to create $perl_pmns from $perl_name"
		    status=1
		    exit
		fi
	    elif $dso_opt || $daemon_opt
	    then
		:	# we have an alternative, so continue on
	    else
		cat $tmp/out
		echo 'Perl PCP::PMDA module is not installed, install it and try again'
		status=1
		exit
	    fi
	else
	    if $dso_opt || $daemon_opt
	    then
		:	# we have an alternative, so continue on
	    else
		echo "Neither pmda${iam}.perl nor pmda${iam}.pl found in ${pmda_dir}"
		echo "Error: no Perl PMDA to install"
		status=1
		exit
	    fi
	fi
    fi

    # automatically generate files for the Python programmers too
    #
    if $python_opt
    then
	python=${PCP_PYTHON_PROG:-python}
	if [ -z "$python_name" ]
	then
	    python_name="${pmda_dir}/pmda${iam}.python"
	    [ -f "$python_name" ] || python_name="${pmda_dir}/pmda${iam}.py"
	fi
	if [ -f "$python_name" ]
	then
	    python_pmns="${pmda_dir}/pmns.python"
	    python_dom="${pmda_dir}/domain.h.python"
	    $python -c 'from pcp import pmda' 2>/dev/null
	    __module=$?
	    $python -c "import ast; ast.parse(open('$python_name').read())" 2>$tmp/err >/dev/null
	    __syntax=$?
	    if test $__module -eq 0 -a $__syntax -eq 0
	    then
		if eval PCP_PYTHON_DOMAIN=1 $python "$python_name" >"$python_dom"
		then
		    :
		else
		    echo "Arrgh! failed to create $python_dom from $python_name"
		    status=1
		    exit
		fi
		if eval PCP_PYTHON_PMNS=1 $python "$python_name" >"$python_pmns"
		then
		    :
		else
		    echo "Arrgh! failed to create $python_pmns from $python_name"
		    status=1
		    exit
		fi
	    elif $dso_opt || $daemon_opt
	    then
		:	# we have an alternative, so continue on
	    else
		test $__module -ne 0 && \
		    echo 'Python pcp.pmda module is not installed, install it and try again'
		test $__syntax -ne 0 && \
		    echo 'Python script '$python_name' has syntax errors.' && cat $tmp/err
		status=1
		exit
	    fi
	else
	    if $dso_opt || $daemon_opt
	    then
		:	# we have an alternative, so continue on
	    else
		echo "Neither pmda${iam}.python nor pmda${iam}.py found in ${pmda_dir}"
		echo "Error: no Python PMDA to install"
		status=1
		exit
	    fi
	fi
    fi

    # Juggle pmns and domain.h in case perl/python pmda install was done here
    # last time
    #
    for file in pmns domain.h
    do
	[ -f $file.save ] && mv $file.save $file
    done

    # Set $domain and $SYMDOM from domain.h
    #
    __check_domain

    case $prog
    in
	*Install*)
	    # Check that $ROOT is not set
	    #
	    __check_root
	    ;;
    esac
}

_install_views()
{
    viewer="$1"
    have_views=false

    [ "`echo *.$viewer`" != "*.$viewer" ] && have_views=true
    if [ -d $PCP_VAR_DIR/config/$viewer ]
    then
	$have_views && echo "Installing $viewer view(s) ..."
	for __i in *.$viewer
	do
	    if [ "$__i" != "*.$viewer" ]
	    then
		__dest=$PCP_VAR_DIR/config/$viewer/`basename $__i .$viewer`
		rm -f $__dest
		cp $__i $__dest
	    fi
	done
    else
	$have_views && \
	echo "Skip installing $viewer view(s) ... no \"$PCP_VAR_DIR/config/$viewer\" directory"
    fi
}

# Configurable PMDA installation
#
# before calling _install,
# 1. set $iam
# 2. set one/some/all of $dso_opt, $perl_opt, $python_opt or $daemon_opt to true
#    (optional, $daemon_opt is true by default)
# 3. if $daemon_opt set one or both of $pipe_opt or $socket_opt true
#    (optional, $pipe_opt is true by default)
# 4. if $socket_opt and there is a default Internet socket, set
#    $socket_inet_def

_install()
{
    if [ -z "$iam" ]
    then
	echo 'Botch: must define $iam before calling _install()'
	status=1
	exit
    fi

    if $dso_opt || $perl_opt || $python_opt || $daemon_opt
    then
	:
    else
	echo 'Botch: must set at least one of $dso_opt, $perl_opt, $python_opt or $daemon_opt to "true"'
	status=1
	exit
    fi
    if $daemon_opt
    then
	if $pipe_opt || $socket_opt
	then
	    :
	else
	    echo 'Botch: must set at least one of $pipe_opt or $socket_opt to "true"'
	    status=1
	    exit
	fi
    fi

    # Select a PMDA style (dso/perl/python/deamon), and for daemons the
    # IPC method for communication between PMCD and the PMDA.
    #
    pmda_options=''
    pmda_default_option=''
    pmda_multiple_options=false

    if $dso_opt
    then
	pmda_options="dso"
	pmda_default_option="dso"
    fi
    if $perl_opt
    then
	pmda_default_option="perl"
	if test -n "$pmda_options"
	then
	    pmda_options="perl or $pmda_options"
	    pmda_multiple_options=true
	else
	    pmda_options="perl"
	fi
    fi
    if $python_opt
    then
	pmda_default_option="python"
	if test -n "$pmda_options"
	then
	    pmda_options="python or $pmda_options"
	    pmda_multiple_options=true
	else
	    pmda_options="python"
	fi
    fi
    if $daemon_opt
    then
	pmda_default_option="daemon"
	if test -n "$pmda_options"
	then
	    pmda_options="daemon or $pmda_options"
	    pmda_multiple_options=true
	else
	    pmda_options="daemon"
	fi
    fi

    pmda_type="$pmda_default_option"
    if $pmda_multiple_options
    then
	while true
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Install $iam as a $pmda_options agent? [$pmda_default_option] ""$PCP_ECHO_C"
	    read pmda_type
	    $__echo && echo "$pmda_type"
	    if [ "X$pmda_type" = Xdaemon -o "X$pmda_type" = X ]
	    then
		pmda_type=daemon
		break
	    elif [ "X$pmda_type" = Xdso ]
	    then
		break
	    elif [ "X$pmda_type" = Xperl ]
	    then
		perl -e 'use PCP::PMDA' >$tmp/out 2>&1
		if test $? -ne 0
		then
		    cat $tmp/out
		    echo 'Perl PCP::PMDA module is not installed, install it and try again'
		else
		    break
		fi
	    elif [ "X$pmda_type" = Xpython ]
	    then
		$python -c 'from pcp import pmda' 2>/dev/null
		if test $? -ne 0
		then
		    echo 'Python pcp pmda module is not installed, install it and try again'
		else
		    break
		fi
	    else
		echo "Must choose one of $pmda_options, please try again"
	    fi
	done
    fi
    if [ "$pmda_type" = daemon ]
    then
	__choose_ipc $pmda_dir
	__args="-d $domain $__args"
	# Optionally use $PCP_DEBUG from the environment to set -D options
	# in pmcd.conf for command line args
	#
	if [ -n "$PCP_DEBUG" ]
	then
	    __args="-D$PCP_DEBUG $__args"
	fi

    elif [ "$pmda_type" = perl ]
    then
	type="pipe	$ipc_prot		perl $perl_name"
	__args=''
    elif [ "$pmda_type" = python ]
    then
	type="pipe	$ipc_prot		$python $python_name"
	__args=''
    else
	type="dso	$dso_entry	$dso_name"
	__args=''
    fi

    # Install binaries
    #
    if [ "$pmda_type" = perl -o "$pmda_type" = python ]
    then
	:	# we can safely skip building binaries
    elif [ -f Makefile -o -f makefile -o -f GNUmakefile ]
    then
	# $PCP_MAKE_PROG may contain command line args ... executable
	# is first word
	#
	if [ ! -f "`echo $PCP_MAKE_PROG | sed -e 's/ .*//'`" -o ! -f "$PCP_INC_DIR/pmda.h" ]
	then
	    echo "$prog: Arrgh, PCP devel environment required to install this PMDA"
	    status=1
	    exit
	fi

	echo "Installing files ..."
	if $PCP_MAKE_PROG install
	then
	    :
	else
	    echo "$prog: Arrgh, \"$PCP_MAKE_PROG install\" failed!"
	    status=1
	    exit
	fi
    fi

    # Fix domain in help for instance domains (if any)
    #
    if [ -f $help_source ]
    then
	sed -e "/^@ $SYMDOM\./s/$SYMDOM\./$domain./" <$help_source \
	| newhelp -n root -o $help_source
    fi

    if [ "X$pmda_type" = Xperl -o "X$pmda_type" = Xpython ]
    then
	# Juggle pmns and domain.h ... save originals and
	# use *.{perl,python} ones created earlier
	for file in pmns domain.h
	do
	    if [ ! -f "$file.$pmda_type" ]
	    then
		echo "Botch: $file.$pmda_type missing ... giving up"
		status=1
		exit
	    fi
	    if [ -f $file ]
	    then
		if diff $file.$pmda_type $file >/dev/null
		then
		    :
		else
		    [ ! -f $file.save ] && mv $file $file.save
		    mv $file.$pmda_type $file
		fi
	    else
		mv $file.$pmda_type $file
	    fi
	done
    fi

    # Check the PMDA's PMNS ... use root if it exists, else pmns
    # if it exists, else skip the check.
    #
    __root=''
    if [ -f root -a ! -f pmns.save ]
    then
	# have a root PMNS file and no pmns.save ... if pmns.save exists
	# then this is one of the schizo PMDAs, like simple, and "root"
	# probably belongs to pmns.save, so we need to synthesize a root
	# PMNS file for the generate pmns file (for Perl or Python PMDA
	# installs)
	#
	__root=root
    elif [ -f pmns ]
    then
	# have pmns file, synthesize a root PMNS file
	__root=$tmp/root
	echo 'root {' >$__root
	echo '#include "pmns"' >>$__root
	echo '}' >>$__root
    fi
    if [ -n "$__root" ]
    then
	if $pmns_dupok
	then
	    __n=-n
	else
	    __n=-N
	fi
	if pminfo $__n $__root 2>$tmp/err >/dev/null
	then
	    :
	else
	    cat $tmp/err
	    echo "Error: PMDA's PMNS is bad"
	    if grep 'Duplicate metric' $tmp/err >/dev/null
	    then
		echo "Hint: set pmns_dupok=true in the PMDA's Install script if duplicate names are"
		echo "      expected in the PMNS"
	    fi
	    status=1
	    exit
	fi
    fi

    $PCP_SHARE_DIR/lib/lockpmns $NAMESPACE
    trap "$PCP_SHARE_DIR/lib/unlockpmns \$NAMESPACE; rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15

    echo "Updating the Performance Metrics Name Space (PMNS) ..."

    # Install the namespace
    #

    if [ ! -f $NAMESPACE ]
    then
	# We may be installing an agent right after an install -
	# before pmcd startup, which has a pre-execution step of
	# rebuilding the namespace root.  Do so now.
	if [ -x $PMNSDIR/Rebuild ]
	then
	    echo "$prog: cannot Rebuild the PMNS for \"$NAMESPACE\""
	    status=1
	    exit
	fi
	cd $PMNSDIR
	./Rebuild -us
	cd $__here
	forced_restart=true
    fi

    for __n in $pmns_name
    do
	if pminfo $__ns_opt $__n >/dev/null 2>&1
	then
            cd $PMNSDIR
	    if  $PCP_BINADM_DIR/pmnsdel -n $PMNSROOT $__n >$tmp/base 2>&1
	    then
		pmsignal -a -s HUP pmcd >/dev/null 2>&1
	    else
		if grep 'Non-terminal "'"$__n"'" not found' $tmp/base >/dev/null
		then
		    :
		elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp/base >/dev/null
		then
		    :
		else
		    echo "$prog: failed to delete \"$__n\" from the PMNS"
		    cat $tmp/base
		    status=1
		    exit
		fi
	    fi
            cd $__here
	fi

	# Put the default domain number into the namespace file
	#
	# If there is only one namespace, then the pmns file will
	# be named "pmns".  If there are multiple metric trees,
	# subsequent pmns files will be named "pmns.<metricname>"
	#
	# the string "pmns" can be overridden by the Install/Remove
	# scripts by altering $pmns_source
	#
	if [ "$__n" = "$iam" -o "$__n" = "$pmns_name" ]
	then
	    __s=$pmns_source
	else
	    __s=$pmns_source.$__n
	fi
	sed -e "s/$SYMDOM:/$domain:/" <$__s >$PMNSDIR/$__n

        cd $PMNSDIR
	if $PCP_BINADM_DIR/pmnsadd -n $PMNSROOT $__n
	then
	    # Ensure the PMNS timestamp will be different the next
	    # time the PMNS is updated 
            #
	    __sighup_pmcd
	else
	    echo "$prog: failed to add the PMNS entries for \"$__n\" ..."
	    echo
	    ls -l
	    status=1
	    exit
	fi
        cd $__here
    done

    trap "rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15
    $PCP_SHARE_DIR/lib/unlockpmns $NAMESPACE

    _install_views pmchart
    _install_views kmchart
    _install_views pmview

    # Terminate old PMDA
    #
    echo "Terminate PMDA if already installed ..."
    __pmda_cull $iam $domain

    # Add PMDA to pmcd's configuration file
    #
    echo "Updating the PMCD control file, and notifying PMCD ..."
    if [ -n "$args" ]
    then
	__args="$__args $args"
    fi
    __pmda_add "$iam	$domain	$type $__args"

    # Check that the agent is running OK
    #
    if $do_check
    then
	# for some PMDAs in QA, we may need to wait a little longer,
	# or differently
	#
	[ -n "$PCPQA_CHECK_DELAY" ] && check_delay=$PCPQA_CHECK_DELAY
	__delay_int=`echo $check_delay | sed -e 's/\..*//g'`
	[ "$__delay_int" -gt 5 ] && echo "Wait $check_delay seconds for the $iam agent to initialize ..."
	$PCP_BINADM_DIR/pmsleep $check_delay
	for __n in $pmns_name
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Check $__n metrics have appeared ... ""$PCP_ECHO_C"
	    pmprobe -i $__ns_opt $__n | tee $tmp/verbose | __filter $__n
	    if $__verbose
	    then
		echo "pminfo output ..."
		cat $tmp/verbose
	    fi
	done
    fi
}

_remove()
{
    # Update the namespace
    #

    $PCP_SHARE_DIR/lib/lockpmns $NAMESPACE
    trap "$PCP_SHARE_DIR/lib/unlockpmns \$NAMESPACE; rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15

    echo "Culling the Performance Metrics Name Space ..."
    cd $PMNSDIR

    for __n in $pmns_name
    do
	$PCP_ECHO_PROG $PCP_ECHO_N "$__n ... ""$PCP_ECHO_C"
	if $PCP_BINADM_DIR/pmnsdel -n $PMNSROOT $__n >$tmp/base 2>&1
	then
	    rm -f $PMNSDIR/$__n
	    __sighup_pmcd
	    echo "done"
	else
	    if grep 'Non-terminal "'"$__n"'" not found' $tmp/base >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    elif grep 'Error: metricpath "'"$__n"'" not defined' $tmp/base >/dev/null
	    then
		echo "not found in Name Space, this is OK"
	    else
		echo "error"
		cat $tmp/base
		status=1
		exit
	    fi
	fi
    done

    # Remove the PMDA and help files
    #
    cd $__here

    echo "Updating the PMCD control file, and notifying PMCD ..."
    __pmda_cull $iam $domain

    if [ -f Makefile -o -f makefile -o -f GNUmakefile ]
    then
	echo "Removing files ..."
	$PCP_MAKE_PROG clobber >/dev/null
    fi
    for __i in *.pmchart
    do
	if [ "$__i" != "*.pmchart" ]
	then
	    __dest=$PCP_VAR_DIR/config/pmchart/`basename $__i .pmchart`
	    rm -f $__dest
	fi
    done
    for __i in *.kmchart
    do
	if [ "$__i" != "*.kmchart" ]
	then
	    __dest=$PCP_VAR_DIR/config/kmchart/`basename $__i .kmchart`
	    rm -f $__dest
	fi
    done

    if $do_check
    then
	for __n in $pmns_name
	do
	    $PCP_ECHO_PROG $PCP_ECHO_N "Check $__n metrics have gone away ... ""$PCP_ECHO_C"
	    if PCP_DERIVED_CONFIG= pminfo -n $NAMESPACE -v -b 1000 $__n >$tmp/base 2>&1
	    then
		echo "Arrgh, something has gone wrong!"
		cat $tmp/base
	    else
		echo "OK"
	    fi
	done
    fi

    trap "rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15
    $PCP_SHARE_DIR/lib/unlockpmns $NAMESPACE
}

_check_userroot()
{
    if [ "$uid" -ne 0 ]
    then
	if [ -n "$PCP_DIR" ]
	then
	    : running in a non-default installation, do not need to be root
	else
	    echo "Error: You must be root (uid 0) to update the pmcd(1) configuration."
	    status=1
	    exit
	fi
    fi
}

_check_directory()
{
    case "$__here"
    in
	*/pmdas/$iam)
	    ;;
	*)
	    echo "Error: expect current directory to be .../pmdas/$iam, not $__here"
	    echo "       (typical location is $PCP_PMDAS_DIR/$iam on this platform)"
	    status=1
	    exit
	    ;;
    esac
}

# preferred public interfaces
#
pmdaSetup()
{
    _setup
}

pmdaChooseConfigFile()
{
    _choose_configfile
}

pmdaInstall()
{
    _install
}

pmdaRemove()
{
    _remove
}

# mainline starts here ...
#

_setup_localhost
_setup_platform

# some useful common variables for Install/Remove scripts
#
# put your PMNS files here
PMNSDIR=$PCP_VAR_DIR/pmns

# pmcd and pcp log files here
if [ ! -z "$PCP_LOGDIR" ]
then
    # this is being discouraged and is no longer documented anywhere
    LOGDIR=$PCP_LOGDIR
else
    if [ -d $PCP_LOG_DIR/pmcd ]
    then
	# the preferred naming scheme
	#
	LOGDIR=$PCP_LOG_DIR/pmcd
    else
	# backwards compatibility for IRIX
	#
	LOGDIR=$PCP_LOG_DIR
    fi
fi

# writeable root of PMNS
NAMESPACE=${PMNS_DEFAULT-$PMNSDIR/root}
PMNSROOT=`basename $NAMESPACE`

# echo without newline - deprecated - use the $PCP_ECHO_* ones from
# /etc/pcp.conf instead, however some old Install and Remove scripts may
# still use $ECHONL, so keep it here
#
ECHONL="echo -n"

# Install control variables
#	Can install as DSO?
dso_opt=false
#	Can install as perl script?
perl_opt=false
#	Can install as python script?
python_opt=false
#	Can install as daemon?
daemon_opt=true
#	If daemon, pipe?
pipe_opt=$default_pipe_opt
#	If daemon, socket?  and default for Internet sockets?
socket_opt=$default_socket_opt
socket_inet_def=''
#	IPC Protocol for daemon (binary only now)
ipc_prot=binary
#	Need to force a restart of pmcd?
forced_restart=false
#	Delay after install before checking (sec)
check_delay=1
#	Delay after sending a signal to pmcd (sec)
signal_delay=1
#	Additional command line args to go in $PCP_PMCDCONF_PATH
args=""
#	Source for the PMNS
pmns_source=pmns
#	Are duplicates expected in the PMDA's PMNS?
pmns_dupok=false
#	Source for the helptext
help_source=help
#	Full pathname to directory where PMDA is to be found ...
#	exectable and/or DSO, domain.h, pmns, control files, etc.
pmda_dir="`pwd`"

if [ "$PCP_PLATFORM" = mingw ]
then
    # Special hack for mingw ...
    # $pmda_dir needs to be an absolute pathname, but if we have a
    # half-baked mingw chroot, then / is not the root of the Windows
    # filesystem.  For example $PCP_DIR is something like
    # C:\git-sdk-64\mingw64 and if pwd returns something like
    # /mingw64/... then we want pmda_dir to be
    # C:/git-sdk-64/mingw64/...
    #
    pmda_dir_1=`echo $pmda_dir | sed -e 's@^/\([^/][^/]*\)/.*@\1@'`
    PCP_DIR_N=`echo $PCP_DIR | sed -e 's@\\\\@/@g' -e 's@.*/\([^/][^/]*\)/*$@\1@'`
    if [ "$pmda_dir_1" = "$PCP_DIR_N" ]
    then
	pmda_dir=`echo $pmda_dir | sed -e 's@^/[^/][^/]*\(/.*\)@\1@'`
	pmda_dir="`echo $PCP_DIR | sed -e 's@\\\\@/@g'`$pmda_dir"
    fi
fi

# Other variables and constants
#
do_check=true
__here=`pwd`
__pmcd_is_dead=false
__echo=false
__verbose=false
__ns_opt=''
__args=''

trap "rm -rf $tmp $tmp.*; exit \$status" 0 1 2 3 15

# Parse command line args
#
while [ $# -gt 0 ]
do
    case $1
    in
	-e)	# echo user input
	    __echo=true
	    ;;

	-n)	# alternate name space
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -n requires a name space file option"
		status=1
		exit
	    fi
	    NAMESPACE=$2
	    PMNSROOT=`basename $NAMESPACE`
	    PMNSDIR=`dirname $NAMESPACE`
	    __ns_opt="-n $2" 
	    shift
	    ;;

	-Q)	# skip check for metrics appearing or going away
	    do_check=false
	    ;;

	-R)	# $ROOT
	    if [ "$prog" = "Remove" ]
	    then
		echo "Usage: $prog [-eNQV] [-n namespace]"
		status=1
		exit
	    fi
	    if [ $# -lt 2 ]
	    then
		echo "$prog: -R requires a directory option"
		status=1
		exit
	    fi
	    root=$2
	    shift
	    ;;

	-V)	# verbose
	    __verbose=true
	    ;;

	*)
	    if [ "$prog" = "Install" ]
	    then
		echo "Usage: $prog [-eQV] [-n namespace] [-R rootdir]"
	    else
		echo "Usage: $prog [-eQV] [-n namespace]"
	    fi
	    status=1
	    exit
	    ;;
    esac
    shift
done
