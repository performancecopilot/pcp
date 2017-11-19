/*
 * Trace PMDA - process level transaction monitoring for libpcp_trace processes
 *
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"
#include "trace.h"
#include "trace_dev.h"
#include "comms.h"
#include "data.h"

#define DEFAULT_TIMESPAN	60	/* one minute  */
#define DEFAULT_BUFSIZE		5	/* twelve second update */

struct timeval	timespan  = { DEFAULT_TIMESPAN, 0 };
struct timeval	interval;
unsigned int	rbufsize  = DEFAULT_BUFSIZE;
int		ctlport	  = -1;
char		*ctlsock;
int		somedebug;

static char	mypath[MAXPATHLEN];
static char	*username;

extern void traceInit(pmdaInterface *dispatch);
extern void traceMain(pmdaInterface *dispatch);
extern int  updateObserveValue(const char *);
extern int  updateCounterValue(const char *);
extern void debuglibrary(void);

static void
usage(void)
{
    fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -d domain   use domain (numeric) for metrics domain of PMDA\n\
  -l logfile  write log into logfile rather than using default file\n\
  -A access   host based access control\n\
  -I port     expect programs to connect on given inet port (number/name)\n\
  -M username user account to run under (default \"pcp\")\n\
  -N buckets  number of historical data buffers maintained\n\
  -T period   time over which samples are considered (default 60 seconds)\n\
  -U units    export observation values using the given units\n\
  -V units    export counter values using the given units\n",
	      pmGetProgname());
    exit(1);
}

static char *
squash(char *str, int *offset)
{
    char	*hspec = NULL;
    char        *p = str;
    int         i = 0;

    hspec = strdup(str);	/* make sure we have space */
    *offset = 0;
    while (isspace((int)*p)) { p++; (*offset)++; }
    while (p && *p != ':' && *p != '\0') {
	hspec[i++] = *p;
	p++;
    }
    hspec[i] = '\0';
    *offset += i;
    return hspec;
}

static int
parseAuth(char *spec)
{
    static int	first = 1;
    int offset, maxconn, specops = TR_OP_ALL, denyops;
    char *p, *endnum;

    if (first) {
	if (__pmAccAddOp(TR_OP_SEND) < 0) {
	    pmNotifyErr(LOG_ERR, "failed to add send auth operation");
	    return -1;
	}
	first = 0;
    }

    if (strncasecmp(spec, "disallow:", 9) == 0) {
	p = squash(&spec[9], &offset);
	if (p == NULL || p[0] == '\0') {
	    fprintf(stderr, "%s: invalid disallow (%s)\n", pmGetProgname(), spec);
	    if (p)
	    	free(p);
	    return -1;
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "deny: host '%s'\n", p);
	denyops = TR_OP_SEND;
	if (__pmAccAddHost(p, specops, denyops, 0) < 0)
	    pmNotifyErr(LOG_ERR, "failed to add authorisation (%s)", p);
	free(p);
    }
    else if (strncasecmp(spec, "allow:", 6) == 0) {
	p = squash(&spec[6], &offset);
	if (p == NULL || p[0] == '\0') {
	    fprintf(stderr, "%s: invalid allow (%s)\n", pmGetProgname(), spec);
	    if (p)
	    	free(p);
	    return -1;
	}
	offset += 7;
	maxconn = (int)strtol(&spec[offset], &endnum, 10);
	if (*endnum != '\0' || maxconn < 0) {
	    fprintf(stderr, "%s: bogus max connection in '%s'\n", pmGetProgname(),
		    &spec[offset]);
	    free(p);
	    return -1;
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "allow: host '%s', maxconn=%d\n", p, maxconn);
	denyops = TR_OP_NONE;
	if (__pmAccAddHost(p, specops, denyops, maxconn) < 0)
	    pmNotifyErr(LOG_ERR, "failed to add authorisation (%s)", p);
	free(p);
    }
    else {
	fprintf(stderr, "%s: access spec is invalid (%s)\n", pmGetProgname(), spec);
	return -1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    char		*endnum;
    int			err = 0;
    int			sep = pmPathSeparator();
    int			c = 0;
    int			sts;

    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "trace" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmGetProgname(), TRACE,
		"trace.log", mypath);

    /* need - port, as well as time interval and time span for averaging */
    while ((c = pmdaGetOpt(argc, argv, "A:D:d:I:l:T:M:N:U:V:?",
						&dispatch, &err)) != EOF) {
	switch(c) {
	case 'A':
	    if (parseAuth(optarg) < 0)
		err++;
	    /* add optarg to access control list */
	    break;
	case 'D':
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		err++;
	    }
	    somedebug = 1;
	    break;
	case 'I':
	    ctlport = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || ctlport < 0)
		ctlsock = optarg;
	    break;
	case 'M':
	    username = optarg;
	    break;
	case 'N':
	    rbufsize = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || rbufsize < 1) {
		fprintf(stderr, "%s: -N requires a positive number.\n", pmGetProgname());
		err++;
	    }
	    break;
	case 'T':
	    if (pmParseInterval(optarg, &timespan, &endnum) < 0) {
		fprintf(stderr, "%s: -T requires a time interval: %s\n",
			pmGetProgname(), endnum);
		free(endnum);
		err++;
	    }
	    break;
	case 'U':
	    if (updateObserveValue(optarg) < 0)
		err++;
	    break;
	case 'V':
	    if (updateCounterValue(optarg) < 0)
		err++;
	    break;
	default:
	    err++;
	}
    }

    if (err)
	usage();

    interval.tv_sec = (int)(timespan.tv_sec / rbufsize);
    interval.tv_usec = (long)((timespan.tv_sec % rbufsize) * 1000000);
    rbufsize++;		/* reserve space for the `working' buffer */

    debuglibrary();

    pmdaOpenLog(&dispatch);
    pmSetProcessIdentity(username);
    traceInit(&dispatch);
    pmdaConnect(&dispatch);
    traceMain(&dispatch);

    exit(0);
}
