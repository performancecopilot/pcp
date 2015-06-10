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
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "data.h"
#include "trace_dev.h"

static pmdaIndom indomtab[] = {	/* list of trace metric instance domains */
#define TRANSACT_INDOM	0
    { TRANSACT_INDOM, 0, NULL },	/* dynamically updated */
#define POINT_INDOM	1
    { POINT_INDOM, 0, NULL },		/* dynamically updated */
#define OBSERVE_INDOM	2
    { OBSERVE_INDOM, 0, NULL },		/* dynamically updated */
#define COUNTER_INDOM	3
    { COUNTER_INDOM, 0, NULL },		/* dynamically updated */
};

static int	transacts;	/* next instance# to allocate */
static int	points;		/* next instance# to allocate */
static int	counters;	/* next instance# to allocate */
static int	observes;	/* next instance# to allocate */
static int	tsortflag;	/* need sort on next request? */
static int	psortflag;	/* need sort on next request? */
static int	csortflag;	/* need sort on next request? */
static int	osortflag;	/* need sort on next request? */

/* all metrics supported in this PMDA - one table entry for each */
static pmdaMetric metrictab[] = {
/* transact.count */
    { NULL,
      { PMDA_PMID(0,0), PM_TYPE_U64, TRANSACT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1, 0,0,PM_COUNT_ONE)} },
/* transact.rate */
    { NULL,
      { PMDA_PMID(0,1), PM_TYPE_FLOAT, TRANSACT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,PM_COUNT_ONE)} },
/* transact.ave_time */
    { NULL,
      { PMDA_PMID(0,2), PM_TYPE_FLOAT, TRANSACT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,1,-1, 0,PM_TIME_SEC,PM_COUNT_ONE)} },
/* transact.min_time */
    { NULL,
      { PMDA_PMID(0,3), PM_TYPE_FLOAT, TRANSACT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,1,0, 0,PM_TIME_SEC,0)} },
/* transact.max_time */
    { NULL,
      { PMDA_PMID(0,4), PM_TYPE_FLOAT, TRANSACT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,1,0, 0,PM_TIME_SEC,0)} },
/* transact.total_time */
    { NULL,
      { PMDA_PMID(0,5), PM_TYPE_DOUBLE, TRANSACT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,1,0, 0,PM_TIME_SEC,0)} },
/* point.count */
    { NULL,
      { PMDA_PMID(0,6), PM_TYPE_U64, POINT_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1, 0,0,PM_COUNT_ONE)} },
/* point.rate */
    { NULL,
      { PMDA_PMID(0,7), PM_TYPE_FLOAT, POINT_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,PM_COUNT_ONE)} },
/* observe.count */
    { NULL,
      { PMDA_PMID(0,8), PM_TYPE_U64, OBSERVE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1, 0,0,PM_COUNT_ONE)} },
/* observe.rate */
    { NULL,
      { PMDA_PMID(0,9), PM_TYPE_FLOAT, OBSERVE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,PM_COUNT_ONE)} },
/* observe.value */
    { NULL,
      { PMDA_PMID(0,10), PM_TYPE_DOUBLE, OBSERVE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0, 0,0,0)} },	/* this may be modified at startup */
/* control.timespan */
    { NULL,
      { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,1,0, 0,PM_TIME_SEC,0)} },
/* control.interval */
    { NULL,
      { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,1,0, 0,PM_TIME_SEC,0)} },
/* control.buckets */
    { NULL,
      { PMDA_PMID(0,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0, 0,0,0)} },
/* control.port */
    { NULL,
      { PMDA_PMID(0,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0, 0,0,0)} },
/* control.reset */
    { NULL,
      { PMDA_PMID(0,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0, 0,0,0)} },
/* control.debug */
    { NULL,
      { PMDA_PMID(0,16), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0, 0,0,0) }, },
/* counter.count */
    { NULL,
      { PMDA_PMID(0,17), PM_TYPE_U64, COUNTER_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1, 0,0,PM_COUNT_ONE) }, },
/* counter.rate */
    { NULL,
      { PMDA_PMID(0,18), PM_TYPE_FLOAT, COUNTER_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,PM_COUNT_ONE) }, },
/* counter.value */
    { NULL,
      { PMDA_PMID(0,19), PM_TYPE_DOUBLE, COUNTER_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,0, 0,0,0) }, },	/* this may be modified at startup */
};

extern void __pmdaStartInst(pmInDom indom, pmdaExt *pmda);

extern int		ctlport;
extern unsigned int	rbufsize;
extern struct timeval	timespan;
extern struct timeval	interval;

static ringbuf_t	ringbuf;	/*  *THE* ring buffer of trace data */
static hashtable_t	summary;	/* globals + recent ringbuf summary */
static hashtable_t	history;	/* holds every instance seen so far */
static unsigned int	rpos;		/* `working' buffer, within ringbuf */
static unsigned int	dosummary = 1;	/* summary refreshed this interval? */
static unsigned int	tindomsize = 0;	/*   updated local to fetch only    */
static unsigned int	pindomsize = 0;	/*   updated local to fetch only    */
static unsigned int	oindomsize = 0;	/*   updated local to fetch only    */
static unsigned int	cindomsize = 0;	/*   updated local to fetch only    */
	/* note: {t,p,o,c}indomsize are only valid when dosummary equals zero */


/* allow configuration of trace.observe.value/trace.counter.value units */
static int
updateValueUnits(const char *str, int offset)
{
    int		units[6], i, sts = 0;
    char	*s, *sptr, *endp;

    if ((sptr = strdup(str)) == NULL)
	return -oserror();
    s = sptr;

    for (i = 0; i < 6; i++) {
	if ((s = strtok((i==0 ? sptr : NULL), ",")) == NULL) {
	    fprintf(stderr, "%s: token parse error in string \"%s\"\n",
		    pmProgname, str);
	    sts = -1;
	    goto leaving;
	}
	units[i] = (int)strtol(s, &endp, 10);
	if (*endp) {
	    fprintf(stderr, "%s: integer parse error for substring \"%s\"\n",
		    pmProgname, s);
	    sts = -1;
	    goto leaving;
	}
    }

    /* update table entry for this value metric */
    metrictab[offset].m_desc.units.dimSpace = units[0];
    metrictab[offset].m_desc.units.dimTime = units[1];
    metrictab[offset].m_desc.units.dimCount = units[2];
    metrictab[offset].m_desc.units.scaleSpace = units[3];
    metrictab[offset].m_desc.units.scaleTime = units[4];
    metrictab[offset].m_desc.units.scaleCount = units[5];

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "%s: value metric units updated using \"%s\"\n"
		"dimSpace=%d, dimTime=%d, dimCount=%d, scaleSpace=%d, "
		"scaleTime=%d, scaleCount=%d\n", pmProgname, str,
		units[0], units[1], units[2], units[3], units[4], units[5]);
    }
#endif

leaving:
    if (sptr != NULL)
	free(sptr);
    return sts;
}

int updateObserveValue(const char *str) { return updateValueUnits(str, 10); }
int updateCounterValue(const char *str) { return updateValueUnits(str, 19); }


static int
compareInstance(const void *a, const void *b)
{
    pmdaInstid	*aa = (pmdaInstid *)a;
    pmdaInstid  *bb = (pmdaInstid *)b;
    return aa->i_inst - bb->i_inst;
}

/*
 * sort reset instance domains to be friendly to pmclients
 * (only PMDA knows when optimal time to sort these infrequently-changing sets)
 */
static void
indomSortCheck(void)
{
    if (tsortflag) {
	qsort(indomtab[TRANSACT_INDOM].it_set,
	      indomtab[TRANSACT_INDOM].it_numinst,
	      sizeof(pmdaInstid), compareInstance);
	tsortflag = 0;
    }
    if (psortflag) {
	qsort(indomtab[POINT_INDOM].it_set,
	      indomtab[POINT_INDOM].it_numinst,
	      sizeof(pmdaInstid), compareInstance);
	psortflag = 0;
    }
    if (osortflag) {
	qsort(indomtab[OBSERVE_INDOM].it_set,
	      indomtab[OBSERVE_INDOM].it_numinst,
	      sizeof(pmdaInstid), compareInstance);
	osortflag = 0;
    }
    if (csortflag) {
	qsort(indomtab[COUNTER_INDOM].it_set,
	      indomtab[COUNTER_INDOM].it_numinst,
	      sizeof(pmdaInstid), compareInstance);
	csortflag = 0;
    }
}

/*
 * wrapper for pmdaInstance which we need to ensure is called with the
 * _sorted_ contents of the instance domain.
 */
static int
traceInstance(pmInDom indom, int foo, char *bar, __pmInResult **iresp, pmdaExt
 *pmda)
{
    indomSortCheck();
    return pmdaInstance(indom, foo, bar, iresp, pmda);
}

/*
 * `summary' table deletion may add to the `history' table.
 */
void
summarydel(void *a)
{
    hashdata_t  *k = (hashdata_t *)a;
    instdata_t	check;

    check.tag = k->tag;
    check.type = k->tracetype;
    check.instid = k->id;
    if (__pmhashlookup(&history, check.tag, &check) == NULL) {
	if (__pmhashinsert(&history, check.tag, &check) < 0)
	    __pmNotifyErr(LOG_ERR, "history table insert failure - '%s' "
		"instance will not maintain its instance number.", check.tag);
    }
    if (k != NULL)
	free(k);	/* don't free k->tag - its in the history table */
}

/*
 * Processes data from pcp_trace-linked client programs.
 *
 * Return negative only on fd-related errors, as that connection will
 * later be closed.  Other errors - report in log file but continue.
 */
int
readData(int clientfd, int *protocol)
{
    __pmTracePDU	*result;
    double	 	data;
    hashdata_t		newhash;
    hashdata_t		*hptr;
    hashdata_t		hash;
    char		*tag;
    int			type, taglen, sts;
    int			freeflag=0;

    if ((sts = __pmtracegetPDU(clientfd, TRACE_TIMEOUT_NEVER, &result)) < 0) {
	__pmNotifyErr(LOG_ERR, "bogus PDU read - %s", pmtraceerrstr(sts));
	return -1;
    }
    else if (sts == TRACE_PDU_DATA) {
	if ((sts = __pmtracedecodedata(result, &tag, &taglen,
						&type, protocol, &data)) < 0)
	    return -1;
	if (type < TRACE_FIRST_TYPE || type > TRACE_LAST_TYPE) {
	    __pmNotifyErr(LOG_ERR, "unknown trace type for '%s' (%d)", tag, type);
	    free(tag);
	    return -1;
	}
	newhash.tag = tag;
	newhash.taglength = taglen;
	newhash.tracetype = type;
    }
    else if (sts == 0) {	/* client has exited - cleanup in mainloop */
	return -1;
    }
    else {	/* unknown PDU type - bail & later kill connection */
	__pmNotifyErr(LOG_ERR, "unknown PDU - expected data PDU"
		" (not type #%d)", sts);
	return -1;
    }

    /*
     * First, update the global summary table with this new data
     */
    if ((hptr = __pmhashlookup(&summary, tag, &newhash)) == NULL) {
	instdata_t	check, *iptr;
	int		size, index, indom;

	check.tag = newhash.tag;
	check.type = newhash.tracetype;
	if ((iptr = __pmhashlookup(&history, check.tag, &check)) != NULL) {
	    newhash.id = iptr->instid;	/* reuse pre-reset instance ID */
	    if (iptr->type == TRACE_TYPE_TRANSACT) tsortflag++;
	    else if (iptr->type == TRACE_TYPE_POINT) psortflag++;
	    else if (iptr->type == TRACE_TYPE_COUNTER) csortflag++;
	    else  /*(iptr->type == TRACE_TYPE_OBSERVE)*/ osortflag++;
	}
	else if (type == TRACE_TYPE_TRANSACT)
	    newhash.id = ++transacts;
	else if (type == TRACE_TYPE_POINT)
	    newhash.id = ++points;
	else if (type == TRACE_TYPE_COUNTER)
	    newhash.id = ++counters;
	else	/* TRACE_TYPE_OBSERVE */
	    newhash.id = ++observes;
	newhash.txcount = -1;	/* first time since reset or start */
	newhash.padding = 0;
	newhash.realcount = 1;	
	newhash.realtime = data;	
	newhash.fd = clientfd;
	newhash.txmin = newhash.txmax = data;
	newhash.txsum = data;
	hptr = &newhash;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "'%s' is new to the summary table!",
		    hptr->tag);
#endif
	if (__pmhashinsert(&summary, hptr->tag, hptr) < 0) {
	    __pmNotifyErr(LOG_ERR, "summary table insert failure - '%s' "
		"data ignored", hptr->tag);
	    free(hptr->tag);
	    return -1;
	}
	/* also update the instance domain */
	if (hptr->tracetype == TRACE_TYPE_TRANSACT)
	    indom = TRANSACT_INDOM;
	else if (hptr->tracetype == TRACE_TYPE_POINT)
	    indom = POINT_INDOM;
	else if (hptr->tracetype == TRACE_TYPE_COUNTER)
	    indom = COUNTER_INDOM;
	else  /*(hptr->tracetype == TRACE_TYPE_OBSERVE)*/
	    indom = OBSERVE_INDOM;

#ifdef DESPERATE
	/* walk the indom table - if we find this new tag in it already, then
	 * something is badly busted.
	 */
	for (sts = 0; sts < indomtab[indom].it_numinst; sts++) {
	    if (strcmp(indomtab[indom].it_set[sts].i_name, hptr->tag) == 0) {
		fprintf(stderr, "'%s' (inst=%d, type=%d) entry in indomtab already!!!\n",
			hptr->tag, indomtab[indom].it_set[sts].i_inst, hptr->tracetype);
		abort();
	    }
	}
#endif

	index = indomtab[indom].it_numinst;
	size = (index+1)*(int)sizeof(pmdaInstid);
	if ((indomtab[indom].it_set = (pmdaInstid *)
			realloc(indomtab[indom].it_set, size)) == NULL) {
	    __pmNotifyErr(LOG_ERR, "dropping instance '%s': %s", hptr->tag,
							osstrerror());
	    free(hptr->tag);
	    return -1;
	}
	indomtab[indom].it_set[index].i_inst = hptr->id;
	indomtab[indom].it_set[index].i_name = hptr->tag;
	indomtab[indom].it_numinst++;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "Updated indom #%d:\n", indom);
	    for (index=0; index < indomtab[indom].it_numinst; index++)
		fprintf(stderr, "  Instance %d='%s'\n",
			indomtab[indom].it_set[index].i_inst,
			indomtab[indom].it_set[index].i_name);
	}
#endif
    }
    else {	/* update an existing entry */
	freeflag++;	/* wont need tag afterwards, so mark for deletion */
	if (hptr->taglength != newhash.taglength) {
	    __pmNotifyErr(LOG_ERR, "hash table update failure - '%s' "
		"data ignored (bad tag length)", tag);
	    free(newhash.tag);
	    return -1;
	}
	else {	/* update existing entries free running counter */
	    hptr->realcount++;
	    if (hptr->tracetype == TRACE_TYPE_TRANSACT)
		hptr->realtime += data;
		/* keep running total of time attributed to transactions */
	    else if (hptr->tracetype == TRACE_TYPE_COUNTER)
		hptr->txsum = data;
		/* counters are 'permanent' and immediately available */
	    else if (hptr->tracetype == TRACE_TYPE_OBSERVE)
		hptr->txsum = data;
		/* observations are 'permanent' and immediately available */
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "'%s' real count updated (%d)",
			hptr->tag, (int)hptr->realcount);
#endif
	}
    }

    /*
     * Next, update the current ring buffer entry with new trace data
     */
    if ((hptr = __pmhashlookup(ringbuf.ring[rpos].stats, tag,
							&newhash)) == NULL) {
	hash.tag = strdup(tag);
	hash.tracetype = type;
	hash.id = 0;	/* the ring buffer is never used to resolve indoms */
	hash.padding = 0;
	hash.realcount = 1;
	hash.realtime = data;
	hash.taglength = (unsigned int)taglen;
	hash.fd = clientfd;
	hash.txcount = 1;
	hash.txmin = hash.txmax = data;
	hash.txsum = data;
	hptr = &hash;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "fresh interval data on fd=%d rpos=%d "
		    "('%s': len=%d type=%d value=%d)", clientfd, rpos,
		    hash.tag, taglen, type, (int)data);
#endif
	if (__pmhashinsert(ringbuf.ring[rpos].stats, hash.tag, hptr) < 0) {
	    __pmNotifyErr(LOG_ERR, "ring buffer insert failure - '%s' "
		"data ignored", hash.tag);
	    free(hash.tag);
	    return -1;
	}
    }
    else {	/* update existing entry */
	hptr->txcount++;
	if (hptr->tracetype == TRACE_TYPE_TRANSACT) {
	    if (data < hptr->txmin)
		hptr->txmin = data;
	    if (data > hptr->txmax)
		hptr->txmax = data;
	    hptr->txsum += data;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "Updating data on fd=%d ('%s': type=%d "
		    "count=%d min=%f max=%f sum=%f)",
		    clientfd, hptr->tag, hptr->tracetype,
		    hptr->txcount, hptr->txmin, hptr->txmax, hptr->txsum);
#endif
    }
    if (freeflag) {
	free(newhash.tag);
	newhash.tag = NULL;
    }

    return hptr->tracetype;
}

static void
clearTable(hashtable_t *t, void *entry)
{
    hashdata_t	*h = (hashdata_t *)entry;
    h->txcount = -1;	/* flag as out-of-date */
}

/*
 * Goes off at set time interval.  The old `tail' becomes the new `head'
 * of the ring buffer & is marked as currently in-progess (and this data
 * is not exported until the next timer event).
 */
void
timerUpdate(void)
{
    /* summary table must be reset for next fetch */
    if (dosummary == 0) {
	__pmhashtraverse(&summary, clearTable);
	dosummary = 1;
    }

    if (ringbuf.ring[rpos].working == 0) {
	__pmNotifyErr(LOG_ERR, "buffered I/O error - ignoring timer event");
	return;
    }

    ringbuf.ring[rpos].working = 0;
    if (rpos == rbufsize-1)
	rpos = 0;	/* return to start of buffer */
    else
	rpos++;
    __pmhashtrunc(ringbuf.ring[rpos].stats);	/* new working set */
    ringbuf.ring[rpos].working = 1;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "Alarm - working buffer is now %d/%d",
		rpos+1, rbufsize);
#endif
}

static void
summariseDataAux(hashtable_t *t, void *entry)
{
    hashdata_t	*hptr;
    hashdata_t	*base = (hashdata_t *)entry;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "summariseDataAux: looking up '%s'", base->tag);
#endif
    /* update summary hash table */
    if ((hptr = __pmhashlookup(&summary, base->tag, base)) == NULL)
	__pmNotifyErr(LOG_ERR, "summariseDataAux: entry in ring buffer, "
		"but not summary ('%s')", base->tag);
    else {	/* update an existing summary */
	if (hptr->tracetype == TRACE_TYPE_TRANSACT) {
	    if (hptr->txcount == -1) {	/* reset coz flagged as out-of-date */
		tindomsize++;
		hptr->txcount = base->txcount;
		hptr->txmin = base->txmin;
		hptr->txmax = base->txmax;
		hptr->txsum = base->txsum;
	    }
	    else {
		hptr->txcount += base->txcount;
		if (base->txmin < hptr->txmin)
		    hptr->txmin = base->txmin;
		if (base->txmax > hptr->txmax)
		    hptr->txmax = base->txmax;
		hptr->txsum += base->txsum;
	    }
	}
	else {
	    if (hptr->txcount == -1) {	/* reset coz flagged as out-of-date */
		if (hptr->tracetype == TRACE_TYPE_POINT)
		    pindomsize++;
		else if (hptr->tracetype == TRACE_TYPE_COUNTER)
		    cindomsize++;
		else if (hptr->tracetype == TRACE_TYPE_OBSERVE)
		    oindomsize++;
		else {
		    __pmNotifyErr(LOG_ERR,
				"bogus trace type - skipping '%s'", hptr->tag);
		    return;
		}
		hptr->txcount = base->txcount;
	    }
	    else {
		hptr->txcount += base->txcount;
	    }
	}
    }
}


/*
 * Create the summary hash table, as well as the instance list for
 * this set of intervals.
 */
static void
summariseData(void)
{
    int	count;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "summariseData: resummarising");
#endif
    /* initialise counters */
    tindomsize = pindomsize = oindomsize = 0;

    /* create the new summary table */
    for (count=0; count < rbufsize; count++) {
	if (ringbuf.ring[count].working == 1)
	    continue;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "ring buffer table %d/%d has %d entries.\n",
		count, rbufsize, ringbuf.ring[count].stats->entries);
#endif
	__pmhashtraverse(ringbuf.ring[count].stats, summariseDataAux);
    }

    /* summary now holds correct data for the rest of this interval */
    dosummary = 0;
}

/*
 * Try to keep trace fetching similar to libpcp_pmda pmdaFetch()
 * we need a different way to get at the instances though
 * so, we can still use __pmdaStartInst, but trace iterator
 * is a bit different.
 */
static pmdaInstid *
nextTraceInst(pmdaExt *pmda)
{
    static pmdaInstid	in = { PM_INDOM_NULL, NULL };
    pmdaInstid		*iptr = NULL;

    if (pmda->e_singular == 0) {
	/* PM_INDOM_NULL ... just the one value */
	iptr = &in;
	pmda->e_singular = -1;
    }
    if (pmda->e_ordinal >= 0) {
	int	j;
	for (j = pmda->e_ordinal; j < pmda->e_idp->it_numinst; j++) {
	    if (__pmInProfile(pmda->e_idp->it_indom, pmda->e_prof,
					pmda->e_idp->it_set[j].i_inst)) {
		iptr = &pmda->e_idp->it_set[j];
		pmda->e_ordinal = j+1;
		break;
	    }
	}
    }
    return iptr;
}

static int
auxFetch(int inst, __pmID_int *idp, char *tag, pmAtomValue *atom)
{
    if (inst != PM_IN_NULL && idp->cluster != 0)
	return PM_ERR_INST;

    /* transaction, point, counter and observe trace values and control data */
    if (idp->cluster == 0) {
	hashdata_t	hash;
	hashdata_t	*hptr;

	hash.tag = tag;

	switch (idp->item) {
	case 0:				/* trace.transact.count */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    atom->ull = hptr->realcount;
	    break;
	case 1:				/* trace.transact.rate */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    atom->f = (float)((double)hptr->txcount/(double)timespan.tv_sec);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_DEBUG, "rate=%f=%f/%f('%s')\n", (float)atom->f,
			(double)hptr->txcount, (double)timespan.tv_sec, tag);
#endif
	    break;
	case 2:				/* trace.transact.ave_time */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    else if (hptr->txcount == 0)
		atom->f = 0;
	    else {
		atom->f = (float)((double)hptr->txsum/(double)hptr->txcount);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1)
		    __pmNotifyErr(LOG_DEBUG, "ave_time=%f=%f/%f('%s')\n", (float)atom->f,
			(double)hptr->txsum, (double)hptr->txcount, tag);
#endif
	    }
	    break;
	case 3:				/* trace.transact.min_time */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    atom->f = (float)hptr->txmin;
	    break;
	case 4:				/* trace.transact.max_time */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    atom->f = (float)hptr->txmax;
	    break;
	case 5:				/* trace.transact.total_time */
	    hash.tracetype = TRACE_TYPE_TRANSACT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    atom->d = hptr->realtime;
	    break;
	case 6:				/* trace.point.count */
	    hash.tracetype = TRACE_TYPE_POINT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    atom->ull = hptr->realcount;
	    break;
	case 7:				/* trace.point.rate */
	    hash.tracetype = TRACE_TYPE_POINT;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    atom->f = (float)((double)hptr->txcount/(double)timespan.tv_sec);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_DEBUG, "rate=%f=%f/%f('%s')\n", (float)atom->f,
			(double)hptr->txcount, (double)timespan.tv_sec, tag);
#endif
	    break;
	case 8:				/* trace.observe.count */
	case 17:			/* trace.counter.count */
	    hash.tracetype = (idp->item == 8)?
			TRACE_TYPE_OBSERVE : TRACE_TYPE_COUNTER;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    atom->ull = hptr->realcount;
	    break;
	case 9:				/* trace.observe.rate */
	case 18:			/* trace.counter.rate */
	    hash.tracetype = (idp->item == 9)?
			TRACE_TYPE_OBSERVE : TRACE_TYPE_COUNTER;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    if (hptr->txcount < 0)	/* not in current time period */
		return 0;
	    atom->f = (float)((double)hptr->txcount/(double)timespan.tv_sec);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		__pmNotifyErr(LOG_DEBUG, "rate=%f=%f/%f('%s')\n", (float)atom->f,
			(double)hptr->txcount, (double)timespan.tv_sec, tag);
#endif
	    break;
	case 10:			/* trace.observe.value */
	case 19:			/* trace.counter.value */
	    hash.tracetype = (idp->item == 10)?
			TRACE_TYPE_OBSERVE : TRACE_TYPE_COUNTER;
	    if ((hptr = __pmhashlookup(&summary, hash.tag, &hash)) == NULL)
		return PM_ERR_INST;
	    atom->d = hptr->txsum;
	    break;
	case 11:			/* trace.control.timespan */
	    atom->ul = timespan.tv_sec;
	    break;
	case 12:			/* trace.control.interval */
	    atom->ul = interval.tv_sec;
	    break;
	case 13:			/* trace.control.buckets */
	    atom->ul = rbufsize-1;
	    break;
	case 14:			/* trace.control.port */
	    atom->ul = ctlport;
	    break;
	case 15:			/* trace.control.reset */
	    atom->ul = 1;
	    break;
	case 16:			/* trace.control.debug */
	    atom->ul = pmDebug;
	    break;
	default:
	    return PM_ERR_PMID;
	}
    }
    else
	return PM_ERR_PMID;

    return 1;
}

static int
getIndomSize(__pmID_int *pmidp)
{
    int size;

    if (pmidp->cluster != 0)
	return 1;

    switch (pmidp->item) {
	case 0:		/* uses summary's real counters (transact) */
	case 5:
	    size = indomtab[TRANSACT_INDOM].it_numinst;
	    break;
	case 1:
	case 2:
	case 3:
	case 4:		/* susceptible to ring buffer updates */
	    size = tindomsize;
	    break;
	case 6:		/* uses summary's real counters (point) */
	    size = indomtab[POINT_INDOM].it_numinst;
	    break;
	case 7:		/* susceptible to ring buffer updates */
	    size = pindomsize;
	    break;
	case 8:
	case 10:	/* uses summary's real counters & data (obs) */
	    size = indomtab[OBSERVE_INDOM].it_numinst;
	    break;
	case 9:		/* susceptible to ring buffer updates */
	    size = oindomsize;
	    break;
	case 17:
	case 19:	/* uses summary's real counters & data (ctr) */
	    size = indomtab[COUNTER_INDOM].it_numinst;
	    break;
	case 18:	/* susceptible to ring buffer updates */
	    size = cindomsize;
	    break;
	default:
	    size = 0;
    }
    return size;
}


static int
traceFetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    static int		maxnpmids = 0;
    static pmResult	*res = NULL;
    pmValueSet		*vset;
    pmDesc		*dp;
    __pmID_int		*pmidp;
    pmdaMetric		*metap;
    pmAtomValue		atom;
    pmdaInstid		*ins;
    pmdaInstid		noinst = { PM_IN_NULL, NULL };
    int			numval;
    int			sts, i, j, need;

    indomSortCheck();
    pmda->e_idp = indomtab;

    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid-1)*(int)sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL) {
	    return -oserror();
	}
	maxnpmids = numpmid;
    }

    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    for (i = 0; i < numpmid; i++) {
	dp = NULL;
	metap = NULL;
	pmidp = (__pmID_int *)&pmidlist[i];
	if (pmda->e_direct) {
	    if (pmidp->item < pmda->e_nmetrics &&
		pmidlist[i] == pmda->e_metrics[pmidp->item].m_desc.pmid) {
		metap = &pmda->e_metrics[pmidp->item];
		dp = &(metap->m_desc);
	    }
	}
	else {	/* search for it */
	    for (j = 0; j < pmda->e_nmetrics; j++) {
		if (pmidlist[i] == pmda->e_metrics[j].m_desc.pmid) {
		    metap = &pmda->e_metrics[j];
		    dp = &(metap->m_desc);
		    break;
		}
	    }
	}
	if (dp == NULL) {
	    __pmNotifyErr(LOG_ERR, "traceFetch: Requested metric %s is not "
					"defined", pmIDStr(pmidlist[i]));
	    numval = PM_ERR_PMID;
	}
	else if (dp->indom != PM_INDOM_NULL) {
	    /*
	     * Only summarise when you have to, so check if data has
	     * already been summarised within this interval.
	     */
	    if (dosummary == 1)
		summariseData();
	    numval = getIndomSize(pmidp);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "instance domain for %s numval=%d",
		pmIDStr(dp->pmid), numval);
#endif
	}
	else
	    numval = 1;

	/* Must use individual malloc()s because of pmFreeResult() */
	if (numval >= 1)
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet)+
					(numval - 1)*sizeof(pmValue));
	else
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet)-
					sizeof(pmValue));
	if (vset == NULL) {
	    if ((res->numpmid = i) > 0)
		__pmFreeResultValues(res);
	    return -oserror();
	}

	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;

	if (dp->indom == PM_INDOM_NULL)
	    ins = &noinst;
	else {
	    __pmdaStartInst(dp->indom, pmda);
	    ins = nextTraceInst(pmda);
	}
	j = 0;
	do {
	    if (ins == NULL) {
		__pmNotifyErr(LOG_ERR, "bogus instance ignored (pmid=%s)",
					pmIDStr(dp->pmid));
		if ((res->numpmid = i) > 0)
		    __pmFreeResultValues(res);
		return PM_ERR_INST;
	    }
	    if (j == numval) {
		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == NULL) {
		    if ((res->numpmid = i) > 0)
			__pmFreeResultValues(res);
		    return -oserror();
		}
	    }
	    vset->vlist[j].inst = ins->i_inst;
	    if ((sts = auxFetch(ins->i_inst, pmidp, ins->i_name, &atom)) < 0) {
		if (sts == PM_ERR_PMID)
		    __pmNotifyErr(LOG_ERR, "unknown PMID requested - '%s'",
				pmIDStr(dp->pmid));
		else if (sts == PM_ERR_INST)
		    __pmNotifyErr(LOG_ERR, "unknown instance requested - %d "
			"(pmid=%s)", ins->i_inst, pmIDStr(dp->pmid));
		else
		    __pmNotifyErr(LOG_ERR, "fetch error (pmid=%s): %s",
				pmIDStr(dp->pmid), pmErrStr(sts));
	    }
	    else if (sts == 0) {	/* not current, so don't use */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    __pmNotifyErr(LOG_DEBUG, "Instance is dated %s (pmid=%s)",
					ins->i_name, pmIDStr(dp->pmid));
#endif
	    }
	    else if ((sts = __pmStuffValue(&atom, &vset->vlist[j], dp->type)) == PM_ERR_TYPE)
		__pmNotifyErr(LOG_ERR, "bad desc type (%d) for metric %s",
				dp->type, pmIDStr(dp->pmid));
	    else if (sts >= 0) {
		vset->valfmt = sts;
		j++;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    __pmNotifyErr(LOG_DEBUG, "Instance is good! %s (pmid=%s)",
					ins->i_name, pmIDStr(dp->pmid));
#endif
	    }
	} while (dp->indom != PM_INDOM_NULL &&
				(ins = nextTraceInst(pmda)) != NULL);
	if (j == 0)
	    vset->numval = sts;
	else
	    vset->numval = j;
    }
    *resp = res;

    return 0;
}


static int
traceStore(pmResult *result, pmdaExt *pmda)
{
    int		i, j;
    int         sts = 0;
    pmValueSet  *vsp = NULL;
    __pmID_int   *pmidp = NULL;
    pmAtomValue	av;
    extern int	afid;
    extern void alarming(int, void *);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster != 0)
	    return PM_ERR_PMID;

	if (pmidp->item == 15) {	/* trace.control.reset */
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "resetting trace metrics");
#endif
	    /* reset the interval timer */
	    if (afid >= 0) {
		__pmAFunregister(afid);
		if ((afid = __pmAFregister(&interval, NULL, alarming)) < 0) {
		    __pmNotifyErr(LOG_ERR, "__pmAFregister failed");
		    exit(1);
		}
	    }

	    /* reset summary and ring buffer hash tables */
	    __pmhashtrunc(&summary);
	    for (j = 0; j < rbufsize; j++) {
		__pmhashtrunc(ringbuf.ring[j].stats);
		ringbuf.ring[j].numstats = 0;
	    }

	    /* clear all the instance domain entries */
	    if (indomtab[TRANSACT_INDOM].it_set) {
		free(indomtab[TRANSACT_INDOM].it_set);
		indomtab[TRANSACT_INDOM].it_set = NULL;
	    }
	    if (indomtab[OBSERVE_INDOM].it_set) {
		free(indomtab[OBSERVE_INDOM].it_set);
		indomtab[OBSERVE_INDOM].it_set = NULL;
	    }
	    if (indomtab[COUNTER_INDOM].it_set) {
		free(indomtab[COUNTER_INDOM].it_set);
		indomtab[COUNTER_INDOM].it_set = NULL;
	    }
	    if (indomtab[POINT_INDOM].it_set) {
		free(indomtab[POINT_INDOM].it_set);
		indomtab[POINT_INDOM].it_set = NULL;
	    }
	    indomtab[TRANSACT_INDOM].it_numinst = 0;
	    indomtab[COUNTER_INDOM].it_numinst = 0;
	    indomtab[OBSERVE_INDOM].it_numinst = 0;
	    indomtab[POINT_INDOM].it_numinst = 0;
	    tindomsize = pindomsize = oindomsize = 0;
	    /* definately need to recompute the summary next fetch */
	    dosummary = 1;
	    __pmNotifyErr(LOG_INFO, "PMDA reset");
	}
	else if (pmidp->item == 16) {	/* trace.control.debug */
	    if (vsp->numval != 1 || vsp->valfmt != PM_VAL_INSITU)
		sts = PM_ERR_BADSTORE;
	    else if (sts >= 0 && ((sts = pmExtractValue(vsp->valfmt,
			&vsp->vlist[0], PM_TYPE_32, &av, PM_TYPE_32)) >= 0)) {
		if (pmDebug != av.l) {
		    pmDebug = av.l;
		    __pmNotifyErr(LOG_INFO, "debug level set to %d", pmDebug);
		    debuglibrary(pmDebug);
		}
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
traceInit(pmdaInterface *dp)
{
    int		rsize, sts;

    if (dp->status != 0)
	return;

    dp->version.two.fetch = traceFetch;
    dp->version.two.store = traceStore;
    dp->version.two.instance = traceInstance;
    dp->version.two.ext->e_direct = 0;
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));

    /* initialise ring buffer */
    rsize = (int)(sizeof(statlist_t) * rbufsize);
    if ((ringbuf.ring = (statlist_t *)malloc(rsize)) == NULL) {
	__pmNotifyErr(LOG_ERR, "failed during ring buffer initialise: %s",
		osstrerror());
	exit(1);
    }
    for (rsize=0; rsize < rbufsize; rsize++) {
	if ((ringbuf.ring[rsize].stats = (hashtable_t *)
	 			malloc(sizeof(hashtable_t))) == NULL) {
	    __pmNotifyErr(LOG_ERR, "ring buffer initialise failed: %s",
		osstrerror());
	    exit(1);
	}
	if ((sts = __pmhashinit(ringbuf.ring[rsize].stats, 0, sizeof(hashdata_t),
						datacmp, datadel)) < 0) {
	    __pmNotifyErr(LOG_ERR, "ring buffer initialisation failed: %s",
		osstrerror());
	    exit(1);
	}
	ringbuf.ring[rsize].working = 0;
    }
    rpos = 0;
    ringbuf.ring[rpos].working = 1;

    /* initialise summary & associated instance domain */
    indomtab[TRANSACT_INDOM].it_numinst = 0;
    indomtab[TRANSACT_INDOM].it_set = NULL;
    indomtab[POINT_INDOM].it_numinst = 0;
    indomtab[POINT_INDOM].it_set = NULL;
    dp->version.two.ext->e_idp = indomtab;
    if ((sts = __pmhashinit(&summary, 0, sizeof(hashdata_t),
						datacmp, summarydel)) < 0) {
	__pmNotifyErr(LOG_ERR, "summary table initialisation failed: %s",
						osstrerror());
	exit(1);
    }
    /* initialise list of reserved instance domains (for store recovery) */
    if ((sts = __pmhashinit(&history, 0, sizeof(instdata_t),
						instcmp, instdel)) < 0) {
	__pmNotifyErr(LOG_ERR, "history table initialisation failed: %s",
						osstrerror());
	exit(1);
    }
}
