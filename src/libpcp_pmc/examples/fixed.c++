/*
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <pcp/pmc/Group.h>
#include <pcp/pmc/Metric.h>
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;

    pmProgname = basename(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':
	    sts = __pmParseDebug(optarg);
            if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			 pmProgname, optarg);
                sts = 1;
            }
            else {
                pmDebug |= sts;
		sts = 0;
	    }
            break;
	case '?':
	default:
	    sts = 1;
	    break;
	}
    }

    if (sts) {
	pmprintf("Usage: %s\n", pmProgname);
	pmflush();
	exit(1);
        /*NOTREACHED*/
    }


    PMC_Group group;

    // Add some simple metrics to the group
    PMC_Metric* hinv_ncpu = group.addMetric("hinv.ncpu");
    PMC_Metric* hinv_ndisk = group.addMetric("hinv.ndisk");
    PMC_Metric* timezone = group.addMetric("pmcd.timezone");

    pmflush();
    if (hinv_ncpu->status() < 0 || 
	hinv_ndisk->status() < 0 ||
	timezone->status() < 0) {
	exit(1);
	/*NOTREACHED*/
    }
	
    // Fetch the metrics
    group.fetch();

    if (hinv_ncpu->error(0) < 0) {
	fprintf(stderr, "%s: %s: %s\n", 
		pmProgname, hinv_ncpu->spec(PMC_true).ptr(), 
		pmErrStr(hinv_ncpu->error(0)));
	sts = 1;
    }
    else
	printf("Number of CPUS = %d\n", (int)hinv_ncpu->value(0));

    if (hinv_ndisk->error(0) < 0) {
	fprintf(stderr, "%s: %s: %s\n", 
		pmProgname, hinv_ndisk->spec(PMC_true).ptr(), 
		pmErrStr(hinv_ndisk->error(0)));
	sts = 1;
    }
    else
	printf("Number of disks = %d\n", (int)hinv_ndisk->value(0));

    if (timezone->error(0) < 0) {
	fprintf(stderr, "%s: %s: %s\n", 
		pmProgname, timezone->spec(PMC_true).ptr(), 
		pmErrStr(timezone->error(0)));
	sts = 1;
    }
    else
	printf("Timezone = %s\n", timezone->strValue(0).ptr());

    return sts;
}
