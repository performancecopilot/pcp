/*
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

#include "pmapi.h"
#include "trace.h"

typedef enum { UNKNOWN, COUNTER, OBSERVE, TRANSACT } trace_t;

int
main(int argc, char **argv)
{
    int		c;
    char	*p;
    char	*me;
    int		err = 0;
    int		verbose = 1;
    /* anything directly influencing the trace API is in this struct */
    struct {
	int	state;
	char	*tag;
	char	*host;
	trace_t	type;	/* COUNTER, OBSERVE, or TRANSACT */
	union {
	    double	value;		/* OBSERVE/COUNTER types */
	    char	*command;	/* TRANSACT type */
	} arg;
    } api;

    pmSetProgname(argv[0]);
    me = pmGetProgname();

    memset(&api, 0, sizeof(api));

    while ((c = getopt(argc, argv, "S:c:e:h:qv:?")) != EOF) {
	switch (c) {
	case 'S':
	    /* allow base 8 or 16 or 10 based on supplied value */
	    c = (int)strtol(optarg, &p, 0);
	    if (p == optarg || *p != '\0') {
		fprintf(stderr, "%s: -S requires a numeric argument\n", me);
		err++;
	    }
	    else
		api.state |= c;
	    break;

	case 'h':
	    api.host = optarg;
	    break;

	case 'q':
	    verbose = 0;
	    break;

	case 'c':
	    if (api.type != UNKNOWN) {
		fprintf(stderr, "%s: only one of -c, -v or -e allowed\n", me);
		err++;
	    }
	    api.type = COUNTER;
	    api.arg.value = strtod(optarg, &p);
	    if (*p != '\0') {
		fprintf(stderr, "%s: -v requires a numeric argument\n", me);
		err++;
	    }
	    break;

	case 'e':
	    if (api.type != UNKNOWN) {
		fprintf(stderr, "%s: only one of -c, -v or -e allowed\n", me);
		err++;
	    }
	    api.type = TRANSACT;
	    api.arg.command = optarg;
	    break;

	case 'v':
	    if (api.type != UNKNOWN) {
		fprintf(stderr, "%s: only one of -c, -v or -e allowed\n", me);
		err++;
	    }
	    api.type = OBSERVE;
	    api.arg.value = strtod(optarg, &p);
	    if (*p != '\0') {
		fprintf(stderr, "%s: -v requires a numeric argument\n", me);
		err++;
	    }
	    break;

	default:
	    err++;
	}
    }

    if (optind == argc-1) {
	api.tag = argv[optind];
	optind++;
    }

    if (err || optind != argc || api.tag == NULL) {
	fprintf(stderr,
"Usage: %s [-q] [-c value|-e command|-v value] [-h host] [-S state] tag\n\n\
Options:\n\
  -c value    export a counter value through the trace PMDA\n\
  -e command  run command and export transaction data\n\
  -h host     send trace data to trace PMDA on given host\n\
  -q          quiet mode - suppress message from successful trace\n\
  -S state    set debug state using pmtracestate(3) as bit-wise\n\
              combination of these flags:\n\
	       1 Shows processing just below the API\n\
	       2 Shows network-related activity\n\
	       4 Shows app<->PMDA IPC traffic\n\
	       8 Shows internal IPC buffer management\n\
	      16 No PMDA communications at all\n\
  -v value    export an observation value through the trace PMDA\n\
\n\
%s always uses the asynchronous PDU protocol mode.\n", me, me);
	exit(0);
    }

    pmtracestate(api.state | PMTRACE_STATE_ASYNC);

    if (api.host != NULL) {
	c = strlen(api.host) + 20;
	if ((p = (char *)malloc(c)) == NULL) {
	    fprintf(stderr, "%s: malloc failed: %s\n",
		    me, pmtraceerrstr(-oserror()));
	    exit(0);
	}
	pmsprintf(p, c, "PCP_TRACE_HOST=%s", api.host);
	if (putenv(p) < 0) {
	    fprintf(stderr, "%s: putenv failed: %s\n",
		    me, pmtraceerrstr(-oserror()));
	    exit(0);
	}
    }

    c = 0;	/* reuse as the exit status */
    switch (api.type) {
    case COUNTER:
	if ((err = pmtracecounter(api.tag, api.arg.value)) < 0) {
	    fprintf(stderr, "%s: counter error: %s\n",
		    me, pmtraceerrstr(err));
	    exit(0);
	}
	else if (verbose) {
	    printf("%s: counter complete (tag=\"%s\", value=%f)\n",
		   me, api.tag, api.arg.value);
	}
	break;

    case OBSERVE:
	if ((err = pmtraceobs(api.tag, api.arg.value)) < 0) {
	    fprintf(stderr, "%s: observation error: %s\n",
		    me, pmtraceerrstr(err));
	    exit(0);
	}
	else if (verbose) {
	    printf("%s: observation complete (tag=\"%s\", value=%f)\n",
		   me, api.tag, api.arg.value);
	}
	break;

    case TRANSACT:
	if ((err = pmtracebegin(api.tag)) < 0) {
	    fprintf(stderr, "%s: transaction begin error: %s\n",
		    me, pmtraceerrstr(err));
	    /* don't exit yet - execute cmd anyway, just no tracing */
	}

	if ((c = system(api.arg.command)) < 0) {
	    fprintf(stderr, "%s: system error running '%s': %s\n",
		    me, api.arg.command, pmtraceerrstr(-oserror()));
	    exit(0);
	}
	if (err < 0)
	    exit(c);

	if ((err = pmtraceend(api.tag)) < 0) {
	    fprintf(stderr, "%s: transaction end error: %s\n",
		    me, pmtraceerrstr(err));
	    exit(c);
	}
	else if (verbose) {
	    printf("%s: transaction complete (tag=\"%s\")\n", me, api.tag);
	}
	break;

    default:
	if ((err = pmtracepoint(api.tag)) < 0) {
	    fprintf(stderr, "%s: point error: %s\n",
		    me, pmtraceerrstr(err));
	    exit(0);
	}
	else if (verbose) {
	    printf("%s: point complete (tag=\"%s\")\n", me, api.tag);
	}
    }
    exit(c);
}
