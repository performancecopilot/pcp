#!/bin/bash

trap "wax_off" 0
PCP_TMP_DIR="/var/tmp"
TRACE_DIR="${PCP_TMP_DIR}/pmdabash"
TRACE_HEADER="${TRACE_DIR}/.$$"
TRACE_DATA="${TRACE_DIR}/$$"

wax_on()
{
	[ -n "${BASH_VERSION}" ] || return 0		# wrong shell
	[ "${BASH_VERSINFO[0]}" -ge 4 ] || return 0	# no support
	[ ! -d "/proc/$$/fd" -o ! -e "/proc/$$/fd/99" ] || return 0
	[ -d "${TRACE_DIR}" ] || return 0		# no pcp pmda

	trap "wax_on 13" 13	# reset on sigpipe (consumer died)
	printf -v TRACESTART '%(%s)T' -2
	mkfifo -m 600 "${TRACE_DATA}" #2>/dev/null || return 0
	# header: version, command, parent, and start time
	echo "version:1 ppid:${PPID} date:${TRACESTART} + ${BASH_SOURCE} $@" \
		> "${TRACE_HEADER}" || return 0
	# setup link between xtrace & fifo
	exec 99>"${TRACE_DATA}"
	BASH_XTRACEFD=99	# magic bash environment variable
	set -o xtrace		# start tracing from here onward
	# traces: time, line#, and (optionally) function
	PS4='time:${SECONDS} line:${LINENO} func:${FUNCNAME[0]-} + '
}

wax_off()
{
	[ -e "${TRACE_DATA}" ] || return 0
	exec 99>/dev/null
	unlink "${TRACE_DATA}" 2>/dev/null
	unlink "${TRACE_HEADER}" 2>/dev/null
}

tired()
{
	sleep $1
}

wax_on $@
count=0
while true
do
	(( count++ ))
	echo "awoke, $count"	# top level
	tired 2		# call a shell function
done

exit 0
