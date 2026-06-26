/*
 * Copyright (c) 2017-2022 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "import.h"
#include "private.h"
#include "archive.h"

static __pmTimestamp	stamp;

/*
 * Transition a CONTEXT_APPEND context to CONTEXT_ACTIVE by delegating
 * all archive-format knowledge to __pmLogOpenAppend() in libpcp.
 * After return the file handles in logctl/archctl are open and positioned
 * at end-of-file; the caller's pmi_context fields are updated from the
 * restored archive label and last timestamp.
 */
static int
check_context_append(pmi_context *current)
{
    __pmLogCtl	*lcp = &current->logctl;
    __pmArchCtl	*acp = &current->archctl;
    int		sts;

    sts = __pmLogOpenAppend(current->archive, acp);
    if (sts < 0)
	return sts;

    /*
     * Load all existing metric and instance-domain descriptors from .meta
     * into the hash tables (hashpmid, hashindom).  check_metric() will use
     * __pmLogLookupDesc() to skip re-writing descriptors that are already
     * present, preventing .meta from growing on every timer-driven invocation
     * of a short-lived collector that runs on a timer.
     *
     * __pmLogOpenAppend() left mdfp positioned at end-of-file; rewind to the
     * start so __pmLogLoadMeta() can read from the beginning of the file, then
     * seek back to the end ready for subsequent appended writes.
     */
    __pmFseek(lcp->mdfp, 0, SEEK_SET);
    if ((sts = __pmLogLoadMeta(acp)) < 0) {
	__pmLogClose(acp);
	return sts;
    }
    __pmFseek(lcp->mdfp, 0, SEEK_END);

    /* Restore version from the archive label (may differ from the default) */
    current->version = __pmLogVersion(lcp);

    /* Use the archive's hostname/timezone/zoneinfo unless the caller overrode them */
    if (current->hostname == NULL && lcp->label.hostname != NULL)
	current->hostname = strdup(lcp->label.hostname);
    if (current->timezone == NULL && lcp->label.timezone != NULL)
	current->timezone = strdup(lcp->label.timezone);
    if (current->zoneinfo == NULL && lcp->label.zoneinfo != NULL)
	current->zoneinfo = strdup(lcp->label.zoneinfo);

    /*
     * Seed last_stamp from the archive's end time so that the monotonicity
     * check in _pmi_write() rejects out-of-order timestamps if a caller
     * tries to write before the archive's existing last timestamp.
     *
     * Do NOT update 'stamp' here: _pmi_put_result() has already set it
     * from result->timestamp (the correct current-write time) before
     * calling check_context_start().  Overwriting stamp with lcp->endtime
     * (the previous session's last timestamp) would produce a stale
     * temporal index entry for the first record of this append session.
     */
    current->last_stamp = lcp->endtime;

    current->state = CONTEXT_ACTIVE;
    return 0;
}

static int
check_context_start(pmi_context *current)
{
    const char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmLogCtl	*lcp;
    __pmArchCtl	*acp;
    int		sts;

    if (current->state == CONTEXT_APPEND) {
	sts = check_context_append(current);
	if (sts != -ENOENT)
	    return sts;
	/*
	 * Archive doesn't exist yet: fall through to create it normally.
	 * This makes PMI_APPEND safe to use unconditionally on first invocation
	 * of a timer-driven collector before any archive exists.
	 */
	current->state = CONTEXT_START;
    }

    if (current->state != CONTEXT_START)
	return 0; /* ok */

    if (current->hostname == NULL) {
	(void)gethostname(myname, MAXHOSTNAMELEN);
	myname[MAXHOSTNAMELEN-1] = '\0';
	host = myname;
    }
    else
	host = current->hostname;

    acp = __pmLogWriterInit(&current->archctl, &current->logctl);
    sts = __pmLogCreate(host, current->archive, current->version, acp, 0);
    if (sts < 0)
	return sts;

    lcp = &current->logctl;
    if (current->timezone != NULL) {
	free(lcp->label.timezone);
	lcp->label.timezone = strdup(current->timezone);
    }
    if (current->zoneinfo != NULL) {
	free(lcp->label.zoneinfo);
	lcp->label.zoneinfo = strdup(current->zoneinfo);
    }
    pmNewZone(lcp->label.timezone);
    current->state = CONTEXT_ACTIVE;

    /*
     * Do the archive label records (it is too late when __pmLogPutResult
     * or __pmLogPutResult2 is called as we've already output some
     * metadata) ... this code is stolen from logputresult() in
     * libpcp
     */
    lcp->label.start.sec = stamp.sec;
    lcp->label.start.nsec = stamp.nsec;
    lcp->label.vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(lcp->tifp, &lcp->label);
    lcp->label.vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(lcp->mdfp, &lcp->label);
    lcp->label.vol = 0;
    __pmLogWriteLabel(acp->ac_mfp, &lcp->label);
    lcp->state = PM_LOG_STATE_INIT;

    return 0; /* ok */
}

static int
check_indom(pmi_context *current, pmInDom indom, int *needti)
{
    int		i, n;
    int		sts = 0;
    __pmArchCtl	*acp = &current->archctl;
    int		type = current->version == PM_LOG_VERS03 ? TYPE_INDOM : TYPE_INDOM_V2;
    __pmLogInDom lid;
    __pmLogInDom old;
    __pmLogInDom new_delta;
    __pmLogInDom	*dup;
    int		*instlist;
    char	**namelist;
    int		needindom;

    for (i = 0; i < current->nindom; i++) {
	if (indom == current->indom[i].indom) {
	    if (current->indom[i].meta_done == 0) {
		lid.stamp = stamp;
		lid.indom = current->indom[i].indom;
		lid.numinst = current->indom[i].ninstance;
		lid.instlist = current->indom[i].inst;
		lid.namelist = current->indom[i].name;
		lid.alloc = 0;
		pmaSortInDom(&lid);
		/*
		 * Duplicate the sorted indom so the hash owns stable
		 * copies; subsequent pmiAddInstance() realloc() calls
		 * cannot leave the hash with dangling name pointers.
		 */
		dup = __pmDupLogInDom(&lid);
		lid.instlist = dup->instlist;
		lid.namelist = dup->namelist;
		lid.alloc = PMLID_INSTLIST | PMLID_NAMELIST | PMLID_NAMES;
		dup->alloc &= ~(PMLID_INSTLIST | PMLID_NAMELIST | PMLID_NAMES);
		__pmFreeLogInDom(dup);

		n = __pmLogGetInDom(acp, indom, NULL, &instlist, &namelist);
		if (n >= 0) {
		    old.numinst = n;
		    old.instlist = instlist;
		    old.namelist = namelist;
		    /*
		     * pmaDeltaInDom() returns:
		     *   0 = identical, skip write (stable indoms: CPUs, disks, etc.)
		     *   1 = changed, write full indom
		     *   2 = changed, delta record is smaller (v3 only)
		     */
		    if (current->version == PM_LOG_VERS03)
			needindom = pmaDeltaInDom(&old, &lid, &new_delta);
		    else
			needindom = pmaSameInDom(&old, &lid) ? 0 : 1;

		    if (needindom == 0) {
			current->indom[i].meta_done = 1;
			continue;
		    }
		    if (needindom == 2) {
			new_delta.stamp = stamp;
			new_delta.indom = indom;
			if ((sts = __pmLogPutInDom(acp, TYPE_INDOM_DELTA, &new_delta)) < 0)
			    return sts;
			/*
			 * Update hash with full indom so the next comparison
			 * starts from the correct full state, not the delta.
			 */
			if ((sts = __pmLogAddInDom(acp, TYPE_INDOM, &lid, NULL)) < 0)
			    return sts;
			current->indom[i].meta_done = 1;
			*needti = 1;
			continue;
		    }
		}

		/* first write, v2, or full indom change */
		if ((sts = __pmLogPutInDom(acp, type, &lid)) < 0)
		    return sts;

		current->indom[i].meta_done = 1;
		*needti = 1;
	    }
	}
    }

    return sts;
}

static int
check_metric(pmi_context *current, pmID pmid, int *needti)
{
    int		m;
    int		sts = 0;
    __pmArchCtl	*acp = &current->archctl;

    for (m = 0; m < current->nmetric; m++) {
	if (pmid != current->metric[m].pmid)
	    continue;
	if (current->metric[m].meta_done == 0) {
	    char	**namelist = &current->metric[m].name;
	    pmDesc	existing;

	    /*
	     * If the archive was opened for appending, __pmLogLoadMeta() will
	     * have populated hashpmid with all descriptors already on disk.
	     * Skip re-writing a descriptor that is already there, but first
	     * verify it is compatible with the registered metric — e.g. a
	     * package upgrade may have corrected a metric's type, semantics,
	     * instance domain or units, in which case the old archive cannot
	     * be extended with the new data.
	     */
	    if (__pmLogLookupDesc(acp, pmid, &existing) == 0) {
		if (existing.type != current->metric[m].desc.type)
		    return PM_ERR_LOGCHANGETYPE;
		if (existing.sem != current->metric[m].desc.sem)
		    return PM_ERR_LOGCHANGESEM;
		if (existing.indom != current->metric[m].desc.indom)
		    return PM_ERR_LOGCHANGEINDOM;
		if (memcmp(&existing.units, &current->metric[m].desc.units,
			   sizeof(existing.units)) != 0)
		    return PM_ERR_LOGCHANGEUNITS;
		current->metric[m].meta_done = 1;
	    }
	    else {
		if ((sts = __pmLogPutDesc(acp, &current->metric[m].desc, 1, namelist)) < 0)
		    return sts;

		current->metric[m].meta_done = 1;
		*needti = 1;
	    }
	}
	if (current->metric[m].desc.indom != PM_INDOM_NULL) {
	    if ((sts = check_indom(current, current->metric[m].desc.indom, needti)) < 0)
		return sts;
	}
	break;
    }

    return sts;
}

static void
newvolume(pmi_context *current)
{
    __pmFILE		*newfp;
    __pmArchCtl		*acp = &current->archctl;
    __pmLogCtl		*lcp = &current->logctl;
    int			nextvol = acp->ac_curvol + 1;

    if ((newfp = __pmLogNewFile(current->archive, nextvol)) == NULL) {
	fprintf(stderr, "logimport: Error: volume %d: %s\n", nextvol, pmErrStr(-oserror()));
	return;
    }

    if (pmDebugOptions.log)
	fprintf(stderr, "logimport: '%s' new log volume %d\n", current->archive, nextvol);

    __pmFclose(acp->ac_mfp);
    acp->ac_mfp = newfp;
    lcp->label.vol = acp->ac_curvol = nextvol;
    __pmLogWriteLabel(acp->ac_mfp, &lcp->label);
    __pmFflush(acp->ac_mfp);
}

/*
 * Approximate byte interval between temporal index entries within a
 * data volume.  Smaller values improve seek performance at the cost
 * of a slightly larger index; 100000 bytes is the historical default.
 */
#define PMI_FLUSH_INTERVAL	((off_t)100000)

static off_t	flushsize = PMI_FLUSH_INTERVAL;

int
_pmi_put_result(pmi_context *current, __pmResult *result)
{
    int		sts;
    __pmPDU	*pb;
    __pmArchCtl	*acp = &current->archctl;
    __pmLogCtl	*lcp = &current->logctl;
    int		k;
    int		needti;
    char	*p;
    static __uint64_t	max_logsz = 0;
    unsigned long off;
    off_t	old_meta_offset;

    /*
     * some front-end tools use lazy discovery of instances and/or process
     * data in non-deterministic order ... it is simpler for everyone if
     * we sort the values into ascending instance order.
     */
    __pmSortInstances(result);

    stamp = result->timestamp;	/* struct assignment */

    /* One time processing for the start of the context. */
    sts = check_context_start(current);
    if (sts < 0)
	return sts;

    old_meta_offset = __pmFtell(lcp->mdfp);;

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));
    sts = __pmEncodeResult(acp->ac_log, result, &pb);
    if (sts < 0)
	return sts;

    needti = 0;
    for (k = 0; k < result->numpmid; k++) {
	sts = check_metric(current, result->vset[k]->pmid, &needti);
	if (sts < 0) {
	    __pmUnpinPDUBuf(pb);
	    return sts;
	}
    }

    if (max_logsz == 0) {
	if ((p = getenv("PCP_LOGIMPORT_MAXLOGSZ")) != NULL)
	    max_logsz = strtoull(p, NULL, 10);
	else if (current->version >= PM_LOG_VERS03)
	    max_logsz = LONGLONG_MAX;
	else  /* PM_LOG_VERS02 */
	    max_logsz = 0x7fffffff;
    }

    off = __pmFtell(acp->ac_mfp) + ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
    if (off >= max_logsz) {
    	newvolume(current);
	flushsize = PMI_FLUSH_INTERVAL;
	needti = 1;
    }

    if (needti || __pmFtell(acp->ac_mfp) + ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int) > flushsize) {
	/*
	 * need new temporal index entry ... seek pointers need to be
	 * _before_ this pmResult and associated metadata (if any)
	 */
	off_t	new_meta_offset;
	__pmFflush(lcp->mdfp);
	new_meta_offset = __pmFtell(lcp->mdfp);;
	__pmFseek(lcp->mdfp, old_meta_offset, SEEK_SET);
	 __pmLogPutIndex(acp, &stamp);
	/* and restore metadata seek pointer */
	__pmFseek(lcp->mdfp, new_meta_offset, SEEK_SET);
	flushsize = __pmFtell(acp->ac_mfp) + PMI_FLUSH_INTERVAL;
    }

    sts = current->version >= PM_LOG_VERS03 ?
	    __pmLogPutResult3(acp, pb) : __pmLogPutResult2(acp, pb);

    __pmUnpinPDUBuf(pb);

    if (sts < 0)
	return sts;

    /*
     * User-API volume rotation: if pmiSetVolumeSize() was called, check
     * the current data volume size after each successful write.  When the
     * threshold is reached, rotate to the next volume and invoke the
     * caller's callback with the path of the just-closed volume so it can
     * arrange compression or other post-processing.
     *
     * This is deliberately checked *after* the write so the completed
     * volume contains a full, consistent record set.
     */
    if (current->max_volume_bytes > 0 &&
	(size_t)__pmFtell(acp->ac_mfp) >= current->max_volume_bytes) {
	int	old_vol = acp->ac_curvol;
	char	vol_path[MAXPATHLEN];

	newvolume(current);
	flushsize = PMI_FLUSH_INTERVAL;

	if (current->on_volume_rotate) {
	    pmsprintf(vol_path, sizeof(vol_path), "%s.%d",
		      current->archive, old_vol);
	    current->on_volume_rotate(vol_path);
	}
    }

    return 0;
}

int
_pmi_put_text(pmi_context *current)
{
    int		sts;
    __pmArchCtl	*acp = &current->archctl;
    pmi_text	*tp;
    char	*existing;
    int		t;
    int		needti;

    /* last_stamp has been set by the caller. */
    stamp = current->last_stamp;

    /* One time processing for the start of the context. */
    sts = check_context_start(current);
    if (sts < 0)
	return sts;

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));

    needti = 0;
    for (t = 0; t < current->ntext; t++) {
	tp = &current->text[t];
	if (tp->meta_done)
	    continue; /* Already written */

	if ((tp->type & PM_TEXT_PMID)) {
	    /*
	     * This text is for a metric. Make sure that the metric desc
	     * has been written.
	     */
	    sts = check_metric(current, tp->id, &needti);
	    if (sts < 0)
		return sts;
	}
	else if ((tp->type & PM_TEXT_INDOM)) {
	    /*
	     * This text is for an indom. Make sure that the indom
	     * has been written.
	     */
	    sts = check_indom(current, tp->id, &needti);
	    if (sts < 0)
		return sts;
	}

	/*
	 * Skip re-writing text that is already present and unchanged in the
	 * archive (loaded into hashtext by __pmLogLoadMeta on append open).
	 */
	existing = NULL;
	if (__pmLogLookupText(acp, tp->id, tp->type, &existing) == 0 &&
	    strcmp(existing, tp->content) == 0) {
	    tp->meta_done = 1;
	    continue;
	}

	/*
	 * Now write out the text record.
	 * libpcp, via __pmLogPutText(), makes a copy of the storage pointed
	 * to by buffer.
	 */
	if ((sts = __pmLogPutText(&current->archctl, tp->id, tp->type,
				  tp->content, 1/*cached*/)) < 0)
	    return sts;

	tp->meta_done = 1;
    }

    if (needti)
	__pmLogPutIndex(acp, &stamp);

    return 0;
}

int
_pmi_put_label(pmi_context *current)
{
    int		sts;
    __pmArchCtl	*acp = &current->archctl;
    pmi_label	*lp;
    int		l;
    int		needti;

    /* last_stamp has been set by the caller. */
    stamp = current->last_stamp;

    /* One time processing for the start of the context. */
    sts = check_context_start(current);
    if (sts < 0)
	return sts;

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));

    needti = 0;
    for (l = 0; l < current->nlabel; l++) {
	lp = &current->label[l];

	if (lp->type == PM_LABEL_ITEM) {
	    /*
	     * This label is for a metric. Make sure that the metric desc
	     * has been written.
	     */
	    sts = check_metric(current, lp->id, &needti);
	    if (sts < 0)
		return sts;
	}
	else if (lp->type == PM_LABEL_INDOM || lp->type == PM_LABEL_INSTANCES) {
	    /*
	     * This label is for an indom. Make sure that the indom
	     * has been written.
	     */
	    sts = check_indom(current, lp->id, &needti);
	    if (sts < 0)
		return sts;
	}

	/*
	 * Now write out the label record.
	 * libpcp, via __pmLogPutLabels(), assumes control of the
	 * storage pointed to by lp->labelset.
	 */
	if ((sts = __pmLogPutLabels(&current->archctl, lp->type, lp->id,
				   1, lp->labelset, &stamp)) < 0)
	    return sts;

	lp->labelset = NULL;
    }

    /* We no longer need the accumulated list of labelsets. */
    free(current->label);
    current->nlabel = 0;
    current->label = NULL;

    if (needti)
	__pmLogPutIndex(acp, &stamp);

    return 0;
}

int
_pmi_end(pmi_context *current)
{
    if (current->state == CONTEXT_END)
	return PM_ERR_NOCONTEXT;

    /* Final temporal index update to finish the archive
     * ... same logic here as in run_done() for pmlogger
     */
    __pmLogPutIndex(&current->archctl, &stamp);

    __pmLogClose(&current->archctl);

    /*
     * For long-running process importers (PMI_PROCESS), remove the import
     * file on clean exit.  For one-shot importers (e.g. sadc via sa1),
     * keep it so pcp-summary can read the configuration between runs.
     */
    if (current->tool_name[0] != '\0' && (current->flags & PMI_PROCESS)) {
	char	path[MAXPATHLEN];
	pmsprintf(path, sizeof(path), "%s/%s",
		  pmGetConfig("PCP_IMPORT_DIR"), current->tool_name);
	unlink(path);
	current->tool_name[0] = '\0';
    }

    current->state = CONTEXT_END;
    return 0;
}
