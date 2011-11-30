	program fapp1

C Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
C 
C This program is free software; you can redistribute it and/or modify it
C under the terms of the GNU General Public License as published by the
C Free Software Foundation; either version 2 of the License, or (at your
C option) any later version.
C 
C This program is distributed in the hope that it will be useful, but
C WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
C or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
C for more details.
C 
C You should have received a copy of the GNU General Public License along
C with this program; if not, write to the Free Software Foundation, Inc.,
C 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

C fapp1.f
C
C Simple program to demonstrate use of the PCP trace performance metrics
C domain agent (PMDA(3)).  This agent needs to be installed before metrics
C can be made available via the performance metrics namespace (PMNS(4)),
C and the Performance Metrics Collector Daemon (PMCD(1)).
C
C Once this program is running, the trace PMDA metrics & instances can be
C viewed through PCP monitor tools such as pmchart(1), pmgadgets(1), and
C pmview(1).  To view the help text associated with each of these metrics,
C use:
C    $ pminfo -tT trace
C
C The pmtracestate constants are defined in /usr/include/pcp/trace.h
C
	external pmtracebegin, pmtraceend, pmtracepoint, pmtraceerrstr, pmtracestate
	integer pmtracebegin, pmtraceend, pmtracepoint
	integer sts
	integer debug
	character*5 prog
	character*40 emesg
	real*8 value
	integer dbg_noagent, dbg_api, dbg_comms, dbg_pdu
	parameter (dbg_noagent = 1, dbg_api = 2, dbg_comms = 4, dbg_pdu = 8)

	prog='fapp1'

C	Addition below is the equivalent to the C 'logical or' operator as
C	trace API constants are all disjoint and the high bit is never set.
	debug = (dbg_api + dbg_comms + dbg_pdu)
	call pmtracestate(debug)

	sts = pmtracebegin('simple')
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtracebegin error: ',emesg
	    stop 1
	endif
	call sleep(2)
	sts = pmtraceend('simple')
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtraceend error: ',emesg
	    stop 1
	endif

	sts = pmtracebegin('ascanbe')
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtracebegin error: ',emesg
	    stop 1
	endif
	call sleep(1)
	sts = pmtraceend('ascanbe')
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtraceend error: ',emesg
	    stop 1
	endif

	sts = pmtracepoint('imouttahere')
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtracepoint error: ',emesg
	    stop 1
	endif

	value = 340.5
	sts = pmtraceobs('end point', value)
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtraceobs error: ',emesg
	    stop 1
	endif

	value = 340.6
	sts = pmtracecounter('new end point', value)
	if (sts .lt. 0) then
	    call pmtraceerrstr(sts, emesg)
	    print *,prog,': pmtracecounter error: ',emesg
	    stop 1
	endif

	end
