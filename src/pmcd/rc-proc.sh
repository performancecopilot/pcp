#
# Common sh(1) procedures to be used in PCP rc scripts
#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
# $Id: rc-proc.sh,v 1.13 2006/06/22 08:51:47 makc Exp $
# 

# source the PCP configuration environment variables
. /etc/pcp.env

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

#
# private functions
#
_cmds_exist()
{
    _have_flag=false
    _have_runlevel=false
    _have_chkconfig=false
    _have_sysvrcconf=false

    [ -f $PCP_RC_DIR/$1 ] && _have_flag=true

    if which runlevel >/dev/null 2>&1
    then
	if [ "$PCP_PLATFORM" = solaris -o "$PCP_PLATFORM" = darwin ]
	then
	    if which runlevel | grep "no runlevel" >/dev/null
	    then
		:
	    else
		_have_runlevel=true
	    fi
	else
	    _have_runlevel=true
	fi
    fi

    if which chkconfig >/dev/null 2>&1
    then
	if [ "$PCP_PLATFORM" = solaris -o "$PCP_PLATFORM" = darwin ]
	then
	    if which chkconfig | grep "no chkconfig" >/dev/null
	    then
		:
	    else
		_have_chkconfig=true
	    fi
	else
	    _have_chkconfig=true
	fi
    fi

    if which sysv-rc-conf >/dev/null 2>&1
    then
	if [ "$PCP_PLATFORM" = solaris -o "$PCP_PLATFORM" = darwin ]
	then
	    if which sysv-rc-conf | grep "no sysv-rc-conf" >/dev/null
	    then
		:
	    else
		_have_sysvrcconf=true
	    fi
	else
	    _have_sysvrcconf=true
	fi
    fi
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
    if [ -f /sbin/init.d/rc2.d/S14argo ]
    then
	# start before the nwrescued (ups) daemon, which uses our port
	# but apparently tolerates using a different port.
	echo 13
    else
	$PCP_AWK_PROG '/^# chkconfig:/ {print $4}' $PCP_RC_DIR/$1
    fi
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
    LANG=C
    _flag=$1

    _ret=1	# return "off" by default
    _rl=3	# default run level if !_have_runlevel

    _cmds_exist $_flag
    $_have_runlevel && _rl=`runlevel | $PCP_AWK_PROG '{print $2}'`

    if [ "$PCP_PLATFORM" = cygwin -o "$PCP_PLATFORM" = interix ]
    then
	# no chkconfig, just do it
	#
	_ret=0
    elif [ "$PCP_PLATFORM" = "darwin" ]
    then
	if [ "`. /etc/hostconfig; echo $PCP`" = "-YES-" ]
	then
	    _ret=0
	fi
    elif $_have_chkconfig
    then
	if chkconfig --list "$_flag" 2>&1 | grep $_rl":on" >/dev/null 2>&1
	then
	    _ret=0 # on
	fi
    elif $_have_sysvrcconf
    then
	if sysv-rc-conf --list "$_flag" 2>&1 | grep $_rl":on" >/dev/null 2>&1
	then
	    _ret=0 # on
	fi
    else
	#
	# don't have chkconfig, so use the existence of the symlink
	#
	if [ -f /etc/debian_version -o "$PCP_PLATFORM" = solaris ]; then
	   if ls /etc/rc$_rl.d/S[0-9]*$_flag >/dev/null 2>&1
	   then
	      _ret=0 # on
	   fi
	else
	   if ls /etc/rc.d/rc$_rl.d/S[0-9]*$_flag >/dev/null 2>&1
	   then
	      _ret=0 # on
	   fi
	fi
    fi

    return $_ret
}

#
# chkconfig "on" $1
# Handles missing chkconfig command.
# (this is used by the pcp rpm %post script)
#
chkconfig_on()
{
    _flag=$1
    [ -z "$_flag" ] && return 1 # fail

    _cmds_exist $_flag
    $_have_flag || return 1 # fail

    if [ "$PCP_PLATFORM" = cygwin -o "$PCP_PLATFORM" = interix ]
    then
	# no chkconfig, just pretend
	#
	return 0
    elif [ "$PCP_PLATFORM" = "darwin" ] 
    then
	echo "To enable $_flag, add the following line to /etc/hostconfig:"
	echo "$_flag=-YES-"
    elif $_have_chkconfig
    then
	# enable default run levels
	chkconfig "$_flag" on >/dev/null 2>&1
    elif $_have_sysvrcconf
    then
	# enable default run levels
	sysv-rc-conf "$_flag" on >/dev/null 2>&1
    else
	_start=`_runlevel_start $_flag`
	_stop=`_runlevel_stop $_flag`
	if [ -f /etc/debian_version ]; then
	   update-rc.d -f $_flag defaults s$_start k$_stop
        elif [ "$PCP_PLATFORM" = solaris ]; then
	   for _r in `_runlevels $_flag`
	   do
	       ln -sf /etc/init.d/$_flag /etc/rc$_r.d/S$_start""$_flag >/dev/null 2>&1
	       ln -sf /etc/init.d/$_flag /etc/rc$_r.d/K$_stop""$_flag >/dev/null 2>&1
	   done
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
# (this is used by the pcp rpm %preun script)
#
chkconfig_off()
{
    _flag=$1
    [ -z "$_flag" ] && return 1 # fail

    _cmds_exist $_flag
    $_have_flag || return 1 # fail

    if [ "$PCP_PLATFORM" = cygwin -o "$PCP_PLATFORM" = interix ]
    then
	# no chkconfig, just pretend
	#
	return 0
    elif $_have_chkconfig
    then
	chkconfig --level 2345 "$_flag" off >/dev/null 2>&1
    elif $_have_sysvrcconf
    then
	sysv-rc-conf --level 2345 "$_flag" off >/dev/null 2>&1
    else
	# remove the symlinks
	if [ -f /etc/debian_version ]; then
	   update-rc.d -f $_flag remove
        elif [ "$PCP_PLATFORM" = solaris ]; then
	  rm -f /etc/rc[0-9].d/[SK][0-9]*$_flag >/dev/null 2>&1
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

    if [ "$PCP_PLATFORM" = cygwin -o "$PCP_PLATFORM" = interix ]
    then
	# no chkconfig, just pretend
	#
	return 0
    else
	echo "    To enable $_flag, run the following as root:"
	if $_have_chkconfig
	then
	    _cmd=`which chkconfig`
	    echo "    # $_cmd $_flag on"
	elif $_have_sysvrcconf
	then
	    _cmd=`which sysv-rc-conf`
	    echo "    # $_cmd $_flag on"
	else
	    if [ -f /etc/debian_version ]; then
	      echo "         update-rc.d -f $_flag defaults s$_start k$_stop"
	    elif [ "$PCP_PLATFORM" = solaris ]; then 
	      _start=`_runlevel_start $_flag`
	      _stop=`_runlevel_stop $_flag`
	      for _r in `_runlevels $_flag`
	      do
		  echo "    # ln -sf /etc/init.d/$_flag /etc/rc$_r.d/S$_start""$_flag"
		  echo "    # ln -sf /etc/init.d/$_flag /etc/rc$_r.d/K$_stop""$_flag"
	      done
	    else
	      _start=`_runlevel_start $_flag`
	      _stop=`_runlevel_stop $_flag`
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
if [ -r /etc/rc.status ]
then
    #
    # SuSE style
    . /etc/rc.status
    RC_STATUS=rc_status
    RC_RESET=rc_reset
    RC_CHECKPROC=checkproc
else
    #
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
	[ $_n -eq 1 ] && return 0

	# not running, but pid exists
	[ -e /var/run/$_b.pid ] && return 1

	# program not installed
	[ ! -e "$1" ] && return 5

	# not running and no pid
	return 3
    }
fi
