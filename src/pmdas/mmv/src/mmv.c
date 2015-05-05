/*
 * Copyright (c) 2012-2014 Red Hat.
 * Copyright (c) 2009-2010 Aconex. All Rights Reserved.
 * Copyright (c) 1995-2000,2009 Silicon Graphics, Inc. All Rights Reserved.
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
 *
 *
 * MMV PMDA
 *
 * This PMDA uses specially formatted files in either /var/tmp/mmv or some
 * other directory, as specified on the command line.  Each file represents
 * a separate "cluster" of values with flat name structure for each cluster.
 * Names for the metrics are optionally prepended with mmv and then the name
 * of the file (by default - this can be changed).
 */

#include "pmapi.h"
#include "mmv_stats.h"
#include "mmv_dev.h"
#include "impl.h"
#include "pmda.h"
#include "./domain.h"
#include <sys/stat.h>
#include <ctype.h>

static int isDSO = 1;
static char *username;

/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

static pmdaMetric * metrics;
static int mcnt;
static pmdaIndom * indoms;
static int incnt;

static int reload;
static __pmnsTree * pmns;
static int statsdir_code;		/* last statsdir stat code */
static time_t statsdir_ts;		/* last statsdir timestamp */
static char * prefix = "mmv";

static char * pcptmpdir;		/* probably /var/tmp */
static char * pcpvardir;		/* probably /var/pcp */
static char * pcppmdasdir;		/* probably /var/pcp/pmdas */
static char pmnsdir[MAXPATHLEN];	/* pcpvardir/pmns */
static char statsdir[MAXPATHLEN];	/* pcptmpdir/<prefix> */

typedef struct {
    char *	name;			/* strdup client name */
    void *	addr;			/* mmap */
    mmv_disk_value_t * values;		/* values in mmap */
    mmv_disk_metric_t *	metrics;	/* metric descs in mmap */
    int		vcnt;			/* number of values */
    int		mcnt;			/* number of metrics */
    pid_t	pid;			/* process identifier */
    int		cluster;		/* cluster identifier */
    __int64_t	len;			/* mmap region len */
    __uint64_t	gen;			/* generation number on open */
} stats_t;

static stats_t * slist;
static int scnt;

/*
 * Choose an unused cluster ID while honouring specific requests.
 * If a specific (non-zero) cluster is requested we always use it.
 */
static int
choose_cluster(int requested, const char *path)
{
    int i;

    if (!requested) {
	int next_cluster = 1;

	for (i = 0; i < scnt; i++) {
	    if (slist[i].cluster == next_cluster) {
		next_cluster++;
		i = 0;	/* restart, we're filling holes */
	    }
	}
	return next_cluster;
    }

    for (i = 0; i < scnt; i++) {
	if (slist[i].cluster == requested) {
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG,
				"MMV: %s: duplicate cluster %d in use",
				pmProgname, requested);
	    break;
	}
    }
    return requested;
}

static int
create_client_stat(const char *client, const char *path, size_t size)
{
    int fd;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_client_stat: %s, %s", client, path);

    if ((fd = open(path, O_RDONLY)) >= 0) {
	void *m = __pmMemoryMap(fd, size, 0);

	close(fd);
	if (m != NULL) {
	    mmv_disk_header_t * hdr = (mmv_disk_header_t *)m;
	    int cluster;

	    if (strncmp(hdr->magic, "MMV", 4)) {
		__pmMemoryUnmap(m, size);
		return -EINVAL;
	    }

	    if (hdr->version != MMV_VERSION) {
		__pmNotifyErr(LOG_ERR, "%s: %s client version %d "
				"not supported (current is %d)",
				pmProgname, prefix, hdr->version, MMV_VERSION);
		__pmMemoryUnmap(m, size);
		return -ENOSYS;
	    }

	    if (!hdr->g1 || hdr->g1 != hdr->g2) {
		/* still in flux, wait till next time */
		__pmMemoryUnmap(m, size);
		return -EAGAIN;
	    }

	    /* optionally verify the creator PID is running */
	    if (hdr->process && (hdr->flags & MMV_FLAG_PROCESS) &&
		!__pmProcessExists((pid_t)hdr->process)) {
		__pmMemoryUnmap(m, size);
		return -ESRCH;
	    }

	    /* all checks out, we'll use this one */
	    cluster = choose_cluster(hdr->cluster, path);
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "MMV: %s: loading %s client: %d \"%s\"",
				    pmProgname, prefix, cluster, path);

	    slist = realloc(slist, sizeof(stats_t)*(scnt+1));
	    if (slist != NULL) {
		slist[scnt].name = strdup(client);
		slist[scnt].addr = m;
		slist[scnt].pid = (pid_t)((hdr->flags & MMV_FLAG_PROCESS)? hdr->process : 0);
		slist[scnt].cluster = cluster;
		slist[scnt].mcnt = 0;
		slist[scnt].gen = hdr->g1;
		slist[scnt].len = size;
		scnt++;
	    } else {
		__pmNotifyErr(LOG_ERR, "%s: client \"%s\" out of memory - %s",
				pmProgname, client, osstrerror());
		__pmMemoryUnmap(m, size);
		scnt = 0;
	    }
	} else {
            __pmNotifyErr(LOG_ERR, "%s: failed to memory map \"%s\" - %s",
				pmProgname, path, osstrerror());
	}
    } else {
	__pmNotifyErr(LOG_ERR, "%s: failed to open client file \"%s\" - %s",
				pmProgname, client, osstrerror());
    }
    return 0;
}

/* check validity of client metric name, return non-zero if bad or duplicate */
static int
verify_metric_name(const char *name, int pos, stats_t *s)
{
    const char *p = name;
    pmID pmid;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: verify_metric_name: %s", name);

    if (p == NULL || *p == '\0' || !isalpha((int)*p)) {
	__pmNotifyErr(LOG_WARNING, "Invalid metric[%d] name start in %s, ignored",
			pos, s->name);
	return -EINVAL;
    }
    for (++p; (p != NULL && *p != '\0'); p++) {
	if (isalnum((int)*p) || *p == '_' || *p == '.')
	    continue;
	__pmNotifyErr(LOG_WARNING, "invalid metric[%d] name in %s (@%c), ignored",
			    pos, s->name, *p);
	return -EINVAL;
    }
    if (pmdaTreePMID(pmns, name, &pmid) == 0)
	return -EEXIST;
    return 0;
}

/* check client item number validity - must not be too large to fit in PMID! */
static int
verify_metric_item(unsigned int item, char *name, stats_t *s)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: verify_metric_item: %u - %s", item, name);

    if (pmid_item(item) != item) {
	__pmNotifyErr(LOG_WARNING, "invalid item %u (%s) in %s, ignored",
			item, name, s->name);
	return -EINVAL;
    }
    return 0;
}

static int
create_metric(pmdaExt *pmda, stats_t *s, mmv_disk_metric_t *m, char *name, pmID pmid)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_metric: %s - %s", name, pmIDStr(pmid));

    metrics = realloc(metrics, sizeof(pmdaMetric) * (mcnt + 1));
    if (metrics == NULL)  {
	__pmNotifyErr(LOG_ERR, "cannot grow MMV metric list: %s", s->name);
	return -ENOMEM;
    }

    metrics[mcnt].m_user = NULL;
    metrics[mcnt].m_desc.pmid = pmid;

    if (m->type == MMV_TYPE_ELAPSED) {
	pmUnits unit = PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0);
	metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
	metrics[mcnt].m_desc.type = MMV_TYPE_I64;
	metrics[mcnt].m_desc.units = unit;
    } else {
	if (m->semantics)
	    metrics[mcnt].m_desc.sem = m->semantics;
	else
	    metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
	metrics[mcnt].m_desc.type = m->type;
	memcpy(&metrics[mcnt].m_desc.units, &m->dimension, sizeof(pmUnits));
    }
    metrics[mcnt].m_desc.indom = (!m->indom || m->indom == PM_INDOM_NULL) ?
				PM_INDOM_NULL : pmInDom_build(pmda->e_domain,
					(s->cluster << 11) | m->indom);
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: map_stats adding metric[%d] %s %s from %s\n",
			mcnt, name, pmIDStr(pmid), s->name);

    mcnt++;
    __pmAddPMNSNode(pmns, pmid, name);

    return 0;
}

/* check client serial number validity, and check for a duplicate */
static int
verify_indom_serial(pmdaExt *pmda, int serial, stats_t *s, pmInDom *p, pmdaIndom **i)
{
    int index;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: verify_indom_serial: %u", serial);

    if (pmInDom_serial(serial) != serial) {
	__pmNotifyErr(LOG_WARNING, "invalid serial %u in %s, ignored",
			serial, s->name);
	return -EINVAL;
    }

    *p = pmInDom_build(pmda->e_domain, (s->cluster << 11) | serial);
    for (index = 0; index < incnt; index++) {
	*i = &indoms[index];
	if (indoms[index].it_indom == *p)
	    return -EEXIST;
    }
    *i = NULL;
    return 0;
}

static int
update_indom(pmdaExt *pmda, stats_t *s, mmv_disk_indom_t *id, pmdaIndom *ip)
{
    int i, j, size, newinsts = 0;
    mmv_disk_instance_t *in = (mmv_disk_instance_t *)((char *)s->addr + id->offset);

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: update_indom: %u (%d insts)",
			id->serial, ip->it_numinst);

    /* first calculate how many new instances, so we know what to alloc */
    for (i = 0; i < id->count; i++) {
	for (j = 0; j < ip->it_numinst; j++)
	    if (ip->it_set[j].i_inst == in[i].internal)
		continue;
	if (j == ip->it_numinst)
	    newinsts++;
    }

    if (!newinsts)
	return 0;

    /* allocate memory, then append new instances to the known set */
    size = sizeof(pmdaInstid) * (ip->it_numinst + newinsts);
    ip->it_set = (pmdaInstid *)realloc(ip->it_set, size);
    if (ip->it_set != NULL) {
	for (i = 0; i < id->count; i++) {
	    for (j = 0; j < ip->it_numinst; j++)
		if (ip->it_set[j].i_inst == in[j].internal)
		    continue;
	    if (j == ip->it_numinst) {
		ip->it_set[j].i_inst = in[i].internal;
		ip->it_set[j].i_name = in[i].external;
		ip->it_numinst++;
	    }
	}
    } else {
	__pmNotifyErr(LOG_ERR, "%s: cannot get memory for instance list in %s",
			pmProgname, s->name);
	ip->it_numinst = 0;
	return -ENOMEM;
    }
    return 0;
}

static int
create_indom(pmdaExt *pmda, stats_t *s, mmv_disk_indom_t *id, pmInDom indom)
{
    int i;
    pmdaIndom *ip;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_indom: %u", id->serial);

    indoms = realloc(indoms, sizeof(pmdaIndom) * (incnt + 1));
    if (indoms == NULL) {
	__pmNotifyErr(LOG_ERR, "%s: cannot grow indom list in %s",
			pmProgname, s->name);
	return -ENOMEM;
    }
    ip = &indoms[incnt++];
    ip->it_indom = indom;
    ip->it_set = (pmdaInstid *)calloc(id->count, sizeof(pmdaInstid));
    if (ip->it_set != NULL) {
	mmv_disk_instance_t * in = (mmv_disk_instance_t *)
				    ((char *)s->addr + id->offset);
	ip->it_numinst = id->count;
	for (i = 0; i < ip->it_numinst; i++) {
	    ip->it_set[i].i_inst = in[i].internal;
	    ip->it_set[i].i_name = in[i].external;
	}
    } else {
	__pmNotifyErr(LOG_ERR, "%s: cannot get memory for instance list in %s",
			pmProgname, s->name);
	ip->it_numinst = 0;
	return -ENOMEM;
    }
    return 0;
}

static void
map_stats(pmdaExt *pmda)
{
    struct dirent **files;
    char name[64];
    int need_reload = 0;
    int i, j, k, sts, num;

    if (pmns)
	__pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	pmns = NULL;
	return;
    }

    /* hard-coded metrics (not from mmap'd files */
    snprintf(name, sizeof(name), "%s.reload", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 0), name);
    snprintf(name, sizeof(name), "%s.debug", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 1), name);
    mcnt = 2;

    if (indoms != NULL) {
	for (i = 0; i < incnt; i++)
	    free(indoms[i].it_set);
	free(indoms);
	indoms = NULL;
	incnt = 0;
    }

    if (slist != NULL) {
	for (i = 0; i < scnt; i++) {
	    free(slist[i].name);
	    __pmMemoryUnmap(slist[i].addr, slist[i].len);
	}
	free(slist);
	slist = NULL;
	scnt = 0;
    }

    num = scandir(statsdir, &files, NULL, NULL);
    for (i = 0; i < num; i++) {
	struct stat statbuf;
	char path[MAXPATHLEN];
	char *client;

	if (files[i]->d_name[0] == '.')
	    continue;

	client = files[i]->d_name;
	sprintf(path, "%s%c%s", statsdir, __pmPathSeparator(), client);

	if (stat(path, &statbuf) >= 0 && S_ISREG(statbuf.st_mode))
	    if (create_client_stat(client, path, statbuf.st_size) == -EAGAIN)
		need_reload = 1;
    }

    for (i = 0; i < num; i++)
	free(files[i]);
    if (num > 0)
	free(files);

    for (i = 0; slist && i < scnt; i++) {
	stats_t * s = slist + i;
	mmv_disk_header_t * hdr = (mmv_disk_header_t *)s->addr;
	mmv_disk_toc_t * toc = (mmv_disk_toc_t *)
			((char *)s->addr + sizeof(mmv_disk_header_t));

	for (j = 0; j < hdr->tocs; j++) {
	    switch (toc[j].type) {
		case MMV_TOC_METRICS: {
		    mmv_disk_metric_t *ml = (mmv_disk_metric_t *)
					((char *)s->addr + toc[j].offset);

		    s->metrics = ml;
		    s->mcnt = toc[j].count;

		    for (k = 0; k < toc[j].count; k++) {
			char name[MAXPATHLEN];
			pmID pmid;

			/* build name, check its legitimate and unique */
			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    sprintf(name, "%s.", prefix);
			else
			    sprintf(name, "%s.%s.", prefix, s->name);
			strcat(name, ml[k].name);
			if (verify_metric_name(name, k, s) != 0)
			    continue;
			if (verify_metric_item(ml[k].item, name, s) != 0)
			    continue;

			pmid = pmid_build(pmda->e_domain, s->cluster, ml[k].item);
			create_metric(pmda, s, &ml[k], name, pmid);
		    }
		    break;
		}

		case MMV_TOC_INDOMS: {
		    mmv_disk_indom_t * id = (mmv_disk_indom_t *)
					((char *)s->addr + toc[j].offset);

		    for (k = 0; k < toc[j].count; k++) {
			int sts, serial = id[k].serial;
			pmInDom pmindom;
			pmdaIndom *ip;

			sts = verify_indom_serial(pmda, serial, s, &pmindom, &ip);
			if (sts == -EINVAL)
			    continue;
			else if (sts == -EEXIST)
			    /* see if we have new instances to add here */
			    update_indom(pmda, s, &id[k], ip);
			else
			    /* first time we've observed this indom */
			    create_indom(pmda, s, &id[k], pmindom);
		    }
		    break;
		}

		case MMV_TOC_VALUES: {
		    s->vcnt = toc[j].count;
		    s->values = (mmv_disk_value_t *)
			((char *)s->addr + toc[j].offset);
		    break;
		}

		default:
		    break;
	    }
	}
    }

    pmdaTreeRebuildHash(pmns, mcnt);	/* for reverse (pmid->name) lookups */
    reload = need_reload;
}

static int
mmv_lookup_stat_metric_value(pmID pmid, unsigned int inst,
	stats_t **sout, mmv_disk_metric_t **mout, mmv_disk_value_t **vout)
{
    __pmID_int * id = (__pmID_int *)&pmid;
    mmv_disk_metric_t * m;
    mmv_disk_value_t * v;
    stats_t * s;
    int si, mi, vi;
    int sts = PM_ERR_PMID;

    for (si = 0; si < scnt; si++) {
	s = &slist[si];
	if (s->cluster != id->cluster)
	    continue;

	m = s->metrics;
	for (mi = 0; mi < s->mcnt; mi++) {
	    if (m[mi].item != id->item)
		continue;

	    sts = PM_ERR_INST;
	    v = s->values;
	    for (vi = 0; vi < s->vcnt; vi++) {
		mmv_disk_metric_t * mt = (mmv_disk_metric_t *)
			((char *)s->addr + v[vi].metric);
		mmv_disk_instance_t * is = (mmv_disk_instance_t *)
			((char *)s->addr + v[vi].instance);

		if ((mt == &m[mi]) &&
		    (mt->indom == PM_INDOM_NULL || mt->indom == 0 ||
		     inst == PM_IN_NULL || is->internal == inst)) {
		    *sout = s;
		    *mout = &m[mi];
		    *vout = &v[vi];
		    return 0;
		}
	    }
	}
    }
    return sts;
}

/*
 * callback provided to pmdaFetch
 */
static int
mmv_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int * id = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (id->cluster == 0) {
	if (id->item == 0) {
	    atom->l = reload;
	    return 1;
	}
	if (id->item == 1) {
	    atom->l = pmDebug;
	    return 1;
	}
	return PM_ERR_PMID;

    } else if (scnt > 0) {	/* We have at least one source of metrics */
	mmv_disk_string_t * str;
	mmv_disk_metric_t * m;
	mmv_disk_value_t * v;
	stats_t * s;
	int rv;

	rv = mmv_lookup_stat_metric_value(mdesc->m_desc.pmid, inst, &s, &m, &v);
	if (rv < 0)
	    return rv;

	switch (m->type) {
	    case MMV_TYPE_I32:
	    case MMV_TYPE_U32:
	    case MMV_TYPE_I64:
	    case MMV_TYPE_U64:
	    case MMV_TYPE_FLOAT:
	    case MMV_TYPE_DOUBLE:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		break;
	    case MMV_TYPE_ELAPSED: {
		atom->ll = v->value.ll;
		if (v->extra < 0) {	/* inside a timed section */
		    struct timeval tv; 
		    __pmtimevalNow(&tv); 
		    atom->ll += (tv.tv_sec * 1e6 + tv.tv_usec) + v->extra;
		}
		break;
	    }
	    case MMV_TYPE_STRING: {
		str = (mmv_disk_string_t *)((char *)s->addr + v->extra);
		atom->cp = str->payload;
		break;
	    }
	    case MMV_TYPE_NOSUPPORT:
		return PM_ERR_APPVERSION;
	}
	return 1;
    }

    return 0;
}

static void
mmv_reload_maybe(pmdaExt *pmda)
{
    int i;
    struct stat s;
    int need_reload = reload;

    /* check if generation numbers changed or monitored process exited */
    for (i = 0; i < scnt; i++) {
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)slist[i].addr;
	if (hdr->g1 != slist[i].gen || hdr->g2 != slist[i].gen) {
	    need_reload++;
	    break;
	}
	if (slist[i].pid && !__pmProcessExists(slist[i].pid)) {
	    need_reload++;
	    break;
	}
    }

    /*
     * check if the directory has been modified, reload if so;
     * note modification may involve removal or newly appeared,
     * a change in permissions from accessible to not (or vice-
     * versa), and so on.
     */
    if (stat(statsdir, &s) >= 0) {
	if (s.st_mtime != statsdir_ts) {
	    need_reload++;
	    statsdir_code = 0;
	    statsdir_ts = s.st_mtime;
	}
    } else {
	i = oserror();
	if (statsdir_code != i) {
	    statsdir_code = i;
	    statsdir_ts = 0;
	    need_reload++;
	}
    }

    if (need_reload) {
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "MMV: %s: reloading", pmProgname);
	map_stats(pmda);

	pmda->e_indoms = indoms;
	pmda->e_nindoms = incnt;
	pmdaRehash(pmda, metrics, mcnt);

	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, 
		      "MMV: %s: %d metrics and %d indoms after reload", 
		      pmProgname, mcnt, incnt);
    }
}

/* Intercept request for descriptor and check if we'd have to reload */
static int
mmv_desc(pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    mmv_reload_maybe(ep);
    return pmdaDesc(pmid, desc, ep);
}

static int
mmv_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    if (type & PM_TEXT_INDOM)
	return PM_ERR_TEXT;

    mmv_reload_maybe(ep);
    if (pmid_cluster(ident) == 0) {
	if (pmid_item(ident) == 0) {
	    static char reloadoneline[] = "Control maps reloading";
	    static char reloadtext[] = 
"Writing anything other then 0 to this metric will result in\n"
"re-reading directory and re-mapping files.\n";

	    *buffer = (type & PM_TEXT_ONELINE) ? reloadoneline : reloadtext;
	    return 0;
	}
	else if (pmid_item(ident) == 1) {
	    static char debugoneline[] = "Debug flag";
	    static char debugtext[] =
"See pmdbg(1).  pmstore into this metric to change the debug value.\n";

	    *buffer = (type & PM_TEXT_ONELINE) ? debugoneline : debugtext;
	    return 0;
	}
	else
	    return PM_ERR_PMID;
    }
    else {
	mmv_disk_string_t * str;
	mmv_disk_metric_t * m;
	mmv_disk_value_t * v;
	stats_t * s;

	if (mmv_lookup_stat_metric_value(ident, PM_IN_NULL, &s, &m, &v) != 0)
	    return PM_ERR_PMID;

	if ((type & PM_TEXT_ONELINE) && m->shorttext) {
	    str = (mmv_disk_string_t *)((char *)s->addr + m->shorttext);
	    *buffer = str->payload;
	    return 0;
	}
	if ((type & PM_TEXT_HELP) && m->helptext) {
	    str = (mmv_disk_string_t *)((char *)s->addr + m->helptext);
	    *buffer = str->payload;
	    return 0;
	}
    }

    return PM_ERR_TEXT;
}

static int
mmv_instance(pmInDom indom, int inst, char *name, 
	     __pmInResult **result, pmdaExt *ep)
{
    mmv_reload_maybe(ep);
    return pmdaInstance(indom, inst, name, result, ep);
}

static int
mmv_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
mmv_store(pmResult *result, pmdaExt *ep)
{
    int i, m;

    mmv_reload_maybe(ep);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet * vsp = result->vset[i];
	__pmID_int * id = (__pmID_int *)&vsp->pmid;

	if (id->cluster == 0) {
	    for (m = 0; m < mcnt; m++) {
		__pmID_int * mid = (__pmID_int *)&(metrics[m].m_desc.pmid);

		if (mid->cluster == 0 && mid->item == id->item) {
		    pmAtomValue atom;
		    int sts;

		    if (vsp->numval != 1 )
			return PM_ERR_BADSTORE;

		    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
					PM_TYPE_32, &atom, PM_TYPE_32)) < 0)
			return sts;
		    if (id->item == 0)
		    reload = atom.l;
		    else if (id->item == 1)
		    	pmDebug = atom.l;
		    else
			return PM_ERR_PERMISSION;
		}
	    }
	}
	else
	    return PM_ERR_PERMISSION;
    }
    return 0;
}

static int
mmv_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaTreePMID(pmns, name, pmid);
}

static int
mmv_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
mmv_children(const char *name, int traverse, char ***kids, int **sts, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

void
__PMDA_INIT_CALL
mmv_init(pmdaInterface *dp)
{
    int	m;
    int sep = __pmPathSeparator();

    if (isDSO) {
	pmdaDSO(dp, PMDA_INTERFACE_4, "mmv", NULL);
    } else {
	__pmSetProcessIdentity(username);
    }

    pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    pcpvardir = pmGetConfig("PCP_VAR_DIR");
    pcppmdasdir = pmGetConfig("PCP_PMDAS_DIR");

    snprintf(statsdir, sizeof(statsdir), "%s%c%s", pcptmpdir, sep, prefix);
    snprintf(pmnsdir, sizeof(pmnsdir), "%s%c" "pmns", pcpvardir, sep);
    statsdir[sizeof(statsdir)-1] = '\0';
    pmnsdir[sizeof(pmnsdir)-1] = '\0';

    /* Initialize internal dispatch table */
    if (dp->status == 0) {
	/*
	 * number of hard-coded metrics here has to match initializer
	 * cases below, and pmns initialization in map_stats()
	 */
	mcnt = 2;
	if ((metrics = malloc(mcnt*sizeof(pmdaMetric))) != NULL) {
	    /*
	     * all the hard-coded metrics have the same semantics
	     */
	    for (m = 0; m < mcnt; m++) {
		if (m == 0)
		    metrics[m].m_user = &reload;
		else if (m == 1)
		    metrics[m].m_user = &pmDebug;
		metrics[m].m_desc.pmid = pmid_build(dp->domain, 0, m);
		metrics[m].m_desc.type = PM_TYPE_32;
		metrics[m].m_desc.indom = PM_INDOM_NULL;
		metrics[m].m_desc.sem = PM_SEM_INSTANT;
		memset(&metrics[m].m_desc.units, 0, sizeof(pmUnits));
	    }
	} else {
	    __pmNotifyErr(LOG_ERR, "%s: pmdaInit - out of memory\n",
				pmProgname);
	    if (isDSO)
		return;
	    exit(0);
	}

	dp->version.four.fetch = mmv_fetch;
	dp->version.four.store = mmv_store;
	dp->version.four.desc = mmv_desc;
	dp->version.four.text = mmv_text;
	dp->version.four.instance = mmv_instance;
	dp->version.four.pmid = mmv_pmid;
	dp->version.four.name = mmv_name;
	dp->version.four.children = mmv_children;
	pmdaSetFetchCallBack(dp, mmv_fetchCallBack);

	pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
	pmdaInit(dp, indoms, incnt, metrics, mcnt);
    }
}

int
main(int argc, char **argv)
{
    char	logfile[32];
    pmdaInterface dispatch = { 0 };

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    if (strncmp(pmProgname, "pmda", 4) == 0 && strlen(pmProgname) > 4)
	prefix = pmProgname + 4;
    snprintf(logfile, sizeof(logfile), "%s.log", prefix);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, MMV, logfile, NULL);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    mmv_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
