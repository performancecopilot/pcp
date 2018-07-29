/*
 * pmdapipe, a configurable command output capture PMDA
 *
 * Copyright (c) 2015 Red Hat.
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
#include "util.h"
#include "pmda.h"

#define DEFAULT_MAXMEM	(2 * 1024 * 1024)	/* 2 megabytes */
size_t maxmem;

pmID *paramline;

static int maxfd;
static fd_set fds;

#define INDOM(serial)	(indomtab[serial].it_indom)
enum {
    PIPE_INDOM,
    ACL_INDOM,
};
static pmdaIndom indomtab[] = {
    { PIPE_INDOM, 0, NULL },
    { ACL_INDOM, 0, NULL },
};
static const int numindoms = sizeof(indomtab)/sizeof(indomtab[0]);

enum {
    PIPE_NUMCLIENTS,
    PIPE_MAXMEMORY,
    PIPE_COMMANDS,
    PIPE_LINE,
    PIPE_FIREHOSE,
    PIPE_QUEUE_MEMUSED,
    PIPE_QUEUE_COUNT,
    PIPE_QUEUE_BYTES,
    PIPE_QUEUE_NUMCLIENTS,

    PIPE_METRIC_COUNT
};

static pmdaMetric metrictab[] = {
    { NULL, 
      { PMDA_PMID(0, PIPE_NUMCLIENTS), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_MAXMEMORY), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_COMMANDS), PM_TYPE_STRING, PIPE_INDOM,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_LINE), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_FIREHOSE), PM_TYPE_EVENT, PIPE_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_QUEUE_MEMUSED), PM_TYPE_U64, PIPE_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_QUEUE_COUNT), PM_TYPE_U32, PIPE_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_QUEUE_BYTES), PM_TYPE_U64, PIPE_INDOM,
	PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { NULL,
      { PMDA_PMID(0, PIPE_QUEUE_NUMCLIENTS), PM_TYPE_U32, PIPE_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
};
static const int nummetrics = sizeof(metrictab)/sizeof(metrictab[0]);

static int
pipe_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    event_client_access(pmda->e_context);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
pipe_profile(pmProfile *prof, pmdaExt *pmda)
{
    event_client_access(pmda->e_context);
    return 0;
}

static int
pipe_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    event_client_access(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
pipe_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct pipe_command	*pc;
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    int			context, queue = -1, sts;

    context = pmdaGetContext();
    if (item >= PIPE_METRIC_COUNT)
	return PM_ERR_PMID;
    if (item == PIPE_LINE)	/* event parameter only */
	return PMDA_FETCH_NOVALUES;

    if (item > PIPE_COMMANDS) {
	if (inst == PM_IN_NULL)
	    return PM_ERR_INST;
	sts = pmdaCacheLookup(INDOM(PIPE_INDOM), inst, NULL, (void **)&pc);
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
    }
    if (item > PIPE_LINE) {
	if ((queue = event_queueid(context, inst)) < 0)
	    return PMDA_FETCH_NOVALUES;
    }

    switch (item) {
    case PIPE_NUMCLIENTS:
	if (inst != PM_IN_NULL)
	    return PM_ERR_INST;
	sts = pmdaEventClients(atom);
	break;
    case PIPE_COMMANDS:
	if (inst == PM_IN_NULL)
	    return PM_ERR_INST;
	sts = pmdaCacheLookup(INDOM(PIPE_INDOM), inst, NULL, (void **)&pc);
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	atom->cp = pc->command;
	break;
    case PIPE_MAXMEMORY:
	atom->ull = (unsigned long long)maxmem;
	sts = PMDA_FETCH_STATIC;
	break;
    case PIPE_FIREHOSE:
	sts = pmdaEventQueueRecords(queue, atom, context,
			event_decoder, event_qdata(context, inst));
	break;
    case PIPE_QUEUE_COUNT:
	sts = pmdaEventQueueCounter(queue, atom);
	break;
    case PIPE_QUEUE_BYTES:
	sts = pmdaEventQueueBytes(queue, atom);
	break;
    case PIPE_QUEUE_MEMUSED:
	sts = pmdaEventQueueMemory(queue, atom);
	break;
    case PIPE_QUEUE_NUMCLIENTS:
	sts = pmdaEventQueueClients(queue, atom);
	break;
    default:
	return PM_ERR_PMID;
    }
    return sts;
}

static int
pipe_store(pmResult *result, pmdaExt *pmda)
{
    int		i, j, sts;

    event_client_access(pmda->e_context);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];

	if (pmID_item(vsp->pmid) != PIPE_FIREHOSE)
	    return PM_ERR_PERMISSION;
	if (vsp->numval != 1)
	    return PM_ERR_BADSTORE;
	if (vsp->valfmt != PM_VAL_SPTR && vsp->valfmt != PM_VAL_DPTR)
	    return PM_ERR_BADSTORE;

	for (j = 0; j < vsp->numval; j++) {
	    struct pipe_command	*pc;
	    unsigned int	inst;
	    pmValue		*vp = &vsp->vlist[j];
	    char		*p;

	    if ((inst = vp->inst) == PM_IN_NULL)
		return PM_ERR_INST;

	    sts = pmdaCacheLookup(INDOM(PIPE_INDOM), inst, NULL, (void **)&pc);
	    if (sts != PMDA_CACHE_ACTIVE)
		return PM_ERR_INST;
	    p = vp->value.pval->vbuf;

	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "store: ctx=%d inst=%u value=\"%s\"",
				pmda->e_context, inst, p);

	    /* see if pipe command exists &| is active - start it if not */
	    if ((sts = event_qactive(pmda->e_context, inst)) < 0)
		return sts;
	    if (sts == 1)	/* already running case */
		return PM_ERR_ISCONN;
	    if ((sts = event_init(pmda->e_context, INDOM(ACL_INDOM), pc, p)) < 0)
		return sts;
	}
    }
    return 0;
}

static void
pipe_end_contextCallBack(int context)
{
    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "end_context on ctx-%d", context);

    pmdaEventEndClient(context);

    /* client context exited, mark inactive and cleanup */
    event_client_shutdown(context);
}

static int
pipe_attribute(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    int	sts;

    event_client_access(ctx);

    if ((sts = pmdaAttribute(ctx, attr, value, length, pmda)) < 0)
	return sts;

    switch (attr) {
    case PMDA_ATTR_USERID:
	event_userid(ctx, value);
	break;
    case PMDA_ATTR_GROUPID:
	event_groupid(ctx, value);
	break;
    default:
	break;
    }
    return 0;
}

static void 
pipe_init(pmdaInterface *dp, const char *configfile, int checkonly)
{
    char	confdir[MAXPATHLEN], config[MAXPATHLEN];
    int		numpipes, sep = pmPathSeparator();

    /* Global pointer to line parameter for event record encoder. */
    paramline = &metrictab[PIPE_LINE].m_desc.pmid;

    /*
     * Read and parse config file(s).
     * If not pointed at a specific file, use the system locations.
     * This includes an optional directory-based configuration too.
     */
    if (configfile) {
	if ((numpipes = event_config(configfile)) < 0)
	    dp->status = numpipes;
    } else {
	pmsprintf(config, sizeof(config), "%s%c" "pipe" "%c" "pipe.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	config[sizeof(config)-1] = '\0';
	pmsprintf(confdir, sizeof(confdir), "%s%c" "pipe.conf.d",
		pmGetConfig("PCP_SYSCONF_DIR"), sep);
	confdir[sizeof(confdir)-1] = '\0';

	if ((numpipes = event_config(config)) < 0)
	    dp->status = numpipes;
	else if ((numpipes = event_config_dir(confdir)) < 0)
	    dp->status = numpipes;
    }

    if (checkonly)
	exit(numpipes <= 0);

    if (dp->status != 0)
	return;
    pmdaSetCommFlags(dp, PMDA_FLAG_AUTHORIZE);

    dp->version.six.fetch = pipe_fetch;
    dp->version.six.store = pipe_store;
    dp->version.six.profile = pipe_profile;
    dp->version.six.instance = pipe_instance;
    dp->version.six.attribute = pipe_attribute;

    pmdaSetFetchCallBack(dp, pipe_fetchCallBack);
    pmdaSetEndContextCallBack(dp, pipe_end_contextCallBack);

    pmdaInit(dp, indomtab, numindoms, metrictab, nummetrics);

    event_acl(INDOM(ACL_INDOM));
    event_indom(INDOM(PIPE_INDOM));
}

int
pipe_setfd(int fd)
{
    if (fd > maxfd)
	maxfd = fd;
    if (pmDebugOptions.appl2)
        pmNotifyErr(LOG_DEBUG, "select: adding fd=%d", fd);
    FD_SET(fd, &fds);
    return fd;
}

int
pipe_clearfd(int fd)
{
    if (pmDebugOptions.appl2)
        pmNotifyErr(LOG_DEBUG, "select: clearing fd=%d", fd);
    FD_CLR(fd, &fds);
    return fd;
}

volatile sig_atomic_t child_exited;

static void
pipe_sigchld(int sig)
{
    child_exited = 1;
}

static void
pipeMain(pmdaInterface *dispatch)
{
    fd_set	readyfds;
    int		nready, pmcdfd;

    pmcdfd = __pmdaInFd(dispatch);
    if (pmcdfd > maxfd)
	maxfd = pmcdfd;

    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);

    signal(SIGCHLD, &pipe_sigchld);

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
        nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);
        if (pmDebugOptions.appl2)
            pmNotifyErr(LOG_DEBUG, "select: nready=%d", nready);
	if (nready < 0) {
	    if (neterror() != EINTR) {
		pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
		exit(1);
	    }
	}
	if (nready > 0) {
	    if (FD_ISSET(pmcdfd, &readyfds)) {
		if (pmDebugOptions.appl0)
		    pmNotifyErr(LOG_DEBUG,
				"processing pmcd PDU [fd=%d]", pmcdfd);
		if (__pmdaMainPDU(dispatch) < 0)
		    break;
	    }
	    event_capture(&readyfds);
	}

	/*
	 * Ordering is important here, exited must be cleared
	 * before calling child_shutdown, which iterates over
	 * all clients reaping any that exited (this must also
	 * handle the case of no exited children being found).
	 */
	if (child_exited) {
	    child_exited = 0;
	    event_child_shutdown();
	}
    }
}

static void
convertUnits(char **endnum, size_t *maxmem)
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

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    { "config",	1, 'c', "FILE", "configuration file - pipe commands" },
    { NULL,	0, 'C', NULL, "verify configuration file then exit" },
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "maxmem", 1, 'm', "BYTES", "maximum memory used per client queue" },
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "c:CD:d:l:m:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    static char		helppath[MAXPATHLEN];
    char		*config = NULL;
    char		*endnum;
    pmdaInterface	desc;
    long		minmem;
    int			sep = pmPathSeparator();
    int			c, Cflag = 0;

    pmSetProgname(argv[0]);

    minmem = getpagesize();
    maxmem = (minmem > DEFAULT_MAXMEM) ? minmem : DEFAULT_MAXMEM;
    pmsprintf(helppath, sizeof(helppath), "%s%c" "pipe" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_6, pmGetProgname(), PIPE,
		"pipe.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &desc)) != EOF) {
	switch (c) {
	case 'c':
	    config = opts.optarg;
	    break;
	case 'C':
	    Cflag = 1;
	    break;
	case 'm':
	    maxmem = strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0')
		convertUnits(&endnum, &maxmem);
	    if (*endnum != '\0' || maxmem < minmem) {
		pmprintf("%s: invalid max memory '%s' (min=%ld)\n",
			    pmGetProgname(), opts.optarg, minmem);
		opts.errors++;
	    }
	    break;
	}
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    if (!Cflag)
	pmdaOpenLog(&desc);

    pipe_init(&desc, config, Cflag);
    pmdaConnect(&desc);
    pipeMain(&desc);
    exit(0);
}
