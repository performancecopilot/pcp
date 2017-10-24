/*
 * Copyright (c) 2013-2014,2016-2017 Red Hat.
 * Copyright (c) 2008-2012 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "local.h"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static pmdaInterface dispatch;
static pmdaMetric *metrictab;
static int mtab_size;
static __pmnsTree *pmns;
static int need_refresh;
static pmdaIndom *indomtab;
static int itab_size;
static int *clustertab;
static int ctab_size;

static HV *metric_names;
static HV *metric_oneline;
static HV *metric_helptext;
static HV *indom_helptext;
static HV *indom_oneline;

static SV *fetch_func;
static SV *refresh_func;
static SV *instance_func;
static SV *store_cb_func;
static SV *fetch_cb_func;

int
clustertab_lookup(int cluster)
{
    int i, found = 0;

    for (i = 0; i < ctab_size; i++) {
	if (cluster == clustertab[i]) {
	    found = 1;
	    break;
	}
    }
    return found;
}

void
clustertab_replace(int index, int cluster)
{
    if (index >= 0 && index < ctab_size)
	clustertab[index] = cluster;
    else
	warn("invalid cluster table replacement requested");
}

void
clustertab_scratch()
{
    memset(clustertab, -1, sizeof(int) * ctab_size);
}

void
clustertab_refresh(int index)
{
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVuv(clustertab[index])));
    PUTBACK;

    perl_call_sv(refresh_func, G_VOID);

    SPAGAIN;
    PUTBACK;
    FREETMPS;
    LEAVE;
}

void
refresh(int numpmid, pmID *pmidlist)
{
    int i, numclusters = 0;
    __pmID_int *pmid;

    /* Create list of affected clusters from pmidlist
     * Note: we overwrite the initial cluster array here, to avoid
     * allocating memory.  The initial array contains all possible
     * clusters whereas we (possibly) construct a subset here.  We
     * do not touch ctab_size at all, however, which lets us reuse
     * the preallocated array space on every fetch.
     */
    clustertab_scratch();
    for (i = 0; i < numpmid; i++) {
	pmid = (__pmID_int *) &pmidlist[i];
	if (clustertab_lookup(pmid->cluster) == 0)
	    clustertab_replace(numclusters++, pmid->cluster);
    }

    /* For each unique cluster, call the cluster refresh method */
    for (i = 0; i < numclusters; i++)
	clustertab_refresh(i);
}

void
pmns_refresh(void)
{
    char *pmid, *next;
    I32 idsize;
    SV *metric;
    int sts;

    if (pmns)
	__pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0)
	croak("failed to create namespace root: %s", pmErrStr(sts));

    hv_iterinit(metric_names);
    while ((metric = hv_iternextsv(metric_names, &pmid, &idsize)) != NULL) {
	unsigned int domain, cluster, item, id;

	domain = strtoul(pmid, &next, 10);
	cluster = strtoul(next+1, &next, 10);
	item = strtoul(next+1, &next, 10);
	id = pmid_build(domain, cluster, item);
	if ((sts = __pmAddPMNSNode(pmns, id, SvPV_nolen(metric))) < 0)
	    croak("failed to add metric %s(%s) to namespace: %s",
		SvPV_nolen(metric), pmIDStr(id), pmErrStr(sts));
    }

    pmdaTreeRebuildHash(pmns, mtab_size); /* for reverse (pmid->name) lookups */
    need_refresh = 0;
}

int
pmns_desc(pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    if (need_refresh)
	pmns_refresh();
    return pmdaDesc(pmid, desc, ep);
}

int
pmns_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    if (need_refresh)
	pmns_refresh();
    return pmdaTreePMID(pmns, name, pmid);
}

int
pmns_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    if (need_refresh)
	pmns_refresh();
    return pmdaTreeName(pmns, pmid, nameset);
}

int
pmns_children(const char *name, int traverse, char ***kids, int **sts, pmdaExt *pmda)
{
    if (need_refresh)
	pmns_refresh();
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

void
pmns_write(void)
{
    __pmnsNode *node;
    char *pppenv = getenv("PCP_PERL_PMNS");
    int root = pppenv ? strcmp(pppenv, "root") == 0 : 0;
    char *prefix = root ? "\t" : "";

    pmns_refresh();

    if (root)
	printf("root {\n");
    for (node = pmns->root->first; node != NULL; node = node->next)
	printf("%s%s\t%u:*:*\n", prefix, node->name, dispatch.domain);
    if (root)
	printf("}\n");
}

void
domain_write(void)
{
    char *p, name[512] = { 0 };
    int i, len = strlen(pmProgname);

    if (len >= sizeof(name) - 1)
	len = sizeof(name) - 2;
    p = pmProgname;
    if (strncmp(pmProgname, "pmda", 4) == 0)
	p += 4;
    for (i = 0; i < len; i++)
	name[i] = toupper(p[i]);
    printf("#define %s %u\n", name, dispatch.domain);
}

void
prefetch(void)
{
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    PUTBACK;

    perl_call_sv(fetch_func, G_VOID|G_NOARGS);

    SPAGAIN;
    PUTBACK;
    FREETMPS;
    LEAVE;
}

int
fetch_wrapper(int numpmid, pmID *pmidlist, pmResult **rp, pmdaExt *pmda)
{
    if (need_refresh)
	pmns_refresh();
    if (fetch_func)
	prefetch();
    if (refresh_func)
	refresh(numpmid, pmidlist);
    return pmdaFetch(numpmid, pmidlist, rp, pmda);
}

int
instance_index(pmInDom indom)
{
    int i;

    for (i = 0; i < itab_size; i++)
	if (indomtab[i].it_indom == indom)
	    return i;
    return PM_INDOM_NULL;
}

void
preinstance(pmInDom indom)
{
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVuv(indom)));
    PUTBACK;

    perl_call_sv(instance_func, G_VOID);

    SPAGAIN;
    PUTBACK;
    FREETMPS;
    LEAVE;
}

int
instance_wrapper(pmInDom indom, int a, char *b, __pmInResult **rp, pmdaExt *pmda)
{
    if (need_refresh)
	pmns_refresh();
    if (instance_func)
	preinstance(instance_index(indom));
    return pmdaInstance(indom, a, b, rp, pmda);
}

void
timer_callback(int afid, void *data)
{
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSViv(local_timer_get_cookie(afid))));
    PUTBACK;

    perl_call_sv(local_timer_get_callback(afid), G_VOID);

    SPAGAIN;
    PUTBACK;
    FREETMPS;
    LEAVE;
}

void
input_callback(SV *input_cb_func, int data, char *string)
{
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSViv(data)));
    XPUSHs(sv_2mortal(newSVpv(string,0)));
    PUTBACK;

    perl_call_sv(input_cb_func, G_VOID);

    SPAGAIN;
    PUTBACK;
    FREETMPS;
    LEAVE;
}

int
fetch_callback(pmdaMetric *metric, unsigned int inst, pmAtomValue *atom)
{
    dSP;
    __pmID_int	*pmid;
    int		sts;
    STRLEN	n_a;	/* required by older Perl versions, used in POPpx */

    ENTER;
    SAVETMPS;	/* allows us to tidy our perl stack changes later */

    (void)n_a;
    pmid = (__pmID_int *) &metric->m_desc.pmid;

    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVuv(pmid->cluster)));
    XPUSHs(sv_2mortal(newSVuv(pmid->item)));
    XPUSHs(sv_2mortal(newSVuv(inst)));
    PUTBACK;

    sts = perl_call_sv(fetch_cb_func, G_ARRAY);
    SPAGAIN;	/* refresh local perl stack pointer after call */
    if (sts != 2) {
	croak("fetch CB error (returned %d values, expected 2)", sts); 
	sts = -EINVAL;
	goto fetch_end;
    }
    sts = POPi;		/* pop function return status */
    if (sts < 0) {
	goto fetch_end;
    }
    else if (sts == 0) {
	sts = POPi;
	goto fetch_end;
    }

    sts = PMDA_FETCH_STATIC;
    switch (metric->m_desc.type) {	/* pop result value */
	case PM_TYPE_32:	atom->l = POPi; break;
	case PM_TYPE_U32:	atom->ul = POPi; break;
	case PM_TYPE_64:	atom->ll = POPl; break;
	case PM_TYPE_U64:	atom->ull = POPl; break;
	case PM_TYPE_FLOAT:	atom->f = POPn; break;
	case PM_TYPE_DOUBLE:	atom->d = POPn; break;
	case PM_TYPE_STRING:	{
	    atom->cp = strdup(POPpx);
	    sts = PMDA_FETCH_DYNAMIC;
	    break;
	}
    }

fetch_end:
    PUTBACK;
    FREETMPS;
    LEAVE;	/* fix up the perl stack, freeing anything we created */
    return sts;
}

int
store_callback(__pmID_int *pmid, unsigned int inst, pmAtomValue av, int type)
{
    dSP;
    int sts;
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVuv(pmid->cluster)));
    XPUSHs(sv_2mortal(newSVuv(pmid->item)));
    XPUSHs(sv_2mortal(newSVuv(inst)));
    switch (type) {
	case PM_TYPE_32:     XPUSHs(sv_2mortal(newSViv(av.l))); break;
	case PM_TYPE_U32:    XPUSHs(sv_2mortal(newSVuv(av.ul))); break;
	case PM_TYPE_64:     XPUSHs(sv_2mortal(newSVuv(av.ll))); break;
	case PM_TYPE_U64:    XPUSHs(sv_2mortal(newSVuv(av.ull))); break;
	case PM_TYPE_FLOAT:  XPUSHs(sv_2mortal(newSVnv(av.f))); break;
	case PM_TYPE_DOUBLE: XPUSHs(sv_2mortal(newSVnv(av.d))); break;
	case PM_TYPE_STRING: XPUSHs(sv_2mortal(newSVpv(av.cp,0)));break;
    }
    PUTBACK;

    sts = perl_call_sv(store_cb_func, G_SCALAR);
    SPAGAIN;	/* refresh local perl stack pointer after call */
    if (sts != 1) {
	croak("store CB error (returned %d values, expected 1)", sts); 
	sts = -EINVAL;
	goto store_end;
    }
    sts = POPi;				/* pop function return status */

store_end:
    PUTBACK;
    FREETMPS;
    LEAVE;	/* fix up the perl stack, freeing anything we created */
    return sts;
}

int
store(pmResult *result, pmdaExt *pmda)
{
    int		i, j;
    int		type;
    int		sts;
    pmAtomValue	av;
    pmValueSet	*vsp;
    __pmID_int	*pmid;

    if (need_refresh)
	pmns_refresh();

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmid = (__pmID_int *)&vsp->pmid;

	/* need to find the type associated with this PMID */
	for (j = 0; j < mtab_size; j++)
	    if (metrictab[j].m_desc.pmid == *(pmID *)pmid)
		break;
	if (j == mtab_size)
	    return PM_ERR_PMID;
	type = metrictab[j].m_desc.type;

	for (j = 0; j < vsp->numval; j++) {
	    sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j],type, &av, type);
	    if (sts < 0)
		return sts;
	    sts = store_callback(pmid, vsp->vlist[j].inst, av, type);
	    if (sts < 0)
		return sts;
	}
    }
    return 0;
}

int
text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    const char *hash;
    int size;
    SV **sv;
    HV *hv;

    if (need_refresh)
	pmns_refresh();

    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	hash = pmIDStr((pmID)ident);
	size = strlen(hash);
	if (type & PM_TEXT_ONELINE)
	    hv = metric_oneline;
	else
	    hv = metric_helptext;
    }
    else {
	hash = pmInDomStr((pmInDom)ident);
	size = strlen(hash);
	if (type & PM_TEXT_ONELINE)
	    hv = indom_oneline;
	else
	    hv = indom_helptext;
    }
    if (hv_exists(hv, hash, size))
	sv = hv_fetch(hv, hash, size, 0);
    else
	sv = NULL;

    if (sv && (*sv))
	*buffer = SvPV_nolen(*sv);
    else
	*buffer = NULL;
    return (*buffer == NULL) ? PM_ERR_TEXT : 0;
}

/*
 * Converts Perl hash ref like {'foo' => \&data, 'boo' => \&data}
 * into an instance structure (indom).
 */
static int
update_hash_indom(SV *insts, pmInDom indom)
{
    int sts;
    SV *data;
    I32 instsize;
    char *instance;
    HV *ihash = (HV *) SvRV(insts);

    sts = pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    if (sts < 0)
	warn("pmda cache inactivation failed: %s", pmErrStr(sts));

    hv_iterinit(ihash);
    while ((data = hv_iternextsv(ihash, &instance, &instsize)) != NULL)
	pmdaCacheStore(indom, PMDA_CACHE_ADD, instance, SvREFCNT_inc(data));

    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    if (sts < 0)
	warn("pmda cache persistence failed: %s", pmErrStr(sts));

    return 0;
}

/*
 * Free all memory associated with a Perl list based indom
 */
static void
release_list_indom(pmdaInstid *instances, int numinst)
{
    int i;

    if (instances && numinst > 0) {
	for (i = 0; i < numinst; i++)
	    free(instances[i].i_name);	/* update_list_indom strdup */
	free(instances);	/* update_list_indom calloc */
    }
}

/*
 * Converts Perl list ref like [a => 'foo', b => 'boo'] into an indom.
 */
static int
update_list_indom(SV *insts, pmdaInstid **set)
{
    int	i, len;
    SV **id;
    SV **name;
    AV *ilist = (AV *) SvRV(insts);
    pmdaInstid *instances;

    if ((len = av_len(ilist)) == -1) {	/* empty */
	*set = NULL;
	return 0;
    }
    if (len++ % 2 == 0) {
	warn("invalid instance list (length must be a multiple of 2)");
	return -1;
    }

    len /= 2;
    instances = (pmdaInstid *) calloc(len, sizeof(pmdaInstid));
    if (instances == NULL) {
	warn("insufficient memory for instance array");
	return -1;
    }
    for (i = 0; i < len; i++) {
	id = av_fetch(ilist,i*2,0);
	name = av_fetch(ilist,i*2+1,0);
	instances[i].i_inst = SvIV(*id);
	instances[i].i_name = strdup(SvPV_nolen(*name));
	if (instances[i].i_name == NULL) {
	    release_list_indom(instances, i);
	    warn("insufficient memory for instance array names");
	    return -1;
	}
    }
    *set = instances;
    return len;
}

/*
 * Reload a Perl instance reference into a populated indom.
 * This interface allows either the hash or list formats,
 * but only the hash format can be persisted and reloaded.
 */
static int
reload_indom(SV *insts, pmInDom indom)
{
    SV *rv = (SV *) SvRV(insts);

    if (! SvROK(insts)) {
	warn("expected a reference for instances argument");
	return -1;
    }
    if (SvTYPE(rv) == SVt_PVHV)
	(void) pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    else if (SvTYPE(rv) != SVt_PVAV)
        warn("instance argument is neither an array nor hash reference");
    return 0;
}

/*
 * Converts a Perl instance reference into a populated indom.
 * This interface handles either the hash or list formats.
 */
static int
update_indom(SV *insts, pmInDom indom, pmdaInstid **set)
{
    SV *rv = (SV *) SvRV(insts);

    if (! SvROK(insts)) {
	warn("expected a reference for instances argument");
	return -1;
    }
    if (SvTYPE(rv) == SVt_PVAV)
	return update_list_indom(insts, set);
    if (SvTYPE(rv) == SVt_PVHV)
	return update_hash_indom(insts, indom);
    warn("instance argument is neither an array nor hash reference");
    return -1;
}


MODULE = PCP::PMDA		PACKAGE = PCP::PMDA


pmdaInterface *
new(CLASS,name,domain)
	char *	CLASS
	char *	name
	int	domain
    PREINIT:
	int	sep;
	char *	p;
	char *	logfile;
	char *	pmdaname;
	char	helpfile[256];
    CODE:
	pmProgname = name;
	RETVAL = &dispatch;
	logfile = local_strdup_suffix(name, ".log");
	pmdaname = local_strdup_prefix("pmda", name);
	__pmSetProgname(pmdaname);
	sep = __pmPathSeparator();
	if ((p = getenv("PCP_PERL_DEBUG")) != NULL) {
	    if (pmSetDebug(p) < 0)
		fprintf(stderr, "unrecognized debug options specification (%s)\n", p);
	}
#ifndef IS_MINGW
	setsid();
#endif
	atexit(&local_atexit);
	pmsprintf(helpfile, sizeof(helpfile), "%s%c%s%c" "help",
			pmGetConfig("PCP_PMDAS_DIR"), sep, name, sep);
	if (access(helpfile, R_OK) != 0) {
	    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmdaname, domain,
			logfile, NULL);
	    dispatch.version.four.text = text;
	}
	else {
	    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmdaname, domain,
			logfile, strdup(helpfile));
	}
	dispatch.version.four.fetch = fetch_wrapper;
	dispatch.version.four.instance = instance_wrapper;
	dispatch.version.four.desc = pmns_desc;
	dispatch.version.four.pmid = pmns_pmid;
	dispatch.version.four.name = pmns_name;
	dispatch.version.four.children = pmns_children;

	if (!local_install())
	    pmdaOpenLog(&dispatch);
	metric_names = newHV();
	metric_oneline = newHV();
	metric_helptext = newHV();
	indom_helptext = newHV();
	indom_oneline = newHV();
    OUTPUT:
	RETVAL

int
pmda_pmid(cluster,item)
	unsigned int	cluster
	unsigned int	item
    CODE:
	RETVAL = pmid_build(dispatch.domain, cluster, item);
    OUTPUT:
	RETVAL

SV *
pmda_pmid_name(cluster,item)
	unsigned int	cluster
	unsigned int	item
    PREINIT:
	const char	*name;
	SV		**rval;
    CODE:
	name = pmIDStr(pmid_build(dispatch.domain, cluster, item));
	rval = hv_fetch(metric_names, name, strlen(name), 0);
	if (!rval || !(*rval))
	    XSRETURN_UNDEF;
	RETVAL = newSVsv(*rval);
    OUTPUT:
	RETVAL

SV *
pmda_pmid_text(cluster,item)
	unsigned int	cluster
	unsigned int	item
    PREINIT:
	const char	*name;
	SV		**rval;
    CODE:
	name = pmIDStr(pmid_build(dispatch.domain, cluster, item));
	rval = hv_fetch(metric_oneline, name, strlen(name), 0);
	if (!rval || !(*rval))
	    XSRETURN_UNDEF;
	RETVAL = newSVsv(*rval);
    OUTPUT:
	RETVAL

SV *
pmda_inst_name(index,instance)
	unsigned int	index
	int		instance
    PREINIT:
	int		i, sts;
	char *		name;
	pmdaIndom *	p;
    CODE:
	if (index >= itab_size)	/* is this a valid indom */
	    XSRETURN_UNDEF;
	p = indomtab + index;
	if (!p->it_set)	{ /* was this indom previously setup via a hash? */
	    sts = pmdaCacheLookup(p->it_indom, instance, &name, NULL);
	    if (sts != PMDA_CACHE_ACTIVE)
		XSRETURN_UNDEF;
	    RETVAL = newSVpv(name,0);
	}
	else {	/* we've been handed an array-based indom */
	    /* Optimistic (fast) direct lookup first, then iterate */
	    i = instance;
	    if (i > p->it_numinst || i < 0 || instance != p->it_set[i].i_inst) {
		for (i = 0; i < p->it_numinst; i++)
		    if (instance == p->it_set[i].i_inst)
			break;
		if (i == p->it_numinst)
		    XSRETURN_UNDEF;
	    }
	    RETVAL = newSVpv(p->it_set[i].i_name,0);
	}
    OUTPUT:
	RETVAL

SV *
pmda_inst_lookup(index,instance)
	unsigned int	index
	int		instance
    PREINIT:
	pmdaIndom *	p;
	SV *		svp;
	int		sts;
    CODE:
	if (index >= itab_size)	/* is this a valid indom */
	    XSRETURN_UNDEF;
	p = indomtab + index;
	if (p->it_set)	/* was this indom previously setup via an array? */
	    XSRETURN_UNDEF;
	sts = pmdaCacheLookup(p->it_indom, instance, NULL, (void *)&svp);
	if (sts != PMDA_CACHE_ACTIVE)
	    XSRETURN_UNDEF;
	RETVAL = SvREFCNT_inc(svp);
    OUTPUT:
	RETVAL

int
pmda_units(dim_space,dim_time,dim_count,scale_space,scale_time,scale_count)
	unsigned int	dim_space
	unsigned int	dim_time
	unsigned int	dim_count
	unsigned int	scale_space
	unsigned int	scale_time
	unsigned int	scale_count
    PREINIT:
	pmUnits	units;
    CODE:
	units.pad = 0;
	units.dimSpace = dim_space;	units.scaleSpace = scale_space;
	units.dimTime = dim_time;	units.scaleTime = scale_time;
	units.dimCount = dim_count;	units.scaleCount = scale_count;
	RETVAL = *(int *)(&units);
    OUTPUT:
	RETVAL

char *
pmda_config(name)
	char * name
    CODE:
	RETVAL = pmGetConfig(name);
	if (!RETVAL)
	    XSRETURN_UNDEF;
    OUTPUT:
	RETVAL

char *
pmda_uptime(now)
	int	now
    PREINIT:
	static char s[32];
	size_t sz = sizeof(s);
	int days, hours, mins, secs;
    CODE:
	days = now / (60 * 60 * 24);
	now %= (60 * 60 * 24);
	hours = now / (60 * 60);
	now %= (60 * 60);
	mins = now / 60;
	now %= 60;
	secs = now;

	if (days > 1)
	    pmsprintf(s, sz, "%ddays %02d:%02d:%02d", days, hours, mins, secs);
	else if (days == 1)
	    pmsprintf(s, sz, "%dday %02d:%02d:%02d", days, hours, mins, secs);
	else
	    pmsprintf(s, sz, "%02d:%02d:%02d", hours, mins, secs);
	RETVAL = s;
    OUTPUT:
	RETVAL

int
pmda_long()
    CODE:
	RETVAL = (sizeof(long) == 4) ? PM_TYPE_32 : PM_TYPE_64;
    OUTPUT:
	RETVAL

int
pmda_ulong()
    CODE:
	RETVAL = (sizeof(unsigned long) == 4) ? PM_TYPE_U32 : PM_TYPE_U64;
    OUTPUT:
	RETVAL

int
pmda_install()
    CODE:
	RETVAL = local_install();
    OUTPUT:
	RETVAL

void
error(self,message)
	pmdaInterface *self
	char *	message
    PREINIT:
	(void)self;
    CODE:
	__pmNotifyErr(LOG_ERR, "%s", message);

int
set_user(self,username)
	pmdaInterface *self
	char * username
    PREINIT:
	(void)self;
    CODE:
	RETVAL = __pmSetProcessIdentity(username);
    OUTPUT:
	RETVAL

void
set_fetch(self,function)
	pmdaInterface *self
	SV *	function
    PREINIT:
	(void)self;
    CODE:
	if (function != (SV *)NULL) {
	    fetch_func = newSVsv(function);
	}

void
set_refresh(self,function)
	pmdaInterface *self
	SV *	function
    PREINIT:
	(void)self;
    CODE:
	if (function != (SV *)NULL) {
	    refresh_func = newSVsv(function);
	}

void
set_instance(self,function)
	pmdaInterface *self
	SV *	function
    PREINIT:
	(void)self;
    CODE:
	if (function != (SV *)NULL) {
	    instance_func = newSVsv(function);
	}

void
set_store_callback(self,cb_function)
	pmdaInterface *self
	SV *	cb_function
    CODE:
	if (cb_function != (SV *)NULL) {
	    store_cb_func = newSVsv(cb_function);
	    self->version.four.store = store;
	}

void
set_fetch_callback(self,cb_function)
	pmdaInterface *self
	SV *	cb_function
    CODE:
	if (cb_function != (SV *)NULL) {
	    fetch_cb_func = newSVsv(cb_function);
	    pmdaSetFetchCallBack(self, fetch_callback);
	}

void
set_inet_socket(self,port)
	pmdaInterface *self
	int	port
    CODE:
	self->version.four.ext->e_io = pmdaInet;
	self->version.four.ext->e_port = port;

void
set_ipv6_socket(self,port)
	pmdaInterface *self
	int	port
    CODE:
	self->version.four.ext->e_io = pmdaIPv6;
	self->version.four.ext->e_port = port;

void
set_unix_socket(self,socket_name)
	pmdaInterface *self
	char *	socket_name
    CODE:
	self->version.four.ext->e_io = pmdaUnix;
	self->version.four.ext->e_sockname = socket_name;

void
clear_metrics(self)
	pmdaInterface *self
    PREINIT:
	(void)self;
    CODE:
	need_refresh = 1;
	if (clustertab)
	    free(clustertab);
	ctab_size = 0;
	if (metrictab)
	    free(metrictab);
	mtab_size = 0;
	hv_clear(metric_names);
	hv_clear(metric_oneline);
	hv_clear(metric_helptext);

void
add_metric(self,pmid,type,indom,sem,units,name,help,longhelp)
	pmdaInterface *self
	int	pmid
	int	type
	int	indom
	int	sem
	int	units
	char *	name
	char *	help
	char *	longhelp
    PREINIT:
	pmdaMetric * p;
	__pmID_int * pmidp;
	const char * hash;
	int          size;
	(void)self;
    CODE:
	need_refresh = 1;
	pmidp = (__pmID_int *)&pmid;
	if (!clustertab_lookup(pmidp->cluster)) {
	    size = sizeof(int) * (ctab_size + 1);
	    clustertab = (int *)realloc(clustertab, size);
	    if (clustertab)
		clustertab[ctab_size++] = pmidp->cluster;
	    else {
		warn("unable to allocate memory for cluster table");
		ctab_size = 0;
		XSRETURN_UNDEF;
	    }
	}

	size = sizeof(pmdaMetric) * (mtab_size + 1);
	metrictab = (pmdaMetric *)realloc(metrictab, size);
	if (metrictab == NULL) {
	    warn("unable to allocate memory for metric table");
	    mtab_size = 0;
	    XSRETURN_UNDEF;
	}

	p = metrictab + mtab_size++;
	p->m_user = NULL;	p->m_desc.pmid = *(pmID *)&pmid;
	p->m_desc.type = type;	p->m_desc.indom = *(pmInDom *)&indom;
	p->m_desc.sem = sem;	p->m_desc.units = *(pmUnits *)&units;

	hash = pmIDStr(pmid);
	size = strlen(hash);
	hv_store(metric_names, hash, size, newSVpv(name,0), 0);
	if (help)
	    hv_store(metric_oneline, hash, size, newSVpv(help,0), 0);
	if (longhelp)
	    hv_store(metric_helptext, hash, size, newSVpv(longhelp,0), 0);

void
clear_indoms(self)
	pmdaInterface *self
    PREINIT:
	(void)self;
    CODE:
	if (indomtab)
	    free(indomtab);
	itab_size = 0;
	if (metrictab)
	    free(metrictab);
	mtab_size = 0;
	hv_clear(indom_oneline);
	hv_clear(indom_helptext);

int
add_indom(self,indom,insts,help,longhelp)
	pmdaInterface *	self
	int	indom
	SV *	insts
	char *	help
	char *	longhelp
    PREINIT:
	pmdaIndom  * p;
	const char * hash;
	int          sts, size;
	(void)self;
    CODE:
	size = sizeof(pmdaIndom) * (itab_size + 1);
	indomtab = (pmdaIndom *)realloc(indomtab, size);
	if (indomtab == NULL) {
	    warn("unable to allocate memory for indom table");
	    itab_size = 0;
	    XSRETURN_UNDEF;
	}
	indom = pmInDom_build(self->domain, indom);
	reload_indom(insts, indom);

	p = indomtab + itab_size;
	memset(p, 0, sizeof(pmdaIndom));
	p->it_indom = indom;

	sts = update_indom(insts, indom, &p->it_set);
	if (sts < 0)
	    XSRETURN_UNDEF;
	p->it_numinst = sts;
	RETVAL = itab_size++;	/* used in calls to replace_indom() */

	hash = pmInDomStr(indom);
	size = strlen(hash);
	if (help)
	    hv_store(indom_oneline, hash, size, newSVpv(help,0), 0);
	if (longhelp)
	    hv_store(indom_helptext, hash, size, newSVpv(longhelp,0), 0);
    OUTPUT:
	RETVAL

int
replace_indom(self,index,insts)
	pmdaInterface *	self
	unsigned int	index
	SV *		insts
    PREINIT:
	pmdaIndom *	p;
	int		sts;
	(void)self;
    CODE:
	if (index >= itab_size) {
	    warn("attempt to replace non-existent instance domain");
	    XSRETURN_UNDEF;
	}
	else {
	    p = indomtab + index;
	    /* was this indom previously setup via an array? */
	    if (p->it_set) {
		release_list_indom(p->it_set, p->it_numinst);
		p->it_numinst = 0;
	    }
	    sts = update_indom(insts, p->it_indom, &p->it_set);
	    if (sts < 0)
		XSRETURN_UNDEF;
	    p->it_numinst = sts;
	    RETVAL = sts;
	}
    OUTPUT:
	RETVAL

int
load_indom(self,index)
	pmdaInterface *	self
	unsigned int	index
    PREINIT:
	pmdaIndom *	p;
	int		sts;
	(void)self;
    CODE:
	if (index >= itab_size) {
	    warn("attempt to load non-existent instance domain");
	    XSRETURN_UNDEF;
	}
	else {
	    p = indomtab + index;
	    /* is this indom setup via an array? (must be hash) */
	    if (p->it_set) {
		warn("cannot load an array instance domain");
		XSRETURN_UNDEF;
	    }
	    sts = pmdaCacheOp(p->it_indom, PMDA_CACHE_LOAD);
	    if (sts < 0)
		warn("pmda cache load failed: %s", pmErrStr(sts));
	    RETVAL = sts;
	}
    OUTPUT:
	RETVAL

int
add_timer(self,timeout,callback,data)
	pmdaInterface *	self
	double	timeout
	SV *	callback
	int	data
    PREINIT:
	(void)self;
    CODE:
	if (local_install() || !callback)
	    XSRETURN_UNDEF;
	RETVAL = local_timer(timeout, newSVsv(callback), data);
    OUTPUT:
	RETVAL

int
add_pipe(self,command,callback,data)
	pmdaInterface *self
	char *	command
	SV *	callback
	int	data
    PREINIT:
	(void)self;
    CODE:
	if (local_install() || !callback)
	    XSRETURN_UNDEF;
	RETVAL = local_pipe(command, newSVsv(callback), data);
    OUTPUT:
	RETVAL

int
add_tail(self,filename,callback,data)
	pmdaInterface *self
	char *	filename
	SV *	callback
	int	data
    PREINIT:
	(void)self;
    CODE:
	if (local_install() || !callback)
	    XSRETURN_UNDEF;
	RETVAL = local_tail(filename, newSVsv(callback), data);
    OUTPUT:
	RETVAL

int
add_sock(self,hostname,port,callback,data)
	pmdaInterface *self
	char *	hostname
	int	port
	SV *	callback
	int	data
    PREINIT:
	(void)self;
    CODE:
	if (local_install() || !callback)
	    XSRETURN_UNDEF;
	RETVAL = local_sock(hostname, port, newSVsv(callback), data);
    OUTPUT:
	RETVAL

int
put_sock(self,id,output)
	pmdaInterface *self
	int	id
	char *	output
    PREINIT:
	size_t	length = strlen(output);
	(void)self;
    CODE:
	RETVAL = __pmWrite(local_files_get_descriptor(id), output, length);
    OUTPUT:
	RETVAL

void
log(self,message)
	pmdaInterface *self
	char *	message
    PREINIT:
	(void)self;
    CODE:
	__pmNotifyErr(LOG_INFO, "%s", message);

void
err(self,message)
	pmdaInterface *self
	char *	message
    PREINIT:
	(void)self;
    CODE:
	__pmNotifyErr(LOG_ERR, "%s", message);

void
connect_pmcd(self)
	pmdaInterface *self
    CODE:
	/*
	 * Call pmdaConnect() to complete the PMDA's IPC channel setup
	 * and complete the connection handshake with pmcd.
	 */
	if (!local_install()) {
	    /*
	     * On success pmdaConnect sets PMDA_EXT_CONNECTED in e_flags
	     * ... this is used in the guard below to stop run() calling
	     * pmdaConnect() again.
	     */
	    pmdaConnect(self);
	}

void
run(self)
	pmdaInterface *self
    CODE:
	if (getenv("PCP_PERL_PMNS") != NULL)
	    pmns_write();	/* generate ascii namespace */
	else if (getenv("PCP_PERL_DOMAIN") != NULL)
	    domain_write();	/* generate the domain header */
	else {		/* or normal operating mode ... */
	    pmns_refresh();
	    pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);
	    if ((self->version.any.ext->e_flags & PMDA_EXT_CONNECTED) != PMDA_EXT_CONNECTED) {
		/*
		 * connect_pmcd() not called before, so need pmdaConnect()
		 * here before falling into the PDU-driven mainloop
		 */
		pmdaConnect(self);
	    }
	    local_pmdaMain(self);
	}

void
debug_metric(self)
	pmdaInterface *self
    PREINIT:
	int	i;
	(void)self;
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	fprintf(stderr, "metric table size = %d\n", mtab_size);
	for (i = 0; i < mtab_size; i++) {
	    fprintf(stderr, "metric idx = %d\n\tpmid = %s\n\ttype = %u\n"
			"\tindom= %d\n\tsem  = %u\n\tunits= %u\n",
		i, pmIDStr(metrictab[i].m_desc.pmid), metrictab[i].m_desc.type,
		(int)metrictab[i].m_desc.indom, metrictab[i].m_desc.sem,
		*(unsigned int *)&metrictab[i].m_desc.units);
	}

void
debug_indom(self)
	pmdaInterface *self
    PREINIT:
	int	i,j;
	(void)self;
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	fprintf(stderr, "indom table size = %d\n", itab_size);
	for (i = 0; i < itab_size; i++) {
	    fprintf(stderr, "indom idx = %d\n\tindom = %d\n"
			    "\tninst = %u\n\tiptr = 0x%p\n",
		    i, *(int *)&indomtab[i].it_indom, indomtab[i].it_numinst,
		    indomtab[i].it_set);
	    for (j = 0; j < indomtab[i].it_numinst; j++) {
		fprintf(stderr, "\t\tid=%d name=%s\n",
		    indomtab[i].it_set[j].i_inst, indomtab[i].it_set[j].i_name);
	    }
	}

void
debug_init(self)
	pmdaInterface *self
    CODE:
	/* NB: debugging only (used in test.pl to verify state) */
	pmdaInit(self, indomtab, itab_size, metrictab, mtab_size);

