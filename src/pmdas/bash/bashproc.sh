# Common bash(1) procedures to be used in the Performance Co-Pilot
# Bash PMDA trace instrumentation mechanism
#
# Copyright (c) 2012 Nathan Scott.  All Rights Reserved.
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

pcp_trace()
{
    command="$1"
    shift

    case "$command"
    in
        setup|init)
            [ -z "${PCP_TMP_DIR}" ] && return 0	# incorrect call sequence
            trap "pcp_trace off" 0
            export PCP_TRACE_DIR="${PCP_TMP_DIR}/bash"
            export PCP_TRACE_HEADER="${PCP_TRACE_DIR}/.$$"
            export PCP_TRACE_DATA="${PCP_TRACE_DIR}/$$"
            ;;

        stop|off)
            exec 99>/dev/null
            # unlink is performed by pmdabash to resolve race conditions
            unset BASH_XTRACEFD PCP_TRACE_DIR PCP_TRACE_HEADER PCP_TRACE_DATA
            ;;

        start|on)
            # series of sanity checks first
            [ -n "${BASH_VERSION}" ] || return 0        # wrong shell
            [ "${BASH_VERSINFO[0]}" -ge 4 ] || return 0 # no support
            [ "${BASH_VERSINFO[0]}" -ne 4 -o "${BASH_VERSINFO[1]}" -ge 2 ] || \
                return 0 # no support
            [ -z "${PCP_TMP_DIR}" ] && return 0	        # incorrect setup
            [ -z "${PCP_TRACE_DIR}" ] && pcp_trace init $@ # not yet tracing
            [ -d "${PCP_TRACE_DIR}" ] || return 0       # no pcp pmda yet

            # is this the child of a traced shell?
            [ -e /proc/self/fd/99 ] && pcp_trace init $@

            trap "pcp_trace on" 13	# reset on sigpipe (consumer died)
            printf -v PCP_START '%(%s)T' -2
            mkfifo -m 600 "${PCP_TRACE_DATA}" 2>/dev/null || return 0
            # header: version, command, parent, and start time
            echo "version:1 ppid:${PPID} date:${PCP_START} + $@" \
                > "${PCP_TRACE_HEADER}" || return 0
            # setup link between xtrace & fifo
            exec 99>"${PCP_TRACE_DATA}"
            BASH_XTRACEFD=99		# magic bash environment variable
            set -o xtrace		# start tracing from here onward
            # traces: time, line#, and (optionally) function
            PS4='time:${SECONDS} line:${LINENO} func:${FUNCNAME[0]-} + '
            ;;
    esac
}
