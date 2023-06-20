/*
 * Indom metadata support for pmlogrewrite
 *
 * Copyright (c) 2017 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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

#include "pcp/pmapi.h"
#include "pcp/libpcp.h"
#include "pcp/archive.h"
#include "./logger.h"
#include <assert.h>
#include "../libpcp/src/internal.h"

/*
 * Find or create a new indomspec_t
 * ... suppress warning message on error if quiet != 0
 */
indomspec_t *
start_indom(pmInDom indom, int quiet)
{
    indomspec_t	*ip;
    int		i;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    if (ip == NULL) {
	int	numinst;
	int	*instlist;
	char	**namelist;

	numinst = pmGetInDomArchive(indom, &instlist, &namelist);
	if (numinst < 0) {
	    if (!quiet && wflag) {
		pmsprintf(mess, sizeof(mess), "Instance domain %s: %s", pmInDomStr(indom), pmErrStr(numinst));
		yywarn(mess);
	    }
	    return NULL;
	}

	ip = (indomspec_t *)malloc(sizeof(indomspec_t));
	if (ip == NULL) {
	    fprintf(stderr, "indomspec malloc(%d) failed: %s\n", (int)sizeof(indomspec_t), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	ip->i_next = indom_root;
	indom_root = ip;
	ip->indom_flags = 0;
	ip->inst_flags = (int *)malloc(numinst*sizeof(int));
	if (ip->inst_flags == NULL) {
	    fprintf(stderr, "indomspec flags malloc(%d) failed: %s\n", (int)(numinst*sizeof(int)), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	for (i = 0; i < numinst; i++)
	    ip->inst_flags[i] = 0;
	ip->old_indom = indom;
	ip->new_indom = indom;
	ip->numinst = numinst;
	ip->old_inst = instlist;
	ip->new_inst = (int *)malloc(numinst*sizeof(int));
	if (ip->new_inst == NULL) {
	    fprintf(stderr, "new_inst malloc(%d) failed: %s\n", (int)(numinst*sizeof(int)), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	ip->old_iname = namelist;
	ip->new_iname = (char **)malloc(numinst*sizeof(char *));
	if (ip->new_iname == NULL) {
	    fprintf(stderr, "new_iname malloc(%d) failed: %s\n", (int)(numinst*sizeof(char *)), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
    }

    return ip;
}

int
change_inst_by_name(pmInDom indom, char *old, char *new)
{
    int		i;
    indomspec_t	*ip;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    assert(ip != NULL);

    for (i = 0; i < ip->numinst; i++) {
	if (inst_name_eq(ip->old_iname[i], old) > 0) {
	    if ((new == NULL && ip->inst_flags[i]) ||
	        (ip->inst_flags[i] & (INST_CHANGE_INAME|INST_DELETE))) {
		pmsprintf(mess, sizeof(mess), "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	if (wflag) {
	    pmsprintf(mess, sizeof(mess), "Unknown instance \"%s\" in iname clause for indom %s", old, pmInDomStr(indom));
	    yywarn(mess);
	}
	return 0;
    }

    if (new == NULL) {
	ip->inst_flags[i] |= INST_DELETE;
	ip->new_iname[i] = NULL;
	return 0;
    }

    if (strcmp(ip->old_iname[i], new) == 0) {
	/* no change ... */
	if (wflag) {
	    pmsprintf(mess, sizeof(mess), "Instance domain %s: Instance: \"%s\": No change", pmInDomStr(indom), ip->old_iname[i]);
	    yywarn(mess);
	}
    }
    else {
	ip->inst_flags[i] |= INST_CHANGE_INAME;
	ip->new_iname[i] = new;
    }

    return 0;
}

int
change_inst_by_inst(pmInDom indom, int old, int new)
{
    int		i;
    indomspec_t	*ip;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    assert(ip != NULL);

    for (i = 0; i < ip->numinst; i++) {
	if (ip->old_inst[i] == old) {
	    if ((new == PM_IN_NULL && ip->inst_flags[i]) ||
	        (ip->inst_flags[i] & (INST_CHANGE_INST|INST_DELETE))) {
		pmsprintf(mess, sizeof(mess), "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	if (wflag) {
	    pmsprintf(mess, sizeof(mess), "Unknown instance %d in inst clause for indom %s", old, pmInDomStr(indom));
	    yywarn(mess);
	}
	return 0;
    }

    if (new == PM_IN_NULL) {
	ip->inst_flags[i] |= INST_DELETE;
	ip->new_inst[i] = PM_IN_NULL;
	return 0;
    }
    
    if (ip->old_inst[i] == new) {
	/* no change ... */
	if (wflag) {
	    pmsprintf(mess, sizeof(mess), "Instance domain %s: Instance: %d: No change", pmInDomStr(indom), ip->old_inst[i]);
	    yywarn(mess);
	}
    }
    else {
	ip->new_inst[i] = new;
	ip->inst_flags[i] |= INST_CHANGE_INST;
    }

    return 0;
}

/*
 * reverse the logic of __pmLogPutInDom()
 */
static void
_pmUnpackInDom(__int32_t *recbuf, __pmLogInDom *lidp)
{
    __pmLogHdr		*hdr;
    int			type;
    int			sts;

    hdr = (__pmLogHdr *)recbuf;
    type = htonl(hdr->type);
    if (type == TYPE_INDOM_DELTA) {
	__pmLogInDom	*idp;
	lidp->indom = ntoh_pmInDom(recbuf[5]);
	idp = pmaUndeltaInDom(inarch.ctxp->c_archctl->ac_log, recbuf);
	if (idp == NULL) {
	    fprintf(stderr, "_pmUnpackInDom: Botch: undelta InDom failed for InDom %s\n", pmInDomStr(lidp->indom));
	    abandon();
	    /*NOTREACHED*/
	}
	lidp->stamp = idp->stamp;
	lidp->numinst = idp->numinst;
	lidp->instlist = idp->instlist;
	lidp->namelist = idp->namelist;
	/* don't free lidp->namelist or lidp->namelist[i] or lidp->instlist */
	lidp->alloc = 0;
    }
    else {
	__int32_t	*buf;
	/* buffer for __pmLogLoadInDom has to start AFTER the header */
	buf = &recbuf[2];
	sts = __pmLogLoadInDom(NULL, 0, type, lidp, &buf);
	if (sts < 0) {
	    fprintf(stderr, "_pmUnpackInDom: __pmLogLoadInDom(type=%d): failed: %s\n", type, pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}
    }

    if (lidp->numinst < 1) {
	fprintf(stderr, "_pmUnpackInDom: InDom %s dodgey: numinst=%d\n", pmInDomStr(lidp->indom), lidp->numinst);
	abandon();
	/*NOTREACHED*/
    }
#if 0
fprintf(stderr, "numinst=%d indom=%s inst[0] %d or \"%s\" inst[%d] %d or \"%s\"\n", lidp->numinst, pmInDomStr(lidp->indom), lidp->instlist[0], lidp->namelist[0], lidp->numinst-1, lidp->instlist[lidp->numinst-1], lidp->namelist[lidp->numinst-1]);
#endif

}

/*
 * Note:
 * 	We unpack the indom metadata record _again_ (was already done when
 * 	the input archive was opened), but the data structure behind
 * 	__pmLogCtl has differences for 32-bit and 64-bit pointers and
 * 	modifying it as part of the rewrite could make badness break
 * 	out later.  It is safer to do it again, populate local copies
 * 	of instlist[] and inamelist[], dink with 'em and then toss them
 * 	away.
 */
void
do_indom(int type)
{
    long	out_offset;
    indomspec_t	*ip;
    int		sts;
    int		i;
    int		j;
    int		pdu_type;
    __pmLogInDom	lid;
    __pmLogInDom	*dup_lid;
    __pmHashNode	*hp;

    lid.numinst = 0;
    lid.alloc = 0;

    out_offset = __pmFtell(outarch.logctl.mdfp);
    _pmUnpackInDom(inarch.metarec, &lid);

    /*
     * Only safe approach from here one is to duplicate lid
     * so that all elements, particularly namelist and each
     * namelist[] are malloc'd
     */
    if ((dup_lid = __pmDupLogInDom(&lid)) == NULL) {
	fprintf(stderr, "%s: Error: __pmDupLogInDom: %s: NULL\n",
			pmGetProgname(), pmInDomStr(lid.indom));
	abandon();
	/*NOTREACHED*/
    }
    __pmFreeLogInDom(&lid);
    lid = *dup_lid;		/* struct assignment */
    lid.alloc &= ~PMLID_SELF;      /* don't free lid */
    free(dup_lid);

    if (lid.indom != PM_INDOM_NULL) {
	/*
	 * if indom's refcount is zero, no need to emit it
	 */
	if ((hp = __pmHashSearch((unsigned int)lid.indom, &indom_hash)) == NULL) {
	    fprintf(stderr, "Botch: InDom: %s: not in indom_hash table\n", pmInDomStr(lid.indom));
	}
	else {
	    int		*refp;
	    refp = (int *)hp->data;
	    if (*refp == 0) {
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "Delete: InDom: %s: no output metrics use this\n",
			pmInDomStr(lid.indom));
		}
		goto done;
	    }
	}
    }

    /*
     * global time stamp adjustment (if any has already been done in the
     * record buffer, so this is reflected in the unpacked value of stamp.
     */
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (ip->old_indom != lid.indom)
	    continue;
	if (ip->indom_flags & INDOM_DUPLICATE) {
	    /*
	     * Save the old indom without changes, then operate on the
	     * duplicate.
	     *
	     * For V3 archives, give ``delta'' indom a try ...
	     */
	    if (outarch.version > PM_LOG_VERS02) {
		int		lsts;

		pdu_type = TYPE_INDOM;
		lsts = pmaTryDeltaInDom(outarch.archctl.ac_log, NULL, &lid);
		if (lsts < 0) {
		    fprintf(stderr, "Botch: pmaTryDeltaInDom duplicate failed: %d\n", lsts);
		    abandon();
		    /*NOTREACHED*/
		}
		if (lsts == 1)
		    pdu_type = TYPE_INDOM_DELTA;
	    }
	    else
		pdu_type = TYPE_INDOM_V2;
	    if ((sts = __pmLogPutInDom(&outarch.archctl, pdu_type, &lid)) < 0) {
		fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
				pmGetProgname(), pmInDomStr(lid.indom), pmErrStr(sts));
		abandon();
		/*NOTREACHED*/
	    }

	    /*
	     * If the old indom was not a duplicate, then libpcp, via
	     * __pmLogPutInDom(), assumes control of the storage pointed to
	     * by lid.instlist and lid.namelist. In that case, we need to
	     * operate on another copy from this point on.
	     */
	    if (sts != PMLOGPUTINDOM_DUP) {
		lid.alloc &= ~(PMLID_INSTLIST|PMLID_NAMELIST|PMLID_NAMES);
		if ((dup_lid = __pmDupLogInDom(&lid)) == NULL) {
		    fprintf(stderr, "%s: Error: __pmDupLogInDom: duplicate %s: NULL\n",
				    pmGetProgname(), pmInDomStr(lid.indom));
		    abandon();
		    /*NOTREACHED*/
		}
		__pmFreeLogInDom(&lid);
		lid = *dup_lid;			/* struct assignment */
		lid.alloc &= ~PMLID_SELF;      /* don't free lid */
		free(dup_lid);
	    }

	    if (pmDebugOptions.appl0) {
		fprintf(stderr, "Metadata: write pre-duplicate ");
		if (pdu_type == TYPE_INDOM_DELTA)
		    fprintf(stderr, "Delta ");
		else if (pdu_type == TYPE_INDOM_V2)
		    fprintf(stderr, "V2 ");
		fprintf(stderr, "InDom %s @ offset=%ld\n", pmInDomStr(lid.indom), out_offset);
	    }
	    out_offset = __pmFtell(outarch.logctl.mdfp);
	}
	if (ip->new_indom != ip->old_indom)
	    lid.indom = ip->new_indom;
	for (i = 0; i < ip->numinst; i++) {
	    for (j = 0; j < lid.numinst; j++) {
		if (ip->old_inst[i] == lid.instlist[j])
		    break;
	    }
	    if (j == lid.numinst)
		continue;
	    if (ip->inst_flags[i] & INST_DELETE) {
		if (pmDebugOptions.appl1)
		    fprintf(stderr, "Delete: instance %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], pmInDomStr(ip->old_indom));
		free(lid.namelist[j]);
		j++;
		while (j < lid.numinst) {
		    lid.instlist[j-1] = lid.instlist[j];
		    lid.namelist[j-1] = lid.namelist[j];
		    j++;
		}
		lid.numinst--;
	    }
	    else {
		if (ip->inst_flags[i] & INST_CHANGE_INST)
		    lid.instlist[j] = ip->new_inst[i];
		if (ip->inst_flags[i] & INST_CHANGE_INAME) {
		    free(lid.namelist[j]);
		    if ((lid.namelist[j] = strdup(ip->new_iname[i])) == NULL) {
			fprintf(stderr, "new_iname[%d] strdup(%s) failed: %s\n",
			    j, ip->new_iname[i], strerror(errno));
			abandon();
			/*NOTREACHED*/
		    }
		}
		if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) && pmDebugOptions.appl1) {
		    if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) == (INST_CHANGE_INST | INST_CHANGE_INAME))
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->new_iname[i], ip->new_inst[i], pmInDomStr(ip->old_indom));
		    else if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) == INST_CHANGE_INST)
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->old_iname[i], ip->new_inst[i], pmInDomStr(ip->old_indom));
		    else
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->new_iname[i], ip->old_inst[i], pmInDomStr(ip->old_indom));
		}
	    }
	}
    }

    if (lid.numinst > 0) {
	/*
	 * some instances remain ... work to be done
	 *
	 * For V3 archives, give ``delta'' indom a try ...
	 */
	if (outarch.version > PM_LOG_VERS02) {
	    int		lsts;

	    pdu_type = TYPE_INDOM;
	    lsts = pmaTryDeltaInDom(outarch.archctl.ac_log, NULL, &lid);
	    if (lsts < 0) {
		fprintf(stderr, "Botch: pmaTryDeltaInDom failed: %d\n", lsts);
		abandon();
		/*NOTREACHED*/
	    }
	    if (lsts == 1)
		pdu_type = TYPE_INDOM_DELTA;
	}
	else
	    pdu_type = TYPE_INDOM_V2;

	if ((sts = __pmLogPutInDom(&outarch.archctl, pdu_type, &lid)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
			    pmGetProgname(), pmInDomStr(lid.indom), pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}
	/*
	 * for everything except TYPE_INDOM_DELTA, libpcp, via
	 * __pmLogPutInDom(), assumes control of the storage pointed
	 * to by lid.instlist and lid.namelist ... we're only concerned
	 * about memory leakage here, as linking this stuff into libpcp's
	 * hashed indom structures is not needed for pmlogrewrite because
	 * we're relying on the one-deep cache of the last full indom that
	 * is being managed below pmaTryDeltaInDom()
	 */
	if (pdu_type != TYPE_INDOM_DELTA)
	    lid.alloc &= ~(PMLID_INSTLIST|PMLID_NAMELIST|PMLID_NAMES);

	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "Metadata: write ");
	    if (pdu_type == TYPE_INDOM_DELTA)
		fprintf(stderr, "Delta ");
	    else if (pdu_type == TYPE_INDOM_V2)
		fprintf(stderr, "V2 ");
	    fprintf(stderr, "InDom %s @ offset=%ld\n", pmInDomStr(lid.indom), out_offset);
	}
    }

done:
    __pmFreeLogInDom(&lid);
}

int
redact_indom(pmInDom indom)
{
    char	iname[22];	/* XXXXXXXXXX [redacted] */
    int		i;
    indomspec_t	*ip;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    assert(ip != NULL);

    for (i = 0; i < ip->numinst; i++) {
	if (ip->inst_flags[i] & (INST_CHANGE_INAME|INST_DELETE)) {
		pmsprintf(mess, sizeof(mess), "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	pmsprintf(iname, sizeof(iname), "%d [redacted]", ip->old_inst[i]);
	ip->new_iname[i] = strdup(iname);
	if (ip->new_iname[i] == NULL) {
	    fprintf(stderr, "redact_indom malloc(%d) failed: %s\n", (int)strlen(iname), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	ip->inst_flags[i] |= INST_CHANGE_INAME;
    }

    return 0;
}
