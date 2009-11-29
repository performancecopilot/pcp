/*
 * Copyright (c) 1995-2000,2009 Silicon Graphics, Inc. All Rights Reserved.
 * Copyright (c) 2009 Aconex. All Rights Reserved.
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

#define NONLEAF(node)	((node)->pmid == PM_ID_NULL)

static int isDSO = 1;

static pmdaMetric * metrics;
static int mcnt;
static pmdaIndom * indoms;
static int incnt;

static int reload;
static __pmnsTree *pmns;
static time_t statsdir_ts;		/* last statsdir timestamp */
static char * prefix = "mmv";

static char * pcptmpdir;		/* probably /var/tmp */
static char * pcpvardir;		/* probably /var/pcp */
static char * pcppmdasdir;		/* probably /var/pcp/pmdas */
static char pmnsdir[MAXPATHLEN];	/* pcpvardir/pmns */
static char statsdir[MAXPATHLEN];	/* pcptmpdir/<prefix> */

typedef struct {
    char *	name;		/* strdup client name */
    void *	addr;		/* mmap */
    mmv_disk_header_t *	hdr;	/* header in mmap */
    mmv_disk_value_t *	values;	/* values in mmap */
    int		vcnt;		/* number of values */
    int		pid;		/* process identifier */
    __int64_t	len;		/* mmap region len */
    time_t	ts;		/* mmap file timestamp */
    int		moff;		/* Index of the first metric in the array */
    int		mcnt;		/* How many metrics have we got */
    int		cluster;	/* cluster identifier */
    __uint64_t	gen;		/* generation number on open */
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
	    __pmNotifyErr(LOG_INFO,
			  "%s: duplicate cluster %d in use",
			  pmProgname, requested);
	    break;
	}
    }
    return requested;
}

/*
 * Fixup the parent pointers of the tree.
 * Fill in the hash table with nodes from the tree.
 * Hashing is done on pmid.
 */
static void
mmv_reindex_hash(__pmnsTree *tree, __pmnsNode *root)
{
    __pmnsNode	*np;

    for (np = root->first; np != NULL; np = np->next) {
	np->parent = root;
	if (np->pmid != PM_ID_NULL) {
	    int i = np->pmid % tree->htabsize;
	    np->hash = tree->htab[i];
	    tree->htab[i] = np;
	}
	mmv_reindex_hash(tree, np);
    }
}

/*
 * "Make the average hash list no longer than 5, and the number
 * of hash table entries not a multiple of 2, 3 or 5."
 * [From __pmFixPMNSHashTab; without mark_all, dinks with pmids]
 */
static void
mmv_rebuild_hash(__pmnsTree *tree, int numpmid)
{
    int htabsize = numpmid / 5;

    if (htabsize % 2 == 0) htabsize++;
    if (htabsize % 3 == 0) htabsize += 2;
    if (htabsize % 5 == 0) htabsize += 2;
    tree->htabsize = htabsize;
    tree->htab = (__pmnsNode **)calloc(htabsize, sizeof(__pmnsNode *));
    if (tree->htab == NULL)
	__pmNotifyErr(LOG_ERR, "%s: out of memory in pmns rebuild - %s",
			pmProgname, strerror(errno));
    else
	mmv_reindex_hash(tree, tree->root);
}

static void
map_stats(pmdaExt *pmda)
{
    struct dirent ** files;
    char name_reload[64];
    int need_reload = 0;
    int i, sts, num;

    if (pmns)
	__pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	pmns = NULL;
	return;
    }

    mcnt = 1;
    snprintf(name_reload, sizeof(name_reload), "%s.reload", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 0), name_reload);

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

	if (stat(path, &statbuf) >= 0 && S_ISREG(statbuf.st_mode)) {
	    int fd;

	    if ((fd = open(path, O_RDONLY)) >= 0) {
		void *m = __pmMemoryMap(fd, statbuf.st_size, 0);

		close(fd);
		if (m == NULL) {
	            __pmNotifyErr(LOG_ERR, 
				  "%s: failed to memory map \"%s\" - %s",
				  pmProgname, path, strerror(errno));
		} else {
		    mmv_disk_header_t * hdr = (mmv_disk_header_t *)m;
		    int cluster;

		    if (strncmp(hdr->magic, "MMV", 4)) {
			__pmMemoryUnmap(m, statbuf.st_size);
			continue;
		    }

		    if (hdr->version != MMV_VERSION) {
			__pmNotifyErr(LOG_ERR, 
					"%s: %s client version %d "
					"not supported (current is %d)",
					pmProgname, prefix,
					hdr->version, MMV_VERSION);
			__pmMemoryUnmap(m, statbuf.st_size);
			continue;
		    }

		    if (!hdr->g1 || hdr->g1 != hdr->g2) {
			/* still in flux, wait till next time */
			__pmMemoryUnmap(m, statbuf.st_size);
			need_reload = 1;
			continue;
		    }

		    /* optionally verify the creator PID is running */
		    if (hdr->process && (hdr->flags & MMV_FLAG_PROCESS) &&
			!__pmProcessExists(hdr->process)) {
			__pmMemoryUnmap(m, statbuf.st_size);
			continue;
		    }

		    /* all checks out, we'll use this one */
		    cluster = choose_cluster(hdr->cluster, path);
		    __pmNotifyErr(LOG_INFO, "%s: loading %s client: %d \"%s\"",
				    pmProgname, prefix, cluster, path);

		    slist = realloc(slist, sizeof(stats_t)*(scnt+1));
		    if (slist != NULL ) {
			slist[scnt].name = strdup(client);
			slist[scnt].addr = m;
			slist[scnt].hdr = hdr;
			slist[scnt].pid = hdr->process;
			slist[scnt].ts = statbuf.st_ctime;
			slist[scnt].cluster = cluster;
			slist[scnt].mcnt = 0;
			slist[scnt].moff = -1;
			slist[scnt].gen = hdr->g1;
			slist[scnt++].len = statbuf.st_size;
		    } else {
			__pmNotifyErr(LOG_ERR, 
					"%s: out of memory on client \"%s\" - %s",
					pmProgname, client, strerror(errno));
			__pmMemoryUnmap(m, statbuf.st_size);
		    }
		}
	    } else {
		__pmNotifyErr(LOG_ERR, 
				"%s: failed to open client file \"%s\" - %s",
			        pmProgname, client, strerror(errno));
	    }
	} else {
	    __pmNotifyErr(LOG_ERR, 
			    "%s: failed to stat client file \"%s\" - %s",
			    pmProgname, client, strerror(errno));
	}
    }

    for (i = 0; i < num; i++)
	free(files[i]);
    if (num)
	free(files);

    for (i = 0; i < scnt; i++) {
	int j;
	stats_t * s = slist + i;
	mmv_disk_header_t * hdr = (mmv_disk_header_t *)s->addr;
	mmv_disk_toc_t * toc = (mmv_disk_toc_t *)
			((char *)s->addr + sizeof(mmv_disk_header_t));

	for (j = 0; j < hdr->tocs; j++) {
	    int k;

	    switch (toc[j].type) {
	    case MMV_TOC_METRICS:
		metrics = realloc(metrics,
				  sizeof(pmdaMetric) * (mcnt + toc[j].count));
		if (metrics != NULL) {
		    mmv_disk_metric_t *ml = (mmv_disk_metric_t *)
					((char *)s->addr + toc[j].offset);

		    if (s->moff < 0)
			s->moff = mcnt;
		    s->mcnt += toc[j].count;

		    for (k = 0; k < toc[j].count; k++) {
			char name[MAXPATHLEN];

			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    sprintf(name, "%s.", prefix);
			else
			    sprintf(name, "%s.%s.", prefix, s->name);

			metrics[mcnt].m_user = ml + k;
			metrics[mcnt].m_desc.pmid = pmid_build(
				pmda->e_domain, s->cluster, ml[k].item);

			if (ml[k].type == MMV_TYPE_ELAPSED) {
			    pmUnits unit = PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0);
			    metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
			    metrics[mcnt].m_desc.type = MMV_TYPE_I64;
			    metrics[mcnt].m_desc.units = unit;
			} else {
			    if (ml[k].semantics)
				metrics[mcnt].m_desc.sem = ml[k].semantics;
			    else
				metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
			    metrics[mcnt].m_desc.type = ml[k].type;
			    memcpy(&metrics[mcnt].m_desc.units,
				   &ml[k].dimension, sizeof(pmUnits));
			}
			metrics[mcnt].m_desc.indom =
				(!ml[k].indom || ml[k].indom == PM_INDOM_NULL) ?
					PM_INDOM_NULL :
					pmInDom_build(pmda->e_domain,
					(s->cluster << 11) | ml[k].indom);

			strcat(name, ml[k].name);
			__pmAddPMNSNode(pmns, pmid_build(
				pmda->e_domain, s->cluster, ml[k].item),
				name);
			mcnt++;
		    }
		} else {
		    __pmNotifyErr(LOG_ERR, "%s: cannot grow metric list",
				  pmProgname);
		    if (isDSO)
			return;
		    exit(1);
		}
		break;

	    case MMV_TOC_INDOMS:
		indoms = realloc(indoms,
				sizeof(pmdaIndom) * (incnt + toc[j].count));
		if (indoms != NULL) {
		    int l;
		    pmdaIndom *ip;
		    mmv_disk_indom_t * id = (mmv_disk_indom_t *)
				((char *)s->addr + toc[j].offset);

		    for (k = 0; k < toc[j].count; k++) {
			ip = &indoms[incnt + k];
			ip->it_indom = pmInDom_build(pmda->e_domain,
				(slist[i].cluster << 11) | id[k].serial);
			ip->it_numinst = id[k].count;
			ip->it_set = (pmdaInstid *)
				calloc(id[k].count, sizeof(pmdaInstid));

			if (ip->it_set != NULL) {
			    mmv_disk_instance_t * in = (mmv_disk_instance_t *)
					((char *)s->addr + id[k].offset);
			    for (l = 0; l < ip->it_numinst; l++) {
				ip->it_set[l].i_inst = in[l].internal;
				ip->it_set[l].i_name = in[l].external;
			    }
			} else {
			    __pmNotifyErr(LOG_ERR, 
				"%s: cannot get memory for instance list",
				pmProgname);
			    if (isDSO)
				return;
			    exit(1);
			}
		    }
		    incnt += toc[j].count;
		} else {
		    __pmNotifyErr(LOG_ERR, "%s: cannot grow indom list",
				  pmProgname);
		    if (isDSO)
			return;
		    exit(1);
		}
		break;

	    case MMV_TOC_VALUES: 
		s->vcnt = toc[j].count;
		s->values = (mmv_disk_value_t *)
			((char *)s->addr + toc[j].offset);
		break;

	    default:
		break;
	    }
	}
    }

    mmv_rebuild_hash(pmns, mcnt);	/* for reverse (pmid->name) lookups */
    reload = need_reload;
}

static mmv_disk_metric_t *
mmv_lookup_metric(pmID pmid, stats_t **sout)
{
    __pmID_int * id = (__pmID_int *)&pmid;
    mmv_disk_metric_t * m;
    stats_t * s;
    int c, i;

    for (c = 0; c < scnt; c++) {
	s = slist + c;
	if (s->cluster == id->cluster)
	    break;
    }
    if (c == scnt)
	return NULL;

    for (i = 0; i < s->mcnt; i++) {
	m = (mmv_disk_metric_t *)metrics[s->moff + i].m_user;
	if (m->item == id->item)
	    break;
    }
    if (i == mcnt)
	return NULL;

    *sout = s;
    return m;
}

/*
 * callback provided to pmdaFetch
 */
static int
mmv_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int * id = (__pmID_int *)&(mdesc->m_desc.pmid);
    int i;

    if (id->cluster == 0) {
	if (id->item == 0) {
	    atom->l = reload;
	    return 1;
	}
	return PM_ERR_PMID;
    } else if (scnt > 0) {	/* We have a least one source of metrics */
	mmv_disk_metric_t * m;
	mmv_disk_value_t * val;
	stats_t * s;

	if ((m = mmv_lookup_metric(mdesc->m_desc.pmid, &s)) == NULL)
	    return PM_ERR_PMID;

	val = s->values;
	for (i = 0; i < s->vcnt; i++) {
	    mmv_disk_metric_t * mt = (mmv_disk_metric_t *)
			((char *)s->addr + val[i].metric);
	    mmv_disk_instance_t * is = (mmv_disk_instance_t *)
			((char *)s->addr + val[i].instance);

	    if ((mt == m) &&
		(mt->indom == PM_INDOM_NULL || mt->indom == 0 ||
		 (is->internal == inst))) {
		switch (m->type) {
		    case MMV_TYPE_I32:
		    case MMV_TYPE_U32:
		    case MMV_TYPE_I64:
		    case MMV_TYPE_U64:
		    case MMV_TYPE_FLOAT:
		    case MMV_TYPE_DOUBLE:
			memcpy(atom, &val[i].value, sizeof(pmAtomValue));
			break;
		    case MMV_TYPE_ELAPSED: {
			atom->ll = val[i].value.ll;
			if (val[i].extra < 0) {	/* inside a timed section */
			    struct timeval tv; 
			    gettimeofday(&tv, NULL); 
			    atom->ll += (tv.tv_sec * 1e6 + tv.tv_usec) +
					val[i].extra;
			}
			break;
		    }
		    case MMV_TYPE_STRING: {
			mmv_disk_string_t * string = (mmv_disk_string_t *)
					((char *)s->addr + val[i].extra);
			atom->cp = string->payload;
			break;
		    }
		    case MMV_TYPE_NOSUPPORT:
			return PM_ERR_APPVERSION;
		}
		return 1;
	    }
	}
	return PM_ERR_PMID;
    }

    return 0;
}

static void
mmv_reload_maybe(pmdaExt *pmda)
{
    int i;
    struct stat s;
    int need_reload = reload;

    /* check if any of the generation numbers have changed (unexpected) */
    for (i = 0; i < scnt; i++) {
	if (slist[i].hdr->g1 != slist[i].gen ||
	    slist[i].hdr->g2 != slist[i].gen) {
	    need_reload++;
	    break;
	}
    }

    /* check if the directory has been modified */
    if (stat(statsdir, &s) >= 0 && s.st_ctime != statsdir_ts) {
	need_reload++;
	statsdir_ts = s.st_ctime;
    }

    if (need_reload) {
	__pmNotifyErr(LOG_INFO, "%s: reloading", pmProgname);
	map_stats(pmda);

	pmda->e_indoms = indoms;
	pmda->e_nindoms = incnt;
	pmda->e_metrics = metrics;
	pmda->e_nmetrics = mcnt;
	pmda->e_direct = 0;

	__pmNotifyErr(LOG_INFO, 
		      "%s: %d metrics and %d indoms after reload", 
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
	    /* mmv.reload */
	    if (type & PM_TEXT_ONELINE)
		*buffer = strdup("Control maps reloading");
	    else
		*buffer = strdup(
"Writing anything other then 0 to this metric will result in\n"
"re-reading directory and re-mapping files.");
		return (*buffer == NULL) ? -ENOMEM : 0;
	}
	else
	    return PM_ERR_PMID;
    }
    else {
	mmv_disk_metric_t * m;
	mmv_disk_string_t * s;
	stats_t * stats;

	if ((m = mmv_lookup_metric(ident, &stats)) == NULL)
	    return PM_ERR_PMID;

	if ((type & PM_TEXT_ONELINE) && m->shorttext) {
	    s = (mmv_disk_string_t *)((char *)stats->addr + m->shorttext);
	    *buffer = strdup(s->payload);
	    return (*buffer == NULL) ? -ENOMEM : 0;
	}
	if ((type & PM_TEXT_HELP) && m->helptext) {
	    s = (mmv_disk_string_t *)((char *)stats->addr + m->helptext);
	    *buffer = strdup(s->payload);
	    return (*buffer == NULL) ? -ENOMEM : 0;
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

	if (id->cluster == 0 && id->item == 0) {
	    for (m = 0; m < mcnt; m++) {
		__pmID_int * mid = (__pmID_int *)&(metrics[m].m_desc.pmid);

		if (mid->cluster == 0 && mid->item == id->item) {
		    pmAtomValue atom;
		    int sts;

		    if (vsp->numval != 1 )
			return PM_ERR_CONV;

		    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
					PM_TYPE_32, &atom, PM_TYPE_32)) < 0)
			return sts;
		    reload = atom.l;
		}
	    }
	}
	else
	    return PM_ERR_PMID;
    }
    return 0;
}

static __pmnsNode *
mmv_lookup_node(__pmnsNode *node, char *name)
{
    while (node != NULL) {
	size_t length = strlen(node->name);
	if (strncmp(name, node->name, length) == 0) {
	    if (name[length] == '\0')
		return node;
	    if (name[length] == '.' && NONLEAF(node))
		return mmv_lookup_node(node->first, name + length + 1);
	}
	node = node->next;
    }
    return NULL;
}

static int
mmv_pmid(char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsNode *node;

    mmv_reload_maybe(pmda);
    if ((node = mmv_lookup_node(pmns->root->first, name)) == NULL)
	return PM_ERR_NAME;
    if (NONLEAF(node))
	return PM_ERR_NAME;
    *pmid = node->pmid;
    return 0;
}

static char *
mmv_absolute_name(__pmnsNode *node, char *buffer)
{
    if (node && node->parent) {
	buffer = mmv_absolute_name(node->parent, buffer);
	strcpy(buffer, node->name);
	buffer += strlen(node->name);
	*buffer++ = '.';
    }
    return buffer;
}

static int
mmv_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsNode *hashchain, *node, *parent;
    int nmatch = 0, length = 0;
    char *p, **list;

    mmv_reload_maybe(pmda);
    if (!pmns)
	return PM_ERR_PMID;

    hashchain = pmns->htab[pmid % pmns->htabsize];
    for (node = hashchain; node != NULL; node = node->hash) {
	if (node->pmid == pmid) {
	    for (parent = node; parent->parent; parent = parent->parent)
		length += strlen(parent->name) + 1;
	    nmatch++;
	}
    }

    if (nmatch == 0)
	return PM_ERR_PMID;

    length += nmatch * sizeof(char *);		/* pointers to names */

    if ((list = (char **)malloc(length)) == NULL)
	return -errno;

    p = (char *)&list[nmatch];
    nmatch = 0;
    for (node = hashchain; node != NULL; node = node->hash) {
	if (node->pmid == pmid) {
	    list[nmatch++] = p;
	    p = mmv_absolute_name(node, p);
	    *(p-1) = '\0';	/* overwrite final '.' */
	}
    }

    *nameset = list;
    return nmatch;
}

static int
mmv_children_relative(__pmnsNode *base, char ***offspring, int **status)
{
    __pmnsNode *node;
    char **list, *p;
    int *leaf, length = 0, nmatch = 0;
 
    for (node = base; node != NULL; node = node->next, nmatch++)
	length += strlen(node->name) + 1;
    length += nmatch * sizeof(char *);	/* pointers to names */
    if ((list = (char **)malloc(length)) == NULL)
	return -errno;
    if ((leaf = (int *)malloc(nmatch * sizeof(int*))) == NULL) {
	free(list);
	return -errno;
    }
    p = (char *)&list[nmatch];
    nmatch = 0;
    for (node = base; node != NULL; node = node->next, nmatch++) {
	leaf[nmatch] = NONLEAF(node) ? PMNS_NONLEAF_STATUS : PMNS_LEAF_STATUS;
	list[nmatch] = p;
	strcpy(p, node->name);
	p += strlen(node->name);
	*p++ = '\0';
    }

    *offspring = list;
    *status = leaf;
    return nmatch;
}

static void
mmv_children_getsize(__pmnsNode *base, int kids, int *length, int *nmetrics)
{
    __pmnsNode *node, *parent;

    /* walk to every leaf & then add its (absolute name) length */
    for (node = base; node != NULL; node = node->next) {
	if (NONLEAF(node)) {
	    mmv_children_getsize(node->first, 1, length, nmetrics);
	    continue;
	}
	for (parent = node; parent->parent; parent = parent->parent)
	    *length += strlen(parent->name) + 1;
	(*nmetrics)++;
	if (!kids)
	    break;
    }
}

/*
 * Fill the pmdaChildren buffers - names and leaf status.  Called recursively
 * to descend down to all leaf nodes.  Offset parameter is the current offset
 * into the name list buffer, and its also returned at the end of each call -
 * it keeps track of where the next name is to start in (list) output buffer.
 */
static char *
mmv_children_getlist(__pmnsNode *base, int kids, int *nmetrics, char *p, char **list, int *leaf)
{
    __pmnsNode *node;
    int count = *nmetrics;
    char *start = p;

    for (node = base; node != NULL; node = node->next) {
	if (NONLEAF(node)) {
	    p = mmv_children_getlist(node->first, 1, &count, p, list, leaf);
	    start = p;
	    continue;
	}
	leaf[count] = PMNS_LEAF_STATUS;
	list[count] = start;
	p = mmv_absolute_name(node, p);
	*(p-1) = '\0';	/* overwrite final '.' */
	start = p;
	count++;
	if (!kids)
	    break;
    }
    *nmetrics = count;
    return p;
}

static int
mmv_children_absolute(__pmnsNode *node, char ***offspring, int **status)
{
    char *p, **list;
    int *leaf, descend = 0, length = 0, nmetrics = 0;

    if (NONLEAF(node)) {
	node = node->first;
	descend = 1;
    }
    mmv_children_getsize(node, descend, &length, &nmetrics);
 
    length += nmetrics * sizeof(char *);	/* pointers to names */
    if ((list = (char **)malloc(length)) == NULL)
	return -errno;
    if ((leaf = (int *)malloc(nmetrics * sizeof(int*))) == NULL) {
	free(list);
	return -errno;
    }

    p = (char *)&list[nmetrics];
    nmetrics = 0;	/* start at the start */
    mmv_children_getlist(node, descend, &nmetrics, p, list, leaf);

    *offspring = list;
    *status = leaf;
    return nmetrics;
}

static int
mmv_children(char *name, int traverse, char ***offspring, int **status, pmdaExt *pmda)
{
    __pmnsNode *node;

    mmv_reload_maybe(pmda);
    if (!pmns)
	return PM_ERR_NAME;

    if ((node = mmv_lookup_node(pmns->root->first, name)) == NULL)
	return PM_ERR_NAME;

    if (traverse == 0)
	return mmv_children_relative(node->first, offspring, status);
    return mmv_children_absolute(node, offspring, status);
}

void
mmv_init(pmdaInterface *dp)
{
    int sep = __pmPathSeparator();

    if (isDSO) {
	pmdaDSO(dp, PMDA_INTERFACE_4, "mmv", NULL);
    }

    pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    pcpvardir = pmGetConfig("PCP_VAR_DIR");
    pcppmdasdir = pmGetConfig("PCP_PMDAS_DIR");

    sprintf(statsdir, "%s%c%s", pcptmpdir, sep, prefix);
    sprintf(pmnsdir, "%s%c" "pmns", pcpvardir, sep);

    /* Initialize internal dispatch table */
    if (dp->status == 0) {
	if ((metrics = malloc(sizeof(pmdaMetric))) != NULL) {
	    metrics[mcnt].m_user = & reload;
	    metrics[mcnt].m_desc.pmid = pmid_build(dp->domain, 0, 0);
	    metrics[mcnt].m_desc.type = PM_TYPE_32;
	    metrics[mcnt].m_desc.indom = PM_INDOM_NULL;
	    metrics[mcnt].m_desc.sem = PM_SEM_INSTANT;
	    memset(&metrics[mcnt].m_desc.units, 0, sizeof(pmUnits));
	    mcnt = 1;
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

	__pmNotifyErr(LOG_INFO, "%s: pmdaInit - %d metrics and %d indoms", 
		      pmProgname, mcnt, incnt);

	pmdaInit(dp, indoms, incnt, metrics, mcnt);
    }
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default "
	  "log name\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int		err = 0;
    char	logfile[32];
    pmdaInterface dispatch = { 0 };

    isDSO = 0;
    __pmSetProgname(argv[0]);
    if (strncmp(pmProgname, "pmda", 4) == 0 && strlen(pmProgname) > 4)
	prefix = pmProgname + 4;
    snprintf(logfile, sizeof(logfile), "%s.log", prefix);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, MMV, logfile, NULL);

    if ((pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF) ||
	err || argc != optind)
	usage();

    pmdaOpenLog(&dispatch);
    mmv_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
