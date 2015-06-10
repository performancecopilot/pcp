#
# Common sh(1) procedures to be used in PCP rc scripts
#
# Copyright (c) 2014-2015 Red Hat.
# Copyright (c) 2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

# source the PCP configuration environment variables
. $PCP_DIR/etc/pcp.env

# These functions use chkconfig if available, else tolerate missing chkconfig
# command (as on SUSE) by manipulating symlinks in /etc/rc.d directly.
#
# Usage:
#
# is_chkconfig_on : return 0 if $1 is chkconfig "on" else 1
# chkconfig_on    : chkconfig $1 "on"
# chkconfig_off   : chkconfig $1 "off"
# chkconfig_on_msg: echo a message about how to chkconfig $1 on
#

VERBOSE_CONFIG=${VERBOSE_CONFIG-false}

#
# private functions
#
_which()
{
    # some versions of which(1) have historically not reflected the
    # correct exit status ... but it appears that all modern platforms
    # get this correct
    #
    # keeping the old logic structure, just in case
    #

    if $PCP_WHICH_PROG $1 >/dev/null 2>&1
    then
	if [ "$PCP_PLATFORM" = broken ]
	then
	    if $PCP_WHICH_PROG $1 | grep "no $1" >/dev/null
	    then
		:
	    else
		return 0
	    fi
	else
	    return 0
	fi
    fi
    return 1
}

_cmds_exist()
{
    _have_flag=false
    [ -f $PCP_RC_DIR/$1 ] && _have_flag=true

    # systemctl is special ... sometimes it is installed, but not with
    # full systemd behind it, e.g Debian-based systems circa 2015
    # ... see special case handling where systemctl might be used in
    # the "do something" sections elsewhere in this file and it only
    # makes sense to try systemctl if the corresponding
    # $PCP_SYSTEMDUNIT_DIR/$_flag.service file exists.
    #
    _have_systemctl=false
    _which systemctl && _have_systemctl=true
    _have_runlevel=false
    _which runlevel && _have_runlevel=true
    _have_chkconfig=false
    _which chkconfig && _have_chkconfig=true
    _have_sysvrcconf=false
    _which sysvrcconf && _have_sysvrcconf=true
    _have_rcupdate=false
    _which rc-update && _have_rcupdate=true
    _have_svcadm=false
    _which svcadm && _have_svcadm=true
}

#
# return the run levels for $1
#
_runlevels()
{
    $PCP_AWK_PROG '/^# chkconfig:/ {print $3}' $PCP_RC_DIR/$1 | sed -e 's/[0-9]/& /g'
}

#
# return rc start number for $1
#
_runlevel_start()
{
    $PCP_AWK_PROG '/^# chkconfig:/ {print $4}' $PCP_RC_DIR/$1
}

#
# return runlevel stop number for $1
#
_runlevel_stop()
{
    $PCP_AWK_PROG '/^# chkconfig:/ {print $5}' $PCP_RC_DIR/$1
}

#
# Return 0 if $1 is chkconfig "on" (enabled) at the current run level
# Handles missing chkconfig command and other assorted atrocities.
#
is_chkconfig_on()
{
    # if non-default install, everything is "on"
    [ -n "$PCP_DIR" ] && return 0

    LANG=C
    _flag=$1

    _ret=1	# return "off" by default
    _rl=3	# default run level if !_have_runlevel

    _cmds_exist $_flag
    $_have_runlevel && _rl=`runlevel | $PCP_AWK_PROG '{print $2}'`

    if [ "$PCP_PLATFORM" = mingw -o "$PCP_PLATFORM" = "freebsd" ]
    then
	# unknown mechanism, just do it
	$VERBOSE_CONFIG && echo "is_chkconfig_on: unconditionally on"
	_ret=0
    elif [ "$PCP_PLATFORM" = "darwin" ]
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using /etc/hostconfig"
	case "$1"
        in
	pmcd)     [ "`. /etc/hostconfig; echo $PMCD`" = "-YES-" ] && _ret=0 ;;
	pmlogger) [ "`. /etc/hostconfig; echo $PMLOGGER`" = "-YES-" ] && _ret=0 ;;
	pmie)     [ "`. /etc/hostconfig; echo $PMIE`" = "-YES-" ] && _ret=0 ;;
	pmproxy)  [ "`. /etc/hostconfig; echo $PMPROXY`" = "-YES-" ] && _ret=0 ;;
	pmwebd)   [ "`. /etc/hostconfig; echo $PMWEBD`" = "-YES-" ] && _ret=0 ;;
	pmmgr)    [ "`. /etc/hostconfig; echo $PMMGR`" = "-YES-" ] && _ret=0 ;;
	esac
    elif [ "$_have_systemctl" = true -a -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/$_flag.service" ]
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using systemctl"
	# if redirected to chkconfig, the answer is buried in stdout
	# otherwise it is in the exit status of the systemctl command
	#
	if systemctl is-enabled "$_flag".service 2>&1 | grep -q 'redirecting to /sbin/chkconfig'
	then
	    $VERBOSE_CONFIG && echo "is_chkconfig_on: redirected to chkconfig"
	    if systemctl is-enabled "$_flag".service 2>&1 | grep -q "^$_flag[ 	][ 	]*on"
	    then
		_ret=0
	    fi
	else
	    systemctl -q is-enabled "$_flag".service
	    _ret=$?
	fi
    elif $_have_chkconfig
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using chkconfig | grep $_r1:on"
	chkconfig --list "$_flag" 2>&1 | grep $_rl":on" >/dev/null 2>&1 && _ret=0
    elif $_have_sysvrcconf
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using sysv-rc-conf | grep $_r1:on"
	sysv-rc-conf --list "$_flag" 2>&1 | grep $_rl":on" >/dev/null 2>&1 && _ret=0
    elif $_have_rcupdate
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using rc-update"
	rc-update show 2>&1 | grep "$_flag" >/dev/null 2>&1 && _ret=0
    elif $_have_svcadm
    then
	$VERBOSE_CONFIG && echo "is_chkconfig_on: using svcs"
	svcs -l pcp/$_flag | grep "enabled  *true" >/dev/null 2>&1 && _ret=0
    else
	#
	# don't know, fallback to using the existence of rc symlinks
	#
	if [ -f /etc/debian_version ]; then
	    $VERBOSE_CONFIG && echo "is_chkconfig_on: using /etc/rc$_rl.d/S[0-9]*$_flag"
	   ls /etc/rc$_rl.d/S[0-9]*$_flag >/dev/null 2>&1 && _ret=0
	else
	    $VERBOSE_CONFIG && echo "is_chkconfig_on: using /etc/rc.d/rc$_rl.d/S[0-9]*$_flag"
	   ls /etc/rc.d/rc$_rl.d/S[0-9]*$_flag >/dev/null 2>&1 && _ret=0
	fi
    fi

    return $_ret
}

#
# chkconfig "on" $1
# Handles missing chkconfig command.
#
chkconfig_on()
{
    # if non-default install, everything is "on"
    [ -n "$PCP_DIR" ] && return 0

    _flag=$1
    [ -z "$_flag" ] && return 1 # fail

    _cmds_exist $_flag
    $_have_flag || return 1 # fail

    if [ "$PCP_PLATFORM" = mingw -o "$PCP_PLATFORM" = "freebsd" ]
    then
	# unknown mechanism, just pretend
	return 0
    elif [ "$PCP_PLATFORM" = "darwin" ] 
    then
	echo "To enable $_flag, add the following line to /etc/hostconfig:"
	case "$_flag"
	in
	pmcd) echo "PMCD=-YES-" ;;
	pmlogger) echo "PMLOGGER=-YES-" ;;
	pmie) echo "PMIE=-YES-" ;;
	pmproxy) echo "PMPROXY=-YES-" ;;
	pmwebd) echo "PMWEBD=-YES-" ;;
	pmmgr) echo "PMMGR=-YES-" ;;
	esac
    elif [ "$_have_systemctl" = true -a -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/$_flag.service" ]
    then
	systemctl --no-reload enable "$_flag".service >/dev/null 2>&1
    elif $_have_chkconfig
    then
	chkconfig "$_flag" on >/dev/null 2>&1
    elif $_have_sysvrcconf
    then
	sysv-rc-conf "$_flag" on >/dev/null 2>&1
    elif $_have_rcupdate
    then
	rc-update add "$_flag" >/dev/null 2>&1
    elif $_have_svcadm
    then
	svcadm enable pcp/$_flag >/dev/null 2>&1
    else
	_start=`_runlevel_start $_flag`
	_stop=`_runlevel_stop $_flag`
	if [ -f /etc/debian_version ]
	then
	    update-rc.d -f $_flag defaults s$_start k$_stop
	else
	    for _r in `_runlevels $_flag`
	    do
		ln -sf ../init.d/$_flag /etc/rc.d/rc$_r.d/S$_start""$_flag >/dev/null 2>&1
		ln -sf ../init.d/$_flag /etc/rc.d/rc$_r.d/K$_stop""$_flag >/dev/null 2>&1
	   done
	fi
    fi

    return 0
}

#
# chkconfig "off" $1
# Handles missing chkconfig command.
#
chkconfig_off()
{
    # if non-default install, everything is "on"
    [ -n "$PCP_DIR" ] && return 1

    _flag=$1
    [ -z "$_flag" ] && return 1 # fail

    _cmds_exist $_flag
    $_have_flag || return 1 # fail

    if [ "$PCP_PLATFORM" = mingw -o "$PCP_PLATFORM" = "freebsd" ]
    then
	# unknown mechanism, just pretend
	return 0
    elif [ "$_have_systemctl" = true -a -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/$_flag.service" ]
    then
	systemctl --no-reload disable "$_flag".service >/dev/null 2>&1
    elif $_have_chkconfig
    then
	chkconfig --level 2345 "$_flag" off >/dev/null 2>&1
    elif $_have_sysvrcconf
    then
	sysv-rc-conf --level 2345 "$_flag" off >/dev/null 2>&1
    elif $_have_rcupdate
    then
	rc-update delete "$_flag" >/dev/null 2>&1
    elif $_have_svcadm
    then
	svcadm disable pcp/$_flag >/dev/null 2>&1
    else
	# remove the symlinks
	if [ -f /etc/debian_version ]
	then
	    update-rc.d -f $_flag remove
	else
	    rm -f /etc/rc.d/rc[0-9].d/[SK][0-9]*$_flag >/dev/null 2>&1
	fi
    fi

    return 0
}

#
# Echo a message about how to chkconfig $1 "on"
# Tolerates missing chkconfig command
#
chkconfig_on_msg()
{
    _flag=$1
    _cmds_exist $_flag
    $_have_flag || return 1 # fail

    if [ "$PCP_PLATFORM" = mingw -o "$PCP_PLATFORM" = "freebsd" ]
    then
	# no mechanism, just pretend
	#
	return 0
    else
	echo "    To enable $_flag, run the following as root:"
	if [ "$_have_systemctl" = true -a -n "$PCP_SYSTEMDUNIT_DIR" -a -f "$PCP_SYSTEMDUNIT_DIR/$_flag.service" ]
	then
	    _cmd=`$PCP_WHICH_PROG systemctl`
	    echo "    # $_cmd enable $_flag.service"
	elif $_have_chkconfig
	then
	    _cmd=`$PCP_WHICH_PROG chkconfig`
	    echo "    # $_cmd $_flag on"
	elif $_have_sysvrcconf
	then
	    _cmd=`$PCP_WHICH_PROG sysvrcconf`
	    echo "    # $_cmd $_flag on"
	elif $_have_rcupdate
	then
	    _cmd=`$PCP_WHICH_PROG rc-update`
	    echo "    # $_cmd add $_flag"
	elif $_have_svcadm
	then
	    _cmd=`$PCP_WHICH_PROG svcadm`
	    echo "    # $_cmd enable pcp/$_flag"
	else
	    _start=`_runlevel_start $_flag`
	    _stop=`_runlevel_stop $_flag`
	    if [ -f /etc/debian_version ]
	    then
		echo "         update-rc.d -f $_flag remove"
		echo "         update-rc.d $_flag defaults $_start $_stop"
	    else
		for _r in `_runlevels $_flag`
		do
		    echo "    # ln -sf ../init.d/$_flag /etc/rc.d/rc$_r.d/S$_start""$_flag"
		    echo "    # ln -sf ../init.d/$_flag /etc/rc.d/rc$_r.d/K$_stop""$_flag"
		done
	    fi
	fi
    fi

    return 0
}

#
# load some rc functions if available
#
# In openSUSE 12.1, /etc/rc.status intercepts our rc script and passes
# control to systemctl which uses systemd ... the result is that messages
# from our rc scripts are sent to syslog by default, and there is no
# apparent way to revert to the classical behaviour, so this "hack" allows
# PCP QA to set $PCPQA_NO_RC_STATUS and continue to see stdout and stderr
# from our rc scripts
# - Ken 1 Dec 2011
#
if [ -r /etc/rc.status -a -z "${PCPQA_NO_RC_STATUS+set}" ]
then
    # SuSE style
    . /etc/rc.status
    RC_STATUS=rc_status
    RC_RESET=rc_reset
    RC_CHECKPROC=checkproc
else
    # Roll our own
    RC_STATUS=_RC_STATUS
    _RC_STATUS()
    {
	_rc_status=$?
	if [ "$1" = "-v" ]
	then
	    if [ $_rc_status -eq 0 ]
	    then $ECHO
	    else
	    	$ECHO "failed (status=$_rc_status)"
	    fi
	fi
	return $_rc_status
    }

    RC_RESET=_RC_RESET
    _RC_RESET()
    {
    	_rc_status=0
	return $_rc_status
    }

    RC_CHECKPROC=_RC_CHECKPROC
    _RC_CHECKPROC()
    {
	# usage
	[ $# -ne 1 ] && return 2

	# running
	_b=`basename "$1"`
	_n=`_get_pids_by_name $_b | wc -l`
	[ $_n -ge 1 ] && return 0

	# not running, but pid exists
	[ -e /var/run/$_b.pid ] && return 1

	# program not installed
	[ ! -e "$1" ] && return 5

	# not running and no pid
	return 3
    }
fi
