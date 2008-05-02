#!/bin/sh
#
# Copyright (c) 2007 Aconex.  All Rights Reserved.
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
# pmcygrunsrv:
#	Manipulates one of the PCP "services" via cygrunsrv.exe.
#	This script provides a dummy "service" executable which can
#	be installed/removed/stopped/started/queried via the Windows
#	(and Cygwin) services mechanisms.
#

# Get standard environment
. /etc/pcp.env

server="$PCP_SHARE_DIR/bin/pmcygwinserver"
user=Administrator
service=pcp	# "pcp", "pmie" and "pmproxy" are valid

verify_service()
{
    service="$1"
    if [ "$1" != pcp -a "$1" != pmie -a "$1" != pmproxy ]; then
	echo "Invalid service name: $service \(not pcp, pmie, or pmproxy\)"
	exit 1
    fi
}

report_service()
{
    service="$1"
    cygrunsrv $verbose -Q $service 2>/dev/null
    if [ $? -ne 0 ]; then
	echo "Service             : $service"
	echo "Current State       : Not installed"
    fi
}

usage()
{
    cat - <<EOF
Usage: pmcygrunsrv [options]

Options:
  -I <svc_name>   Installes a new service named <svc_name>.
  -R <svc_name>   Removes a service named <svc_name>.
  -S <svc_name>   Starts a service named <svc_name>.
  -E <svc_name>   Stops a service named <svc_name>.
  -Q <svc_name>   Queries a service named <svc_name>.
  -L              Lists services that have been installed.

Services:
  Valid <svc_names> are pcp, pmie, and pmproxy.
EOF
    exit 1
}

install=false
remove=false
start=false
stop=false
query=false
list=false
verbose=

while getopts I:R:S:E:Q:LVu:? c
do
    case $c
    in
	I)	verify_service $OPTARG; install=true ;;
	R)	verify_service $OPTARG; remove=true  ;;
	S)	verify_service $OPTARG; start=true   ;;
	E)	verify_service $OPTARG; stop=true    ;;
	Q)	verify_service $OPTARG; query=true   ;;
	L)	list=true ;;
	V)	verbose=-V ;;
	u)	user=$OPTARG ;;
	*)	usage ;;
    esac
done
if $install    ; then
    cygrunsrv -I $service -u $user -n -o -y tcpip -s 1 -p $server -a $service \
	-1 /var/log/pcp/cygwin/$service.log -2 /var/log/pcp/cygwin/$service.log
elif $remove   ; then
    cygrunsrv -R $service
elif $query    ; then
    cygrunsrv -Q $service $verbose
elif $start    ; then
    cygrunsrv -S $service
elif $stop     ; then
    cygrunsrv -E $service
else
    report_service pcp
    report_service pmie
    report_service pmproxy
fi
exit 0
