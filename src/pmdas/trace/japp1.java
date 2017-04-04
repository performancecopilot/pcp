//
// Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
// 
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//

public class japp1 {
    public static void main(String[] args) {
	int	sts;
	trace	pcp = new trace();

	pcp.pmtracestate(pcp.PMTRACE_STATE_API);

	sts = pcp.pmtracebegin("transacting in java");
	if (sts < 0)
	    System.out.println("pmtracebegin error: " + pcp.pmtraceerrstr(sts));

	sts = pcp.pmtraceend("transacting in java");
	if (sts < 0)
	    System.out.println("pmtraceend error: " + pcp.pmtraceerrstr(sts));

	sts = pcp.pmtracepoint("java point");
	if (sts < 0)
	    System.out.println("pmtracepoint error: " + pcp.pmtraceerrstr(sts));

	sts = pcp.pmtraceobs("observing from java", 789.034018);
	if (sts < 0)
	    System.out.println("pmtraceobs error: " + pcp.pmtraceerrstr(sts));

	sts = pcp.pmtracecounter("counter from java", 789.034019);
	if (sts < 0)
	    System.out.println("pmtracecounter error: " + pcp.pmtraceerrstr(sts));
    }
}
