/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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
#include "ohead.h"
#include "domain.h"
#include "libpcp.h"
#include <assert.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#else
#error "Need pthread support"
#endif

#define LOG_PRI(p)      ((p) & LOG_PRIMASK)

static pthread_t 	threadid;
pthread_mutex_t		proctab_mutex;	/* control access to proctab[] */

/*
 * all metrics supported in this PMDA - one table entry for each,
 * built dynamically in ohead_init()
 */
static pmdaMetric	*metrics;
static int      	nmetric;

/* 
 * one instance domain per group of interest, built dynamically
 * in ohead_init()
 */
static pmdaIndom	*indomtab;

/*
 * PMNS for (all) dynamic metrics, built in ohead_init()
 */
static pmdaNameSpace *pmns;

extern void refresh(void *);

/*
 * check and maybe rebuild indomtab[] for a specific indom ...
 * need to have proctab_mutex held befoe calling here
 */
static void
refresh_indom(int gid)
{
    int		i;
    int		sts;

    /* grouptab[] and indomtab[] entries are aligned */
    for (i = 0; i < ngroup; i++) {
	grouptab_t	*gp = &grouptab[i];
	if (gp->id == gid) {
	    if (cycles != gp->indom_cycle) {
		/* need to rebuild indomtab[] for this indom */
		int		j;
		int		k;
		char	extname[1024];
		fprintf(stderr, "refresh_indom: rebuild [%d] cycles=%d my cycle=%d nproc=%d (was %d)\n", i, cycles, gp->indom_cycle, gp->nproc, indomtab[i].it_numinst);
		if (indomtab[i].it_set != NULL) {
		    for (j = 0; j < indomtab[i].it_numinst; j++)
			free(indomtab[i].it_set[j].i_name);
		    free(indomtab[i].it_set);
		}
		indomtab[i].it_numinst = gp->nproc;
		indomtab[i].it_set = (pmdaInstid *)malloc(indomtab[i].it_numinst * sizeof(pmdaInstid));
		if (indomtab[i].it_set == NULL) {
		    pmNoMem("refresh_indom: it_set", indomtab[i].it_numinst * sizeof(pmdaInstid), PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		/*
		 * j iterates over the proctab[] entries (including
		 * possibly deleted ones)
		 * k increments as it_set[] is filled in with the
		 * non-deleted ones
		 */
		k = 0;
		for (j = 0; j < gp->nproctab; j++) {
		    if ((gp->proctab[j].state & P_GONE))
			continue;
		    assert(k < indomtab[i].it_numinst);
		    indomtab[i].it_set[k].i_inst = gp->proctab[j].pid;
		    if ((sts = pmsprintf(extname, sizeof(extname), "%" FMT_PID " %s", gp->proctab[j].pid, gp->proctab[j].cmd)) < 0) {
			pmNotifyErr(LOG_ERR, "refresh_indom: failed to build inst name \"%" FMT_PID "%s\": %s\n",
					gp->proctab[j].pid, gp->proctab[j].cmd, pmErrStr(sts));
			exit(1);
		    }
		    if ((indomtab[i].it_set[k].i_name = strdup(extname)) == NULL) {
			pmNoMem("refresh_indom: name strdup", strlen(extname), PM_FATAL_ERR);
			/* NOTREACHED */
		    }
		    k++;
		}
		gp->indom_cycle = cycles;
	    }
	    break;
	}
    }
    if (i == ngroup)
	pmNotifyErr(LOG_WARNING, "refresh_indom: gid %d not in grouptab[]\n", gid);
}

static int
ohead_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *ext)
{
    int			i;		/* over pmidlist[] */
    int			j;		/* over vset->vlist[] */
    int			sts;
    int			need;
    int			inst;
    int			numval;
    int			gid;
    int			found;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    pmValueSet		*vset;
    pmAtomValue		atom;
    pmDesc		*dp = NULL;
    int			type;
    grouptab_t		*gp;

    /*
     * In the pthread world we don't have asyncronous notification that
     * a thread has died, so we use pthread_kill to check is refresh
     * is still running.
     */
    int err;

    if ( (err = pthread_kill (threadid, 0)) != 0 ) {
	pmNotifyErr(LOG_CRIT, "'refresh' thread is not responding: %s\n", 
		    strerror (err));
	exit (1);
    }

    if (numpmid > maxnpmids) 
    {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -oserror();
	maxnpmids = numpmid;
    }

    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    pthread_mutex_lock(&proctab_mutex);
    for (i = 0; i < numpmid; i++) {

	dp = NULL;

	/* only a few metrics, so linear scan is OK */
	for (j = 0; j<nmetric; j++) {
	    if (metrics[j].m_desc.pmid == pmidlist[i]) {
		dp = &metrics[j].m_desc;
		break;
	    }
	}

	if (dp == NULL)
	    numval = PM_ERR_PMID;
	else {
	    if (dp->indom != PM_INDOM_NULL) {
		if ((gid = pmInDom_serial(dp->indom)) != 4095) {
		    refresh_indom(gid);
		}
		numval = 0;
		__pmdaStartInst(dp->indom, ext);
		while(__pmdaNextInst(&inst, ext)) {
		    numval++;
		}
	    }
	    else {
		numval = 1;
	    }
	}

	if (numval >= 1)
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) + 
					    (numval - 1)*sizeof(pmValue));
	else
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) -
						       sizeof(pmValue));

	if (vset == NULL) {
	    if (i) {
		res->numpmid = i;
		__pmFreeResultValues(res);
	    }
	    pthread_mutex_unlock(&proctab_mutex);
	    return -oserror();
	}
	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;
	
	if (dp->indom == PM_INDOM_NULL)
	    inst = PM_IN_NULL;
	else {
	    __pmdaStartInst(dp->indom, ext);
	    __pmdaNextInst(&inst, ext);
	}

	type = dp->type;
	j = 0;

	do {
	    if (j == numval) {
		/* more instances than expected! */
		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			    sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == NULL) {
		    if (i) {
			res->numpmid = i;
			__pmFreeResultValues(res);
		    }
		    pthread_mutex_unlock(&proctab_mutex);
		    return -oserror();
		}
	    }
	    vset->vlist[j].inst = inst;
	    found = 0;

	    if (pmID_cluster(pmidlist[i]) == 4095) {
		int		k;
		if (pmID_item(pmidlist[i]) == 0) {
		    /* ohead.control.refresh */
		    found = 1;
fprintf(stderr, "fetch refreshtime=%d\n", refreshtime);
		    atom.ul = refreshtime;
		}
		else {
		    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
			if (gp->id == inst) {
			    found = 1;
			    switch (pmID_item(pmidlist[i])) {

			    case 10:	/* ohead.nproc */
				atom.ul = gp->nproc;
				break;

			    case 11:	/* ohead.nproc_active */
				atom.ul = gp->nproc_active;
				break;

			    case 12:	/* ohead.cpu */
				atom.d = 0;
				for (k  = 0; k < gp->nproctab; k++) {
				    proctab_t	*pp = &gp->proctab[k];
				    atom.d += pp->burn.utime + pp->burn.stime;
				}
				break;

			    default:
				j = PM_ERR_PMID;
				break;
			    }
			    break;
			}
		    }
		    if (gp == &grouptab[ngroup])
			j = PM_ERR_PMID;
		}
	    }
	    else {
		gid = pmID_cluster(pmidlist[i]);
		for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
		    if (gp->id == gid) {
			int		k;
			for (k  = 0; k < gp->nproctab; k++) {
			    proctab_t	*pp = &gp->proctab[k];
			    if ((pp->state & P_DATA) == 0)
				continue;
			    if (pp->pid == inst) {
				found = 1;
				switch (pmID_item(pmidlist[i])) {

				    case 0:	/* ohead.<group>.cpu */
					atom.d = pp->burn.stime + pp->burn.utime;
					break;

				    case 10:	/* ohead.<group>.stime */
					atom.d = pp->burn.stime;
					break;

				    case 11:	/* ohead.<group>.utime */
					atom.d = pp->burn.utime;
					break;

				    default:
					j = PM_ERR_PMID;
					break;
				}
				break;
			    }
			}
			break;
		    }
		}
		if (gp == &grouptab[ngroup])
		    j = PM_ERR_PMID;
	    }

	    if (j < 0)
		break;

	    if (found) {
		sts = __pmStuffValue(&atom, &vset->vlist[j], type);
		if (sts < 0) {
		    __pmFreeResultValues(res);
		    pthread_mutex_unlock(&proctab_mutex);
		    return sts;
		}
		if (j == 0)
		    vset->valfmt = sts;
		j++;	/* next element in vlist[] for next instance */
	    }
	} while (dp->indom != PM_INDOM_NULL && __pmdaNextInst(&inst, ext));

	vset->numval = j;
    }
    pthread_mutex_unlock(&proctab_mutex);

    *resp = res;
    return 0;
}

static int
ohead_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int		gid;

    if ((gid = pmInDom_serial(indom)) != 4095) {
	pthread_mutex_lock(&proctab_mutex);
	refresh_indom(gid);
	pthread_mutex_unlock(&proctab_mutex);
    }

    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
ohead_text(int ident, int type, char **buffer, pmdaExt *ext)
{
    // TODO
    *buffer = "a lot of use you are!";

    return 0;
}

static int
ohead_store(pmResult *result, pmdaExt *ext)
{
    int		i;
    pmValueSet	*vsp;
    int		sts = 0;
    int		ival;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (pmID_cluster(vsp->pmid) == 4095) {
	    switch (pmID_item(vsp->pmid)) {
		case 0:	/* ohead.control.refreshtime PMID: ...4095.0 */
		    ival = vsp->vlist[0].value.lval;
		    if (ival < 0) {
			sts = PM_ERR_SIGN;
			break;
		    }
		    /*
		     * this won't take effect until the current rereshtime
		     * alarm goes off ...
		     */
		    refreshtime = ival;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else {
	    /* not one of the metrics we are willing to change */
	    sts = PM_ERR_PMID;
	    break;
	}
    }
    return sts;
}

static int
ohead_pmid(const char *name, pmID *pmid, pmdaExt *ext)
{
    return pmdaTreePMID(pmns, name, pmid);
}

static int
ohead_name(pmID pmid, char ***namelist, pmdaExt *ext)
{
    return pmdaTreeName(pmns, pmid, namelist);
    return -1;
}

static int
ohead_children(const char *name, int traverse, char ***offspring, int **status, pmdaExt *ext)
{
    return pmdaTreeChildren(pmns, name, traverse, offspring, status);
}

void
ohead_init(pmdaInterface *dp)
{
    grouptab_t	*gp;
    int		i;
    int		sts;
    int		domain = dp->domain;

    if (dp->status != 0)
	return;

    if ((sts = pmdaTreeCreate(&pmns)) < 0) {
        pmNotifyErr(LOG_ERR, "%s: failed to create pmns tree: %s\n",
                        pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    nmetric = ngroup * 3 + 4;
    metrics = (pmdaMetric *)malloc(nmetric * sizeof(pmdaMetric));
    if (metrics == NULL) {
	pmNoMem("ohead_init: metrics", nmetric * sizeof(pmdaMetric), PM_FATAL_ERR);
	/* NOTREACHED */
    }

    indomtab = (pmdaIndom *)malloc((ngroup+1) * sizeof(pmdaIndom));
    i = 0;
    for (gp = grouptab; gp < &grouptab[ngroup+1]; gp++) {
	if (gp == &grouptab[ngroup]) {
	    /* global indom with one instance per group */
	    int		g;
	    indomtab[i].it_indom = 4095;
	    indomtab[i].it_numinst = ngroup;
	    indomtab[i].it_set = (pmdaInstid *)malloc(ngroup * sizeof(pmdaInstid));
	    if (indomtab[i].it_set == NULL) {
		pmNoMem("ohead_init: indomtab", ngroup * sizeof(pmdaInstid), PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    for (g = 0; g < ngroup; g++) {
		indomtab[i].it_set[g].i_inst = grouptab[g].id;
		indomtab[i].it_set[g].i_name = grouptab[g].name;
	    }
	}
	else {
	    /* one indom per group */
	    indomtab[i].it_indom = gp->id;
	    /* numinst and it_set filled out in refresh() */
	    indomtab[i].it_numinst = 0;
	    indomtab[i].it_set = NULL;
	}
	i++;
    }

    /* ohead.control.refresh */
    i = 0;
    metrics[i].m_user = NULL;
    metrics[i].m_desc.pmid = pmID_build(domain,4095,0);
    metrics[i].m_desc.type = PM_TYPE_U32;
    metrics[i].m_desc.indom = PM_INDOM_NULL;
    metrics[i].m_desc.sem = PM_SEM_DISCRETE;
    memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
    metrics[i].m_desc.units.dimTime = 1;
    metrics[i].m_desc.units.scaleTime = PM_TIME_SEC;
    if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, "ohead.control.refresh")) < 0) {
        pmNotifyErr(LOG_ERR, "%s: failed to insert \"ohead.control.refresh\" into pmns tree: %s\n",
                        pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* ohead.nproc */
    i++;
    metrics[i].m_user = NULL;
    metrics[i].m_desc.pmid = pmID_build(domain,4095,10);
    metrics[i].m_desc.type = PM_TYPE_U32;
    metrics[i].m_desc.indom = pmInDom_build(domain,4095);
    metrics[i].m_desc.sem = PM_SEM_INSTANT;
    memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
    metrics[i].m_desc.units.dimCount = 1;
    metrics[i].m_desc.units.scaleCount = PM_COUNT_ONE;
    if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, "ohead.nproc")) < 0) {
	pmNotifyErr(LOG_ERR, "%s: failed to insert \"ohead.nproc\" into pmns tree: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* ohead.nproc_active */
    i++;
    metrics[i].m_user = NULL;
    metrics[i].m_desc.pmid = pmID_build(domain,4095,11);
    metrics[i].m_desc.type = PM_TYPE_U32;
    metrics[i].m_desc.indom = pmInDom_build(domain,4095);
    metrics[i].m_desc.sem = PM_SEM_INSTANT;
    memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
    metrics[i].m_desc.units.dimCount = 1;
    metrics[i].m_desc.units.scaleCount = PM_COUNT_ONE;
    if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, "ohead.nproc_active")) < 0) {
	pmNotifyErr(LOG_ERR, "%s: failed to insert \"ohead.nproc_active\" into pmns tree: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* ohead.cpu */
    i++;
    metrics[i].m_user = NULL;
    metrics[i].m_desc.pmid = pmID_build(domain,4095,12);
    metrics[i].m_desc.type = PM_TYPE_DOUBLE;
    metrics[i].m_desc.indom = pmInDom_build(domain,4095);
    metrics[i].m_desc.sem = PM_SEM_INSTANT;
    memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
    if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, "ohead.cpu")) < 0) {
	pmNotifyErr(LOG_ERR, "%s: failed to insert \"ohead.cpu\" into pmns tree: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    for (gp = grouptab; gp < &grouptab[ngroup]; gp++) {
	char	namebuf[1024];

	/* ohead.<grp>.cpu */
	i++;
	metrics[i].m_user = NULL;
	metrics[i].m_desc.pmid = pmID_build(domain,gp->id,0);
	metrics[i].m_desc.type = PM_TYPE_DOUBLE;
	metrics[i].m_desc.indom = pmInDom_build(domain,gp->id);
	metrics[i].m_desc.sem = PM_SEM_INSTANT;
	memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
	if ((sts = pmsprintf(namebuf, sizeof(namebuf), "ohead.%s.cpu", gp->name)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to build \"ohead.%s.cpu\" name for pmns tree: %s\n",
			    pmGetProgname(), gp->name, pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, namebuf)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to insert \"%s\" into pmns tree: %s\n",
			    pmGetProgname(), namebuf, pmErrStr(sts));
	    exit(1);
	}

	/* ohead.<grp>.stime */
	i++;
	metrics[i].m_user = NULL;
	metrics[i].m_desc.pmid = pmID_build(domain,gp->id,10);
	metrics[i].m_desc.type = PM_TYPE_DOUBLE;
	metrics[i].m_desc.indom = pmInDom_build(domain,gp->id);
	metrics[i].m_desc.sem = PM_SEM_INSTANT;
	memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
	if ((sts = pmsprintf(namebuf, sizeof(namebuf), "ohead.%s.stime", gp->name)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to build \"ohead.%s.stime\" name for pmns tree: %s\n",
			    pmGetProgname(), gp->name, pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, namebuf)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to insert \"%s\" into pmns tree: %s\n",
			    pmGetProgname(), namebuf, pmErrStr(sts));
	    exit(1);
	}

	/* ohead.<grp>.utime */
	i++;
	metrics[i].m_user = NULL;
	metrics[i].m_desc.pmid = pmID_build(domain,gp->id,11);
	metrics[i].m_desc.type = PM_TYPE_DOUBLE;
	metrics[i].m_desc.indom = pmInDom_build(domain,gp->id);
	metrics[i].m_desc.sem = PM_SEM_INSTANT;
	memset((void *)&metrics[i].m_desc.units, 0, sizeof(pmUnits));;
	if ((sts = pmsprintf(namebuf, sizeof(namebuf), "ohead.%s.utime", gp->name)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to build \"ohead.%s.utime\" name for pmns tree: %s\n",
			    pmGetProgname(), gp->name, pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmdaTreeInsert(pmns, metrics[i].m_desc.pmid, namebuf)) < 0) {
	    pmNotifyErr(LOG_ERR, "%s: failed to insert \"%s\" into pmns tree: %s\n",
			    pmGetProgname(), namebuf, pmErrStr(sts));
	    exit(1);
	}
    }
    assert(i == nmetric-1);

    dp->version.four.fetch = ohead_fetch;
    dp->version.four.instance = ohead_instance;
    dp->version.four.store = ohead_store;
    dp->version.four.text = ohead_text;
    dp->version.four.pmid = ohead_pmid;
    dp->version.four.name = ohead_name;
    dp->version.four.children = ohead_children;

    pthread_mutex_init(&proctab_mutex, NULL);

    pmdaInit(dp, indomtab, ngroup+1, metrics, nmetric);

    /* start the pthread for async refreshes */
    {
	int err = pthread_create(&threadid, NULL, (void (*))refresh, NULL);
	if ( err != 0 ) {
	    pmNotifyErr(LOG_CRIT, "Cannot spawn a new thread: %s\n", 
			strerror (err));
	    dp->status = err;
	} else {
	    dp->status = 0;
	    pmNotifyErr(LOG_INFO, "Started refresh thread\n");
	}
    }
}
