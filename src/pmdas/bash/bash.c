/*
 * Bash -x trace PMDA
 *
 * Copyright (c) 2012-2014,2016 Red Hat.
 * Copyright (c) 2012 Nathan Scott.
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
#include "domain.h"
#include "event.h"
#include "pmda.h"

#define DEFAULT_MAXMEM	(2 * 1024 * 1024)	/* 2 megabytes */
long bash_maxmem;

static int bash_interval_expired;
static struct timeval bash_interval = { 1, 0 };

static char *username;

#define BASH_INDOM	0
static pmdaIndom indoms[] = { { BASH_INDOM, 0, NULL } };
#define INDOM_COUNT	(sizeof(indoms)/sizeof(indoms[0]))

static pmdaMetric metrics[] = {

#define bash_xtrace_numclients	0
    { NULL, { PMDA_PMID(0,bash_xtrace_numclients), PM_TYPE_U32,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
#define bash_xtrace_maxmem	1
    { NULL, { PMDA_PMID(0,bash_xtrace_maxmem), PM_TYPE_U64,
	BASH_INDOM, PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
#define bash_xtrace_queuemem	2
    { NULL, { PMDA_PMID(0,bash_xtrace_queuemem), PM_TYPE_U64,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
#define bash_xtrace_count	3
    { NULL, { PMDA_PMID(0,bash_xtrace_count), PM_TYPE_U32,
	BASH_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
#define bash_xtrace_records	4
    { NULL, { PMDA_PMID(0,bash_xtrace_records), PM_TYPE_EVENT,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

#define bash_xtrace_parameters_pid	5
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_pid), PM_TYPE_U32,
	PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_parent	6
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_parent), PM_TYPE_U32,
	PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_lineno	7
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_lineno), PM_TYPE_U32,
	PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_function	8
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_function), PM_TYPE_STRING,
	PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_command	9
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_command), PM_TYPE_STRING,
	PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
};
#define METRIC_COUNT	(sizeof(metrics)/sizeof(metrics[0]))

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "memory", 1, 'm', "SIZE", "maximum memory used per logfile (default 2MB)" },
    { "interval", 1, 's', "DELTA", "default delay between iterations (default 1 sec)" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:m:s:U:?",
    .long_options = longopts,
};


static void
check_processes(int context)
{
    pmdaEventNewClient(context);
    event_refresh(indoms[BASH_INDOM].it_indom);
}

static int
bash_instance(pmInDom indom, int inst, char *name,
		__pmInResult **result, pmdaExt *pmda)
{
    check_processes(pmda->e_context);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
bash_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    check_processes(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
bash_trace_parser(bash_process_t *bash, bash_trace_t *trace,
	struct timeval *timestamp, const char *buffer, size_t size)
{
    trace->flags = (PM_EVENT_FLAG_ID | PM_EVENT_FLAG_PARENT);

    /* empty event inserted into queue to signal process has exited */
    if (size <= 0) {
	trace->flags |= PM_EVENT_FLAG_END;
	if (fstat(bash->fd, &bash->stat) < 0 || !S_ISFIFO(bash->stat.st_mode))
	    memcpy(&trace->timestamp, timestamp, sizeof(*timestamp));
	else
	    process_stat_timestamp(bash, &trace->timestamp);
	close(bash->fd);
    } else {
	char	*p = (char *)buffer, *end = (char *)buffer + size - 1;
	int	sz, time = -1;

	/* version 1 format: time, line#, function, and command line */
	p += extract_int(p, "time:", sizeof("time:")-1, &time);
	p += extract_int(p, "line:", sizeof("line:")-1, &trace->line);
	p += extract_str(p, end - p, "func:", sizeof("func:")-1,
			trace->function, sizeof(trace->function));
	sz = extract_cmd(p, end - p, "+", sizeof("+")-1,
			trace->command, sizeof(trace->command));
	if (sz <= 0)	/* wierd trace - no command */
	    trace->command[0] = '\0';

	if (strncmp(trace->command, "pcp_trace ", 10) == 0 ||
	    strncmp(trace->function, "pcp_trace", 10) == 0)
	    return 1;	/* suppress tracing function, its white noise */

	if (time != -1) {	/* normal case */
	    trace->timestamp.tv_sec = bash->starttime.tv_sec + time;
	    trace->timestamp.tv_usec = bash->starttime.tv_usec;
	} else {		/* wierd trace */
	    memcpy(&trace->timestamp, timestamp, sizeof(*timestamp));
	}

	if (event_start(bash, &trace->timestamp))
	    trace->flags |= PM_EVENT_FLAG_START;

	if (pmDebugOptions.appl0)
	    __pmNotifyErr(LOG_DEBUG,
		"event parsed: flags: %x time: %d line: %d func: '%s' cmd: '%s'",
		trace->flags, time, trace->line, trace->function, trace->command);
    }
    return 0;
}

static int
bash_trace_decoder(int eventarray,
	void *buffer, size_t size, struct timeval *timestamp, void *data)
{
    pmAtomValue		atom;
    bash_process_t	*process = (bash_process_t *)data;
    bash_trace_t	trace = { 0 };
    pmID		pmid;
    int			sts, count = 0;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "bash_trace_decoder[%ld bytes]", (long)size);

    if (bash_trace_parser(process, &trace, timestamp, (const char *)buffer, size))
	return 0;

    sts = pmdaEventAddRecord(eventarray, &trace.timestamp, trace.flags);
    if (sts < 0)
	return sts;

    atom.ul = process->pid;
    pmid = metrics[bash_xtrace_parameters_pid].m_desc.pmid;
    sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_U32, &atom);
    if (sts < 0)
	return sts;
    count++;

    atom.ul = process->parent;
    pmid = metrics[bash_xtrace_parameters_parent].m_desc.pmid;
    sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_U32, &atom);
    if (sts < 0)
	return sts;
    count++;

    if (trace.line) {
	atom.ul = trace.line;
	pmid = metrics[bash_xtrace_parameters_lineno].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_U32, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    if (trace.function[0] != '\0') {
	atom.cp = trace.function;
	pmid = metrics[bash_xtrace_parameters_function].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    if (trace.command[0] != '\0') {
	atom.cp = trace.command;
	pmid = metrics[bash_xtrace_parameters_command].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    return count;
}

static int
bash_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    bash_process_t	*bp;

    if (idp->cluster != 0)
	return PM_ERR_PMID;

    switch (idp->item) {
    case bash_xtrace_maxmem:
	atom->ull = (unsigned long long)bash_maxmem;
	return PMDA_FETCH_STATIC;
    case bash_xtrace_parameters_pid:
    case bash_xtrace_parameters_parent:
    case bash_xtrace_parameters_lineno:
    case bash_xtrace_parameters_function:
    case bash_xtrace_parameters_command:
	return PMDA_FETCH_NOVALUES;
    }

    if (PMDA_CACHE_ACTIVE !=
	pmdaCacheLookup(indoms[BASH_INDOM].it_indom, inst, NULL, (void **)&bp))
	return PM_ERR_INST;

    switch (idp->item) {
    case bash_xtrace_numclients:
	return pmdaEventQueueClients(bp->queueid, atom);
    case bash_xtrace_queuemem:
	return pmdaEventQueueMemory(bp->queueid, atom);
    case bash_xtrace_count:
	return pmdaEventQueueCounter(bp->queueid, atom);
    case bash_xtrace_records:
	return pmdaEventQueueRecords(bp->queueid, atom, pmdaGetContext(),
				     bash_trace_decoder, bp);
    }

    return PM_ERR_PMID;
}

static int
bash_store_metric(pmValueSet *vsp, int context)
{
    __pmID_int	*idp = (__pmID_int *)&vsp->pmid;
    pmInDom	processes = indoms[BASH_INDOM].it_indom;
    int		sts;

    if (idp->cluster != 0 || idp->item != bash_xtrace_records)
	return PM_ERR_PERMISSION;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "bash_store_metric walking bash set");

    pmdaCacheOp(processes, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(processes, PMDA_CACHE_WALK_NEXT)) != -1) {
	bash_process_t *bp;

	if (!pmdaCacheLookup(processes, sts, NULL, (void **)&bp))
            continue;
	if ((sts = pmdaEventSetAccess(context, bp->queueid, 1)) < 0)
	    return sts;
	if (pmDebugOptions.appl0)
            __pmNotifyErr(LOG_DEBUG,
			"Access granted client=%d bash=%d queueid=%d",
                        context, bp->pid, bp->queueid);
    }
    return 0;
}

static int
bash_store(pmResult *result, pmdaExt *pmda)
{
    int		i, sts;
    int		context = pmda->e_context;

    check_processes(context);
    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "bash_store called (%d)", result->numpmid);
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];

	if (pmDebugOptions.appl0)
	    __pmNotifyErr(LOG_DEBUG, "bash_store_metric called");
	if ((sts = bash_store_metric(vsp, context)) < 0)
	    return sts;
    }
    return 0;
}

static void
bash_end_contextCallBack(int context)
{
    pmdaEventEndClient(context);
}

static void
timer_expired(int sig, void *ptr)
{
    bash_interval_expired = 1;
}

void
bash_main(pmdaInterface *dispatch)
{
    fd_set	fds, readyfds;
    int		maxfd, nready, pmcdfd;

    pmcdfd = __pmdaInFd(dispatch);
    maxfd = pmcdfd;

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);

    /* arm interval timer */
    if (__pmAFregister(&bash_interval, NULL, timer_expired) < 0) {
	__pmNotifyErr(LOG_ERR, "registering event interval handler");
	exit(1);
    }

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);
	if (pmDebugOptions.appl2)
            __pmNotifyErr(LOG_DEBUG, "select: nready=%d interval=%d",
                          nready, bash_interval_expired);
	if (nready < 0) {
	    if (neterror() != EINTR) {
		__pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
		exit(1);
	    } else if (!bash_interval_expired) {
		continue;
	    }
	}

	__pmAFblock();
	if (nready > 0 && FD_ISSET(pmcdfd, &readyfds)) {
	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
	    if (__pmdaMainPDU(dispatch) < 0) {
		__pmAFunblock();
		exit(1);        /* fatal if we lose pmcd */
	    }
	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_DEBUG, "completed pmcd PDU [fd=%d]", pmcdfd);
	}
	if (bash_interval_expired) {
	    bash_interval_expired = 0;
	    event_refresh(indoms[BASH_INDOM].it_indom);
	}
	__pmAFunblock();
    }
}

void 
bash_init(pmdaInterface *dp)
{
    if (username)
	__pmSetProcessIdentity(username);

    if (dp->status != 0)
	return;

    dp->version.four.fetch = bash_fetch;
    dp->version.four.store = bash_store;
    dp->version.four.instance = bash_instance;
    pmdaSetFetchCallBack(dp, bash_fetchCallBack);
    pmdaSetEndContextCallBack(dp, bash_end_contextCallBack);
    pmdaInit(dp, indoms, INDOM_COUNT, metrics, METRIC_COUNT);

    event_init();
}

static void
convertUnits(char **endnum, long *maxmem)
{
    switch ((int) **endnum) {
	case 'b':
	case 'B':
		break;
	case 'k':
	case 'K':
		*maxmem *= 1024;
		break;
	case 'm':
	case 'M':
		*maxmem *= 1024 * 1024;
		break;
	case 'g':
	case 'G':
		*maxmem *= 1024 * 1024 * 1024;
		break;
    }
    (*endnum)++;
}

int
main(int argc, char **argv)
{
    static char		helppath[MAXPATHLEN];
    char		*endnum;
    pmdaInterface	desc;
    long		minmem;
    int			c, sep = __pmPathSeparator();

    __pmSetProgname(argv[0]);

    minmem = getpagesize();
    bash_maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    pmsprintf(helppath, sizeof(helppath), "%s%c" "bash" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_5, pmProgname, BASH, "bash.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &desc)) != EOF) {
	switch (c) {
	case 'm':
	    bash_maxmem = strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0')
		convertUnits(&endnum, &bash_maxmem);
	    if (*endnum != '\0' || bash_maxmem < minmem) {
		pmprintf("%s: invalid max memory '%s' (min=%ld)\n",
			    pmProgname, opts.optarg, minmem);
		opts.errors++;
	    }
	    break;

	case 's':
	    if (pmParseInterval(opts.optarg, &bash_interval, &endnum) < 0) {
		pmprintf("%s: -s requires a time interval: %s\n",
			 pmProgname, endnum);
		free(endnum);
		opts.errors++;
	    }
	    break;
	}
    }

    if (opts.errors) {
    	pmdaUsageMessage(&opts);
	exit(1);
    }

    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    bash_init(&desc);
    pmdaConnect(&desc);
    bash_main(&desc);
    exit(0);
}
