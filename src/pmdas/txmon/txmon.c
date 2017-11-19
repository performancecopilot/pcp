/*
 * txmon PMDA
 *
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "domain.h"

#define CACHELINE	32
#define RND_TO_CACHE_LINE(x) (((x + CACHELINE - 1) / CACHELINE) * CACHELINE)

/*
 * txmon PMDA
 *
 * This PMDA is a sample that illustrates how a PMDA might be
 * constructed with libpcp_pmda to use a System V shared memory (shm)
 * segment to transfer performance data between the collectors on the
 * application side (see ./txrecord and ./genload for a simple example)
 * and this PCP PMDA
 */

/*
 * list of instances ... created dynamically, after parsing cmd line options
 */
static pmdaInstid *tx_indom;

/*
 * list of instance domains ... initialized after parsing cmd line options
 */
static pmdaIndom indomtab[] = {
#define TX_INDOM	0
    { TX_INDOM, 0, NULL },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric metrictab[] = {
/* count */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, TX_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* ave_time */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_FLOAT, TX_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,1,-1,0,PM_TIME_SEC,PM_COUNT_ONE) } },
/* max_time */
    { NULL,
      { PMDA_PMID(0,2), PM_TYPE_FLOAT, TX_INDOM, PM_SEM_INSTANT,
      	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0 ) } },
/* reset_count */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_U32, TX_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
/* control.level */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) } },
/* control.reset */
    { NULL, 
      { PMDA_PMID(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) } },
};

#include "./txmon.h"

static int	shmid = -1;

static char	mypath[MAXPATHLEN];
static char	*username;

/*
 * callback provided to pmdaFetch
 */
static int
txmon_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    stat_t		*sp;
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    unsigned int	real_count;

    if (inst != PM_IN_NULL && mdesc->m_desc.indom == PM_INDOM_NULL)
	return PM_ERR_INST;

    if (cluster != 0)
	return PM_ERR_PMID;

    if (item <= 3) {
	if (inst >= control->n_tx)
	    return PM_ERR_INST;

	sp = (stat_t *)((__psint_t)control + control->index[inst]);

	switch (item) {
	    case 0:				/* txmon.count */
		if (control->level < 1)
		    return PM_ERR_AGAIN;
		atom->ul = sp->count;
		break;
	    case 1:				/* txmon.ave_time */
		if (control->level < 2)
		    return PM_ERR_AGAIN;
		real_count = sp->count - sp->reset_count;
		atom->f = real_count > 0 ? sp->sum_time / real_count : -1;
		break;
	    case 2:				/* txmon.max_time */
		if (control->level < 2)
		    return PM_ERR_AGAIN;
		atom->f = sp->max_time;
		break;
	    case 3:				/* txmon.reset_count */
		if (control->level < 1)
		    return PM_ERR_AGAIN;
		atom->ul = sp->count - sp->reset_count;
		break;
	}
    }
    else {
	switch (item) {

	    case 4:				/* txmon.control.level */
		atom->ul = control->level;
		break;
	    case 5:				/* txmon.control.reset */
		atom->ul = 1;
		break;
	    default:
		return PM_ERR_PMID;
	}
    }

    return 0;
}

/*
 * support the storage of a value into the control metrics
 */
static int
txmon_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		n;
    int		val;
    int		sts = 0;
    pmValueSet	*vsp = NULL;
    stat_t	*sp;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];

	if (pmID_cluster(vsp->pmid) == 0) {	/* all storable metrics are cluster 0 */

	    switch (pmID_item(vsp->pmid)) {
		case 0:				/* no store for these ones */
		case 1:
		case 2:
		case 3:
		    sts = PM_ERR_PERMISSION;
		    break;

	    	case 4:				/* txmon.control.level */
		    val = vsp->vlist[0].value.lval;
		    if (val < 0) {
			sts = PM_ERR_SIGN;
			val = 0;
		    }
		    control->level = val;
		    break;

		case 5:				/* txmon.control.reset */
		    for (n = 0; n < control->n_tx; n++) {
			sp = (stat_t *)((__psint_t)control + control->index[n]);
			sp->reset_count = sp->count;
			sp->sum_time = 0;
			sp->max_time = -1;
		    }
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else
	    sts = PM_ERR_PMID;
    }
    return sts;
}


/*
 * Initialise the agent
 */
void 
txmon_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    pmSetProcessIdentity(username);

    dp->version.two.store = txmon_store;

    pmdaSetFetchCallBack(dp, txmon_fetchCallBack);
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "after pmdaSetFetchCallBack() control @ %p\n", control);
    }

    pmdaInit(dp, indomtab, 1, metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "after pmdaInit() control @ %p\n", control);
    }
}

/*
 * come here on exit()
 */
static void
done(void)
{
    if (shmid != -1)
	/* remove the shm segment */
	shmctl(shmid, IPC_RMID, NULL);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
    .short_usage = "[options] tx_type [...]",
};

/*
 * Set up the agent.
 */
int
main(int argc, char **argv)
{
    int			n, sep = pmPathSeparator();
    char		*p;
    pmdaInterface	dispatch;
    size_t		index_size;
    size_t		shm_size;
    stat_t		*sp;

    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "txmon" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmGetProgname(), TXMON,
		"txmon.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    n = argc - opts.optind;
    if (n < 1) {
	pmprintf("No transaction types specified\n\n");
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);

    /*
     * create the instance domain table ... one entry per transaction type
     */
    if ((tx_indom = (pmdaInstid *)malloc(n * sizeof(pmdaInstid))) == NULL) {
	fprintf(stderr, "malloc(%d): %s\n",
	    n * (int)sizeof(pmdaInstid), osstrerror());
	exit(1);
    }
    indomtab[0].it_numinst = n;
    indomtab[0].it_set = tx_indom;

    /*
     * size shm segment so each stat_t starts on a cache line boundary
     */
    index_size =
	RND_TO_CACHE_LINE(sizeof(control->level) +
			  sizeof(control->n_tx) +
			  n * sizeof(control->index[0]));
    shm_size = index_size + n * RND_TO_CACHE_LINE(sizeof(stat_t));

    /*
     * create the shm segment, and install exit() handler to remove it
     */
    if ((shmid = shmget(KEY, shm_size, IPC_CREAT|0666)) < 0) {
	fprintf(stderr, "shmget: %s\n", osstrerror());
	exit(1);
    }
    atexit(done);
    if ((control = (control_t *)shmat(shmid, NULL, 0)) == (control_t *)-1) {
	fprintf(stderr, "shmat: %s\n", osstrerror());
	exit(1);
    }
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "shmat -> control @ %p\n", control);
    }

    /*
     * set up the shm control info and directory
     */
    control->n_tx = n;
    control->level = 2;		/* arbitrary default stats level */
    p = (char *)control;
    p = &p[index_size];
    for (n = 0; n < control->n_tx; n++) {
	/*
	 * Note: it is important that the index[] entries are byte
	 *      offsets from the start of the shm segment ... using
	 *      pointers may cause problems for 32-bit and 64-bit apps
	 *	attaching to the shm segment
	 */
	control->index[n] = p - (char *)control;
	sp = (stat_t *)p;
	strncpy(sp->type, argv[opts.optind++], MAXNAMESIZE);
	sp->type[MAXNAMESIZE-1] = '\0';		/* ensure null terminated */
	sp->reset_count = 0;
	sp->count = 0;
	sp->max_time = -1.0;
	sp->sum_time = 0.0;
	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "index[%d]=%d @ %p name=\"%s\"\n", n, control->index[n], p, sp->type);
	}
	p += RND_TO_CACHE_LINE(sizeof(stat_t));

	/*
	 * and set up the corresponding indom table entries
	 */
	tx_indom[n].i_inst = n;
	tx_indom[n].i_name = sp->type;
    }

    /*
     * the real work is done below here ...
     */
    txmon_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
