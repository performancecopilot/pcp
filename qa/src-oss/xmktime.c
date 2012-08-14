/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

time_t	xx[] = { 8*60*60, 825042862, -1 };

char	*tz[] = {
    "EST-11EST-10,86/2:00,303/2:00", "UTC", "PST7PDT7", (char *)0
};

int
main()
{
    struct tm	*tmp;
    struct tm	mytm;
    char	buf[28];
    time_t	ans;
    int		i;
    int		j;
    int		m;

    printf("standard libc routines\n");
    for (i = 0; xx[i] != -1; i++) {
	tmp = localtime(&xx[i]);
	ans = mktime(tmp);
	printf("initial %d -> %d %s", (int)xx[i], (int)ans, ctime(&ans));
	for (m = -3; m < 4; m++) {
	    if (m == 0) continue;
	    tmp = localtime(&xx[i]);
	    tmp->tm_mon += m;
	    ans = mktime(tmp);
	    if (m < 0)
		printf("%d months -> %d %s", m, (int)ans, ctime(&ans));
	    else
		printf("+%d months -> %d %s", m, (int)ans, ctime(&ans));
	}
	printf("\n");
    }

    for (j = 0; tz[j] != (char *)0; j++) {
	printf("pmNewZone(\"%s\")\n", tz[j]);
	pmNewZone(tz[j]);
	for (i = 0; xx[i] != -1; i++) {
	    tmp = pmLocaltime(&xx[i], &mytm);
	    ans = __pmMktime(tmp);
	    printf("initial %d -> %d %s", (int)xx[i], (int)ans, pmCtime(&ans, buf));
	    for (m = -3; m < 4; m++) {
		if (m == 0) continue;
		tmp = pmLocaltime(&xx[i], &mytm);
		tmp->tm_mon += m;
		ans = __pmMktime(tmp);
		if (m < 0)
		    printf("%d months -> %d %s", m, (int)ans, pmCtime(&ans, buf));
		else
		    printf("+%d months -> %d %s", m, (int)ans, pmCtime(&ans, buf));
	    }
	    printf("\n");
	}
    }

    exit(0);
}
