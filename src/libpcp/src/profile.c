/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "internal.h"

static int *
_subtract(int *list, int *list_len, int *arg, int arg_len)
{
    int		*new;
    int		len = *list_len;
    int		new_len = 0;
    int		i, j;

    if (list == NULL)
	/* noop */
	return NULL;

    new = (int *)malloc(len * sizeof(int));
    if (new == NULL)
	return NULL;

    for (i=0; i < len; i++) {
	for (j=0; j < arg_len; j++)
	    if (list[i] == arg[j])
		break;
	if (j == arg_len)
	    /* this instance survived */
	    new[new_len++] = list[i];
    }
    free(list);
    *list_len = new_len;
    return new;
}

static int *
_union(int *list, int *list_len, int *arg, int arg_len)
{
    int		*new;
    int		len = *list_len;
    int		new_len = *list_len;
    int		i, j;

    if (list == NULL) {
	list = (int *)malloc(arg_len * sizeof(int));
	memcpy((void *)list, (void *)arg, arg_len * sizeof(int));
	*list_len = arg_len;
	return list;
    }

    new = (int *)realloc((void *)list, (len + arg_len) * sizeof(int));
    if (new == NULL)
	return NULL;

    for (i=0; i < arg_len; i++) {
	for (j=0; j < new_len; j++) {
	    if (arg[i] == new[j])
		break;
	}
	if (j == new_len)
	    /* instance is not already in the list */
	    new[new_len++] = arg[i];
    }
    *list_len = new_len;
    return new;
}

static void
_setGlobalState(__pmContext *ctxp, int state)
{
    __pmInDomProfile	*p, *p_end;

    /* free everything and set the global state */
    if (ctxp->c_instprof->profile) {
	for (p=ctxp->c_instprof->profile, p_end = p + ctxp->c_instprof->profile_len;
	    p < p_end; p++) {
	    if (p->instances)
		free(p->instances);
	    p->instances_len = 0;
	}

	free(ctxp->c_instprof->profile);
	ctxp->c_instprof->profile = NULL;
	ctxp->c_instprof->profile_len = 0;
    }

    ctxp->c_instprof->state = state;
    ctxp->c_sent = 0;
}

static __pmInDomProfile *
_newprof(pmInDom indom, __pmContext *ctxp)
{
    __pmInDomProfile	*p;

    if (ctxp->c_instprof->profile == NULL) {
	/* create a new profile for this inDom in the default state */
	p = ctxp->c_instprof->profile = (__pmInDomProfile *)malloc(
	    sizeof(__pmInDomProfile));
	if (p == NULL)
	    /* fail, no changes */
	    return NULL;
	ctxp->c_instprof->profile_len = 1;
    }
    else {
	/* append a new profile to the end of the list */
	ctxp->c_instprof->profile_len++;
	p = (__pmInDomProfile *)realloc((void *)ctxp->c_instprof->profile, 
	    ctxp->c_instprof->profile_len * sizeof(__pmInDomProfile));
	if (p == NULL)
	    /* fail, no changes */
	    return NULL;
	ctxp->c_instprof->profile = p;
	p = ctxp->c_instprof->profile + ctxp->c_instprof->profile_len - 1;
    }

    /* initialise a new profile entry : default = include all instances */
    p->indom = indom;
    p->instances = NULL;
    p->instances_len = 0;
    p->state = PM_PROFILE_INCLUDE;
    return p;
}

__pmInDomProfile *
__pmFindProfile(pmInDom indom, const __pmProfile *prof)
{
    __pmInDomProfile	*p, *p_end;

    if (prof != NULL && prof->profile_len > 0)
	/* search for the profile entry for this instance domain */
	for (p=prof->profile, p_end=p+prof->profile_len; p < p_end; p++) {
	    if (p->indom == indom)
		/* found : an entry for this instance domain already exists */
		return p;
	}

    /* not found */
    return NULL;
}

int
__pmInProfile(pmInDom indom, const __pmProfile *prof, int inst)
{
    __pmInDomProfile	*p;
    int			*in, *in_end;

    if (prof == NULL)
	/* default if no profile for any instance domains */
	return 1;

    if ((p = __pmFindProfile(indom, prof)) == NULL)
	/* no profile for this indom => use global default */
	return (prof->state == PM_PROFILE_INCLUDE) ? 1 : 0;

    for (in=p->instances, in_end=in+p->instances_len; in < in_end; in++)
	if (*in == inst)
	    /* present in the list => inverse of default for this indom */
	    return (p->state == PM_PROFILE_INCLUDE) ? 0 : 1;

    /* not in the list => use default for this indom */
    return (p->state == PM_PROFILE_INCLUDE) ? 1 : 0;
}

void
__pmFreeProfile(__pmProfile *prof)
{
    __pmInDomProfile	*p, *p_end;

    if (prof != NULL) {
	if (prof->profile != NULL) {
	    for (p=prof->profile, p_end = p+prof->profile_len; p < p_end; p++) {
		if (p->instances)
		    free(p->instances);
	    }
	    if (prof->profile_len)
		free(prof->profile);
	}
	free(prof);
    }
}

int
pmAddProfile(pmInDom indom, int instlist_len, int instlist[])
{
    int			sts;
    __pmContext		*ctxp;
    __pmInDomProfile	*prof;

    if (indom == PM_INDOM_NULL && instlist != NULL)
	/* semantic disconnect! */
	return PM_ERR_PROFILESPEC;

    if ((sts = pmWhichContext()) < 0)
	return sts;
    ctxp = __pmHandleToPtr(sts);
    if (ctxp == NULL)
	return PM_ERR_NOCONTEXT;

    if (indom == PM_INDOM_NULL && instlist_len == 0) {
	_setGlobalState(ctxp, PM_PROFILE_INCLUDE);
	goto SUCCESS;
    }

    if ((prof = __pmFindProfile(indom, ctxp->c_instprof)) == NULL) {
	if ((prof = _newprof(indom, ctxp)) == NULL) {
	    /* fail */
	    PM_UNLOCK(ctxp->c_lock);
	    return -oserror();
	}
	else {
	    /* starting state: exclude all except the supplied list */
	    prof->state = PM_PROFILE_EXCLUDE;
	}
    }

    /* include all instances? */
    if (instlist_len == 0 || instlist == NULL) {
	/* include all instances in this domain */
	if (prof->instances)
	    free(prof->instances);
	prof->instances = NULL;
	prof->instances_len = 0;
	prof->state = PM_PROFILE_INCLUDE;
	goto SUCCESS;
    }

    switch (prof->state) {
    case PM_PROFILE_INCLUDE:
	/*
	 * prof->instances is an exclusion list (all else included)
	 * => traverse and remove the specified instances (if present)
	 */
	prof->instances = _subtract(
	    prof->instances, &prof->instances_len,
	    instlist, instlist_len);
	break;

    case PM_PROFILE_EXCLUDE:
	/*
	 * prof->instances is an inclusion list (all else excluded)
	 * => traverse and add the specified instances (if not already present)
	 */
	prof->instances = _union(
	    prof->instances, &prof->instances_len,
	    instlist, instlist_len);
	break;
    }

SUCCESS:
    ctxp->c_sent = 0;
    if (pmDebugOptions.profile) {
	char	strbuf[20];
	fprintf(stderr, "pmAddProfile() indom: %s\n", pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	__pmDumpProfile(stderr, indom, ctxp->c_instprof);
    }
    PM_UNLOCK(ctxp->c_lock);
    return 0;
}

int
pmDelProfile(pmInDom indom, int instlist_len, int instlist[])
{
    int			sts;
    __pmContext		*ctxp;
    __pmInDomProfile	*prof;

    if (indom == PM_INDOM_NULL && instlist != NULL)
	/* semantic disconnect! */
	return PM_ERR_PROFILESPEC;

    if ((sts = pmWhichContext()) < 0)
	return sts;
    ctxp = __pmHandleToPtr(sts);
    if (ctxp == NULL)
	return PM_ERR_NOCONTEXT;

    if (indom == PM_INDOM_NULL && instlist_len == 0) {
	_setGlobalState(ctxp, PM_PROFILE_EXCLUDE);
	goto SUCCESS;
    }

    if ((prof = __pmFindProfile(indom, ctxp->c_instprof)) == NULL) {
	if ((prof = _newprof(indom, ctxp)) == NULL) {
	    /* fail */
	    PM_UNLOCK(ctxp->c_lock);
	    return -oserror();
	}
	else {
	    /* starting state: include all except the supplied list */
	    prof->state = PM_PROFILE_EXCLUDE;
	}
    }

    /* include all instances? */
    if (instlist_len == 0 || instlist == NULL) {
	/* include all instances in this domain */
	if (prof->instances)
	    free(prof->instances);
	prof->instances = NULL;
	prof->instances_len = 0;
	prof->state = PM_PROFILE_EXCLUDE;
	goto SUCCESS;
    }

    switch (prof->state) {
    case PM_PROFILE_INCLUDE:
	/*
	 * prof->instances is an exclusion list (all else included)
	 * => traverse and add the specified instances (if not already present)
	 */
	prof->instances = _union(
	    prof->instances, &prof->instances_len,
	    instlist, instlist_len);
	break;

    case PM_PROFILE_EXCLUDE:
	/*
	 * prof->instances is an inclusion list (all else excluded)
	 * => traverse and remove the specified instances (if present)
	 */
	prof->instances = _subtract(
	    prof->instances, &prof->instances_len,
	    instlist, instlist_len);
	break;
    }

SUCCESS:
    ctxp->c_sent = 0;
    if (pmDebugOptions.profile) {
	char	strbuf[20];
	fprintf(stderr, "pmDelProfile() indom: %s\n", pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	__pmDumpProfile(stderr, indom, ctxp->c_instprof);
    }
    PM_UNLOCK(ctxp->c_lock);
    return 0;
}

void
__pmDumpProfile(FILE *f, int indom, const __pmProfile *pp)
{
    int			j;
    int			k;
    __pmInDomProfile	*prof;
    char		strbuf[20];

    fprintf(f, "Dump Instance Profile state=%s, %d profiles",
	pp->state == PM_PROFILE_INCLUDE ? "INCLUDE" : "EXCLUDE",
	pp->profile_len);
    if (indom != PM_INDOM_NULL)
	fprintf(f, ", dump restricted to indom=%d [%s]", 
	        indom, pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
    fprintf(f, "\n");

    for (prof=pp->profile, j=0; j < pp->profile_len; j++, prof++) {
	if (indom != PM_INDOM_NULL && indom != prof->indom)
	    continue;
	fprintf(f, "\tProfile [%d] indom=%d [%s] state=%s %d instances\n",
	    j, prof->indom, pmInDomStr_r(prof->indom, strbuf, sizeof(strbuf)),
	    (prof->state == PM_PROFILE_INCLUDE) ? "INCLUDE" : "EXCLUDE",
	    prof->instances_len);

	if (prof->instances_len) {
	    fprintf(f, "\t\tInstances:");
	    for (k=0; k < prof->instances_len; k++)
		fprintf(f, " [%d]", prof->instances[k]);
	    fprintf(f, "\n");
	}
    }
}
