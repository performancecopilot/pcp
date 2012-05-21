#!/bin/bash

trap "wax_off; exit" 1 2 3 4 5 6 7 8 9 10 11 12 15
TRACEFILE="/var/tmp/pcp-bash/$$"
TRACEFD=99

wax_on()
{
	[ ! -e "/proc/$$/fd/$TRACEFD" ] || return 0
	[ -d `dirname "${TRACEFILE}"` ] || return 0
	[ "${BASH_VERSINFO[0]}" -ge 4 ] || return 0

	trap "wax_on 13" 13	# reset on sigpipe (consumer died)
	printf -v TRACESTART '%(%s)T' -2
	mkfifo -m 600 "${TRACEFILE}" 2>/dev/null || return 0
	# header trace: command, parent, and start time
	PS4='bash:${BASH_SOURCE} ppid:${PPID} date:${TRACESTART} + '
	exec 99>"$TRACEFILE"
	BASH_XTRACEFD="$TRACEFD"
	set -o xtrace
	# recurring traces: time, line#, and (optionally) function
	PS4='time:${SECONDS} line:${LINENO} func:${FUNCNAME[0]-} + '
}

wax_off()
{
	[ -e "${TRACEFILE}" ] || return 0
	exec 99>/dev/null
	unlink "${TRACEFILE}" 2>/dev/null
}

tired()
{
	sleep $1
}

wax_on
count=0
while true
do
	(( count++ ))
	echo "awoke, $count"	# top level
	tired 2			# call a shell function
done

exit 0
