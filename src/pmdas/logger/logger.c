/*
 * Logger, a configurable log file monitoring PMDA
 *
 * Copyright (c) 2011-2012 Red Hat.
 * Copyright (c) 2011 Nathan Scott.  All Rights Reserved.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 *
 * Debug options
 * APPL0	configfile processing and PMNS setup
 * APPL1	loading event data from the log files
 * APPL2	interaction with PMCD
 */

#include "domain.h"
#include "event.h"
#include "util.h"
#include "pmda.h"

/*
 * Logger PMDA
 *
 * Metrics
 *	logger.numclients			- number of attached clients
 *	logger.numlogfiles			- number of monitored logfiles
 *	logger.param_string			- string event data
 *	logger.perfile.{LOGFILE}.count		- observed event count
 *	logger.perfile.{LOGFILE}.bytes		- observed events size
 *	logger.perfile.{LOGFILE}.size		- logfile size
 *	logger.perfile.{LOGFILE}.path		- logfile path
 *	logger.perfile.{LOGFILE}.numclients	- number of attached
 *						  clients/logfile
 *	logger.perfile.{LOGFILE}.records	- event records/logfile
 */

#define DEFAULT_MAXMEM	(2 * 1024 * 1024)	/* 2 megabytes */
long maxmem;

int maxfd;
fd_set fds;
static int interval_expired;
static struct timeval interval = { 2, 0 };
static char *username;

static int nummetrics;
static __pmnsTree *pmns;

typedef struct dynamic_metric_info {
    int		handle;
    int		pmid_index;
    const char *help_text;
} dynamic_metric_info_t;
static dynamic_metric_info_t *dynamic_metric_infotab;

static pmdaMetric dynamic_metrictab[] = {
/* perfile.{LOGFILE}.count */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* perfile.{LOGFILE}.bytes */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* perfile.{LOGFILE}.size */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* perfile.{LOGFILE}.path */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* perfile.{LOGFILE}.numclients */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* perfile.{LOGFILE}.records */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* perfile.{LOGFILE}.queuemem */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
};

static char *dynamic_nametab[] = {
/* perfile.{LOGFILE}.count */
    "count",
/* perfile.{LOGFILE}.bytes */
    "bytes",
/* perfile.{LOGFILE}.size */
    "size",
/* perfile.{LOGFILE}.path */
    "path",
/* perfile.{LOGFILE}.numclients */
    "numclients",
/* perfile.{LOGFILE}.records */
    "records",
/* perfile.{LOGFILE}.queuemem */
    "queuemem",
};

static const char *dynamic_helptab[] = {
/* perfile.{LOGFILE}.count */
    "The cumulative number of events seen for this logfile.",
/* perfile.{LOGFILE}.bytes */
    "Cumulative number of bytes in events seen for this logfile.",
/* perfile.{LOGFILE}.size */
    "The current size of this logfile.",
/* perfile.{LOGFILE}.path */
    "The path for this logfile.",
/* perfile.{LOGFILE}.numclients */
    "The number of attached clients for this logfile.",
/* perfile.{LOGFILE}.records */
    "Event records for this logfile.",
/* perfile.{LOGFILE}.queuemem */
    "Amount of memory used for event data.",
};

static pmdaMetric static_metrictab[] = {
/* numclients */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
    	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* numlogfiles */
    { NULL,
      { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* param_string */
    { NULL,
      { PMDA_PMID(0,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* perfile.maxmem */
    { NULL, 				/* m_user gets filled in later */
      { PMDA_PMID(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
};

static pmdaMetric *metrictab;

static int
logger_profile(__pmProfile *prof, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return 0;
}

static int
logger_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
valid_pmid(unsigned int cluster, unsigned int item)
{
    if (cluster != 0 || item > nummetrics)
	return PM_ERR_PMID;
    return 0;
}

static int
logger_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int		sts;

    if ((sts = valid_pmid(idp->cluster, idp->item)) < 0)
	return sts;

    sts = PMDA_FETCH_STATIC;
    if (idp->item < 4) {
	switch (idp->item) {
	    case 0:			/* logger.numclients */
		sts = pmdaEventClients(atom);
		break;
	    case 1:			/* logger.numlogfiles */
		atom->ul = event_logcount();
		break;
	    case 2:			/* logger.param_string */
		sts = PMDA_FETCH_NOVALUES;
		break;
	    case 3:			/* logger.maxmem */
		atom->ull = (unsigned long long)maxmem;
		break;
	    default:
		return PM_ERR_PMID;
	}
    }
    else {
	dynamic_metric_info_t	*pinfo;
	int			queue;

	if ((pinfo = ((mdesc != NULL) ? mdesc->m_user : NULL)) == NULL)
	    return PM_ERR_PMID;
	queue = event_queueid(pinfo->handle);

	switch (pinfo->pmid_index) {
	    case 0:			/* perfile.{LOGFILE}.count */
		sts = pmdaEventQueueCounter(queue, atom);
		break;
	    case 1:			/* perfile.{LOGFILE}.bytes */
		sts = pmdaEventQueueBytes(queue, atom);
		break;
	    case 2:			/* perfile.{LOGFILE}.size */
		atom->ull = event_pathsize(pinfo->handle);
		break;
	    case 3:			/* perfile.{LOGFILE}.path */
		atom->cp = (char *)event_pathname(pinfo->handle);
		break;
	    case 4:			/* perfile.{LOGFILE}.numclients */
		sts = pmdaEventQueueClients(queue, atom);
		break;
	    case 5:			/* perfile.{LOGFILE}.records */
		sts = pmdaEventQueueRecords(queue, atom, pmdaGetContext(),
					    event_decoder, &pinfo->handle);
		break;
	    case 6:			/* perfile.{LOGFILE}.queuemem */
		sts = pmdaEventQueueMemory(queue, atom);
		break;
	    default:
		return PM_ERR_PMID;
	}
    }
    return sts;
}

static int
logger_store(pmResult *result, pmdaExt *pmda)
{
    int		i, j, sts;

    pmdaEventNewClient(pmda->e_context);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet		*vsp = result->vset[i];
	__pmID_int		*idp = (__pmID_int *)&vsp->pmid;
	dynamic_metric_info_t	*pinfo = NULL;
	void			*filter;
	int			queueid;

	if ((sts = valid_pmid(idp->cluster, idp->item)) < 0)
	    return sts;
	for (j = 0; j < pmda->e_nmetrics; j++) {
	    if (vsp->pmid == pmda->e_metrics[j].m_desc.pmid) {
		pinfo = pmda->e_metrics[j].m_user;
		break;
	    }
	}
	if (pinfo == NULL)
	    return PM_ERR_PMID;
	if (pinfo->pmid_index != 5)
	    return PM_ERR_PERMISSION;
	queueid = event_queueid(pinfo->handle);

	if (vsp->numval != 1 || vsp->valfmt != PM_VAL_SPTR)
	    return PM_ERR_BADSTORE;

	sts = event_regex_alloc(vsp->vlist[0].value.pval->vbuf, &filter);
	if (sts < 0)
	    return sts;

	sts = pmdaEventSetFilter(pmda->e_context, queueid, filter,
				event_regex_apply, event_regex_release);
	if (sts < 0 )
	    return sts;
    }
    return 0;
}

static void
logger_end_contextCallBack(int context)
{
    pmdaEventEndClient(context);
}

static int
logger_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreePMID(pmns, name, pmid);
}

static int
logger_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
logger_children(const char *name, int traverse, char ***kids, int **sts,
		pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

static int
logger_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    int numstatics = sizeof(static_metrictab)/sizeof(static_metrictab[0]);

    pmdaEventNewClient(pmda->e_context);

    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	/* Lookup pmid in the metric table. */
	int item = pmid_item(ident);

	/* If the PMID item was for a dynamic metric... */
	if (item >= numstatics && item < nummetrics
	    /* and the PMID matches... */
	    && metrictab[item].m_desc.pmid == (pmID)ident
	    /* and we've got user data... */
	    && metrictab[item].m_user != NULL) {
	    dynamic_metric_info_t *pinfo = metrictab[item].m_user;

	    /* Return the correct help text. */
	    *buffer = (char *)pinfo->help_text;
	    return 0;
	}
    }
    return pmdaText(ident, type, buffer, pmda);
}

void 
logger_init(pmdaInterface *dp, const char *configfile)
{
    size_t size;
    int	i, j, sts, item, numloggers;
    int numstatics = sizeof(static_metrictab)/sizeof(static_metrictab[0]);
    int numdynamics = sizeof(dynamic_metrictab)/sizeof(dynamic_metrictab[0]);
    pmdaMetric *pmetric;
    char name[MAXPATHLEN * 2];
    dynamic_metric_info_t *pinfo;

    __pmSetProcessIdentity(username);

    /* Read and parse config file. */
    if ((numloggers = event_config(configfile)) < 0)
	return;

    /* Create the dynamic metric info table based on the logfile table */
    size = sizeof(struct dynamic_metric_info) * numdynamics * numloggers;
    if ((dynamic_metric_infotab = malloc(size)) == NULL) {
	__pmNoMem("logger_init(dynamic)", size, PM_FATAL_ERR);
	return;
    }
    pinfo = dynamic_metric_infotab;
    for (i = 0; i < numloggers; i++) {
	for (j = 0; j < numdynamics; j++) {
	    pinfo->handle = i;
	    pinfo->pmid_index = j;
	    pinfo->help_text = dynamic_helptab[j];
	    pinfo++;
	}
    }

    /* Create the metric table based on the static and dynamic metric tables */
    nummetrics = numstatics + (numloggers * numdynamics);
    size = sizeof(pmdaMetric) * nummetrics;
    if ((metrictab = malloc(size)) == NULL) {
	free(dynamic_metric_infotab);
	__pmNoMem("logger_init(static)", size, PM_FATAL_ERR);
	return;
    }
    memcpy(metrictab, static_metrictab, sizeof(static_metrictab));
    pmetric = &metrictab[numstatics];
    pinfo = dynamic_metric_infotab;
    item = numstatics;
    for (i = 0; i < numloggers; i++) {
	memcpy(pmetric, dynamic_metrictab, sizeof(dynamic_metrictab));
	for (j = 0; j < numdynamics; j++) {
	    pmetric[j].m_desc.pmid = PMDA_PMID(0, item++);
	    pmetric[j].m_user = pinfo++;
	}
	pmetric += numdynamics;
    }

    if (dp->status != 0)
	return;

    dp->version.four.fetch = logger_fetch;
    dp->version.four.store = logger_store;
    dp->version.four.profile = logger_profile;
    dp->version.four.pmid = logger_pmid;
    dp->version.four.name = logger_name;
    dp->version.four.children = logger_children;
    dp->version.four.text = logger_text;

    pmdaSetFetchCallBack(dp, logger_fetchCallBack);
    pmdaSetEndContextCallBack(dp, logger_end_contextCallBack);

    pmdaInit(dp, NULL, 0, metrictab, nummetrics);

    /* Create the dynamic PMNS tree and populate it. */
    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	pmns = NULL;
	return;
    }
    pmetric = &metrictab[numstatics];
    for (i = 0; i < numloggers; i++) {
	const char *id = event_pmnsname(i);
	for (j = 0; j < numdynamics; j++) {
	    snprintf(name, sizeof(name),
			"logger.perfile.%s.%s", id, dynamic_nametab[j]);
	    __pmAddPMNSNode(pmns, pmetric[j].m_desc.pmid, name);
	}
	pmetric += numdynamics;
    }
    /* for reverse (pmid->name) lookups */
    pmdaTreeRebuildHash(pmns, (numloggers * numdynamics));

    /* initialise the event and client tracking code */
    event_init(metrictab[2].m_desc.pmid);
}

static void
logger_timer(int sig, void *ptr)
{
    interval_expired = 1;
}

void
loggerMain(pmdaInterface *dispatch)
{
    fd_set		readyfds;
    int			nready, pmcdfd;

    pmcdfd = __pmdaInFd(dispatch);
    if (pmcdfd > maxfd)
	maxfd = pmcdfd;

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);

    /* arm interval timer */
    if (__pmAFregister(&interval, NULL, logger_timer) < 0) {
	__pmNotifyErr(LOG_ERR, "registering event interval handler");
	exit(1);
    }

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_DEBUG, "select: nready=%d interval=%d",
			  nready, interval_expired);
	if (nready < 0) {
	    if (neterror() != EINTR) {
		__pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
		exit(1);
	    } else if (!interval_expired) {
		continue;
	    }
	}

	__pmAFblock();
	if (nready > 0 && FD_ISSET(pmcdfd, &readyfds)) {
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "processing pmcd PDU [fd=%d]", pmcdfd);
	    if (__pmdaMainPDU(dispatch) < 0) {
		__pmAFunblock();
		exit(1);	/* fatal if we lose pmcd */
	    }
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "completed pmcd PDU [fd=%d]", pmcdfd);
	}
	if (interval_expired) {
	    interval_expired = 0;
	    event_refresh();
	}
	__pmAFunblock();
    }
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
	"Usage: %s [options] configfile\n\n"
	"Options:\n"
	"  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	"  -l logfile   write log into logfile rather than the default\n"
	"  -m memory    maximum memory used per logfile (default %ld bytes)\n"
	"  -s interval  default delay between iterations (default %d sec)\n"
	"  -U username  user account to run under (default \"pcp\")\n",
		pmProgname, maxmem, (int)interval.tv_sec);
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

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    minmem = getpagesize();
    maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    snprintf(helppath, sizeof(helppath), "%s%c" "logger" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_5, pmProgname, LOGGER,
		"logger.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:m:s:U:?", &desc, &err)) != EOF) {
	switch (c) {
	    case 'm':
		maxmem = strtol(optarg, &endnum, 10);
		if (*endnum != '\0')
		    convertUnits(&endnum, &maxmem);
		if (*endnum != '\0' || maxmem < minmem) {
		    fprintf(stderr, "%s: invalid max memory '%s' (min=%ld)\n",
			    pmProgname, optarg, minmem);
		    err++;
		}
		break;

	    case 's':
		if (pmParseInterval(optarg, &interval, &endnum) < 0) {
		    fprintf(stderr, "%s: -s requires a time interval: %s\n",
			    pmProgname, endnum);
		    free(endnum);
		    err++;
		}
		break;

	    case 'U':
		username = optarg;
		break;

	    default:
		err++;
		break;
	}
    }

    if (err || optind != argc -1)
    	usage();

    pmdaOpenLog(&desc);
    logger_init(&desc, argv[optind]);
    pmdaConnect(&desc);
    loggerMain(&desc);
    event_shutdown();
    exit(0);
}
