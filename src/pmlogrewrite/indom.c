/*
 * Indom metadata support for pmlogrewrite
 *
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
#include "impl.h"
#include "logger.h"
#include <assert.h>

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
		snprintf(mess, sizeof(mess), "Instance domain %s: %s\n", pmInDomStr(indom), pmErrStr(numinst));
		yywarn(mess);
	    }
	    return NULL;
	}

	ip = (indomspec_t *)malloc(sizeof(indomspec_t));
	if (ip == NULL) {
	    fprintf(stderr, "indomspec malloc(%d) failed: %s\n", (int)sizeof(indomspec_t), strerror(errno));
	    exit(1);
	}
	ip->i_next = indom_root;
	indom_root = ip;
	ip->flags = (int *)malloc(numinst*sizeof(int));
	if (ip->flags == NULL) {
	    fprintf(stderr, "indomspec flags malloc(%d) failed: %s\n", numinst*(int)sizeof(int), strerror(errno));
	    exit(1);
	}
	for (i = 0; i < numinst; i++)
	    ip->flags[i] = 0;
	ip->old_indom = indom;
	ip->new_indom = indom;
	ip->numinst = numinst;
	ip->old_inst = instlist;
	ip->new_inst = (int *)malloc(numinst*sizeof(int));
	if (ip->new_inst == NULL) {
	    fprintf(stderr, "new_inst malloc(%d) failed: %s\n", numinst*(int)sizeof(int), strerror(errno));
	    exit(1);
	}
	ip->old_iname = namelist;
	ip->new_iname = (char **)malloc(numinst*sizeof(char *));
	if (ip->new_iname == NULL) {
	    fprintf(stderr, "new_iname malloc(%d) failed: %s\n", numinst*(int)sizeof(char *), strerror(errno));
	    exit(1);
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
	char	*p;
	char	*q;
	int	match = 0;
	for (p = ip->old_iname[i], q = old; ; p++, q++) {
	    if (*p == '\0' || *p == ' ') {
		if (*q == '\0' || *q == ' ')
		    match = 1;
		break;
	    }
	    if (*q == '\0' || *q == ' ') {
		if (*p == '\0' || *p == ' ')
		    match = 1;
		break;
	    }
	    if (*p != *q)
		break;
	}
	if (match) {
	    if ((new == NULL && ip->flags[i]) ||
	        (ip->flags[i] & (INST_CHANGE_INAME|INST_DELETE))) {
		sprintf(mess, "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	sprintf(mess, "Unknown instance \"%s\" in name clause for indom %s", old, pmInDomStr(indom));
	return -1;
    }

    if (new == NULL) {
	ip->flags[i] |= INST_DELETE;
	ip->new_iname[i] = NULL;
	return 0;
    }

    if (strcmp(ip->old_iname[i], new) == 0) {
	/* no change ... */
	if (wflag) {
	    snprintf(mess, sizeof(mess), "Instance domain %s: Instance: \"%s\": No change\n", pmInDomStr(indom), ip->old_iname[i]);
	    yywarn(mess);
	}
    }
    else {
	ip->flags[i] |= INST_CHANGE_INAME;
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
	    if ((new == PM_IN_NULL && ip->flags[i]) ||
	        (ip->flags[i] & (INST_CHANGE_INST|INST_DELETE))) {
		sprintf(mess, "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	sprintf(mess, "Unknown instance %d in inst clause for indom %s", old, pmInDomStr(indom));
	return -1;
    }

    if (new == PM_IN_NULL) {
	ip->flags[i] |= INST_DELETE;
	ip->new_inst[i] = PM_IN_NULL;
	return 0;
    }
    
    if (ip->old_inst[i] == new) {
	/* no change ... */
	if (wflag) {
	    snprintf(mess, sizeof(mess), "Instance domain %s: Instance: %d: No change\n", pmInDomStr(indom), ip->old_inst[i]);
	    yywarn(mess);
	}
    }
    else {
	ip->new_inst[i] = new;
	ip->flags[i] |= INST_CHANGE_INST;
    }

    return 0;
}

typedef struct {
    __pmLogHdr	hdr;
    __pmTimeval	stamp;
    pmInDom	indom;
    int		numinst;
    char	other[1];
} indom_t;

/*
 * reverse the logic of __pmLogPutInDom()
 */
static void
_pmUnpackInDom(__pmPDU *pdubuf, pmInDom *indom, __pmTimeval *tp, int *numinst, int **instlist, char ***inamelist)
{
    indom_t	*idp;
    int		i;
    int		*ip;
    char	*strbuf;

    idp = (indom_t *)pdubuf;

    tp->tv_sec = ntohl(idp->stamp.tv_sec);
    tp->tv_usec = ntohl(idp->stamp.tv_usec);
    *indom = __ntohpmInDom(idp->indom);
    *numinst = ntohl(idp->numinst);
    *instlist = (int *)malloc(*numinst * sizeof(int));
    if (*instlist == NULL) {
	fprintf(stderr, "_pmUnpackInDom instlist malloc(%d) failed: %s\n", (int)(*numinst * sizeof(int)), strerror(errno));
	exit(1);
    }
    ip = (int *)idp->other;
    for (i = 0; i < *numinst; i++)
	(*instlist)[i] = ntohl(*ip++);
    *inamelist = (char **)malloc(*numinst * sizeof(char *));
    if (*inamelist == NULL) {
	fprintf(stderr, "_pmUnpackInDom inamelist malloc(%d) failed: %s\n", (int)(*numinst * sizeof(char *)), strerror(errno));
	exit(1);
    }
    /*
     * ip[i] is stridx[i], which is offset into strbuf[]
     */
    strbuf = (char *)&ip[*numinst];
    for (i = 0; i < *numinst; i++) {
	(*inamelist)[i] = &strbuf[ntohl(ip[i])];
    }
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
    pmInDom	indom;
    __pmTimeval	stamp;
    int		numinst;
    int		*instlist;
    char	**inamelist;
    indomspec_t	*ip;
    int		sts;
    int		i;
    int		j;

    out_offset = ftell(outarch.logctl.l_mdfp);
    _pmUnpackInDom(inarch.metarec, &indom, &stamp, &numinst, &instlist, &inamelist);

    /*
     * global time stamp adjustment (if any has already been done in the
     * PDU buffer, so this is reflected in the unpacked value of stamp.
     */
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (ip->old_indom != indom)
	    continue;
	if (ip->new_indom != ip->old_indom)
	    indom = ip->new_indom;
	for (i = 0; i < ip->numinst; i++) {
	    for (j = 0; j < numinst; j++) {
		if (ip->old_inst[i] == instlist[j])
		    break;
	    }
	    if (j == numinst)
		continue;
	    if (ip->flags[i] & INST_DELETE) {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1)
		    fprintf(stderr, "Delete: instance %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], pmInDomStr(ip->old_indom));
#endif
		j++;
		while (j < numinst) {
		    instlist[j-1] = instlist[j];
		    inamelist[j-1] = inamelist[j];
		    j++;
		}
		numinst--;
	    }
	    else {
		if (ip->flags[i] & INST_CHANGE_INST)
		    instlist[j] = ip->new_inst[i];
		if (ip->flags[i] & INST_CHANGE_INAME)
		    inamelist[j] = ip->new_iname[i];
#if PCP_DEBUG
		if ((ip->flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) && (pmDebug & DBG_TRACE_APPL1))
		    fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->new_iname[i], ip->new_inst[i], pmInDomStr(ip->old_indom));
#endif
	    }
	}
    }

    if ((sts = __pmLogPutInDom(&outarch.logctl, indom, &stamp, numinst, instlist, inamelist)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
			pmProgname, pmInDomStr(indom), pmErrStr(sts));
	exit(1);
    }
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "Metadata: write InDom %s @ offset=%ld\n", pmInDomStr(indom), out_offset);
    }
#endif

    free(instlist);
    free(inamelist);
}
