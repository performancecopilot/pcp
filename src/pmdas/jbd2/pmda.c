/*
 * Journal Block Device v2 PMDA
 *
 * Copyright (c) 2013-2014 Red Hat.
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
#include "domain.h"
#include "convert.h"
#include "proc_jbd2.h"

static int _isDSO = 1;	/* =0 I am a daemon */
static char *prefix = "/proc/fs/jbd2";
static char *username;

#define	JBD2_INDOM 0
#define INDOM(i) (indomtab[i].it_indom)
static pmdaIndom indomtab[] = {
    { JBD2_INDOM, 0, NULL }, /* cached */
};

static pmdaMetric metrictab[] = {

/* jbd2.njournals */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* jbd2.transaction.count */
    { NULL, { PMDA_PMID(0,1), KERNEL_ULONG, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.requested */
    { NULL, { PMDA_PMID(0,2), KERNEL_ULONG, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.max_blocks */
    { NULL, { PMDA_PMID(0,3), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* jbd2.transaction.total.time.waiting */
    { NULL, { PMDA_PMID(0,4), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.time.request_delay */
    { NULL, { PMDA_PMID(0,5), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.time.running */
    { NULL, { PMDA_PMID(0,6), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.time.being_locked */
    { NULL, { PMDA_PMID(0,7), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.time.flushing_ordered_mode_data */
    { NULL, { PMDA_PMID(0,8), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.time.logging */
    { NULL, { PMDA_PMID(0,9), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.total.blocks */
    { NULL, { PMDA_PMID(0,10), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.total.blocks_logged */
    { NULL, { PMDA_PMID(0,11), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.total.handles */
    { NULL, { PMDA_PMID(0,12), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* jbd2.transaction.average.time.waiting */
    { NULL, { PMDA_PMID(0,13), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.request_delay */
    { NULL, { PMDA_PMID(0,14), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.running */
    { NULL, { PMDA_PMID(0,15), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.being_locked */
    { NULL, { PMDA_PMID(0,16), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.flushing_ordered_mode_data */
    { NULL, { PMDA_PMID(0,17), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.logging */
    { NULL, { PMDA_PMID(0,18), PM_TYPE_U32, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* jbd2.transaction.average.time.committing */
    { NULL, { PMDA_PMID(0,19), PM_TYPE_U64, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
/* jbd2.transaction.average.blocks */
    { NULL, { PMDA_PMID(0,20), KERNEL_ULONG, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.average.blocks_logged */
    { NULL, { PMDA_PMID(0,21), KERNEL_ULONG, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* jbd2.transaction.average.handles */
    { NULL, { PMDA_PMID(0,22), KERNEL_ULONG, JBD2_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

static int
jbd2_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    refresh_jbd2(prefix, INDOM(JBD2_INDOM));
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
jbd2_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    refresh_jbd2(prefix, INDOM(JBD2_INDOM));
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
jbd2_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int			sts;
    proc_jbd2_t		*jbd2;

    switch (idp->cluster) {
    case 0:
	if (!idp->item) { /* jbd2.njournals */
	    atom->ul = pmdaCacheOp(INDOM(JBD2_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;
	}

	/* lookup the instance (journal device) */
	sts = pmdaCacheLookup(INDOM(JBD2_INDOM), inst, NULL, (void **)&jbd2);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	if (jbd2->version < 2)
	    return 0;

	switch (idp->item) {

	case 1:		/* transaction.count */
	    _pm_assign_ulong(atom, jbd2->tid);
	    break;
	case 2:		/* transaction.requested */
	    if (jbd2->version < 3)
		return 0;
	    _pm_assign_ulong(atom, jbd2->requested);
	    break;
	case 3:		/* transaction.max_blocks */
	    atom->ul = jbd2->max_buffers;
	    break;

	case 4:		/* transaction.total.time.waiting */
	    atom->ull = jbd2->waiting * jbd2->tid;
	    break;
	case 5:		/* transaction.total.time.request_delay */
	    if (jbd2->version < 3)
		return 0;
	    atom->ull = jbd2->request_delay * jbd2->requested;
	    break;
	case 6:		/* transaction.total.time.running */
	    atom->ull = jbd2->running * jbd2->tid;
	    break;
	case 7:		/* transaction.total.time.being_locked */
	    atom->ull = jbd2->locked * jbd2->tid;
	    break;
	case 8:		/* transaction.total.time.flushing_ordered_mode_data */
	    atom->ull = jbd2->flushing * jbd2->tid;
	    break;
	case 9:		/* transaction.total.time.logging */
	    atom->ull = jbd2->logging * jbd2->tid;
	    break;
	case 10:	/* transaction.total.blocks */
	    atom->ull = jbd2->blocks * jbd2->tid;
	    break;
	case 11:	/* transaction.total.blocks_logged */
	    atom->ull = jbd2->blocks_logged * jbd2->tid;
	    break;
	case 12:	/* transaction.total.handles */
	    atom->ull = jbd2->handles * jbd2->tid;
	    break;

	case 13:	/* transaction.average.time.waiting */
	    atom->ul = jbd2->waiting;
	    break;
	case 14:	/* transaction.total.time.request_delay */
	    if (jbd2->version < 3)
		return 0;
	    atom->ul = jbd2->request_delay;
	    break;
	case 15:	/* transaction.total.time.running */
	    atom->ul = jbd2->running;
	    break;
	case 16:	/* transaction.total.time.being_locked */
	    atom->ul = jbd2->locked;
	    break;
	case 17:	/* transaction.total.time.flushing_ordered_mode_data */
	    atom->ul = jbd2->flushing;
	    break;
	case 18:	/* transaction.total.time.logging */
	    atom->ul = jbd2->logging;
	    break;
	case 19:	/* transaction.average.time.committing */
	    atom->ull = jbd2->average_commit_time;
	    break;
	case 20:	/* transaction.total.blocks */
	    _pm_assign_ulong(atom, jbd2->blocks);
	    break;
	case 21:	/* transaction.total.blocks_logged */
	    _pm_assign_ulong(atom, jbd2->blocks_logged);
	    break;
	case 22:	/* transaction.total.handles */
	    _pm_assign_ulong(atom, jbd2->handles);
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
jbd2_init(pmdaInterface *dp)
{
    size_t	nmetrics, nindoms;

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "jbd2" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_4, "jbd2 DSO", helppath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.four.instance = jbd2_instance;
    dp->version.four.fetch = jbd2_fetch;
    pmdaSetFetchCallBack(dp, jbd2_fetchCallBack);

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_DIRECT);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "", 1, 'j', "PATH", "path to stats files (default \"/proc/fs/jbd2\")" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:j:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			c, sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		help[MAXPATHLEN];

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(help, sizeof(help), "%s%c" "jbd2" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, JBD2, "jbd2.log", help);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch(c) {
	case 'j':
	    prefix = opts.optarg;
	    break;
	}
    }
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    jbd2_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
