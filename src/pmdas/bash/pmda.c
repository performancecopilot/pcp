/*
 * Bash -x trace PMDA
 *
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
#include <ctype.h>

#define DEFAULT_MAXMEM	(2 * 1024 * 1024)	/* 2 megabytes */
long bash_maxmem;

static int bash_interval_expired;
static struct timeval bash_interval = { 2, 0 };

#define BASH_INDOM	0
static pmdaIndom indoms[] = {
    { BASH_INDOM, 0, NULL },
};
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
    { NULL, { PMDA_PMID(0,bash_xtrace_count), PM_TYPE_U64,
	BASH_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
#define bash_xtrace_records	4
    { NULL, { PMDA_PMID(0,bash_xtrace_records), PM_TYPE_EVENT,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

#define bash_xtrace_parameters_pid	5
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_pid), PM_TYPE_U32,
	BASH_INDOM, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_parent	6
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_parent), PM_TYPE_U32,
	BASH_INDOM, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_script	7
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_script), PM_TYPE_STRING,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_lineno	8
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_lineno), PM_TYPE_U32,
	BASH_INDOM, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_function	9
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_function), PM_TYPE_STRING,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
#define bash_xtrace_parameters_command	10
    { NULL, { PMDA_PMID(0,bash_xtrace_parameters_command), PM_TYPE_STRING,
	BASH_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
};
#define METRIC_COUNT	(sizeof(metrics)/sizeof(metrics[0]))


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
extract_int(char *s, const char *field, size_t length, int *value)
{
    char *endnum;
    int num;

    if (strncmp(s, field, length) != 0)
	return 0;
    num = strtol(s + length, &endnum, 10);
    if (*endnum != ',' && *endnum != '\0' && !isspace(*endnum))
	return 0;
    *value = num;
    return endnum - s + 1;
}

static int
extract_str(char *s, size_t end, const char *field, size_t length, char **value)
{
    char *p;

    if (strncmp(s, field, length) != 0)
	return 0;
    p = s + length;
    while (*p != ',' && *p != '\0' && !isspace(*p))
        p++;
    *p = '\0';
    *value = s + length;
    return p - s + 1;
}

static void
extract_cmd(char *s, size_t end, const char *field, size_t length, char **value)
{
    char *p;

    for (p = s; p < s + end; p++) {
	if (strncmp(p, field, length) == 0) {	
	    p++;
	    if (*p == ' ')
		p++;
	    *value = s;
	    break;
	}
	*p = '\0';
	p++;
    }
}

static int
bash_trace_parser(bash_process_t *bash, const char *buffer, size_t size)
{
    char *p = (char *)buffer, *end = (char *)buffer + size - 1;
    int	first = 0, flags = (PM_EVENT_FLAG_ID | PM_EVENT_FLAG_PARENT);

    /* empty event inserted into queue to signal process has exited */
    if (size == 0)
	return flags | PM_EVENT_FLAG_END;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "processing buffer[%d]: %s", size, buffer);

    p += extract_str(p, end - p, "bash:", sizeof("bash:")-1, &bash->script);
    if (p != buffer)
	first = 1;	/* bash only sent once, used to flag a start event */
    p += extract_int(p, "ppid:", sizeof("ppid:")-1, &bash->parent);
    p += extract_int(p, "line:", sizeof("line:")-1, &bash->line);
    p += extract_int(p, "time:", sizeof("time:")-1, &bash->time);
    p += extract_int(p, "date:", sizeof("date:")-1, &bash->date);
    p += extract_str(p, end - p, "func:", sizeof("func:")-1, &bash->function);
    extract_cmd(p, end - p, "+", 1, &bash->command);	/* command follows '+' */

    return flags | (first ? PM_EVENT_FLAG_START : PM_EVENT_FLAG_POINT);
}

int
bash_process(int inst, bash_process_t **process)
{
    if (PMDA_CACHE_ACTIVE ==
	pmdaCacheLookup(indoms[BASH_INDOM].it_indom, inst, NULL, (void **)process))
	return 0;
    return PM_ERR_INST;
}

static int
bash_trace_decoder(int eventarray,
	void *buffer, size_t size, struct timeval *timestamp, void *data)
{
    pmAtomValue		atom;
    bash_process_t	*process = (bash_process_t *)data;
    int			sts, flags, count = 0;
    pmID		pmid;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "bash_trace_decoder[%d bytes]", size);

    flags = bash_trace_parser(process, (const char *)buffer, size);
    sts = pmdaEventAddRecord(eventarray, timestamp, flags);
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

    if (process->line) {
	atom.ul = process->line;
	pmid = metrics[bash_xtrace_parameters_lineno].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_U32, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    if (process->script) {
	atom.cp = process->script;
	pmid = metrics[bash_xtrace_parameters_script].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    if (process->function) {
	atom.cp = process->function;
	pmid = metrics[bash_xtrace_parameters_function].m_desc.pmid;
	sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
	if (sts < 0)
	    return sts;
	count++;
    }

    if (process->command) {
	atom.cp = process->command;
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

    if (bash_process(inst, &bp) < 0)
	return PM_ERR_INST;

    switch (idp->item) {
    case bash_xtrace_numclients:
	return pmdaEventQueueClients(bp->queueid, atom);
    case bash_xtrace_maxmem:
	atom->ull = (unsigned long long)bash_maxmem;
	return PMDA_FETCH_STATIC;
    case bash_xtrace_queuemem:
	return pmdaEventQueueMemory(bp->queueid, atom);
    case bash_xtrace_count:
	return pmdaEventQueueCounter(bp->queueid, atom);
    case bash_xtrace_records:
	return pmdaEventQueueRecords(bp->queueid, atom, pmdaGetContext(),
				     bash_trace_decoder, bp);
    case bash_xtrace_parameters_pid:
    case bash_xtrace_parameters_parent:
    case bash_xtrace_parameters_script:
    case bash_xtrace_parameters_lineno:
    case bash_xtrace_parameters_function:
    case bash_xtrace_parameters_command:
	return PMDA_FETCH_NOVALUES;
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

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "bash_store_metric walking bash set");

    pmdaCacheOp(processes, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(processes, PMDA_CACHE_WALK_NEXT)) != -1) {
	bash_process_t *bp;

	if (!pmdaCacheLookup(processes, sts, NULL, (void **)&bp))
            continue;
	if ((sts = pmdaEventSetAccess(context, bp->queueid, 1)) < 0)
	    return sts;
	if (pmDebug & DBG_TRACE_APPL0)
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
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "bash_store called (%d)", result->numpmid);
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];

    if (pmDebug & DBG_TRACE_APPL0)
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
	if (pmDebug & DBG_TRACE_APPL2)
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
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
	    if (__pmdaMainPDU(dispatch) < 0) {
		__pmAFunblock();
		exit(1);        /* fatal if we lose pmcd */
	    }
	    if (pmDebug & DBG_TRACE_APPL0)
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

static void
usage(void)
{
    fprintf(stderr,
	"Usage: %s [options]\n\n"
	"Options:\n"
	"  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	"  -l logfile   write log into logfile rather than the default\n"
	"  -m memory    maximum memory used per logfile (default %ld bytes)\n"
	"  -s interval  default delay between iterations (default %d sec)\n",
		pmProgname, bash_maxmem, (int)bash_interval.tv_sec);
    exit(1);
}

int
main(int argc, char **argv)
{
    static char		helppath[MAXPATHLEN];
    char		*endnum;
    pmdaInterface	desc;
    long		minmem;
    int			c, err = 0, sep = __pmPathSeparator();

    minmem = getpagesize();
    bash_maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "bash" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_5, pmProgname, BASH, "bash.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:m:s:?", &desc, &err)) != EOF) {
	switch (c) {
	    case 'm':
		bash_maxmem = strtol(optarg, &endnum, 10);
		if (*endnum != '\0')
		    convertUnits(&endnum, &bash_maxmem);
		if (*endnum != '\0' || bash_maxmem < minmem) {
		    fprintf(stderr, "%s: invalid max memory '%s' (min=%ld)\n",
			    pmProgname, optarg, minmem);
		    err++;
		}
		break;

	    case 's':
		if (pmParseInterval(optarg, &bash_interval, &endnum) < 0) {
		    fprintf(stderr, "%s: -s requires a time interval: %s\n",
			    pmProgname, endnum);
		    free(endnum);
		    err++;
		}
		break;

	    default:
		err++;
		break;
	}
    }

    if (err)
    	usage();

    pmdaOpenLog(&desc);
    bash_init(&desc);
    pmdaConnect(&desc);
    bash_main(&desc);
    exit(0);
}
