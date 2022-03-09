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

#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"
#include <assert.h>
#include "../libpcp/src/internal.h"

/*
 * Find or create a new indomspec_t
 */
indomspec_t *
start_indom(pmInDom indom)
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
	    if (wflag) {
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
_pmUnpackInDom(__int32_t *recbuf, __pmLogInDom_io *lidp)
{
    __pmLogHdr		*hdr;
    __int32_t		*buf;
    int			type;
    int			i;
    int			allinbuf;
    char		*s;
    size_t		size;
    int			*instlist;
    char		**namelist;

    hdr = (__pmLogHdr *)recbuf;
    type = htonl(hdr->type);
    /* buffer for __pmLogLoadInDom has to start AFTER the header */
    buf = &recbuf[2];
    allinbuf = __pmLogLoadInDom(NULL, 0, type, lidp, &buf);
    if (allinbuf < 0) {
	fprintf(stderr, "_pmUnpackInDom: __pmLogLoadInDom(type=%d): failed: %s\n", type, pmErrStr(allinbuf));
	abandon();
	/*NOTREACHED*/
    }
    if (lidp->numinst < 1) {
	fprintf(stderr, "_pmUnpackInDom: InDom %s dodgey: numinst=%d\n", pmInDomStr(lidp->indom), lidp->numinst);
	abandon();
	/*NOTREACHED*/
    }
#if 0
fprintf(stderr, "numinst=%d indom=%s inst[0] %d or \"%s\" inst[%d] %d or \"%s\"\n", lidp->numinst, pmInDomStr(lidp->indom), lidp->instlist[0], lidp->namelist[0], lidp->numinst-1, lidp->instlist[lidp->numinst-1], lidp->namelist[lidp->numinst-1]);
#endif

    /* Copy the instances to a new buffer */
    instlist = (int *)malloc(lidp->numinst * sizeof(int));
    if (instlist == NULL) {
	fprintf(stderr, "_pmUnpackInDom instlist malloc(%d) failed: %s\n", (int)(lidp->numinst * sizeof(int)), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    for (i = 0; i < lidp->numinst; i++)
	instlist[i] = lidp->instlist[i];
    lidp->instlist = instlist;

#if 0
fprintf(stderr, "after instlist assignment, instlist[0] = %d instlist[%d] = %d\n", (*instlist)[i], lidp->numinst-1, (*instlist)[lidp->numinst-1]);
#endif

    /*
     * Copy the name list to a new buffer. Place the pointers and the names
     * in the same buffer so that they can be easily freed.
     */
    size = lidp->numinst * sizeof(char *);
    for (i = 0; i < lidp->numinst; i++)
	size += strlen(lidp->namelist[i]) + 1;
    namelist = (char **)malloc(size);
    if (namelist == NULL) {
	fprintf(stderr, "_pmUnpackInDom namelist malloc(%d) failed: %s\n",
		(int)size, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    s = (char *)(namelist + lidp->numinst);
    for (i = 0; i < lidp->numinst; i++) {
	namelist[i] = s;
	strcpy(s, lidp->namelist[i]);
	s += strlen(lidp->namelist[i]) + 1;
    }
    if (!allinbuf)
	free(lidp->namelist);
    lidp->namelist = namelist;
}

static void
_pmDupInDomData(int numinst, int **instlist, char ***inamelist)
{
    int		*new_ilist;
    char	**new_namelist;
    size_t	size;

    /* Copy the instance list. */
    size = numinst * sizeof(int);
    new_ilist = (int *)malloc(size);
    if (new_ilist == NULL) {
	fprintf(stderr, "_pmDupInDomData instlist malloc(%d) failed: %s\n",
		(int)size, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    memcpy (new_ilist, *instlist, size);
    *instlist = new_ilist;

    /*
     * Copy the name list. It's ok to share the same string buffer.
     * It will be reallocated and the string pointers updated, if necessary,
     * later.
     */
    size = numinst * sizeof(char *);
    new_namelist = (char **)malloc(size);
    if (new_namelist == NULL) {
	fprintf(stderr, "_pmpmDupInDomData inamelist malloc(%d) failed: %s\n",
		(int)size, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    memcpy (new_namelist, *inamelist, size);
    *inamelist = new_namelist;
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
do_indom(void)
{
    long	out_offset;
    indomspec_t	*ip;
    int		sts;
    int		i;
    int		j;
    int		need_alloc = 0;
    int		pdu_type;
    __pmLogInDom_io	lid;

    lid.numinst = 0;

    if (outarch.version == PM_LOG_VERS02)
	pdu_type = TYPE_INDOM_V2;
    else
	pdu_type = TYPE_INDOM;

    out_offset = __pmFtell(outarch.logctl.mdfp);
    _pmUnpackInDom(inarch.metarec, &lid);

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
	     */
	    if ((sts = __pmLogPutInDom(&outarch.archctl, pdu_type, &lid)) < 0) {
		fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
				pmGetProgname(), pmInDomStr(lid.indom), pmErrStr(sts));
		abandon();
		/*NOTREACHED*/
	    }

	    /*
	     * If the old indom was not a duplicate, then libpcp, via
	     * __pmLogPutInDom(), assumes control of the storage pointed to by
	     * lid.instlist and lid.namelist. In that case, we need to operate on copies
	     * from this point on.
	     */
	    if (sts != PMLOGPUTINDOM_DUP)
		_pmDupInDomData(lid.numinst, &lid.instlist, &lid.namelist);

	    if (pmDebugOptions.appl0) {
		fprintf(stderr, "Metadata: write pre-duplicate InDom %s @ offset=%ld\n", pmInDomStr(lid.indom), out_offset);
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
		j++;
		while (j < lid.numinst) {
		    lid.instlist[j-1] = lid.instlist[j];
		    lid.namelist[j-1] = lid.namelist[j];
		    j++;
		}
		need_alloc = 1;
		lid.numinst--;
	    }
	    else {
		if (ip->inst_flags[i] & INST_CHANGE_INST)
		    lid.instlist[j] = ip->new_inst[i];
		if (ip->inst_flags[i] & INST_CHANGE_INAME) {
		    lid.namelist[j] = ip->new_iname[i];
		    need_alloc = 1;
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
	 */

	if (need_alloc) {
	    /*
	     * __pmLogPutInDom assumes the elements of lid.namelist[] point into
	     * of a contiguous allocation starting at lid.namelist[0] ... if we've
	     * changed an instance name or moved instance names about, then we
	     * need to reallocate the strings for lid.namelist[]
	     */
	    int	need = 0;
	    char	*new;
	    char	*p;

	    for (j = 0; j < lid.numinst; j++)
		need += strlen(lid.namelist[j]) + 1;
	    new = (char *)malloc(need);
	    if (new == NULL) {
		fprintf(stderr, "lid.namelist[] malloc(%d) failed: %s\n", need, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }
	    p = new;
	    for (j = 0; j < lid.numinst; j++) {
		strcpy(p, lid.namelist[j]);
		lid.namelist[j] = p;
		p += strlen(p) + 1;
	    }
	}

	/*
	 * libpcp, via __pmLogPutInDom(), assumes control of the storage pointed
	 * to by lid.instlist and lid.namelist.
	 */
	if ((sts = __pmLogPutInDom(&outarch.archctl, pdu_type, &lid)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
			    pmGetProgname(), pmInDomStr(lid.indom), pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}
	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "Metadata: write InDom %s @ offset=%ld\n", pmInDomStr(lid.indom), out_offset);
	}

	/*
	 * If the indom was a duplicate, then we are responsible for freeing the
	 * associated storage.
	 */
	if (sts == PMLOGPUTINDOM_DUP) {
	    if (need_alloc)
		free(lid.namelist[0]);
	    free(lid.namelist);
	    free(lid.instlist);
	}
    }
    else {
	/* all instances have been deleted */
	free(lid.namelist);
	free(lid.instlist);
    }
}
