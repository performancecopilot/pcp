/*
 * Apache PMDA
 *
 * Copyright (C) 2012-2014,2016 Red Hat.
 * Copyright (C) 2008-2010 Aconex.  All Rights Reserved.
 * Copyright (C) 2000 Michal Kara.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#include "pmhttp.h"
#include "domain.h"
#include <inttypes.h>

static char url[256];
static char uptime_s[64];
static char *username;

static int http_port = 80;
static char *http_server = "localhost";
static char *http_path = "server-status";
static struct http_client *http_client;

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "server", 1, 'S', "HOST", "use remote host, instead of localhost" },
    { "port", 1, 'P', "PORT", "use given port on server, instead of port 80" },
    { "location", 1, 'L', "LOC", "use location on server, instead of 'server-status'" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:i:l:pu:L:P:S:U:6:?",
    .long_options = longopts,
};

static pmdaMetric metrictab[] = {
/* apache.total_accesses */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.total_kbytes */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
/* apache.uptime */
    { NULL, { PMDA_PMID(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) } },
/* apache.requests_per_sec */
    { NULL, { PMDA_PMID(0,3), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) } },
/* apache.bytes_per_sec */
    { NULL, { PMDA_PMID(0,4), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) } },
/* apache.bytes_per_request */
    { NULL, { PMDA_PMID(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,-1,PM_SPACE_BYTE,0,PM_COUNT_ONE) } },
/* apache.busy_servers */
    { NULL, { PMDA_PMID(0,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.idle_servers */
    { NULL, { PMDA_PMID(0,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_waiting */
    { NULL, { PMDA_PMID(0,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_starting */
    { NULL, { PMDA_PMID(0,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_reading */
    { NULL, { PMDA_PMID(0,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_writing_reply */
    { NULL, { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_keepalive */
    { NULL, { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_dns_lookup */
    { NULL, { PMDA_PMID(0,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_logging */
    { NULL, { PMDA_PMID(0,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_finishing */
    { NULL, { PMDA_PMID(0,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_open_slot */
    { NULL, { PMDA_PMID(0,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_closing */
    { NULL, { PMDA_PMID(0,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.sb_idle_cleanup */
    { NULL, { PMDA_PMID(0,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* apache.uptime_s */
    { NULL, { PMDA_PMID(0,19), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,0,0,0,0) } },
};

struct {
    unsigned int	flags;		/* Tells which values are valid */
    __uint64_t		uptime;
    __uint64_t		total_accesses;
    __uint64_t		total_kbytes;
    double		requests_per_sec;
    double		bytes_per_sec;
    unsigned int	bytes_per_request;
    unsigned int	busy_servers;
    unsigned int	idle_servers;
    unsigned int	sb_waiting;
    unsigned int	sb_starting;
    unsigned int	sb_reading;
    unsigned int	sb_writing_reply;
    unsigned int	sb_keepalive;
    unsigned int	sb_dns_lookup;
    unsigned int	sb_logging;
    unsigned int	sb_finishing;
    unsigned int	sb_open_slot;
    unsigned int	sb_closing;
    unsigned int	sb_idle_cleanup;
} data;

/*
 * Valid values of flags -  tell us which values are currently setup.
 * Depending on server version and configuration, some may be missing.
 */
enum {
    ACCESSES		= (1<<0),
    KILOBYTES		= (1<<1),
    UPTIME		= (1<<2),
    REQPERSEC		= (1<<3),
    BYTESPERSEC		= (1<<4),
    BYTESPERREQ		= (1<<5),
    BUSYSERVERS		= (1<<6),
    IDLESERVERS		= (1<<7),
    SCOREBOARD		= (1<<8),
};

static void uptime_string(time_t now, char *s, size_t sz)
{
    int days, hours, minutes, seconds;

    days = now / (60 * 60 * 24);
    now %= (60 * 60 * 24);
    hours = now / (60 * 60);
    now %= (60 * 60);
    minutes = now / 60;
    now %= 60;
    seconds = now;

    if (days > 1)
	pmsprintf(s, sz, "%ddays %02d:%02d:%02d", days, hours, minutes, seconds);
    else if (days == 1)
	pmsprintf(s, sz, "%dday %02d:%02d:%02d", days, hours, minutes, seconds);
    else
	pmsprintf(s, sz, "%02d:%02d:%02d", hours, minutes, seconds);
}

static void dumpData(void)
{
    uptime_string(data.uptime, uptime_s, sizeof(uptime_s));
    fprintf(stderr, "Apache data from %s port %d, path %s:\n",
	    http_server, http_port, http_path);
    fprintf(stderr, "  flags=0x%x\n", data.flags);
    fprintf(stderr, "  uptime=%" PRIu64 " (%s)\n", data.uptime, uptime_s);
    fprintf(stderr, "  accesses=%" PRIu64 "  kbytes=%" PRIu64
		    "  req/sec=%.2f  b/sec=%.2f\n",
	    data.total_accesses, data.total_kbytes,
	    data.requests_per_sec, data.bytes_per_sec);
    fprintf(stderr, "  b/req=%u  busyserv=%u  idleserv=%u\n",
	    data.bytes_per_request, data.busy_servers, data.idle_servers);
    fprintf(stderr, "  scoreboard: waiting=%u starting=%u reading=%u\n",
	    data.sb_waiting, data.sb_starting, data.sb_reading);
    fprintf(stderr, "    writing_reply=%u keepalive=%u dn_lookup=%u\n",
	    data.sb_writing_reply, data.sb_keepalive, data.sb_dns_lookup);
    fprintf(stderr, "    logging=%u finishing=%u open_slot=%u\n",
	    data.sb_logging, data.sb_finishing, data.sb_open_slot);
    fprintf(stderr, "    closing=%u idle_cleanup=%u\n",
	    data.sb_closing, data.sb_idle_cleanup);
}

/*
 * Refresh data. Returns 1 of OK, 0 on error.
 */
static int refreshData(void)
{
    char	res[BUFSIZ];
    int		len;
    char	*s,*s2,*s3;

    if (pmDebugOptions.appl0)
	fprintf(stderr, "Doing pmhttpClientFetch(%s)\n", url);

    len = pmhttpClientFetch(http_client, url, res, sizeof(res), NULL, 0);
    if (len < 0) {
	if (pmDebugOptions.appl1)
	    __pmNotifyErr(LOG_ERR, "HTTP fetch (stats) failed\n");
	return 0; /* failed */
    }

    memset(&data, 0, sizeof(data));

    for (s = res; *s; ) {
	s2 = s;
	s3 = NULL;
	for (; *s && *s != 10; s++) {
	    if (*s == ':') {
		s3 = s + 1;
		if (*s3) {
		    *s3++ = 0;
		    s++;
		}
	    }
	}

	if (*s == 10)
	    *s++ = 0;

	if (strcmp(s2, http_server) == 0 ||
	    strcmp(s2, "CurrentTime:") == 0 ||
	    strcmp(s2, "RestartTime:") == 0 ||
	    strncmp(s2, "Server", 6) == 0 ||
	    strncmp(s2, "Parent", 6) == 0 ||
	    strncmp(s2, "Load", 4) == 0 ||
	    strncmp(s2, "CPU", 3) == 0)
		/* ignored */ ;
	else if (strcmp(s2, "Total Accesses:") == 0) {
	    data.total_accesses = strtoull(s3, (char **)NULL, 10);
	    data.flags |= ACCESSES;
	}
	else if (strcmp(s2, "Total kBytes:") == 0) {
	    data.total_kbytes = strtoull(s3, (char **)NULL, 10);
	    data.flags |= KILOBYTES;
	}
	else if (strcmp(s2, "Uptime:") == 0) {
	    data.uptime = strtoull(s3, (char **)NULL, 10);
	    data.flags |= UPTIME;
	}
	else if (strcmp(s2, "ReqPerSec:") == 0) {
	    data.requests_per_sec = strtod(s3, (char **)NULL);
	    data.flags |= REQPERSEC;
	}
	else if (strcmp(s2, "BytesPerSec:") == 0) {
	    data.bytes_per_sec = strtod(s3, (char **)NULL);
	    data.flags |= BYTESPERSEC;
	}
	else if (strcmp(s2, "BytesPerReq:") == 0) {
	    data.bytes_per_request = (unsigned int)strtoul(s3, (char **)NULL, 10);
	    data.flags |= BYTESPERREQ;
	}
	else if ((strcmp(s2, "BusyServers:") == 0) ||
		 (strcmp(s2, "BusyWorkers:") == 0)) {
	    data.busy_servers = (unsigned int)strtoul(s3, (char **)NULL, 10);
	    data.flags |= BUSYSERVERS;
	}
	else if ((strcmp(s2, "IdleServers:") == 0) ||
		 (strcmp(s2, "IdleWorkers:") == 0)) {
	    data.idle_servers = (unsigned int)strtoul(s3, (char **)NULL, 10);
	    data.flags |= IDLESERVERS;
	}
	else if (strcmp(s2, "Scoreboard:") == 0) {
	    data.flags |= SCOREBOARD;
	    while(*s3) {
		switch(*s3) {
		case '_':
		    data.sb_waiting++;
		    break;
		case 'S':
		    data.sb_starting++;
		    break;
		case 'R':
		    data.sb_reading++;
		    break;
		case 'W':
		    data.sb_writing_reply++;
		    break;
		case 'K':
		    data.sb_keepalive++;
		    break;
		case 'D':
		    data.sb_dns_lookup++;
		    break;
		case 'C':
		    data.sb_closing++;
		    break;
		case 'L':
		    data.sb_logging++;
		    break;
		case 'G':
		    data.sb_finishing++;
		    break;
		case 'I':
		    data.sb_idle_cleanup++;
		    break;
		case '.':
		    data.sb_open_slot++;
		    break;
		default:
		    if (pmDebugOptions.appl1) {
			__pmNotifyErr(LOG_WARNING,
				"Unknown scoreboard character '%c'\n", *s3);
		    }
		}
		s3++;
	     }
	}
	else if (pmDebugOptions.appl1) {
	    __pmNotifyErr(LOG_WARNING, "Unknown value name '%s'!\n", s2);
	}
    }

    if (pmDebugOptions.appl2)
	dumpData();
    return 1;
}

static int
apache_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (!refreshData())
	return PM_ERR_AGAIN;
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
apache_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->cluster != 0)
	return PM_ERR_PMID;
    else if (inst != PM_IN_NULL)
	return PM_ERR_INST;

    switch (idp->item) {
	case 0:
	    if (!(data.flags & ACCESSES))
		return 0;
	    atom->ull = data.total_accesses;
	    break;
	case 1:
 	    if (!(data.flags & KILOBYTES))
		return 0;
	    atom->ull = data.total_kbytes;
	    break;
	case 2:
 	    if (!(data.flags & UPTIME))
		return 0;
	    atom->ull = data.uptime;
	    break;
	case 3:
 	    if (!(data.flags & REQPERSEC))
		return 0;
	    atom->d = data.requests_per_sec;
	    break;
	case 4:
 	    if (!(data.flags & BYTESPERSEC))
		return 0;
	    atom->d = data.bytes_per_sec;
	    break;
	case 5:
 	    if (!(data.flags & BYTESPERREQ))
		return 0;
	    atom->ul = data.bytes_per_request;
	    break;
	case 6:
 	    if (!(data.flags & BUSYSERVERS))
		return 0;
	    atom->ul = data.busy_servers;
	    break;
	case 7:
 	    if (!(data.flags & IDLESERVERS))
		return 0;
	    atom->ul = data.idle_servers;
	    break;
	case 8:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_waiting;
	    break;
	case 9:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_starting;
	    break;
	case 10:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_reading;
	    break;
	case 11:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_writing_reply;
	    break;
	case 12:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_keepalive;
	    break;
	case 13:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_dns_lookup;
	    break;
	case 14:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_logging;
	    break;
	case 15:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_finishing;
	    break;
	case 16:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_open_slot;
	    break;
	case 17:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_closing;
	    break;
	case 18:
 	    if (!(data.flags & SCOREBOARD))
		return 0;
	    atom->ul = data.sb_idle_cleanup;
	    break;
	case 19:
 	    if (!(data.flags & UPTIME))
		return 0;
	    uptime_string(data.uptime, uptime_s, sizeof(uptime_s));
	    atom->cp = uptime_s;
	    break;

	default:
	    return PM_ERR_PMID;
    }

    return 1;
}

void 
apache_init(pmdaInterface *dp)
{
    __pmSetProcessIdentity(username);

    if ((http_client = pmhttpNewClient()) == NULL) {
	__pmNotifyErr(LOG_ERR, "HTTP client creation failed\n");
	exit(1);
    }
    pmsprintf(url, sizeof(url), "http://%s:%u/%s?auto", http_server, http_port, http_path);

    dp->version.two.fetch = apache_fetch;
    pmdaSetFetchCallBack(dp, apache_fetchCallBack);
    pmdaInit(dp, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

int
main(int argc, char **argv)
{
    int			c, sep = __pmPathSeparator();
    pmdaInterface	pmda;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    pmsprintf(helppath, sizeof(helppath), "%s%c" "apache" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&pmda, PMDA_INTERFACE_3, pmProgname, APACHE, "apache.log",
		helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &pmda)) != EOF) {
	switch(c) {
	case 'S':
	    http_server = opts.optarg;
	    break;
	case 'P':
	    http_port = (int)strtol(opts.optarg, (char **)NULL, 10);
	    break;
	case 'L':
	    if (opts.optarg[0] == '/')
		opts.optarg++;
	    http_path = opts.optarg;
	    break;
	}
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&pmda);
    apache_init(&pmda);
    pmdaConnect(&pmda);
    pmdaMain(&pmda);
    exit(0);
}
