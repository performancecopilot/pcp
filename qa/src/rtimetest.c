/*
 * Copyright (c) 2013-2015 Red Hat.
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
#include <pcp/impl.h>

void
set_tm(struct timeval *ntv, struct tm *ntm, struct tm *btm, int mon,
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

    if (ntv != NULL) {
	ntv->tv_sec = __pmMktime(ntm);
	ntv->tv_usec = 0;
    }
}

void
dump_dt(char *str, struct tm *atm)
{
    int pfx;
    printf("\"%s\"%n", str, &pfx);
    printf("%*s", 33 - pfx, " ");
    printf("%d-%.2d-%.2d %.2d:%.2d:%.2d\n",
	   atm->tm_year + 1900,
	   atm->tm_mon + 1,
	   atm->tm_mday, atm->tm_hour, atm->tm_min, atm->tm_sec);
}

int
main(int argc, char *argv[])
{
    struct timeval tvstart;	// .tv_sec .tv_usec
    struct timeval tvend;
    struct timeval tvrslt;
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
    char c;
    int sts;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s [-D debug] [strftime_fmt ...]\n", pmProgname);
	exit(1);
    }

    ttstart = 1392649730;	// time(&ttstart) => time_t
    ttstart = 1390057730;
    tvstart.tv_sec = ttstart;
    tvstart.tv_usec = 0;
    localtime_r(&ttstart, &tmstart);	// time_t => tm
    set_tm(&tvend, &tmend, &tmstart, 0, 27, 11, 28);
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
    *tmtmp_c = ' ';
    if (__pmParseTime(tmtmp_str, &tvstart, &tvend, &tvrslt, &errmsg) != 0) {
	printf ("%s: %s\n", errmsg, tmtmp_str);
    }
    
    localtime_r(&tvrslt.tv_sec, &tmrslt);	// time_t => tm
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

	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "tmtmp: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		tmtmp.tm_sec, tmtmp.tm_min, tmtmp.tm_hour, tmtmp.tm_mday,
		tmtmp.tm_mon, tmtmp.tm_year, tmtmp.tm_wday, tmtmp.tm_yday,
		tmtmp.tm_isdst);

	len = strftime(buffer, sizeof(buffer), fmt, &tmtmp);
	if (len != 0) {
	    struct timeval  rsltStart;
	    struct timeval  rsltEnd;
	    struct timeval  rsltOffset;
	    if (strcmp(fmt, "now") == 0)
		printf
		    ("These time terms for a specific day are relative to the current time.\n");
	    if (__pmParseTime(buffer, &tvstart, &tvend, &tvrslt, &errmsg) != 0) {
		printf ("%s: %s\n", errmsg, tmtmp_str);
	    }
	    localtime_r(&tvrslt.tv_sec, &tmrslt);	// time_t => tm

	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "tmrslt: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		    tmrslt.tm_sec, tmrslt.tm_min, tmrslt.tm_hour, tmrslt.tm_mday,
		    tmrslt.tm_mon, tmrslt.tm_year, tmrslt.tm_wday, tmrslt.tm_yday,
		    tmrslt.tm_isdst);

	    printf("#1 ");
	    dump_dt(buffer, &tmrslt);
	    if (pmParseTimeWindow(buffer, NULL, NULL, NULL, &tvstart, &tvend, &rsltStart, &rsltEnd, &rsltOffset, &errmsg) < 0) {
		printf ("%s: %s\n", errmsg, tmtmp_str);
	    }
	    localtime_r(&rsltStart.tv_sec, &tmrslt);	// time_t => tm

	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "tmrslt: sec=%d min=%d hour=%d mday=%d mon=%d year=%d wday=%d yday=%d isdst=%d\n",
		    tmrslt.tm_sec, tmrslt.tm_min, tmrslt.tm_hour, tmrslt.tm_mday,
		    tmrslt.tm_mon, tmrslt.tm_year, tmrslt.tm_wday, tmrslt.tm_yday,
		    tmrslt.tm_isdst);

	    printf("#2 ");
	    dump_dt(buffer, &tmrslt);
	    localtime_r(&rsltEnd.tv_sec, &tmrslt);	// time_t => tm

	    if (pmDebug & DBG_TRACE_APPL0)
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
