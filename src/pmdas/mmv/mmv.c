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
 * This PMDA uses specially formatted files in either /var/tmp/mmv or 
 * some other directory, as specified on the command line. Each file represent
 * a separate "cluster" of values with flat name structure for each cluster.
 * Names for the metrics are optionally prepended with mmv and then the name
 * of the file.
 */

#include "mmv_dev.h"
#include "impl.h"
#include "pmda.h"
#include "./domain.h"
#include <sys/stat.h>

static pmdaInterface dispatch;

static pmdaMetric * metrics;
static int mcnt;
static pmdaIndom * indoms;
static int incnt;

static int reload;
static time_t statsdir_ts;		/* last statsdir timestamp */

static char * pcptmpdir;		/* probably /var/tmp */
static char * pcpvardir;		/* probably /var/pcp */
static char * pcppmdasdir;		/* probably /var/pcp/pmdas */
static char pmnsdir[MAXPATHLEN];	/* pcpvardir/pmns */
static char statsdir[MAXPATHLEN];	/* pcptmpdir/mmv */

typedef struct {
    char *	name;		/* strdup client name */
    void *	addr;		/* mmap */
    mmv_disk_header_t *	hdr;	/* header in mmap */
    mmv_disk_value_t *	values;	/* values in mmap */
    __int64_t *	extra;		/* per-value value: str offset / old time */
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

static int
update_namespace(void)
{
    char script[MAXPATHLEN];
    int sep = __pmPathSeparator();

    snprintf(script, sizeof(script),
		"%s%c" "lib" "%c" "ReplacePmnsSubtree mmv %s%c" "mmv.new",
		pmGetConfig("PCP_SHARE_DIR"), sep, sep, pmnsdir, sep);
    if (system(script) == -1) {
	__pmNotifyErr (LOG_ERR, "%s: cannot exec %s", pmProgname, script);
	return 1;
    }

    return 0;
}

static void
write_pmnspath(__pmnsNode *base, FILE *f)
{
    if (base && base->parent) {
        write_pmnspath(base->parent, f);
        fprintf(f, "%s.", base->name);
    }
}

static void
write_pmnsnode(__pmnsNode *base, FILE *f)
{
    __pmnsNode *np;

    /* Print out full path to this part of the tree */
    write_pmnspath(base->parent, f);
    fprintf(f, "%s {\n", base->name);

    /* Print out nodes at this level of the tree */
    for (np = base->first; np != NULL; np = np->next) {
        if (np->pmid == PM_ID_NULL)
            fprintf(f, "\t%s\n", np->name);
        else
            fprintf(f, "\t%s\t\t%u:%u:%u\n", np->name,
			pmid_domain(np->pmid),
			pmid_cluster(np->pmid),
			pmid_item(np->pmid));
    }
    fprintf(f, "}\n\n");

    /* Print out all the children of this subtree */
    for (np = base->first; np != NULL; np = np->next)
        if (np->pmid == PM_ID_NULL)
            write_pmnsnode(np, f);
}

static void
write_pmnsfile(__pmnsTree *pmns)
{
    char tmppath[MAXPATHLEN];
    char path[MAXPATHLEN];
    char *fname = tmppath;
    FILE *f = NULL;

    putenv("TMPDIR=");	/* temp file must be in pmnsdir, for rename */

#if HAVE_MKSTEMP
    sprintf(tmppath, "%s%cmmv-XXXXXX", pmnsdir, __pmPathSeparator());
    int fd = mkstemp(tmppath);
    if (fd != -1)
	f = fdopen(fd, "w");
#else
    fname = tempnam(pmnsdir, "mmv");
    if (fname != NULL) {
	strncpy(tmppath, fname, sizeof(tmppath));
	free(fname);
	fname = tmppath;
	f = fopen(fname, "w");
    }
#endif

    if (f == NULL)
	__pmNotifyErr(LOG_ERR, "%s: failed to generate temporary file %s: %s",
			pmProgname, fname, strerror(errno));
    else {
	__pmnsNode *node;
	for (node = pmns->root->first; node != NULL; node = node->next)
	    write_pmnsnode(node, f);
	fclose(f);
	sprintf(path, "%s%c" "mmv.new", pmnsdir, __pmPathSeparator());
	if (rename2(fname, path) < 0)
	    __pmNotifyErr(LOG_ERR, "%s: cannot rename %s to %s - %s",
			pmProgname, fname, path, strerror (errno));
    }
}

/*
 * Choose an unused cluster ID while trying to honour specific requests
 */
static int
choose_cluster(int requested, const char *path)
{
    int i, cluster, next_cluster = 1;

    for (i = 1; i < scnt; i++) {
	if (slist[i].cluster == next_cluster) {
	    next_cluster++;
	    i = 0;	/* restart */
	}
    }

    if (!requested)
	return next_cluster;

    cluster = requested;
    for (i = 0; i < scnt; i++) {
	if (slist[i].cluster == cluster) {
	    __pmNotifyErr(LOG_WARNING,
			    "%s: duplicate cluster %d requested "
			    "(using %d instead) - \"%s\"",
			    pmProgname, cluster, next_cluster, path);
	    cluster = next_cluster;
	    break;
	}
    }
    return cluster;
}

static void
map_stats(void)
{
    __pmnsTree *pmns;
    struct dirent ** files;
    int need_reload = 0;
    int i, sts, num;

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	return;
    }

    mcnt = 1;
    __pmAddPMNSNode(pmns, pmid_build(dispatch.domain, 0, 0), "mmv.reload");

    if (indoms != NULL) {
	for (i = 0; i < incnt; i++)
	    free(indoms[i].it_set);
	free (indoms);
	indoms = NULL;
	incnt = 0;
    }

    if (slist != NULL) {
	for (i = 0; i < scnt; i++) {
	    free(slist[i].name);
	    free(slist[i].extra);
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

	if (strncmp(files[i]->d_name, ".", 2) == 0 ||
	    strncmp(files[i]->d_name, "..", 3) == 0)
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
					"%s: mmv client version %d "
					"not supported (current is %d)",
					pmProgname, hdr->version, MMV_VERSION);
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
		    __pmNotifyErr(LOG_INFO, "%s: loading mmv client: %d \"%s\"",
				    pmProgname, cluster, path);

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

			sprintf(name, "mmv.%s.", s->name);

			metrics[mcnt].m_user = ml + k;
			metrics[mcnt].m_desc.pmid = pmid_build(
				dispatch.domain, s->cluster, ml[k].item);

			if (ml[k].type == MMV_ENTRY_INTEGRAL) {
			    metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
			    metrics[mcnt].m_desc.type = MMV_ENTRY_I64;
			} else {
			    if (ml[k].semantics)
				metrics[mcnt].m_desc.sem = ml[k].semantics;
			    else
				metrics[mcnt].m_desc.sem = PM_SEM_COUNTER;
			    metrics[mcnt].m_desc.type = ml[k].type;
			}
			metrics[mcnt].m_desc.indom =
				(ml[k].indom == -1) ? PM_INDOM_NULL :
				pmInDom_build(dispatch.domain,
					(s->cluster << 11) | ml[k].indom);
			memcpy(&metrics[mcnt].m_desc.units,
				&ml[k].dimension, sizeof(pmUnits));

			strcat(name, ml[k].name);
			__pmAddPMNSNode(pmns, pmid_build(
				dispatch.domain, s->cluster, ml[k].item),
				name);
			mcnt++;
		    }
		} else {
		    __pmNotifyErr(LOG_ERR, "%s: cannot grow metric list",
				  pmProgname);
		    exit(1);
		}
		break;

	    case MMV_TOC_INDOMS:
		indoms = realloc(indoms, (incnt+1) * sizeof(pmdaIndom));

		if (indoms != NULL) {
		    mmv_disk_indom_t * id = (mmv_disk_indom_t *)
				((char *)s->addr + toc[j].offset);

		    indoms[incnt].it_indom = pmInDom_build(dispatch.domain,
					(slist[i].cluster << 11) | id->serial);
		    indoms[incnt].it_numinst = id->count;
		    indoms[incnt].it_set = (pmdaInstid *)
			calloc(id->count, sizeof(pmdaInstid));

		    if (indoms[incnt].it_set != NULL) {
			mmv_disk_instance_t * in = (mmv_disk_instance_t *)
				((char *)s->addr + id->offset);
			for (k = 0; k < indoms[incnt].it_numinst; k++) {
			    indoms[incnt].it_set[k].i_inst = in[k].internal;
			    indoms[incnt].it_set[k].i_name = in[k].external;
			}
		    } else {
			__pmNotifyErr(LOG_ERR, 
				"%s: cannot get memory for instance list",
				pmProgname);
			exit(1);
		    }
		    incnt++;
		} else {
		    __pmNotifyErr(LOG_ERR, "%s: cannot grow indom list",
				  pmProgname);
		    exit(1);
		}
		break;

	    case MMV_TOC_VALUES: 
		s->vcnt = toc[j].count;
		s->values = (mmv_disk_value_t *)
			((char *)s->addr + toc[j].offset);
		s->extra = (__int64_t *)malloc(s->vcnt * sizeof(__int64_t));
		if (!s->extra) {
		    __pmNotifyErr(LOG_ERR, 
				  "%s: cannot get memory for values",
				  pmProgname);
		    exit(1);
		} else {
		    for (k = 0; k < s->vcnt; k++)
			s->extra[k] = -1;
		}
		break;

	    default:
		break;
	    }
	}
    }

    write_pmnsfile(pmns);
    __pmFreePMNS(pmns);

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

	    if ((mt == m) && ((mt->indom < 0) || (is->internal == inst))) {
		switch (m->type) {
		    case MMV_ENTRY_I32:
		    case MMV_ENTRY_U32:
		    case MMV_ENTRY_I64:
		    case MMV_ENTRY_U64:
		    case MMV_ENTRY_FLOAT:
		    case MMV_ENTRY_DOUBLE:
			memcpy(atom, &val[i].value, sizeof(pmAtomValue));
			break;
		    case MMV_ENTRY_INTEGRAL: {
			struct timeval tv; 
			gettimeofday (&tv, NULL); 
			atom->ll = val[i].value.ll + 
			    val[i].extra * (tv.tv_sec*1e6 + tv.tv_usec);
			break;
		    }
		    case MMV_ENTRY_STRING: {
			mmv_disk_string_t * string = (mmv_disk_string_t *)
					((char *)s->addr + val[i].extra);
			atom->cp = string->payload;
			break;
		    }
		    case MMV_ENTRY_NOSUPPORT:
			return PM_ERR_APPVERSION;
		}
		return 1;
	    }
	}
	return PM_ERR_PMID;
    }

    return 0;
}

static int
mmv_reload_maybe(void)
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
	/* something changed - reload */
	pmdaExt * pmda = dispatch.version.two.ext; /* we know it is V.2 */

	__pmSendError(pmda->e_outfd, PDU_BINARY, PM_ERR_PMDANOTREADY);
	__pmNotifyErr(LOG_INFO, "%s: reloading", pmProgname);

	map_stats();

	pmda->e_indoms = indoms;
	pmda->e_nindoms = incnt;
	pmda->e_metrics = metrics;
	pmda->e_nmetrics = mcnt;
	pmda->e_direct = 0;

	__pmNotifyErr(LOG_INFO, 
		      "%s: %d metrics and %d indoms after reload", 
		      pmProgname, mcnt, incnt);

	reload = update_namespace();
    }

    return need_reload;
}

/* Intercept request for descriptor and check if we'd have to reload */
static int
mmv_desc(pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    if (mmv_reload_maybe())
	return PM_ERR_PMDAREADY;
    return pmdaDesc(pmid, desc, ep);
}

static int
mmv_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    if (type & PM_TEXT_INDOM)
	return PM_ERR_TEXT;

    if (mmv_reload_maybe())
	return PM_ERR_PMDAREADY;
    else if (pmid_cluster(ident) == 0)
	return pmdaText(ident, type, buffer, ep);
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
    if (mmv_reload_maybe())
	return PM_ERR_PMDAREADY;
    return pmdaInstance(indom, inst, name, result, ep);
}

static int
mmv_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    if (mmv_reload_maybe())
	return PM_ERR_PMDAREADY;
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
mmv_store(pmResult *result, pmdaExt *ep)
{
    int i, m;

    if (mmv_reload_maybe())
	return PM_ERR_PMDAREADY;

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

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int		err = 0;
    int		sep = __pmPathSeparator();
    char	mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    snprintf(mypath, sizeof(mypath), "%s%c" "mmv" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, MMV,
		"mmv.log", mypath);

    if ((pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF) ||
	err || argc != optind)
	usage();

    pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    pcpvardir = pmGetConfig("PCP_VAR_DIR");
    pcppmdasdir = pmGetConfig("PCP_PMDAS_DIR");

    sprintf(statsdir, "%s%c" "mmv", pcptmpdir, sep);
    sprintf(pmnsdir, "%s%c" "pmns", pcpvardir, sep);

    pmdaOpenLog(&dispatch);

    /* Initialize internal dispatch table */
    if (dispatch.status == 0) {
	if ((metrics = malloc(sizeof(pmdaMetric))) != NULL) {
	    metrics[mcnt].m_user = & reload;
	    metrics[mcnt].m_desc.pmid = pmid_build(dispatch.domain, 0, 0);
	    metrics[mcnt].m_desc.type = PM_TYPE_32;
	    metrics[mcnt].m_desc.indom = PM_INDOM_NULL;
	    metrics[mcnt].m_desc.sem = PM_SEM_INSTANT;
	    memset (&metrics[mcnt].m_desc.units, 0, sizeof(pmUnits));
	    mcnt = 1;
	} else {
	    __pmNotifyErr(LOG_ERR, "%s: pmdaInit - out of memory\n",
				pmProgname);
	    exit(0);
	}

	dispatch.version.two.fetch = mmv_fetch;
	dispatch.version.two.store = mmv_store;
	dispatch.version.two.desc = mmv_desc;
	dispatch.version.two.text = mmv_text;
	dispatch.version.two.instance = mmv_instance;

	pmdaSetFetchCallBack(&dispatch, mmv_fetchCallBack);

	__pmNotifyErr(LOG_INFO, "%s: pmdaInit - %d metrics and %d indoms", 
		      pmProgname, mcnt, incnt);

	pmdaInit(&dispatch, indoms, incnt, metrics, mcnt);
    }

    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
