/*
 * Copyright (c) 2013-2015,2022 Red Hat.
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
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

void
set_tm(struct timespec *nts, struct tm *ntm, struct tm *btm, int mon,
       int mday, int hour, int min)
{
    memcpy(ntm, btm, sizeof(struct tm));
    if (mon > 0)
	ntm->tm_mon = mon;
    if (mday > 0)
	ntm->tm_mday = mday;
    if (hour > 0)
	ntm->tm_hour = hour;
    if (min > 0)
	ntm->tm_min = min;

    if (nts != NULL) {
	nts->tv_sec = __pmMktime(ntm);
	nts->tv_nsec = 0;
    }
}

void
dump_dt(char *str, struct tm *atm)
{
    printf("\"%s\"", str);
    printf("%*s", (int)(33 - strlen(str) - 2), " ");
    printf("%d-%.2d-%.2d %.2d:%.2d:%.2d\n",
	   atm->tm_year + 1900,
	   atm->tm_mon + 1,
	   atm->tm_mday, atm->tm_hour, atm->tm_min, atm->tm_sec);
}

int
main(int argc, char *argv[])
{
    struct timespec tsstart;	// .tv_sec .tv_nsec
    struct timespec tsend;
    struct timespec tsrslt;
    struct tm tmstart;		// .tm_sec .tm_min .tm_hour .tm_mday
    // .tm_mon .tm_year .tm_wday .tm_yday
    struct tm tmend;
    struct tm tmrslt;
    struct tm tmtmp;
    time_t ttstart;
    char buffer[256];
    char *errmsg;
    char *tmtmp_str;
    char *tz;
    int errflag = 0;
    int c;
    int sts;
    time_t clock;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s [-D debug] [strftime_fmt ...]\n", pmGetProgname());
	exit(1);
    }

    ttstart = 1390057730;
    tsstart.tv_sec = ttstart;
    tsstart.tv_nsec = 0;
    localtime_r(&ttstart, &tmstart);	// time_t => tm
    set_tm(&tsend, &tmend, &tmstart, 0, 27, 11, 28);
    printf("   ");
    dump_dt("start ", &tmstart);
    printf("   ");
    dump_dt("end   ", &tmend);

    tz = getenv("TZ");
    if (tz != NULL) {
	pmNewZone(tz);
    }

    printf("These time terms are relative to the start/end time.\n"
	   "#1 __pmParseTime #2 pmParseTimeWindow/Start #3 pmParseTimeWindow/End.\n");
    set_tm(NULL, &tmtmp, &tmstart, 0, 19, 11, 45);
    tmtmp_str = asctime(&tmtmp);
    char *tmtmp_c = strchr(tmtmp_str, '\n');
    if (tmtmp_c)
	*tmtmp_c = ' ';
    if (__pmParseHighResTime(tmtmp_str, &tsstart, &tsend, &tsrslt, &errmsg) != 0) {
	printf ("%s: %s\n", errmsg, tmtmp_str);
    }
    
    clock = tsrslt.tv_sec;
    localtime_r(&clock, &tmrslt);	// time_t => tm
    printf("   ");
    dump_dt(tmtmp_str, &tmrslt);

    // See strftime for a description of the % formats
    char *strftime_fmt[] = {
	"+1minute",
	"-1 minute",
	"-1minute",
	"%F",
	"%D",
	"%D %r",
	"%D %r -1month",
	"%D %r tomorrow",
	"%D %r yesterday",
	"%D %R",
	"%D %T",
	"%d %b %Y %X",
	"1 day ago",
	"1 week ago",
	"@%F",
	"@%D",
	"@%D %r",
	"@%D %R",
	"@%D %T",
	"@%D %T GMT",
	"@%d %b %Y %X",
	"@next day",
	"@1 day ago",
	"1 day",
	"5 minutes 5 seconds",
	"last week",
	"last day",
	"next day",
	// relative to current time terms begin here
	"now",
	"today",
	"@yesterday",
	"yesterday",
	"tomorrow",
	"sunday",
	"first sunday",
	"this sunday",
	"next sunday",
	"last sunday",
	"monday",
	"first monday",
	"this monday",
	"next monday",
	"last monday",
	"tuesday",
	"first tuesday",
	"this tuesday",
	"next tuesday",
	"last tuesday",
	"wednesday",
	"first wednesday",
	"this wednesday",
	"next wednesday",
	"last wednesday",
	"thursday",
	"first thursday",
	"this thursday",
	"next thursday",
	"last thursday",
	"friday",
	"first friday",
	"this friday",
	"next friday",
	"last friday",
	"saturday",
	"first saturday",
	"this saturday",
	"next saturday",
	"last saturday",
    };



    int sfx;
    for (sfx = 0; sfx < (sizeof(strftime_fmt) / sizeof(void *)); sfx++) {
	char *fmt = strftime_fmt[sfx];
	int len;

	/* non-flag args are argv[optind] ... argv[argc-1] */
	if (optind < argc) {
	    /* over-ride from command line */
	    if (sfx+optind >= argc) return 0;
	    fmt = argv[sfx+optind];
	}

	if (pmDebugOptions.appl0)
	    fprintf(stderr, "tmtmp: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		tmtmp.tm_sec, tmtmp.tm_min, tmtmp.tm_hour, tmtmp.tm_mday,
		tmtmp.tm_mon, tmtmp.tm_year, tmtmp.tm_wday, tmtmp.tm_yday,
		tmtmp.tm_isdst);

	len = strftime(buffer, sizeof(buffer), fmt, &tmtmp);
	if (len != 0) {
	    struct timespec rsltStart;
	    struct timespec rsltEnd;
	    struct timespec rsltOffset;
	    if (strcmp(fmt, "now") == 0)
		printf
		    ("These time terms for a specific day are relative to the current time.\n");
	    if (__pmParseHighResTime(buffer, &tsstart, &tsend, &tsrslt, &errmsg) != 0) {
		printf ("%s: %s\n", errmsg, tmtmp_str);
	    }
	    clock = tsrslt.tv_sec;
	    localtime_r(&clock, &tmrslt);	// time_t => tm

	    if (pmDebugOptions.appl0)
		fprintf(stderr, "tmrslt: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		    tmrslt.tm_sec, tmrslt.tm_min, tmrslt.tm_hour, tmrslt.tm_mday,
		    tmrslt.tm_mon, tmrslt.tm_year, tmrslt.tm_wday, tmrslt.tm_yday,
		    tmrslt.tm_isdst);

	    printf("#1 ");
	    dump_dt(buffer, &tmrslt);
	    if (pmParseHighResTimeWindow(buffer, NULL, NULL, NULL, &tsstart, &tsend, &rsltStart, &rsltEnd, &rsltOffset, &errmsg) < 0) {
		printf ("%s: %s\n", errmsg, tmtmp_str);
	    }
	    clock = rsltStart.tv_sec;
	    localtime_r(&clock, &tmrslt);	// time_t => tm

	    if (pmDebugOptions.appl0)
		fprintf(stderr, "tmrslt: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		    tmrslt.tm_sec, tmrslt.tm_min, tmrslt.tm_hour, tmrslt.tm_mday,
		    tmrslt.tm_mon, tmrslt.tm_year, tmrslt.tm_wday, tmrslt.tm_yday,
		    tmrslt.tm_isdst);

	    printf("#2 ");
	    dump_dt(buffer, &tmrslt);
	    clock = rsltEnd.tv_sec;
	    localtime_r(&clock, &tmrslt);	// time_t => tm

	    if (pmDebugOptions.appl0)
		fprintf(stderr, "tmrslt: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		    tmrslt.tm_sec, tmrslt.tm_min, tmrslt.tm_hour, tmrslt.tm_mday,
		    tmrslt.tm_mon, tmrslt.tm_year, tmrslt.tm_wday, tmrslt.tm_yday,
		    tmrslt.tm_isdst);

	    printf("#3 ");
	    dump_dt(buffer, &tmrslt);
	}
	else
	    printf("strftime format \"%s\" not recognized\n", fmt);
    }

    return 0;
}
