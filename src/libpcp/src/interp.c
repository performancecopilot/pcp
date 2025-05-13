/*
 * Copyright (c) 2015-2017,2022 Red Hat.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Thread-safe notes:
 *
 * nr[] and nr_cache[] are diagnostic counters that are maintained with
 * non-atomic updates ... we've decided that it is acceptable for their
 * values to be subject to possible (but unlikely) missed updates
 *
 * the one-trip initialization of ignore_mark_records and ignore_mark_gap
 * is not guarded as the same value would result from concurrent repeated
 * execution
 */

/*
 * Note: _FORTIFY_SOURCE cannot be set here because the calls to memcpy()
 *       with vp->vbuf as the destination raise a warning that is not
 *       correct as the allocation for a pmValueBlock always extends
 *       vbuf[] to be the correct size, not the [1] as per the declaration
 *       in pmapi.h
 */
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0
#endif

#include <limits.h>
#include <inttypes.h>
#include <assert.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"

#define UPD_MARK_NONE	0
#define UPD_MARK_FORW	1
#define UPD_MARK_BACK	2

#if defined(HAVE_CONST_LONGLONG)
#define SIGN_64_MASK 0x8000000000000000LL
#else
#define SIGN_64_MASK 0x8000000000000000
#endif

typedef union {				/* value from pmResult */
    pmValueBlock	*pval;
    int			lval;
} value;

/*
 * state values for s_prior and s_next ...
 * first 3 are mutually exclusive and setting any of them clears
 * S_SCANNED ... S_SCANNED maybe set independently
 */
#define S_UNDEFINED	1	/* no searching done yet */
#define S_MARK		2	/* found <mark> at t_prior or t_next before
				   a value was found */
#define S_VALUE		4	/* found value at t_prior or t_next */
#define S_SCANNED	16	/* region between t_req and t_prior or
				   t_next has been scanned already */

#define SET_UNDEFINED(state) state = S_UNDEFINED
#define SET_MARK(state) state = S_MARK
#define SET_VALUE(state) state = S_VALUE
#define SET_SCANNED(state) state |= S_SCANNED
#define CLEAR_SCANNED(state) state &= ~S_SCANNED

#define IS_UNDEFINED(state) ((state & S_UNDEFINED) == S_UNDEFINED)
#define IS_MARK(state) ((state & S_MARK) == S_MARK)
#define IS_VALUE(state) ((state & S_VALUE) == S_VALUE)
#define IS_SCANNED(state) ((state & S_SCANNED) == S_SCANNED)

typedef struct instcntl {		/* metric-instance control */
    struct instcntl	*want;		/* ones of interest */
    struct instcntl	*unbound;	/* not yet bound above [or below] */
    int			search;		/* looking for found this one? */
    int			inst;		/* instance identifier */
    int			inresult;	/* will be in this result */
    double		t_prior;
    int			s_prior;	/* state at t_prior */
    value		v_prior;
    double		t_next;
    int			s_next;		/* state at t_next */
    value		v_next;
    double		t_first;	/* no records before this */
    double		t_last;		/* no records after this */
    double		t_birth;	/* (optional) instance first seen */
    double		t_death;	/* (optional) instance last seen */
    struct pmidcntl	*metric;	/* back to metric control */
} instcntl_t;

typedef struct pmidcntl {		/* metric control */
    pmDesc		desc;
    int			valfmt;		/* used to build result */
    int			numval;		/* number of instances in this result */
    int			last_numval;	/* number of instances in previous result */
    __pmHashCtl		hc;		/* metric-instances */
} pmidcntl_t;

typedef struct {
    __pmResult	*rp;		/* cached pmResult from __pmLogRead */
    int		sts;		/* from __pmLogRead */
    char	*c_name;	/* log name */
    int		vol;		/* log volume */
    long	head_posn;	/* posn in file before forwards __pmLogRead */
    long	tail_posn;	/* posn in file after forwards __pmLogRead */
    int		mode;		/* PM_MODE_FORW or PM_MODE_BACK */
    int		used;		/* used count for LFU replacement */
} cache_t;

#define NUMCACHE 4

/*
 * diagnostic counters ... indexed by PM_MODE_FORW (2) and
 * PM_MODE_BACK	(3), hence 4 elts for cached and non-cached reads
 */
static long	nr_cache[PM_MODE_BACK+1];
static long	nr[PM_MODE_BACK+1];

/*
 * called with the context lock held
 */
static int
cache_read(__pmContext *ctxp, int mode, __pmResult **rp)
{
    __pmArchCtl	*acp = ctxp->c_archctl;
    long	posn;
    cache_t	*cp;
    cache_t	*lfup;
    cache_t	*cache;
    char	*save_curlog_name;
    int		sts;
    int		save_curvol;
    int		archive_changed;

    /*
     * If the previous __pmLogRead generated a virtual MARK record and we have
     * changed direction, then we need to generate that record again.
     */
    if (acp->ac_mark_done != 0 && acp->ac_mark_done != mode) {
	sts = __pmLogGenerateMark_ctx(ctxp, acp->ac_mark_done, rp);
	acp->ac_mark_done = 0;
	return sts;
    }

    /* Look for a cache hit. */
    if (acp->ac_vol == acp->ac_curvol) {
	posn = __pmFtell(acp->ac_mfp);
	assert(posn >= 0);
    }
    else
	posn = 0;

    if (acp->ac_cache == NULL) {
	/* cache initialization */
	acp->ac_cache = cache = (cache_t *)calloc(NUMCACHE, sizeof(cache_t));
	if (!cache)
	    return -ENOMEM;
	acp->ac_cache_idx = 0;
    }
    else
	cache = acp->ac_cache;

    if (pmDebugOptions.log && pmDebugOptions.desperate) {
	fprintf(stderr, "cache_read: fd=%d mode=%s vol=%d (curvol=%d) %s_posn=%ld ",
	    __pmFileno(acp->ac_mfp),
	    mode == PM_MODE_FORW ? "forw" : "back",
	    acp->ac_vol, acp->ac_curvol,
	    mode == PM_MODE_FORW ? "head" : "tail",
	    (long)posn);
    }

    acp->ac_cache_idx = (acp->ac_cache_idx + 1) % NUMCACHE;
    lfup = &cache[acp->ac_cache_idx];
    for (cp = cache; cp < &cache[NUMCACHE]; cp++) {
	if (cp->c_name != NULL && strcmp(cp->c_name, acp->ac_log->name) == 0 &&
	    cp->vol == acp->ac_vol &&
	    ((mode == PM_MODE_FORW && cp->head_posn == posn) ||
	     (mode == PM_MODE_BACK && cp->tail_posn == posn)) &&
	    cp->rp != NULL) {
	    *rp = cp->rp;
	    cp->used++;
	    if (mode == PM_MODE_FORW)
		__pmFseek(acp->ac_mfp, cp->tail_posn, SEEK_SET);
	    else
		__pmFseek(acp->ac_mfp, cp->head_posn, SEEK_SET);
	    if (pmDebugOptions.log && pmDebugOptions.desperate) {
		__pmTimestamp	tmp;
		double		t_this;
		tmp = cp->rp->timestamp;	/* struct assignment */
		t_this = __pmTimestampSub(&tmp, __pmLogStartTime(acp));
		fprintf(stderr, "hit cache[%d] t=%.6f\n",
			(int)(cp - cache), t_this);
		nr_cache[mode]++;
	    }
	    acp->ac_mark_done = 0;
	    sts = cp->sts;
	    return sts;
	}
    }

    if (pmDebugOptions.log && pmDebugOptions.desperate)
	fprintf(stderr, "miss\n");
    nr[mode]++;

    if (lfup->rp != NULL)
	__pmFreeResult(lfup->rp);

    /*
     * We need to know when we cross archive or volume boundaries.
     * The only way to know if the archive has changed is to check whether the
     * archive name has changed.
     */
    if ((save_curlog_name = strdup(acp->ac_log->name)) == NULL) {
	pmNoMem("__pmLogFetchInterp.save_curlog_name",
		  strlen(acp->ac_log->name) + 1, PM_FATAL_ERR);
    }
    save_curvol = acp->ac_curvol;

    lfup->sts = __pmLogRead_ctx(ctxp, mode, NULL, &lfup->rp, PMLOGREAD_NEXT);
    if (lfup->sts < 0)
	lfup->rp = NULL;
    *rp = lfup->rp;

    archive_changed = strcmp(save_curlog_name, acp->ac_log->name) != 0;
    free(save_curlog_name);

    /*
     * vol/arch switch since last time, or vol/arch switch or virtual mark
     * record generated in __pmLogRead_ctx() ...
     * new vol/arch, stdio stream and we don't know where we started from
     * ... don't cache
     */
    if (posn == 0 || save_curvol != acp->ac_curvol || archive_changed ||
	acp->ac_mark_done) {
	if (lfup->c_name) {
	    free(lfup->c_name);
	    lfup->c_name = NULL;
	}
	if (pmDebugOptions.log && pmDebugOptions.desperate)
	    fprintf(stderr, "cache_read: reload vol switch, mark cache[%d] unused\n",
		(int)(lfup - cache));
    }
    else {
	lfup->mode = mode;
	lfup->vol = acp->ac_vol;
	lfup->used = 1;
	/* Try to avoid excessive copying/freeing of the archive name. */
	if (lfup->c_name != NULL && strcmp(lfup->c_name, acp->ac_log->name) != 0) {
	    free(lfup->c_name);
	    lfup->c_name = NULL;
	}
	if (lfup->c_name == NULL) {
	    if ((lfup->c_name = strdup(acp->ac_log->name)) == NULL) {
		pmNoMem("__pmLogFetchInterp.name",
			  strlen(acp->ac_log->name) + 1, PM_FATAL_ERR);
	    }
	}
	if (mode == PM_MODE_FORW) {
	    lfup->head_posn = posn;
	    lfup->tail_posn = __pmFtell(acp->ac_mfp);
	    assert(lfup->tail_posn >= 0);
	}
	else {
	    lfup->tail_posn = posn;
	    lfup->head_posn = __pmFtell(acp->ac_mfp);
	    assert(lfup->head_posn >= 0);
	}
	if (pmDebugOptions.log && pmDebugOptions.desperate) {
	    fprintf(stderr, "cache_read: reload cache[%d] vol=%d (curvol=%d) head=%ld tail=%ld ",
		(int)(lfup - cache), lfup->vol, acp->ac_curvol,
		(long)lfup->head_posn, (long)lfup->tail_posn);
	    if (lfup->sts == 0)
		fprintf(stderr, "sts=%d\n", lfup->sts);
	    else {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "sts=%s\n", pmErrStr_r(lfup->sts, errmsg, sizeof(errmsg)));
	    }
	}
    }

    return lfup->sts;
}

/*
 * prior == 1 for ?_prior fields, else use ?_next fields
 */
static void
dumpval(FILE *f, int type, int valfmt, int prior, instcntl_t *icp)
{
    int		state;
    value	*vp;
    if (prior) {
	state = icp->s_prior;
	vp = &icp->v_prior;
    }
    else {
	state = icp->s_next;
	vp = &icp->v_next;
    }
    if (!IS_VALUE(state)) {
	fprintf(f, " state=");
	if (IS_UNDEFINED(state)) fprintf(f, "<undefined>");
	else if (IS_MARK(state)) fprintf(f, "<mark>");
	else fprintf(f, "<botch=%d>", state);
	if (IS_SCANNED(state)) fprintf(f, "&<scanned>");
	return;
    }
    if (IS_SCANNED(state)) fprintf(f, " state=<scanned>");
    if (type == PM_TYPE_32 || type == PM_TYPE_U32)
	fprintf(f, " v=%d", vp->lval);
    else if (type == PM_TYPE_FLOAT && valfmt == PM_VAL_INSITU) {
	float		tmp;
	memcpy((void *)&tmp, (void *)&vp->lval, sizeof(tmp));
	fprintf(f, " v=%f", (double)tmp);
    }
    else if (valfmt == PM_VAL_DPTR || valfmt == PM_VAL_SPTR) {
	if (type == PM_TYPE_64) {
	    __int64_t	tmp;
	    memcpy((void *)&tmp, (void *)vp->pval->vbuf, sizeof(tmp));
	    fprintf(f, " v=%"PRIi64, tmp);
	}
	else if (type == PM_TYPE_U64) {
	    __uint64_t	tmp;
	    memcpy((void *)&tmp, (void *)vp->pval->vbuf, sizeof(tmp));
	    fprintf(f, " v=%"PRIu64, tmp);
	}
	else if (type == PM_TYPE_FLOAT) {
	    float		tmp;
	    memcpy((void *)&tmp, (void *)vp->pval->vbuf, sizeof(tmp));
	    fprintf(f, " v=%f", (double)tmp);
	}
	else if (type == PM_TYPE_DOUBLE) {
	    double		tmp;
	    memcpy((void *)&tmp, (void *)vp->pval->vbuf, sizeof(tmp));
	    fprintf(f, " v=%f", tmp);
	}
	else
	    fprintf(f, " v=??? (lval=%d)", vp->lval);
    }
    else
	fprintf(f, " bad valfmt %d v=??? (lval=%d)", valfmt, vp->lval);
}

static void
dumpicp(const char *tag, instcntl_t *icp)
{
    char	strbuf[20];
    fprintf(stderr, "%s: pmid %s", tag, pmIDStr_r(icp->metric->desc.pmid, strbuf, sizeof(strbuf)));
    if (icp->inst != PM_IN_NULL)
	fprintf(stderr, "[inst=%d]", icp->inst);
    fputc(':', stderr);
    if (icp->t_first >= 0)
	fprintf(stderr, " t_first=%.6f", icp->t_first);
    fprintf(stderr, " t_prior=%.6f", icp->t_prior);
    dumpval(stderr, icp->metric->desc.type, icp->metric->valfmt, 1, icp);
    fprintf(stderr, " t_next=%.6f", icp->t_next);
    dumpval(stderr, icp->metric->desc.type, icp->metric->valfmt, 0, icp);
    if (icp->t_last >= 0)
	fprintf(stderr, " t_last=%.6f", icp->t_last);
    if (icp->t_birth >= 0)
	fprintf(stderr, " t_birth=%.6f", icp->t_birth);
    if (icp->t_death >= 0)
	fprintf(stderr, " t_death=%.6f", icp->t_death);
    fputc('\n', stderr);
}

/*
 * Update the upper (next) and lower (prior) bounds.
 * Parameters do_mark and done control the context in which this is
 * being done:
 * UPD_MARK_FORW, NULL - roll forwards getting to t_req
 * UPD_MARK_BACK, NULL - roll backwards getting to t_req
 * UPD_MARK_NONE, NULL - fine positioning (forwards or backwards) after
 * 			 seeking into the archive using the index
 * UPD_MARK_BACK, &done - specifically looking for lower bound
 * UPD_MARK_BACK, &done - specifically looking for upper bound
 */
static int
update_bounds(__pmContext *ctxp, double t_req, __pmResult *logrp, int do_mark, int *done, int *seen_mark)
{
    /*
     * for every metric in the result from the log
     *   for every instance in the result from the log
     *     if we have ever asked for this metric and instance, update the
     *        range bounds, if necessary
     */
    int			k;
    int			i;
    __pmHashCtl		*hcp = &ctxp->c_archctl->ac_pmid_hc;
    __pmHashNode	*hp;
    __pmHashNode	*ihp;
    pmidcntl_t		*pcp;
    instcntl_t		*icp;
    double		t_this;
    __pmTimestamp	tmp;
    int			changed;
    static int		ignore_mark_records = -1;
    static double	ignore_mark_gap = 0;

    tmp = logrp->timestamp;		/* struct assignment */
    t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));

    if (logrp->numpmid == 0) {
	if (ignore_mark_records == -1) {
	    /* one-trip initialization */
	    char	*str;
	    PM_LOCK(__pmLock_extcall);
	    str = getenv("PCP_IGNORE_MARK_RECORDS");	/* THREADSAFE */
	    if (str != NULL) {
		if (str[0] != '\0') {
		    /*
		     * set and value is time in -t format ... if <= this time
		     * between pmResults, ignore the <mark> record
		     */
		    char		*endnum;
		    struct timespec	gap;
		    if (pmParseInterval(str, &gap, &endnum) < 0) {
			/*
			 * probably should use pmprintf(), but that takes us into
			 * deadlock hell!
			 */
			fprintf(stderr, "%s: Warning: bad $PCP_IGNORE_MARK_RECORDS: not in pmParseInterval(3) format:\n%s\n",
				pmGetProgname(), endnum);
			free(endnum);
			ignore_mark_records = 0;
		    }
		    else {
			ignore_mark_gap = pmtimespecToReal(&gap);
			ignore_mark_records = 1;
		    }
		}
		else {
		    /*
		     * set and no value, 0 is the sentinal to ignore all <mark> records
		     */
		    ignore_mark_gap = 0;
		    ignore_mark_records = 1;
		}
	    }
	    else
		/* do not ignore mark_records */
		ignore_mark_records = 0;
	    PM_UNLOCK(__pmLock_extcall);
	}
	if (ignore_mark_records && ignore_mark_gap == 0) {
	    if (pmDebugOptions.interp)
		fprintf(stderr, "update_bounds: ignore mark: unconditional\n");
	    return 0;
	}
	else if (ignore_mark_records) {
	    /*
	     * <prior>
	     * <mark>	<-- we've just read this one
	     * <next>
	     * Need to peek backwards and forwards to get the timestamp
	     * (don't care about anything else) from the <prior> and <next>
	     * records, but be careful to leave the file in the same state
	     * we found it wrt seek offset
	     */
	    double	t_next = -1;
	    double	t_prior = -1;
	    __pmResult	*peek;
	    int		sts;
	    char	errmsg[PM_MAXERRMSGLEN];
	    int		save_arch = 0;
	    int		save_vol = 0;
	    long	save_offset = 0;

	    /* Save the initial state. */
	    save_arch = ctxp->c_archctl->ac_cur_log;
	    save_vol = ctxp->c_archctl->ac_vol;
	    save_offset = ctxp->c_archctl->ac_offset;

	    /* <next> */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_FORW, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark read <next> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    	/*
		 * forward read failed, assume <mark> is at end of
		 * archive and seek pointer still correct
		 */
		goto check;
	    }
	    tmp = peek->timestamp;		/* stuct assignment */
	    t_next = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    __pmFreeResult(peek);

	    /* backup -> should be <next> record */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_BACK, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark unread <next> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		/* should not happen ... */
		goto restore;
	    }
	    __pmFreeResult(peek);

	    /* backup -> should be <mark> record */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_BACK, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark unread <mark> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		/* should not happen ... */
		goto restore;
	    }
	    if (peek->numpmid != 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark unread <mark> expecting <mark> got numpmid=%d\n", peek->numpmid);
	    }
	    __pmFreeResult(peek);

	    /* <prior> */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_BACK, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark read <prior> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    	/*
		 * backwards read failed, assume <mark> is at start of
		 * archive and seek pointer still correct
		 */
		goto check;
	    }
	    tmp = peek->timestamp;		/* struct assignment */
	    t_prior = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    __pmFreeResult(peek);

	    /* backup -> should be <prior> record */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_FORW, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark unread <prior> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		/* should not happen ... */
		goto restore;
	    }
	    __pmFreeResult(peek);

	    /* backup -> should be <mark> record */
	    sts = __pmLogRead_ctx(ctxp, PM_MODE_FORW, NULL, &peek, PMLOGREAD_NEXT);
	    if (sts < 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark reread <mark> failed: %s\n",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		/* should not happen ... */
		goto restore;
	    }
	    if (peek->numpmid != 0) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: mark reread <mark> expecting <mark> got numpmid=%d\n", peek->numpmid);
	    }
	    __pmFreeResult(peek);
	    goto check;

restore:
	    /* Restore the initial state. */
	    ctxp->c_archctl->ac_cur_log = save_arch;
	    ctxp->c_archctl->ac_vol = save_vol;
	    ctxp->c_archctl->ac_offset = save_offset;

check:
	    if (t_prior != -1 && t_next != -1 && t_next - t_prior <= ignore_mark_gap) {
		if (pmDebugOptions.interp)
		    fprintf(stderr, "update_bounds: ignore mark: gap %.6f - %.6f <= %.6f\n", t_next, t_prior, ignore_mark_gap);
		return 0;
	    }
	}
    }

    if (logrp->numpmid == 0) {
	/* mark record, discontinuity in log */
	if (do_mark == UPD_MARK_NONE)
	    return 0; /* ok */

	for (icp = (instcntl_t *)ctxp->c_archctl->ac_want; icp != NULL; icp = icp->want) {
	    if (t_this <= t_req &&
		(t_this >= icp->t_prior || icp->t_prior > t_req)) {
		/* <mark> is closer than best lower bound to date */
		icp->t_prior = t_this;
		SET_MARK(icp->s_prior);
		if (do_mark == UPD_MARK_BACK) {
		    if (icp->search && done != NULL) {
			/* stop searching for this one */
			icp->search = 0;
			(*done)++;
			/* don't need to scan again to this <mark> */
			SET_SCANNED(icp->s_prior);
		    }
		}
		else {
		    if (seen_mark != NULL)
			*seen_mark = 1;
		    if (pmDebugOptions.interp && pmDebugOptions.desperate) {
			fprintf(stderr, "<mark> rolling forwards\n");
		    }
		}

		if (icp->metric->valfmt != PM_VAL_INSITU) {
		    if (icp->v_prior.pval != NULL) {
			__pmUnpinPDUBuf((void *)icp->v_prior.pval);
			icp->v_prior.pval = NULL;
		    }
		}
		if (pmDebugOptions.interp)
		    dumpicp("update_bounds: mark@prior", icp);
	    }
	    if (t_this >= t_req &&
		((t_this <= icp->t_next || icp->t_next < 0) ||
		  icp->t_next < t_req)) {
		/* <mark> is closer than best upper bound to date */
		icp->t_next = t_this;
		SET_MARK(icp->s_next);
		if (do_mark == UPD_MARK_FORW) {
		    if (icp->search && done != NULL) {
			/* stop searching for this one */
			icp->search = 0;
			(*done)++;
			/* don't need to scan again to this <mark> */
			SET_SCANNED(icp->s_next);
		    }
		}
		else {
		    if (seen_mark != NULL)
			*seen_mark = 1;
		    if (pmDebugOptions.interp && pmDebugOptions.desperate) {
			fprintf(stderr, "<mark> rolling backwards\n");
		    }
		}

		if (icp->metric->valfmt != PM_VAL_INSITU) {
		    if (icp->v_next.pval != NULL) {
			__pmUnpinPDUBuf((void *)icp->v_next.pval);
			icp->v_next.pval = NULL;
		    }
		}
		if (pmDebugOptions.interp)
		    dumpicp("update_bounds: mark@next", icp);
	    }
	}
	return 0;
    }

    changed = 0;
    for (k = 0; k < logrp->numpmid; k++) {
	hp = __pmHashSearch((int)logrp->vset[k]->pmid, hcp);
	if (hp == NULL)
	    continue;
	pcp = (pmidcntl_t *)hp->data;
	if (pcp->valfmt == -1 && logrp->vset[k]->numval > 0) {
	    /*
	     * All the code assumes we have consistent encoding between
	     * the data type from the metadata and the valfmt in the
	     * pmResult ... if this is not the case then the archive is
	     * corrupted.
	     */
	    int		ok;
	    switch (pcp->desc.type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
			ok = (logrp->vset[k]->valfmt == PM_VAL_INSITU);
			break;
		case PM_TYPE_FLOAT:
			ok = (logrp->vset[k]->valfmt == PM_VAL_INSITU || logrp->vset[k]->valfmt == PM_VAL_DPTR || logrp->vset[k]->valfmt == PM_VAL_SPTR);
			break;
		default:
			ok = (logrp->vset[k]->valfmt == PM_VAL_DPTR || logrp->vset[k]->valfmt == PM_VAL_SPTR);
			break;
	    }
	    if (!ok) {
		if (pmDebugOptions.log) {
		    char	strbuf[20];
		    fprintf(stderr, "update_bounds: corrupted archive: PMID %s: valfmt %d",
			pmIDStr_r(logrp->vset[k]->pmid, strbuf, sizeof(strbuf)),
			logrp->vset[k]->valfmt);
		    fprintf(stderr, " unexpected for type %s\n",
			pmTypeStr_r(pcp->desc.type, strbuf, sizeof(strbuf)));
		}
		return PM_ERR_LOGREC;
	    }
	    pcp->valfmt = logrp->vset[k]->valfmt;
	}
	else if (pcp->valfmt != -1 && logrp->vset[k]->numval > 0 && pcp->valfmt != logrp->vset[k]->valfmt) {
	    /* bad archive ... value encoding in pmResult has changed */
	    if (pmDebugOptions.log) {
		char	strbuf[20];
		fprintf(stderr, "update_bounds: corrupted archive: PMID %s: valfmt changed from %d to %d\n",
		    pmIDStr_r(logrp->vset[k]->pmid, strbuf, sizeof(strbuf)),
		    pcp->valfmt, logrp->vset[k]->valfmt);
	    }
	    return PM_ERR_LOGREC;
	}
	for (i = 0; i < logrp->vset[k]->numval; i++) {
	    pmInDom vlistIndom = logrp->vset[k]->vlist[i].inst;

	    ihp = __pmHashSearch((int)vlistIndom, &pcp->hc);
	    if (ihp == NULL) {
		ihp = __pmHashSearch(PM_IN_NULL, &pcp->hc);
		if (ihp == NULL)
		    continue;
	    }
	    icp = (instcntl_t *)ihp->data;
	    
	    if (icp->inst == PM_IN_NULL)
		assert(i == 0);
	    if (pmDebugOptions.interp && pmDebugOptions.desperate)
		dumpicp("update_bounds: match", icp);

	    if (t_this <= t_req &&
		(icp->t_prior > t_req || t_this >= icp->t_prior)) {
		/*
		 * at or before the requested time, and this is the
		 * closest-to-date lower bound
		 */
		changed = 1;
		if (icp->t_prior < icp->t_next && icp->t_prior >= t_req) {
		    /* shuffle prior to next */
		    icp->t_next = icp->t_prior;
		    icp->s_next = icp->s_prior;
		    CLEAR_SCANNED(icp->s_next);
		    if (pcp->valfmt == PM_VAL_INSITU)
			icp->v_next.lval = icp->v_prior.lval;
		    else {
			if (icp->v_next.pval != NULL)
			    __pmUnpinPDUBuf((void *)icp->v_next.pval);
			icp->v_next.pval = icp->v_prior.pval;
		    }
		}
		icp->t_prior = t_this;
		SET_VALUE(icp->s_prior);
		if (pcp->valfmt == PM_VAL_INSITU)
		    icp->v_prior.lval = logrp->vset[k]->vlist[i].value.lval;
		else {
		    if (icp->v_prior.pval != NULL)
			__pmUnpinPDUBuf((void *)icp->v_prior.pval);
		    icp->v_prior.pval = logrp->vset[k]->vlist[i].value.pval;
		    __pmPinPDUBuf((void *)icp->v_prior.pval);
		}
		if (do_mark == UPD_MARK_BACK && icp->search && done != NULL) {
		    /* one we were looking for */
		    changed |= 2;
		    icp->search = 0;
		    (*done)++;
		    /* don't need to scan this region again */
		    SET_SCANNED(icp->s_prior);
		}
	    }
	    if (t_this >= t_req &&
		(icp->t_next < t_req || t_this <= icp->t_next)) {
		/*
		 * at or after the requested time, and this is the
		 * closest-to-date upper bound
		 */
		changed |= 1;
		if (icp->t_prior < icp->t_next && icp->t_next <= t_req) {
		    /* shuffle next to prior */
		    icp->t_prior = icp->t_next;
		    icp->s_prior = icp->s_next;
		    CLEAR_SCANNED(icp->s_prior);
		    if (pcp->valfmt == PM_VAL_INSITU)
			icp->v_prior.lval = icp->v_next.lval;
		    else {
			if (icp->v_prior.pval != NULL)
			    __pmUnpinPDUBuf((void *)icp->v_prior.pval);
			icp->v_prior.pval = icp->v_next.pval;
		    }
		}
		icp->t_next = t_this;
		SET_VALUE(icp->s_next);
		if (pcp->valfmt == PM_VAL_INSITU)
		    icp->v_next.lval = logrp->vset[k]->vlist[i].value.lval;
		else {
		    if (icp->v_next.pval != NULL)
			__pmUnpinPDUBuf((void *)icp->v_next.pval);
		    icp->v_next.pval = logrp->vset[k]->vlist[i].value.pval;
		    __pmPinPDUBuf((void *)icp->v_next.pval);
		}
		if (do_mark == UPD_MARK_FORW && icp->search && done != NULL) {
		    /* one we were looking for */
		    changed |= 2;
		    icp->search = 0;
		    (*done)++;
		    /* don't need to scan this region again */
		    SET_SCANNED(icp->s_next);
		}
	    }
	    if (pmDebugOptions.interp && changed) {
		if (changed & 2)
		    dumpicp("update_bounds: update+search", icp);
		else
		    dumpicp("update_bounds: update", icp);
	    }
	}
    }

    return 0;
}

static int
do_roll(__pmContext *ctxp, double t_req, int *seen_mark)
{
    __pmResult		*logrp;
    __pmTimestamp	tmp;
    double		t_this;
    int			err;
    int			sts;

    /*
     * now roll forwards in the direction of log reading
     * to make sure we are up to t_req
     */
    if (ctxp->c_direction > 0) {
	while ((err = cache_read(ctxp, PM_MODE_FORW, &logrp)) >= 0) {
	    tmp = logrp->timestamp;		/* struct assignment */
	    t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    if (t_this > t_req)
		break;

	    if (pmDebugOptions.interp)
		fprintf(stderr, "do_roll: forw to t=%.6f%s\n",
		    t_this, logrp->numpmid == 0 ? " <mark>" : "");
	    ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
	    assert(ctxp->c_archctl->ac_offset >= 0);
	    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
	    sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_FORW, NULL, seen_mark);
	    if (sts < 0)
		return sts;
	}
    }
    else {
	while ((err = cache_read(ctxp, PM_MODE_BACK, &logrp)) >= 0) {
	    tmp = logrp->timestamp;		/* struct assignment */
	    t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    if (t_this < t_req)
		break;

	    if (pmDebugOptions.interp)
		fprintf(stderr, "do_roll: back to t=%.6f%s\n",
		    t_this, logrp->numpmid == 0 ? " <mark>" : "");
	    ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
	    assert(ctxp->c_archctl->ac_offset >= 0);
	    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
	    sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_BACK, NULL, seen_mark);
	    if (sts < 0)
		return sts;
	}
    }

    if (err == PM_ERR_LOGREC) {
	if (pmDebugOptions.interp || pmDebugOptions.log) {
	    fprintf(stderr, "Error: corrupted archive '%s', vol %d\n",
		ctxp->c_archctl ->ac_log->name, ctxp->c_archctl->ac_curvol);
	}
    	return err;
    }

    return 0;
}


#define HASH_THRESHOLD 16

/*
 * use the instance domain metadata to set t_birth and t_death for an
 * instance if the instance domain ever has more than HASH_THRESHOLD
 * instances
 */
static void
time_caliper(__pmContext *ctxp, instcntl_t *icp)
{
    int			sts;
    int			j;
    double		t_indom;
    double		t_indom_prior;
    __pmLogCtl		*lcp;
    __pmHashNode	*hp;
    __pmHashNode	*ip;
    __pmHashNode	*jp;
    __pmLogTrimInDom	*indomp;
    __pmLogTrimInst	*instp;
    __pmLogInDom	*idp;

    /* default state, we know nothing */
    icp->t_birth = icp->t_death = -1;
    if (icp->metric->desc.indom == PM_INDOM_NULL) {
	/* no instance domain, nothing to see here */
	return;
    }

    lcp = ctxp->c_archctl->ac_log;
    if ((hp = __pmHashSearch((unsigned int)icp->metric->desc.indom, &lcp->trimindom)) == NULL) {
	/*
	 * assume first time for this indom ...
	 * - add indom to trimindom hash
	 * - walk all instances
	 *   + add to histinst hash if not already there
	 *   + update t_birth if this is the earliest observation for this
	 *     instance
	 * - at the end of each timestamped snapshot of the indom, update
	 *   t_death the instance is present now, but is not present at the
	 *   next (later) timestamp for this indom
	 */
	int	maxinst;
	if (pmDebugOptions.qa) {
	    char	strbuf[20];
	    fprintf(stderr, "time_caliper: build hashes for indom %s\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)));
	}
	if ((indomp = (__pmLogTrimInDom *)malloc(sizeof(__pmLogTrimInDom))) == NULL) {
	    pmNoMem("time_caliper.__pmLogTrimInDom", sizeof(__pmLogTrimInDom), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	indomp->hashinst.nodes = indomp->hashinst.hsize = 0;
	sts = __pmHashAdd((unsigned int)icp->metric->desc.indom, (void *)indomp, &lcp->trimindom);
	if (sts < 0) {
	    char	strbuf[20];
	    fprintf(stderr, "time_caliper: Botch: indom %s trimindom __pmHashAdd failed: %d\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)), sts);
	    free(indomp);
	    return;
	}
	if ((hp = __pmHashSearch((unsigned int)icp->metric->desc.indom, &lcp->trimindom)) == NULL) {
	    char	strbuf[20];
	    fprintf(stderr, "time_caliper: Botch: indom %s: trimindom __pmHashSearch failed\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)));
	    return;
	}
	if ((jp = __pmHashSearch((unsigned int)icp->metric->desc.indom, &lcp->hashindom)) == NULL) {
	    char	strbuf[20];
	    fprintf(stderr, "time_caliper: Botch: indom %s: hashindom __pmHashSearch failed\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)));
	    return;
	}
	/*
	 * only do the work of building the per-instance time limits
	 * for big indoms ...
	 */
	maxinst = 0;
	for (idp = (__pmLogInDom *)jp->data; idp != NULL; idp = idp->next) {
	    if (idp->numinst > maxinst) {
		maxinst = idp->numinst;
		if (maxinst >= HASH_THRESHOLD)
		    break;
	    }
	}
	if (maxinst < HASH_THRESHOLD)
	    return;

	if (pmDebugOptions.qa) {
	    char	strbuf[20];
	    fprintf(stderr, "time_caliper: indom %s maxinst=%d\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)), maxinst);
	}

	t_indom_prior = -1;
	for (idp = (__pmLogInDom *)jp->data; idp != NULL; idp = idp->next) {
	    t_indom = __pmTimestampSub(&idp->stamp, __pmLogStartTime(ctxp->c_archctl));
	    for (j = 0; j < idp->numinst; j++) {
		if ((ip = __pmHashSearch((unsigned int)idp->instlist[j], &indomp->hashinst)) == NULL) {
		    /*
		     * this instance has not been seen before (remember we
		     * are going backwards in time)
		     */
		    if ((instp = (__pmLogTrimInst *)malloc(sizeof(__pmLogTrimInst))) == NULL) {
			pmNoMem("time_caliper.__pmLogTrimInst", sizeof(__pmLogTrimInDom), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    instp->t_birth = t_indom;
		    instp->t_death = t_indom_prior;
		    sts = __pmHashAdd((unsigned int)idp->instlist[j], (void *)instp, &indomp->hashinst);
		    if (sts < 0) {
			char	strbuf[20];
			fprintf(stderr, "time_caliper: Botch: indom %s inst %d hashinst __pmHashAdd failed: %d\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)), idp->instlist[j], sts);
			free(instp);
			return;
		    }
		    if (__pmHashSearch((unsigned int)idp->instlist[j], &indomp->hashinst) == NULL) {
			char	strbuf[20];
			fprintf(stderr, "time_caliper: Botch: indom %s inst %d hashinst __pmHashSearch failed\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)), idp->instlist[j]);
			return;
		    }
		}
		else {
		    instp = (__pmLogTrimInst *)ip->data;
		    instp->t_birth = t_indom;
		}
	    }
	    t_indom_prior = t_indom;
	}
    }

    if (hp != NULL) {
	/* found indom, now for instance ... */
	indomp = (__pmLogTrimInDom *)hp->data;
	if ((ip = __pmHashSearch((unsigned int)icp->inst, &indomp->hashinst)) != NULL) {
	    instp = (__pmLogTrimInst *)ip->data;
	    icp->t_birth = instp->t_birth;
	    icp->t_death = instp->t_death;
	    if (pmDebugOptions.qa) {
		char	strbuf[20];
		fprintf(stderr, "time_caliper: indom %s and inst %d -> %.6f .. %.6f\n", pmInDomStr_r(icp->metric->desc.indom, strbuf, sizeof(strbuf)), icp->inst, icp->t_birth, icp->t_death);
	    }
	}
    }

    return;
}

/*
 * classes of "unbound" list instcntl_t scanning effort ...
 * counted in nuis[] below
 */
#define NUIS_PASS2 		0
#define NUIS_FIRST 		1
#define NUIS_FIRST_FORGET	2
#define NUIS_FIRST_TRIM		3
#define NUIS_PASS3		4
#define NUIS_LAST		5
#define NUIS_LAST_FORGET	6
#define NUIS_LAST_TRIM		7

int
__pmLogFetchInterp(__pmContext *ctxp, int numpmid, pmID pmidlist[], __pmResult **result)
{
    int			i, j, k, sts;
    double		t_req, t_this;
    __pmResult		*rp, *logrp;
    __pmHashCtl		*hcp = &ctxp->c_archctl->ac_pmid_hc;
    __pmHashNode	*hp, *ihp;
    pmidcntl_t		*pcp = NULL;	/* initialize to pander to gcc */
    instcntl_t		*icp = NULL;	/* initialize to pander to gcc */
    instcntl_t		*ub, *ub_prev;
    int			back = 0;
    int			forw = 0;
    int			done;
    int			done_roll;
    int			seen_mark;
    static int		dowrap = -1;
    __pmTimestamp	tmp;
    long		nuis[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			/* number of unbound items scanned */

    PM_LOCK(__pmLock_extcall);
    if (dowrap == -1) {
	/* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	if (getenv("PCP_COUNTER_WRAP") == NULL)		/* THREADSAFE */
	    dowrap = 0;
	else
	    dowrap = 1;
    }
    PM_UNLOCK(__pmLock_extcall);

    t_req = __pmTimestampSub(&ctxp->c_origin, __pmLogStartTime(ctxp->c_archctl));
    if (pmDebugOptions.interp) {
	fprintf(stderr, "__pmLogFetchInterp @ ");
	__pmPrintTimestamp(stderr, &ctxp->c_origin);
	fprintf(stderr, " t_req=%.6f curvol=%d posn=%ld (vol=%d) serial=%d\n",
	    t_req, ctxp->c_archctl->ac_curvol,
	    (long)ctxp->c_archctl->ac_offset, ctxp->c_archctl->ac_vol,
	    ctxp->c_archctl->ac_serial);
	nr_cache[PM_MODE_FORW] = nr[PM_MODE_FORW] = 0;
	nr_cache[PM_MODE_BACK] = nr[PM_MODE_BACK] = 0;
    }

    /*
     * the 0.001 is magic slop for 1 msec, which is about as accurate
     * as we can expect any of this timing stuff to be ...
     */
    if (t_req < -0.001) {
	sts = PM_ERR_EOL;
	goto all_done;
    }

    if (t_req > ctxp->c_archctl->ac_end + 0.001) {
	__pmTimestamp	end;
	/*
	 * Past end of the current archive ... see if it has grown since we
	 * last looked, or if we can switch to the next archive in the context.
	 */
	if (__pmGetArchiveEnd_ctx(ctxp, &end) >= 0) {
	    ctxp->c_archctl->ac_end = __pmTimestampSub(&end, __pmLogStartTime(ctxp->c_archctl));
	}
	/* Check if request time is past the end of this archive
	 * and there are no subsequent archives
	 */
	if ((t_req > ctxp->c_archctl->ac_end)
	    && (ctxp->c_archctl->ac_cur_log >= ctxp->c_archctl->ac_num_logs - 1)) {
	    /* No more archive data. */
	    sts = PM_ERR_EOL;
	    goto all_done;
	}
    }

    /*
     * first pass ... scan all metrics, establish which ones are in
     * the log, and which instances are being requested ... also build
     * the skeletal pmResult
     */
    ctxp->c_archctl->ac_want = NULL;
    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL)
	    continue;
	hp = __pmHashSearch((int)pmidlist[j], hcp);
	if (hp == NULL) {
	    /* first time we've been asked for this one in this context */
	    if ((pcp = (pmidcntl_t *)malloc(sizeof(pmidcntl_t))) == NULL) {
		pmNoMem("__pmLogFetchInterp.pmidcntl_t", sizeof(pmidcntl_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    pcp->valfmt = -1;
	    pcp->last_numval = -1;
	    __pmHashInit(&pcp->hc);
	    sts = __pmHashAdd((int)pmidlist[j], (void *)pcp, hcp);
	    if (sts < 0) {
		free(pcp);
		return sts;
	    }
	    sts = __pmLogLookupDesc(ctxp->c_archctl, pmidlist[j], &pcp->desc);
	    if (sts < 0)
		/* not in the archive */
		pcp->desc.type = -1;
	    else {
		/* enumerate all the instances from the domain underneath */
		int		*instlist = NULL;
		char		**namelist = NULL;
		int		hsts = 0;

		if (pcp->desc.indom == PM_INDOM_NULL) {
		    sts = 1;
		    if ((instlist = (int *)malloc(sizeof(int))) == NULL) {
			pmNoMem("__pmLogFetchInterp.instlist", sizeof(int), PM_FATAL_ERR);
		    }
		    instlist[0] = PM_IN_NULL;
		}
		else {
		    sts = pmGetInDomArchive_ctx(ctxp, pcp->desc.indom, &instlist, &namelist);
		    if (sts > 0) {
			/* Pre allocate enough space for the instance domain. */
			hsts = __pmHashPreAlloc(sts, &pcp->hc);
			if (hsts < 0) {
			    free(pcp);
			    goto done_icp;
			}
		    }
		}
		for (i = 0; i < sts; i++) {
		    if ((icp = (instcntl_t *)malloc(sizeof(instcntl_t))) == NULL) {
			pmNoMem("__pmLogFetchInterp.instcntl_t", sizeof(instcntl_t), PM_FATAL_ERR);
		    }
		    icp->metric = pcp;
		    icp->inst = instlist[i];
		    icp->t_first = icp->t_last = -1;
		    icp->t_prior = icp->t_next = -1;
		    SET_UNDEFINED(icp->s_prior);
		    SET_UNDEFINED(icp->s_next);
		    icp->v_prior.pval = icp->v_next.pval = NULL;
		    time_caliper(ctxp, icp);
		    hsts = __pmHashAdd((int)instlist[i], (void *)icp, &pcp->hc);
		    if (hsts < 0) {
			free(icp);
			goto done_icp;
		    }
		}
	    done_icp:
		if (instlist != NULL)
		    free(instlist);
		if (namelist != NULL)
		    free(namelist);
		if (hsts < 0)
		    return hsts; /* hash allocation error */
	    }
	}
	else
	    /* seen this one before */
	    pcp = (pmidcntl_t *)hp->data;

	pcp->numval = 0;
	if (pcp->desc.type == -1) {
	    pcp->numval = PM_ERR_PMID_LOG;
	}
	else if (pcp->desc.indom != PM_INDOM_NULL) {
	    /* use the profile to filter the instances to be returned */
	    for (i = 0; i < pcp->hc.hsize; i++) {
		for (ihp = pcp->hc.hash[i]; ihp != NULL; ihp = ihp->next) {
		    icp = (instcntl_t *)ihp->data;
		    icp->search = 0;
		    if (__pmInProfile(pcp->desc.indom, ctxp->c_instprof, icp->inst)) {
			icp->inresult = 1;
			icp->want = (instcntl_t *)ctxp->c_archctl->ac_want;
			ctxp->c_archctl->ac_want = icp;
			pcp->numval++;
		    }
		    else
			icp->inresult = 0;
		}
	    }
	}
	else {
	    /* There will be only one instance */
	    ihp = __pmHashWalk(&pcp->hc, PM_HASH_WALK_START);
	    assert(ihp);
	    icp = (instcntl_t *)ihp->data;
	    icp->inresult = 1;
	    icp->search = 0;
	    icp->want = (instcntl_t *)ctxp->c_archctl->ac_want;
	    ctxp->c_archctl->ac_want = icp;
	    pcp->numval = 1;
	    ihp = __pmHashWalk(&pcp->hc, PM_HASH_WALK_NEXT);
	    assert(!ihp);
	}
    }

    if (ctxp->c_archctl->ac_serial == 0) {
	/* need gross positioning from temporal index */
	__pmLogSetTime(ctxp);
	ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
	assert(ctxp->c_archctl->ac_offset >= 0);
	ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;

	/*
	 * and now fine-tuning ...
	 * back-up (relative to the direction we are reading the log)
	 * to make sure
	 */
	if (ctxp->c_direction > 0) {
	    while (cache_read(ctxp, PM_MODE_BACK, &logrp) >= 0) {
		tmp = logrp->timestamp;		/* struct assignment */
		t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
		if (t_this <= t_req) {
		    break;
		}
		ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
		assert(ctxp->c_archctl->ac_offset >= 0);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
		sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_NONE, NULL, NULL);
		if (sts < 0) {		
		    return sts;
		}
	    }
	}
	else {
	    while (cache_read(ctxp, PM_MODE_FORW, &logrp) >= 0) {
		tmp = logrp->timestamp;		/* struct assignment */
		t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
		if (t_this > t_req) {
		    break;
		}
		ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
		assert(ctxp->c_archctl->ac_offset >= 0);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
		sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_NONE, NULL, NULL);
		if (sts < 0) {
		    return sts;
		}
	    }
	}
	ctxp->c_archctl->ac_serial = 1;
	/*
	 * forget "previous" numval's
	 */
	for (j = 0; j < numpmid; j++) {
	    if (pmidlist[j] == PM_ID_NULL)
		continue;
	    hp = __pmHashSearch((int)pmidlist[j], hcp);
	    assert(hp != NULL);
	    pcp = (pmidcntl_t *)hp->data;
	    pcp->last_numval = -1;
	}
    }

    /* get to the last remembered place */
    sts = __pmLogChangeVol(ctxp->c_archctl, ctxp->c_archctl->ac_vol);
    if (sts < 0)
	goto all_done;
    __pmFseek(ctxp->c_archctl->ac_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);

    seen_mark = 0;	/* interested in <mark> records seen from here on */

    /*
     * optimization to supress roll forwards unless really needed ...
     * if the sample interval is much shorter than the time between log
     * records, then do not roll forwards unless some scanning is
     * required ... and if scanning is required in the "forwards"
     * direction, no need to roll forwards
     */
    done_roll = 0;

    /*
     * second pass ... see which metrics are not currently bounded below
     */
    ctxp->c_archctl->ac_unbound = NULL;
    for (icp = (instcntl_t *)ctxp->c_archctl->ac_want; icp != NULL; icp = icp->want) {
	assert(icp->inresult);
	nuis[NUIS_PASS2]++;
	if (icp->t_first >= 0 && t_req < icp->t_first)
	    /* before earliest observation, don't bother */
	    continue;
	if (icp->t_birth >= 0 && t_req < icp->t_birth)
	    /* from time_caliper(): before instance appears, don't bother */
	    continue;
	if (icp->t_death >= 0 && t_req > icp->t_death)
	    /* from time_caliper(): after instance vanishes, don't bother */
	    continue;
	if (icp->t_prior < 0 || icp->t_prior > t_req) {
	    if (back == 0 && !done_roll) {
		done_roll = 1;
		if (ctxp->c_direction > 0)  {
		    /* forwards before scanning back */
		    sts = do_roll(ctxp, t_req, &seen_mark);
		    if (sts < 0) {
			return sts;
		    }
		}
	    }
	}

	/*
	 *  At this stage there _may_ be a value earlier in the
	 *  archive of interest ...
	 *  s_prior undefined => have not explored in this direction,
	 *  	so need to go back (unless we've already scanned in
	 *  	this direction)
	 *  t_prior > t_req and reading backwards or not already
	 *  	scanned in this direction => need to push t_prior to
	 *  	be <= t_req if possible
	 *  t_next is mark and t_prior == t_req => search back
	 *  	to try and bound t_req with valid values
	 */
	if ((IS_UNDEFINED(icp->s_prior) && !IS_SCANNED(icp->s_prior)) ||
	    (icp->t_prior > t_req && (ctxp->c_direction < 0 || !IS_SCANNED(icp->s_prior))) ||
	    (IS_MARK(icp->s_next) && icp->t_prior == t_req)) {
	    back++;
	    icp->search = 1;
	    /* Add it to the unbound list in descending order of t_first */
	    ub_prev = NULL;
	    for (ub = (instcntl_t *)ctxp->c_archctl->ac_unbound;
		 ub != NULL;
		 ub = ub->unbound) {
		nuis[NUIS_FIRST]++;
		if (icp->t_first >= ub->t_first)
		    break;
		ub_prev = ub;
	    }
	    if (ub_prev)
		ub_prev->unbound = icp;
	    else
		ctxp->c_archctl->ac_unbound = icp;
	    icp->unbound = ub;
	    if (pmDebugOptions.interp)
		dumpicp("search back", icp);
	}
    }

    if (back) {
	/*
	 * at least one metric requires a bound from earlier in the log ...
	 * position ourselves, ... and search
	 */
	sts = __pmLogChangeVol(ctxp->c_archctl, ctxp->c_archctl->ac_vol);
	if (sts < 0)
	    goto all_done;
	__pmFseek(ctxp->c_archctl->ac_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);
	done = 0;

	while (done < back) {
	    if ((sts = cache_read(ctxp, PM_MODE_BACK, &logrp)) < 0) {
		if (sts == PM_ERR_LOGREC) {
		    if (pmDebugOptions.interp || pmDebugOptions.log) {
		        fprintf(stderr, "Error: corrupted archive scanning backwards in '%s', volume %d\n",
			    ctxp->c_archctl->ac_log->name, ctxp->c_archctl->ac_curvol);
		    }
		    return sts;
		}
		/* otherwise, we probably ran into start of log */
		if (pmDebugOptions.interp) {
		    fprintf(stderr, "Start of Log, %d metric-inst not found\n",
			    back - done);
		}
		break;
	    }
	    tmp = logrp->timestamp;		/* struct assignment */
	    t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    if (ctxp->c_direction < 0 && t_this >= t_req) {
		/* going backwards, and not up to t_req yet */
		ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
		assert(ctxp->c_archctl->ac_offset >= 0);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
	    }
	    sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_BACK, &done, &seen_mark);
	    if (sts < 0) {
		return sts;
	    }

	    /*
	     * Forget about those that can never be found from here
	     * in this direction. The unbound list is sorted in order of
	     * descending t_first. We can abandon the traversal once t_first is
	     * less than t_this. Trim the list as instances are resolved.
	     */
	    ub_prev = NULL;
	    for (icp = (instcntl_t *)ctxp->c_archctl->ac_unbound; icp != NULL; icp = icp->unbound) {
		nuis[NUIS_FIRST_FORGET]++;
		if (icp->t_first < t_this)
		    break;
		if (icp->search) {
		    icp->search = 0;
		    SET_SCANNED(icp->s_prior);
		    done++;
		    /* Remove this item from the list. */
		    if (ub_prev)
			ub_prev->unbound = icp->unbound;
		    else
			ctxp->c_archctl->ac_unbound = icp->unbound;
		}
		else
		    ub_prev = icp;
	    }
	}
	/* end of search, trim t_first as required */
	for (icp = (instcntl_t *)ctxp->c_archctl->ac_unbound; icp != NULL; icp = icp->unbound) {
	    nuis[NUIS_FIRST_TRIM]++;
	    if ((IS_UNDEFINED(icp->s_prior) || icp->t_prior > t_req) &&
		icp->t_first < t_req) {
		icp->t_first = t_req;
		SET_SCANNED(icp->s_prior);
		if (pmDebugOptions.interp)
		    dumpicp("no values before t_first", icp);
	    }
	    icp->search = 0;
	}
    }

    /*
     * third pass ... see which metrics are not currently bounded above
     */
    ctxp->c_archctl->ac_unbound = NULL;
    for (icp = (instcntl_t *)ctxp->c_archctl->ac_want; icp != NULL; icp = icp->want) {
	assert(icp->inresult);
	nuis[NUIS_PASS3]++;
	if (icp->t_last >= 0 && t_req > icp->t_last)
	    /* after latest observation, don't bother */
	    continue;
	if (icp->t_birth >= 0 && t_req < icp->t_birth)
	    /* from time_caliper(): before instance appears, don't bother */
	    continue;
	if (icp->t_death >= 0 && t_req > icp->t_death)
	    /* from time_caliper(): after instance vanishes, don't bother */
	    continue;
	if (icp->t_next < 0 || icp->t_next < t_req) {
	    if (forw == 0 && !done_roll) {
		done_roll = 1;
		if (ctxp->c_direction < 0)  {
		    /* backwards before scanning forwards */
		    sts = do_roll(ctxp, t_req, &seen_mark);
		    if (sts < 0) {
			return sts;
		    }
		}
	    }
	}

	/*
	 *  At this stage there _may_ be a value later in the
	 *  archive of interest ...
	 *  s_next undefined => have not explored in this direction,
	 *  	so need to go back (unless we've already scanned in
	 *  	this direction)
	 *  t_next < t_req and reading forwards or not already
	 *  	scanned in this direction => need to push t_next to
	 *  	be >= t_req if possible
	 *  t_prior is mark and t_next == t_req => search forward
	 *  	to try and bound t_req with valid values
	 */
	if ((IS_UNDEFINED(icp->s_next) && !IS_SCANNED(icp->s_next)) ||
	    (icp->t_next < t_req && (ctxp->c_direction > 0 || !IS_SCANNED(icp->s_next))) ||
	    (IS_MARK(icp->s_prior) && icp->t_next == t_req)) {
	    forw++;
	    icp->search = 1;

	    /* Add it to the unbound list in ascending order of t_last */
	    ub_prev = NULL;
	    for (ub = (instcntl_t *)ctxp->c_archctl->ac_unbound;
		 ub != NULL;
		 ub = ub->unbound) {
		nuis[NUIS_LAST]++;
		if (icp->t_last <= ub->t_last)
		    break;
		ub_prev = ub;
	    }
	    if (ub_prev)
		ub_prev->unbound = icp;
	    else
		ctxp->c_archctl->ac_unbound = icp;
	    icp->unbound = ub;
	    if (pmDebugOptions.interp)
		dumpicp("search forw", icp);
	}
    }

    if (forw) {
	/*
	 * at least one metric requires a bound from later in the log ...
	 * position ourselves ... and search
	 */
	sts = __pmLogChangeVol(ctxp->c_archctl, ctxp->c_archctl->ac_vol);
	if (sts < 0)
	    goto all_done;
	__pmFseek(ctxp->c_archctl->ac_mfp, ctxp->c_archctl->ac_offset, SEEK_SET);
	done = 0;

	while (done < forw) {
	    if ((sts = cache_read(ctxp, PM_MODE_FORW, &logrp)) < 0) {
		if (sts == PM_ERR_LOGREC) {
		    if (pmDebugOptions.interp || pmDebugOptions.log) {
		        fprintf(stderr, "Error: corrupted archive scanning forwards in '%s', volume %d\n",
			    ctxp->c_archctl->ac_log->name, ctxp->c_archctl->ac_curvol);
		    }
		    return sts;
		}
		/* otherwise we probably ran into end of log */
		if (pmDebugOptions.interp) {
		    fprintf(stderr, "End of Log, %d metric-insts not found\n",
		    		forw - done);
		}
		break;
	    }
	    tmp = logrp->timestamp;		/* struct assignment */
	    t_this = __pmTimestampSub(&tmp, __pmLogStartTime(ctxp->c_archctl));
	    if (ctxp->c_direction > 0 && t_this <= t_req) {
		/* going forwards, and not up to t_req yet */
		ctxp->c_archctl->ac_offset = __pmFtell(ctxp->c_archctl->ac_mfp);
		assert(ctxp->c_archctl->ac_offset >= 0);
		ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_curvol;
	    }
	    sts = update_bounds(ctxp, t_req, logrp, UPD_MARK_FORW, &done, &seen_mark);
	    if (sts < 0) {
		return sts;
	    }

	    /*
	     * Forget about those that can never be found from here
	     * in this direction. The unbound list is sorted in order of
	     * ascending t_last. We can abandon the traversal once t_last
	     * is greater than than t_this. Trim the list as instances are
	     * resolved.
	     */
	    ub_prev = NULL;
	    for (icp = (instcntl_t *)ctxp->c_archctl->ac_unbound; icp != NULL; icp = icp->unbound) {
		nuis[NUIS_LAST_FORGET]++;
		if (icp->t_last > t_this)
		    break;
		if (icp->search && icp->t_last >= 0) {
		    icp->search = 0;
		    SET_SCANNED(icp->s_next);
		    done++;
		    /* Remove this item from the list. */
		    if (ub_prev)
			ub_prev->unbound = icp->unbound;
		    else
			ctxp->c_archctl->ac_unbound = icp->unbound;
		}
		else
		    ub_prev = icp;
	    }
	}

	/* end of search, trim t_last as required */
	for (icp = (instcntl_t *)ctxp->c_archctl->ac_unbound; icp != NULL; icp = icp->unbound) {
	    nuis[NUIS_LAST_TRIM]++;
	    if (icp->t_next < t_req &&
		(icp->t_last < 0 || t_req < icp->t_last)) {
		icp->t_last = t_req;
		SET_SCANNED(icp->s_next);
		if (pmDebugOptions.interp) {
		    char	strbuf[20];
		    fprintf(stderr, "pmid %s inst %d no values after t_last=%.6f\n",
			    pmIDStr_r(icp->metric->desc.pmid, strbuf, sizeof(strbuf)), icp->inst, icp->t_last);
		}
	    }
	    icp->search = 0;
	}
    }

    /*
     * check to see how many qualifying values there are really going to be
     */
    for (icp = (instcntl_t *)ctxp->c_archctl->ac_want; icp != NULL; icp = icp->want) {
	assert(icp->inresult);
	pcp = (pmidcntl_t *)icp->metric;
	if (pcp->desc.sem == PM_SEM_DISCRETE) {
	    if (IS_MARK(icp->s_prior) || IS_UNDEFINED(icp->s_prior) ||
		icp->t_prior > t_req ||
		(icp->t_birth != -1 && icp->t_birth > t_req)) {
		/* no earlier value, so no value */
		pcp->numval--;
		icp->inresult = 0;
	    }
	    else if (icp->t_death != -1 && t_req > icp->t_death) {
		/* instance has gone away, so no value */
		pcp->numval--;
		icp->inresult = 0;
	    }
	}
	else {
	    /* assume COUNTER or INSTANT */
	    if (IS_MARK(icp->s_prior) || IS_UNDEFINED(icp->s_prior) ||
		icp->t_prior > t_req ||
		IS_MARK(icp->s_next) || IS_UNDEFINED(icp->s_next) || icp->t_next < t_req) {
		/* in mid-range, and no bound, so no value */
		pcp->numval--;
		icp->inresult = 0;
	    }
	    else if (pcp->desc.sem == PM_SEM_COUNTER) {
		/*
		 * for counters, has to be arithmetic also,
		 * else cannot interpolate ...
		 */
		if (pcp->desc.type != PM_TYPE_32 &&
		    pcp->desc.type != PM_TYPE_U32 &&
		    pcp->desc.type != PM_TYPE_64 &&
		    pcp->desc.type != PM_TYPE_U64 &&
		    pcp->desc.type != PM_TYPE_FLOAT &&
		    pcp->desc.type != PM_TYPE_DOUBLE)
		    pcp->numval = PM_ERR_TYPE;
		else if (seen_mark && pcp->last_numval > 0) {
		    /*
		     * Counter metric and immediately previous
		     * __pmLogFetchInterp() returned some values, but
		     * found a <mark> record scanning archive, return
		     * "no values" this time so PMAPI clients do not
		     * assume last inst-values and this inst-values are
		     * part of a continuous data stream, so for example
		     * rate conversion should not happen.
		     */
		    pcp->numval--;
		    icp->inresult = 0;
		}
	    }
	}
    }

    /* Build the final result. */
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_CALL);
    if ((rp = __pmAllocResult(numpmid)) == NULL) {
	return -oserror();
    }
    rp->timestamp = ctxp->c_origin;
    rp->numpmid = numpmid;

    for (j = 0; j < numpmid; j++) {
	if (pmidlist[j] == PM_ID_NULL) {
	    rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) -
					    sizeof(pmValue));
	}
	else {
	    hp = __pmHashSearch((int)pmidlist[j], hcp);
	    assert(hp != NULL);
	    pcp = (pmidcntl_t *)hp->data;

	    if (pcp->numval >= 1)
		rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) +
						(pcp->numval - 1)*sizeof(pmValue));
	    else
		rp->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet) -
						sizeof(pmValue));
	}

	if (rp->vset[j] == NULL) {
	    pmNoMem("__pmLogFetchInterp.vset", sizeof(pmValueSet), PM_FATAL_ERR);
	}

	rp->vset[j]->pmid = pmidlist[j];
	if (pmidlist[j] == PM_ID_NULL) {
	    rp->vset[j]->numval = 0;
	    continue;
	}
	rp->vset[j]->numval = pcp->numval;
	rp->vset[j]->valfmt = pcp->valfmt;

	i = 0;
	if (pcp->numval > 0) {
	    for (k = 0; k < pcp->hc.hsize; k++) {
		for (ihp = pcp->hc.hash[k]; ihp != NULL; ihp = ihp->next) {
		    icp = (instcntl_t *)ihp->data;
		    if (!icp->inresult)
			continue;
		    if (pmDebugOptions.interp && done_roll) {
			char	strbuf[20];
			fprintf(stderr, "pmid %s inst %d prior: t=%.6f",
				pmIDStr_r(pmidlist[j], strbuf, sizeof(strbuf)), icp->inst, icp->t_prior);
			dumpval(stderr, pcp->desc.type, icp->metric->valfmt, 1, icp);
			fprintf(stderr, " next: t=%.6f", icp->t_next);
			dumpval(stderr, pcp->desc.type, icp->metric->valfmt, 0, icp);
			fprintf(stderr, " t_first=%.6f t_last=%.6f\n",
				icp->t_first, icp->t_last);
		    }
		    rp->vset[j]->vlist[i].inst = icp->inst;
		    if (pcp->desc.type == PM_TYPE_32 || pcp->desc.type == PM_TYPE_U32) {
			if (icp->t_prior == t_req)
			    rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			else if (icp->t_next == t_req)
			    rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
			else {
			    if (pcp->desc.sem == PM_SEM_DISCRETE) {
				if (icp->t_prior >= 0) {
				    if (!IS_VALUE(icp->s_next) || t_req <= (icp->t_prior + icp->t_next)/2)
					rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
				    else
					rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
				}
			    }
			    else if (pcp->desc.sem == PM_SEM_INSTANT) {
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (t_req <= (icp->t_prior + icp->t_next)/2)
					rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
				    else
					rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
				}
			    }
			    else {
				/* assume COUNTER */
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (pcp->desc.type == PM_TYPE_32) {
					if (icp->v_next.lval >= icp->v_prior.lval ||
					    dowrap == 0) {
					    rp->vset[j]->vlist[i++].value.lval = 0.5 +
						icp->v_prior.lval + (t_req - icp->t_prior) *
						(icp->v_next.lval - icp->v_prior.lval) /
						(icp->t_next - icp->t_prior);
					}
					else {
					    /* not monotonic increasing and want wrap */
					    rp->vset[j]->vlist[i++].value.lval = 0.5 +
						(t_req - icp->t_prior) *
						(__int32_t)(UINT_MAX - icp->v_prior.lval + 1 + icp->v_next.lval) /
						(icp->t_next - icp->t_prior);
					    rp->vset[j]->vlist[i].value.lval += icp->v_prior.lval;
					}
				    }
				    else {
					pmAtomValue     av;
					pmAtomValue     *avp_prior = (pmAtomValue *)&icp->v_prior.lval;
					pmAtomValue     *avp_next = (pmAtomValue *)&icp->v_next.lval;
					if (avp_next->ul >= avp_prior->ul) {
					    av.ul = 0.5 + avp_prior->ul +
						(t_req - icp->t_prior) *
						(avp_next->ul - avp_prior->ul) /
						(icp->t_next - icp->t_prior);
					}
					else {
					    /* not monotonic increasing */
					    if (dowrap) {
						av.ul = 0.5 +
						    (t_req - icp->t_prior) *
						    (__uint32_t)(UINT_MAX - avp_prior->ul + 1 + avp_next->ul ) /
						    (icp->t_next - icp->t_prior);
						av.ul += avp_prior->ul;
					    }
					    else {
						__uint32_t	ul;
						ul = avp_prior->ul - avp_next->ul;
						av.ul = 0.5 + avp_prior->ul -
						    (t_req - icp->t_prior) * ul /
						    (icp->t_next - icp->t_prior);
					    }
					}
					rp->vset[j]->vlist[i++].value.lval = av.ul;
				    }
				}
			    }
			}
		    }
		    else if (pcp->desc.type == PM_TYPE_FLOAT && icp->metric->valfmt == PM_VAL_INSITU) {
			/* OLD style FLOAT insitu */
			if (icp->t_prior == t_req)
			    rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
			else if (icp->t_next == t_req)
			    rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
			else {
			    if (pcp->desc.sem == PM_SEM_DISCRETE) {
				if (icp->t_prior >= 0) {
				    if (!IS_VALUE(icp->s_next) || t_req <= (icp->t_prior + icp->t_next)/2)
					rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
				    else
					rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
				}
			    }
			    else if (pcp->desc.sem == PM_SEM_INSTANT) {
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (t_req <= (icp->t_prior + icp->t_next)/2)
					rp->vset[j]->vlist[i++].value.lval = icp->v_prior.lval;
				    else
					rp->vset[j]->vlist[i++].value.lval = icp->v_next.lval;
				}
			    }
			    else {
				/* assume COUNTER */
				pmAtomValue	av;
				pmAtomValue	*avp_prior = (pmAtomValue *)&icp->v_prior.lval;
				pmAtomValue	*avp_next = (pmAtomValue *)&icp->v_next.lval;
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    av.f = avp_prior->f + (t_req - icp->t_prior) *
					(avp_next->f - avp_prior->f) /
					(icp->t_next - icp->t_prior);
				    /* yes this IS correct ... */
				    rp->vset[j]->vlist[i++].value.lval = av.l;
				}
			    }
			}
		    }
		    else if (pcp->desc.type == PM_TYPE_FLOAT) {
			/* NEW style FLOAT in pmValueBlock */
			int			need;
			pmValueBlock	*vp;
			int			ok = 1;

			need = PM_VAL_HDR_SIZE + sizeof(float);
			if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			    sts = -oserror();
			    goto bad_alloc;
			}
			vp->vlen = need;
			vp->vtype = PM_TYPE_FLOAT;
			rp->vset[j]->valfmt = PM_VAL_DPTR;
			rp->vset[j]->vlist[i++].value.pval = vp;
			if (icp->t_prior == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
			else if (icp->t_next == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(float));
			else {
			    if (pcp->desc.sem == PM_SEM_DISCRETE) {
				if (icp->t_prior >= 0) {
				    if (!IS_VALUE(icp->s_next) || t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(float));
				}
				else
				    ok = 0;
			    }
			    else if (pcp->desc.sem == PM_SEM_INSTANT) {
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(float));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(float));
				}
				else
				    ok = 0;
			    }
			    else {
				/* assume COUNTER */
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    pmAtomValue	av;
				    void		*avp_prior = icp->v_prior.pval->vbuf;
				    void		*avp_next = icp->v_next.pval->vbuf;
				    float	f_prior;
				    float	f_next;

				    memcpy((void *)&av.f, avp_prior, sizeof(av.f));
				    f_prior = av.f;
				    memcpy((void *)&av.f, avp_next, sizeof(av.f));
				    f_next = av.f;
				    
				    av.f = f_prior + (t_req - icp->t_prior) *
					(f_next - f_prior) /
					(icp->t_next - icp->t_prior);
				    memcpy((void *)vp->vbuf, (void *)&av.f, sizeof(av.f));
				}
				else
				    ok = 0;
			    }
			}
			if (!ok) {
			    i--;
			    free(vp);
			}
		    }
		    else if (pcp->desc.type == PM_TYPE_64 || pcp->desc.type == PM_TYPE_U64) {
			int			need;
			pmValueBlock	*vp;
			int			ok = 1;
			
			need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
			if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			    sts = -oserror();
			    goto bad_alloc;
			}
			vp->vlen = need;
			if (pcp->desc.type == PM_TYPE_64)
			    vp->vtype = PM_TYPE_64;
			else
			    vp->vtype = PM_TYPE_U64;
			rp->vset[j]->valfmt = PM_VAL_DPTR;
			rp->vset[j]->vlist[i++].value.pval = vp;
			if (icp->t_prior == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
			else if (icp->t_next == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(__int64_t));
			else {
			    if (pcp->desc.sem == PM_SEM_DISCRETE) {
				if (icp->t_prior >= 0) {
				    if (!IS_VALUE(icp->s_next) || t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(__int64_t));
				}
				else
				    ok = 0;
			    }
			    else if (pcp->desc.sem == PM_SEM_INSTANT) {
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(__int64_t));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(__int64_t));
				}
				else
				    ok = 0;
			    }
			    else {
				/* assume COUNTER */
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    pmAtomValue	av;
				    void		*avp_prior = (void *)icp->v_prior.pval->vbuf;
				    void		*avp_next = (void *)icp->v_next.pval->vbuf;
				    if (pcp->desc.type == PM_TYPE_64) {
					__int64_t	ll_prior;
					__int64_t	ll_next;
					memcpy((void *)&av.ll, avp_prior, sizeof(av.ll));
					ll_prior = av.ll;
					memcpy((void *)&av.ll, avp_next, sizeof(av.ll));
					ll_next = av.ll;
					if (ll_next >= ll_prior || dowrap == 0)
					    av.ll = ll_next - ll_prior;
					else
					    /* not monotonic increasing and want wrap */
					    av.ll = (__int64_t)(ULONGLONG_MAX - ll_prior + 1 +  ll_next);
					av.ll = (__int64_t)(0.5 + (double)ll_prior +
							    (t_req - icp->t_prior) * (double)av.ll / (icp->t_next - icp->t_prior));
					memcpy((void *)vp->vbuf, (void *)&av.ll, sizeof(av.ll));
				    }
				    else {
					__int64_t	ull_prior;
					__int64_t	ull_next;
					memcpy((void *)&av.ull, avp_prior, sizeof(av.ull));
					ull_prior = av.ull;
					memcpy((void *)&av.ull, avp_next, sizeof(av.ull));
					ull_next = av.ull;
					if (ull_next >= ull_prior) {
					    av.ull = ull_next - ull_prior;
#if !defined(HAVE_CAST_U64_DOUBLE)
					    {
						double tmp;
						
						if (SIGN_64_MASK & av.ull)
						    tmp = (double)(__int64_t)(av.ull & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
						else
						    tmp = (double)(__int64_t)av.ull;
						
						av.ull = (__uint64_t)(0.5 + (double)ull_prior +
								      (t_req - icp->t_prior) * tmp /
								      (icp->t_next - icp->t_prior));
					    }
#else
					    av.ull = (__uint64_t)(0.5 + (double)ull_prior +
								  (t_req - icp->t_prior) * (double)av.ull /
								  (icp->t_next - icp->t_prior));
#endif
					}
					else {
					    /* not monotonic increasing */
					    if (dowrap) {
						av.ull = ULONGLONG_MAX - ull_prior + 1 +
						    ull_next;
#if !defined(HAVE_CAST_U64_DOUBLE)
						{
						    double tmp;
						    
						    if (SIGN_64_MASK & av.ull)
							tmp = (double)(__int64_t)(av.ull & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
						    else
							tmp = (double)(__int64_t)av.ull;
						    
						    av.ull = (__uint64_t)(0.5 + (double)ull_prior +
									  (t_req - icp->t_prior) * tmp /
									  (icp->t_next - icp->t_prior));
						}
#else
						av.ull = (__uint64_t)(0.5 + (double)ull_prior +
								      (t_req - icp->t_prior) * (double)av.ull /
								      (icp->t_next - icp->t_prior));
#endif
					    }
					    else {
						__uint64_t	ull;
						ull = ull_prior - ull_next;
#if !defined(HAVE_CAST_U64_DOUBLE)
						{
						    double xull;
						    
						    if (SIGN_64_MASK & av.ull)
							xull = (double)(__int64_t)(ull & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
						    else
							xull = (double)(__int64_t)ull;
						    
						    av.ull = (__uint64_t)(0.5 + (double)ull_prior -
									  (t_req - icp->t_prior) * xull /
									  (icp->t_next - icp->t_prior));
						}
#else
						av.ull = (__uint64_t)(0.5 + (double)ull_prior -
								      (t_req - icp->t_prior) * (double)ull /
								      (icp->t_next - icp->t_prior));
#endif
					    }
					}
					memcpy((void *)vp->vbuf, (void *)&av.ull, sizeof(av.ull));
				    }
				}
				else
				    ok = 0;
			    }
			}
			if (!ok) {
			    i--;
			    free(vp);
			}
		    }
		    else if (pcp->desc.type == PM_TYPE_DOUBLE) {
			pmValueBlock	*vp;
			int		need;
			int		ok = 1;
			
			need = PM_VAL_HDR_SIZE + sizeof(double);
			if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			    sts = -oserror();
			    goto bad_alloc;
			}
			vp->vlen = need;
			vp->vtype = PM_TYPE_DOUBLE;
			rp->vset[j]->valfmt = PM_VAL_DPTR;
			rp->vset[j]->vlist[i++].value.pval = vp;
			if (icp->t_prior == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
			else if (icp->t_next == t_req)
			    memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(double));
			else {
			    if (pcp->desc.sem == PM_SEM_DISCRETE) {
				if (icp->t_prior >= 0) {
				    if (!IS_VALUE(icp->s_next) || t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(double));
				}
				else
				    ok = 0;
			    }
			    else if (pcp->desc.sem == PM_SEM_INSTANT) {
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    if (t_req <= (icp->t_prior + icp->t_next)/2)
					memcpy((void *)vp->vbuf, (void *)icp->v_prior.pval->vbuf, sizeof(double));
				    else
					memcpy((void *)vp->vbuf, (void *)icp->v_next.pval->vbuf, sizeof(double));
				}
				else
				    ok = 0;
			    }
			    else {
				/* assume COUNTER */
				if (icp->t_prior >= 0 && icp->t_next >= 0) {
				    pmAtomValue	av;
				    void	*avp_prior = (void *)icp->v_prior.pval->vbuf;
				    void	*avp_next = (void *)icp->v_next.pval->vbuf;
				    double	d_prior;
				    double	d_next;
				    memcpy((void *)&av.d, avp_prior, sizeof(av.d));
				    d_prior = av.d;
				    memcpy((void *)&av.d, avp_next, sizeof(av.d));
				    d_next = av.d;
				    av.d = d_prior + (t_req - icp->t_prior) *
					(d_next - d_prior) /
					(icp->t_next - icp->t_prior);
				    memcpy((void *)vp->vbuf, (void *)&av.d, sizeof(av.d));
				}
				else
				    ok = 0;
			    }
			}
			if (!ok) {
			    i--;
			    free(vp);
			}
		    }
		    else if ((pcp->desc.type == PM_TYPE_AGGREGATE ||
			      pcp->desc.type == PM_TYPE_EVENT ||
			      pcp->desc.type == PM_TYPE_HIGHRES_EVENT ||
			      pcp->desc.type == PM_TYPE_STRING) &&
			     icp->t_prior >= 0) {
			int		need;
			pmValueBlock	*vp;
			
			need = icp->v_prior.pval->vlen;
			
			vp = (pmValueBlock *)malloc(need);
			if (vp == NULL) {
			    sts = -oserror();
			    goto bad_alloc;
			}
			rp->vset[j]->valfmt = PM_VAL_DPTR;
			rp->vset[j]->vlist[i++].value.pval = vp;
			memcpy((void *)vp, icp->v_prior.pval, need);
		    }
		    else {
			/* unknown type - skip it, else junk in result */
			i--;
		    }
		}
	    }
	}
	pcp->last_numval = pcp->numval;
    }

    *result = rp;
    sts = 0;

all_done:
    ctxp->c_origin.sec += ctxp->c_delta.sec;
    ctxp->c_origin.nsec += ctxp->c_delta.nsec;
    while (ctxp->c_origin.nsec > 1000000000) {
	ctxp->c_origin.sec++;
	ctxp->c_origin.nsec -= 1000000000;
    }
    while (ctxp->c_origin.nsec < 0) {
	ctxp->c_origin.sec--;
	ctxp->c_origin.nsec += 1000000000;
    }

    if (pmDebugOptions.interp) {
	fprintf(stderr, "__pmLogFetchInterp: log reads: forward %ld",
	    nr[PM_MODE_FORW]);
	if (nr_cache[PM_MODE_FORW])
	    fprintf(stderr, " (+%ld cached)", nr_cache[PM_MODE_FORW]);
	fprintf(stderr, " backwards %ld",
	    nr[PM_MODE_BACK]);
	if (nr_cache[PM_MODE_BACK])
	    fprintf(stderr, " (+%ld cached)", nr_cache[PM_MODE_BACK]);
	fprintf(stderr, "\n");
    }
    if (pmDebugOptions.qa) {
	fprintf(stderr, "__pmLogFetchInterp: unbound items scanned:");
	fprintf(stderr, " pass2: %ld", nuis[NUIS_PASS2]);
	fprintf(stderr, " first: %ld", nuis[NUIS_FIRST]);
	fprintf(stderr, " forget: %ld", nuis[NUIS_FIRST_FORGET]);
	fprintf(stderr, " trim: %ld", nuis[NUIS_FIRST_TRIM]);
	fprintf(stderr, " pass3: %ld", nuis[NUIS_PASS3]);
	fprintf(stderr, " last: %ld", nuis[NUIS_LAST]);
	fprintf(stderr, " forget: %ld", nuis[NUIS_LAST_FORGET]);
	fprintf(stderr, " trim: %ld\n", nuis[NUIS_LAST_TRIM]);
    }

    return sts;

bad_alloc:
    /*
     * leaks a little (vlist[] stuff) ... but does not really matter at
     * this point, chance of anything good happening from here on are
     * pretty remote
     */
    rp->vset[j]->numval = i;
    while (++j < numpmid)
	rp->vset[j]->numval = 0;
    __pmFreeResult(rp);

    return sts;

}

void
__pmLogResetInterp(__pmContext *ctxp)
{
    __pmHashCtl	*hcp = &ctxp->c_archctl->ac_pmid_hc;
    double	t_req;
    __pmHashNode	*hp;
    __pmHashNode	*ihp;
    int		i, k;
    pmidcntl_t	*pcp;
    instcntl_t	*icp;

    if (hcp->hsize == 0)
	return;

    t_req = __pmTimestampSub(&ctxp->c_origin, __pmLogStartTime(ctxp->c_archctl));
    for (k = 0; k < hcp->hsize; k++) {
	for (hp = hcp->hash[k]; hp != NULL; hp = hp->next) {
	    pcp = (pmidcntl_t *)hp->data;
	    for (i = 0; i < pcp->hc.hsize; i++) {
		for (ihp = pcp->hc.hash[i]; ihp != NULL; ihp = ihp->next) {
		    icp = (instcntl_t *)ihp->data;
		    if (icp->t_prior > t_req || icp->t_next < t_req) {
			icp->t_prior = icp->t_next = -1;
			SET_UNDEFINED(icp->s_prior);
			SET_UNDEFINED(icp->s_next);
			if (pcp->valfmt != PM_VAL_INSITU) {
			    if (icp->v_prior.pval != NULL)
				__pmUnpinPDUBuf((void *)icp->v_prior.pval);
			    if (icp->v_next.pval != NULL)
				__pmUnpinPDUBuf((void *)icp->v_next.pval);
			}
			icp->v_prior.pval = icp->v_next.pval = NULL;
		    }
		}
	    }
	}
    }
}

/*
 * Free interp data when context is closed ...
 * - pinned PDU buffers holding values used for interpolation
 * - hash structures for finding metrics and instances
 * - read_cache contents
 *
 * Called with ctxp->c_lock held.
 */
void
__pmFreeInterpData(__pmContext *ctxp)
{
    if (ctxp->c_archctl->ac_pmid_hc.hsize > 0) {
	/* we have done some interpolation ... */
	__pmHashCtl	*hcp = &ctxp->c_archctl->ac_pmid_hc;
	__pmHashNode	*hp;
	__pmHashNode	*ihp;
	pmidcntl_t	*pcp;
	instcntl_t	*icp;
	int		i, j;

	for (j = 0; j < hcp->hsize; j++) {
	    __pmHashNode	*last_hp = NULL;
	    /*
	     * Don't free __pmHashNode until hp->next has been traversed,
	     * hence free lags one node in the chain (last_hp used for free).
	     * Same for linked list of instcntl_t structs (use last_ihp
	     * for free in this case).
	     */
	    for (hp = hcp->hash[j]; hp != NULL; hp = hp->next) {
		pcp = (pmidcntl_t *)hp->data;
		for (i = 0; i < pcp->hc.hsize; i++) {
		    __pmHashNode	*last_ihp = NULL;
		    for (ihp = pcp->hc.hash[i]; ihp != NULL; ihp = ihp->next) {
			icp = (instcntl_t *)ihp->data;
			if (pcp->valfmt != PM_VAL_INSITU) {
			    /*
			     * Held values may be in PDU buffers, unpin the PDU
			     * buffers just in case (__pmUnpinPDUBuf is a NOP if
			     * the value is not in a PDU buffer)
			     */
			    if (icp->v_prior.pval != NULL) {
				if (pmDebugOptions.interp && pmDebugOptions.desperate) {
				    char	strbuf[20];
				    fprintf(stderr, "release pmid %s inst %d prior\n",
					    pmIDStr_r(pcp->desc.pmid, strbuf, sizeof(strbuf)), icp->inst);
				}
				__pmUnpinPDUBuf((void *)icp->v_prior.pval);
			    }
			    if (icp->v_next.pval != NULL) {
				if (pmDebugOptions.interp && pmDebugOptions.desperate) {
				    char	strbuf[20];
				    fprintf(stderr, "release pmid %s inst %d next\n",
					    pmIDStr_r(pcp->desc.pmid, strbuf, sizeof(strbuf)), icp->inst);
				}
				__pmUnpinPDUBuf((void *)icp->v_next.pval);
			    }
			}
			if (last_ihp != NULL) {
			    if (last_ihp->data != NULL)
				free(last_ihp->data);
			    free(last_ihp);
			}
			last_ihp = ihp;
		    }
		    if (last_ihp != NULL) {
			if (last_ihp->data != NULL)
			    free(last_ihp->data);
			free(last_ihp);
		    }
		}
		if (pcp->hc.hash) {
		    free(pcp->hc.hash);
		    /* just being paranoid here */
		    pcp->hc.hash = NULL;
		}
		pcp->hc.hsize = 0;
		if (last_hp != NULL) {
		    if (last_hp->data != NULL)
			free(last_hp->data);
		    free(last_hp);
		}
		last_hp = hp;
	    }
	    if (last_hp != NULL) {
		if (last_hp->data != NULL)
		    free(last_hp->data);
		free(last_hp);
	    }
	}
	if (hcp->hash) {
	    free(hcp->hash);
	    /* just being paranoid here */
	    hcp->hash = NULL;
	}
	hcp->hsize = 0;
    }

    if (ctxp->c_archctl->ac_cache != NULL) {
	/* read cache allocated, work to be done */
	cache_t		*cache = (cache_t *)ctxp->c_archctl->ac_cache;
	cache_t		*cp;

	for (cp = cache; cp < &cache[NUMCACHE]; cp++) {
	    if (pmDebugOptions.log && pmDebugOptions.interp) {
		fprintf(stderr, "read cache entry "
			PRINTF_P_PFX "%p: c_name=%s rp="
			PRINTF_P_PFX "%p\n",
			cp, cp->c_name ? cp->c_name : "(none)",
			cp->rp);
	    }
	    if (cp->c_name != NULL) {
		free(cp->c_name);
		cp->c_name = NULL;
	    }
	    if (cp->rp != NULL) {
		__pmFreeResult(cp->rp);
		cp->rp = NULL;
	    }
	    cp->used = 0;
	}
    }
}
