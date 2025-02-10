/*
 * Copyright (c) 2012-2021,2024 Red Hat.
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
 * This PMDA uses specially formatted files from $PCP_TMP_DIR/mmv or some
 * other directory, as specified on the command line.  Each file represents
 * a separate "cluster" of values with flat name structure for each cluster.
 * Names for the metrics are optionally prepended with mmv and then the name
 * of the file (by default - this can be changed).
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>
#include <pcp/libpcp.h>
#include <pcp/deprecated.h>
#include <pcp/pmda.h>
#include "./domain.h"
#include <sys/stat.h>
#include <inttypes.h>
#include <ctype.h>
#include <math.h>

static int isDSO = 1;
static char *username;
static const char *mmv_prefix = "mmv";
static const char *pmproxy_prefix = "pmproxy";
static void *privdata;

static int setup;
static pmAtomValue aNaN;

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

typedef struct {
    char		*name;		/* strdup client name */
    void		*addr;		/* mmap */
    mmv_disk_value_t	*values;	/* values in mmap */
    mmv_disk_metric_t	*metrics1;	/* v1 metric descs in mmap */
    mmv_disk_metric2_t	*metrics2;	/* v2 metric descs in mmap */
    mmv_disk_label_t	*labels; 	/* labels desc in mmap */
    int			vcnt;		/* number of values */
    int			mcnt1;		/* number of metrics */
    int			mcnt2;		/* number of v2 metrics */
    int			lcnt;		/* number of labels */
    int			version;	/* v1/v2/v3 version number */
    int			cluster;	/* cluster identifier */
    pid_t		pid;		/* process identifier */
    __int64_t		len;		/* mmap region len */
    __uint64_t		gen;		/* generation number on open */
} stats_t;

typedef struct {
    pmdaMetric		*metrics;
    pmdaIndom		*indoms;
    pmdaNameSpace	*pmns;
    stats_t		*slist;
    int			scnt;
    int			mtot;
    int			intot;
    int			reload;		/* require reload of maps */
    int			notify;		/* notify pmcd of changes */
    int			statsdir_code;	/* last statsdir stat code */
    struct stat		statsdir_stat;	/* last statsdir stat struct */
    const char		*prefix;
    char		*pcptmpdir;		/* probably /var/tmp */
    char		*pcpvardir;		/* probably /var/pcp */
    char		*pcppmdasdir;		/* probably /var/pcp/pmdas */
    char		pmnsdir[MAXPATHLEN];	/* pcpvardir/pmns */
    char		statsdir[MAXPATHLEN];	/* pcptmpdir/<prefix> */
    char		buffer[MMV_STRINGMAX];	/* temporary fetch buffer */
} agent_t;

/* enforce reasonable limits for various data structures */
#define MAX_MMV_ITEMS	((1<<10)-1)
#define MAX_MMV_SERIAL	((1<<22)-1)
#define MAX_MMV_CLUSTER ((1<<12)-1)
#define MAX_MMV_LABELS	((1<<8)-1)

/*
 * Check cluster number validity (must be in range 0 .. 1<<12).
 */
static int
valid_cluster(int requested)
{
    return (requested >= 0 && requested <= MAX_MMV_CLUSTER);
}

/*
 * Choose an unused cluster ID while honouring specific requests.
 * If a specific (non-zero) cluster is requested we always use it.
 */
static int
choose_cluster(agent_t *ap, int requested, const char *path)
{
    int i;

    if (!requested) {
	int next_cluster = 1;

	for (i = 0; i < ap->scnt; i++) {
	    if (ap->slist[i].cluster == next_cluster) {
		next_cluster++;
		i = 0;	/* restart, we're filling holes */
	    }
	}
	if (!valid_cluster(next_cluster))
	    return -EAGAIN;

	return next_cluster;
    }

    if (!valid_cluster(requested))
	return -EINVAL;

    for (i = 0; i < ap->scnt; i++) {
	if (ap->slist[i].cluster == requested) {
	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG,
				"%s: duplicate cluster %d in use",
				ap->prefix, requested);
	    break;
	}
    }
    return requested;
}

static int
create_client_stat(agent_t *ap, const char *client, const char *path, size_t size)
{
    mmv_disk_header_t	header;
    stats_t		*sp;
    size_t		offset;
    void		*m;
    int			cluster;
    int			in, fd;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "%s: create_client_stat: %s, %s", ap->prefix, client, path);

    /* sanity check */
    if (size < sizeof(mmv_disk_header_t)) {
	pmNotifyErr(LOG_ERR,
	    "%s: %s: size %d too small, expecting at least %d",
	    ap->prefix, client, (int)size, (int)sizeof(mmv_disk_header_t));
	return -EINVAL;
    }

    if ((fd = open(path, O_RDONLY)) >= 0) {
	m = __pmMemoryMap(fd, size, 0);
	close(fd);

	if (m != NULL) {
	    header = *(mmv_disk_header_t *)m;
	    if (strncmp(header.magic, "MMV", 4)) {
		__pmMemoryUnmap(m, size);
		pmNotifyErr(LOG_ERR,
		    "%s: %s: magic \"%3.3s\" not \"MMV\" as expected",
		    ap->prefix, client, header.magic);
		return -EINVAL;
	    }

	    if (header.version != MMV_VERSION1 &&
		header.version != MMV_VERSION2 &&
		header.version != MMV_VERSION3) {
		pmNotifyErr(LOG_ERR,
		    "%s: %s version %d unsupported (current is %d)",
		    ap->prefix, client, header.version, MMV_VERSION);
		__pmMemoryUnmap(m, size);
		return -ENOSYS;
	    }

	    if (header.g1 == 0 || header.g1 != header.g2) {
		/* still in flux, wait until next refresh */
		__pmMemoryUnmap(m, size);
		if (header.g1 == 0) {
		    if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG,
			    "%s: %s: generation 0, no data yet",
			    ap->prefix, client);
		}
		else
		    pmNotifyErr(LOG_INFO,
			"%s: %s: generation in flux, g1=%" FMT_UINT64 " g2=%" FMT_UINT64,
			ap->prefix, client, header.g1, header.g2);
		return -EAGAIN;
	    }

	    /* must have entries for at least metric descs and values */
	    if (header.tocs < 2) {
		__pmMemoryUnmap(m, size);
		pmNotifyErr(LOG_ERR,
		    "%s: %s: bad header.doc %d, expecting at least 2",
		    ap->prefix, client, header.tocs);
		return -EINVAL;
	    }
	    offset = header.tocs * sizeof(mmv_disk_toc_t);
	    if (size < sizeof(mmv_disk_header_t) + offset) {
		__pmMemoryUnmap(m, size);
		pmNotifyErr(LOG_ERR,
		    "%s: %s: bad total size %d, expecting %d",
		    ap->prefix, client, (int)size, (int)(sizeof(mmv_disk_header_t) + offset));
		return -EINVAL;
	    }

	    /* optionally verify the creator PID is running */
	    if (header.process && (header.flags & MMV_FLAG_PROCESS) &&
		!__pmProcessExists((pid_t)header.process)) {
		__pmMemoryUnmap(m, size);
		pmNotifyErr(LOG_ERR,
		    "%s: %s: owning process PID %" FMT_PID " vanished",
		    ap->prefix, client, (pid_t)header.process);
		return -ESRCH;
	    }

	    if ((cluster = choose_cluster(ap, header.cluster, path)) < 0) {
		__pmMemoryUnmap(m, size);
		pmNotifyErr(LOG_ERR,
		    "%s: %s: choose_cluster failed",
		    ap->prefix, client);
		return cluster;
	    }

	    /* all checks out so far, we'll use this one */
	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "%s: %s: loading client cluster=%d for PMIDs from %s",
				    ap->prefix, client, cluster, path);

	    in = ap->scnt;
	    sp = realloc(ap->slist, sizeof(stats_t) * (in + 1));
	    if (sp != NULL) {
		memset(&sp[in], 0, sizeof(stats_t));
		sp[in].name = strdup(client);
		sp[in].addr = m;
		if ((header.flags & MMV_FLAG_PROCESS) != 0)
		    sp[in].pid = (pid_t)header.process;
		sp[in].version = header.version;
		sp[in].cluster = cluster;
		sp[in].gen = header.g1;
		sp[in].len = size;
		ap->slist = sp;
		ap->scnt++;
	    } else {
		pmNotifyErr(LOG_ERR, "%s: %s: realloc out of memory: %s",
				ap->prefix, client, osstrerror());
		__pmMemoryUnmap(m, size);
		ap->scnt = 0;
		free(ap->slist);
		ap->slist = NULL;
	    }
	} else {
	    pmNotifyErr(LOG_ERR, "%s: %s: failed to memory map \"%s\": %s",
				ap->prefix, client, path, osstrerror());
	}
    } else {
	pmNotifyErr(LOG_ERR, "%s: %s: failed to open: %s",
				ap->prefix, client, osstrerror());
    }
    return 0;
}

/* check validity of client metric name, return non-zero if bad or duplicate */
static int
verify_metric_name(agent_t *ap, const char *name, int pos, stats_t *s)
{
    const char		*p = name;
    pmID		pmid;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "verify_metric_name: %s", name);

    if (p == NULL || *p == '\0' || !isalpha((int)*p)) {
	pmNotifyErr(LOG_ERR, "%s: invalid metric[%d] name start in %s, ignored",
			ap->prefix, pos, s->name);
	return -EINVAL;
    }
    for (++p; (p != NULL && *p != '\0'); p++) {
	if (isalnum((int)*p) || *p == '_' || *p == '.')
	    continue;
	pmNotifyErr(LOG_ERR, "%s: invalid metric[%d] name in %s (@%c), ignored",
			    ap->prefix, pos, s->name, *p);
	return -EINVAL;
    }
    if (pmdaTreePMID(ap->pmns, name, &pmid) == 0)
	return -EEXIST;
    return 0;
}

/*
 * Check client item number validity - must not be too large to fit in PMID!
 * and must be unique within a cluster
 *
 * We're checking the kth entry in ml[].
 */
static int
verify_metric_item(mmv_disk_metric_t *ml, int k, char *name, stats_t *s)
{
    mmv_disk_metric_t	*mp = &ml[k];
    unsigned int	item = mp->item;
    int			j;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "verify_metric_item: item=%u name=%s", item, name);

    if (pmID_item(item) != item) {
	pmNotifyErr(LOG_ERR, "verify_metric_item: invalid item %u (%s) in %s, ignored",
			item, name, s->name);
	return -EINVAL;
    }

    for (j = 0; j < k; j++) {
	if (ml[j].item == item) {
	    pmNotifyErr(LOG_ERR, "verify_metric_item: duplicate item %u - [%d] and [%d] %s, second will be ignored", item, j, k, name);
	    return -EINVAL;
	}
    }

    return 0;
}

static int
verify_metric_item2(mmv_disk_metric2_t *ml, int k, char *name, stats_t *s)
{
    mmv_disk_metric2_t	*mp = &ml[k];
    unsigned int	item = mp->item;
    int			j;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "verify_metric_item2: item=%u name=%s", item, name);

    if (pmID_item(item) != item) {
	pmNotifyErr(LOG_ERR, "verify_metric_item2: invalid item %u (%s) in %s, ignored",
			item, name, s->name);
	return -EINVAL;
    }

    for (j = 0; j < k; j++) {
	if (ml[j].item == item) {
	    pmNotifyErr(LOG_ERR, "verify_metric_item2: duplicate item %u - [%d] and [%d] %s, second will be ignored", item, j, k, name);
	    return -EINVAL;
	}
    }

    return 0;
}

static int
create_metric(pmdaExt *pmda, stats_t *s, char *name, pmID pmid, unsigned indom,
	mmv_metric_type_t type, mmv_metric_sem_t semantics, pmUnits units)
{
    pmdaMetric		*mp;
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "create_metric: name=%s PMID=%s", name, pmIDStr(pmid));

    mp = realloc(ap->metrics, sizeof(pmdaMetric) * (ap->mtot + 1));
    if (mp == NULL)  {
	pmNotifyErr(LOG_ERR, "%s: cannot grow MMV metric list: %s", ap->prefix, s->name);
	return -ENOMEM;
    }
    ap->metrics = mp;
    ap->metrics[ap->mtot].m_user = ap;
    ap->metrics[ap->mtot].m_desc.pmid = pmid;

    if (type == MMV_TYPE_ELAPSED) {
	pmUnits unit = PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0);
	ap->metrics[ap->mtot].m_desc.sem = PM_SEM_COUNTER;
	ap->metrics[ap->mtot].m_desc.type = MMV_TYPE_I64;
	ap->metrics[ap->mtot].m_desc.units = unit;
    } else {
	if (semantics)
	    ap->metrics[ap->mtot].m_desc.sem = semantics;
	else
	    ap->metrics[ap->mtot].m_desc.sem = PM_SEM_COUNTER;
	ap->metrics[ap->mtot].m_desc.type = type;
	memcpy(&ap->metrics[ap->mtot].m_desc.units, &units, sizeof(pmUnits));
    }
    if (!indom || indom == PM_INDOM_NULL)
	ap->metrics[ap->mtot].m_desc.indom = PM_INDOM_NULL;
    else
	ap->metrics[ap->mtot].m_desc.indom = 
		pmInDom_build(pmda->e_domain, (s->cluster << 11) | indom);

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG,
			"map_stats adding metric[%d] name=%s PMID=%s from %s\n",
			ap->mtot, name, pmIDStr(pmid), s->name);

    ap->mtot++;
    pmdaTreeInsert(ap->pmns, pmid, name);

    return 0;
}

/* check client serial number validity, and check for a duplicate */
static int
verify_indom_serial(pmdaExt *pmda, int serial, stats_t *s, pmInDom *p, pmdaIndom **i)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    int			index;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "verify_indom_serial: %s: serial=%u", s->name, serial);

    if (pmInDom_serial(serial) != serial) {
	pmNotifyErr(LOG_ERR, "%s: verify_indom_serial: %s: invalid serial %u, ignored",
			ap->prefix, s->name, serial);
	return -EINVAL;
    }

    *p = pmInDom_build(pmda->e_domain, (s->cluster << 11) | serial);
    for (index = 0; index < ap->intot; index++) {
	*i = &ap->indoms[index];
	if (ap->indoms[index].it_indom == *p)
	    return -EEXIST;
    }
    *i = NULL;
    return 0;
}

static int
update_indom(pmdaExt *pmda, stats_t *s, __uint64_t offset, __uint32_t count,
		mmv_disk_indom_t *id, pmdaIndom *ip)
{
    mmv_disk_instance_t *in1 = NULL;
    mmv_disk_instance2_t *in2 = NULL;
    mmv_disk_string_t	*string;
    pmdaInstid		*iip;
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    int			i, j, size, newinsts = 0;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "update_indom on %s: %u (%d insts, count %d)",
			s->name, id->serial, ip->it_numinst, count);

    if (s->version == MMV_VERSION1) {
	in1 = (mmv_disk_instance_t *)((char *)s->addr + offset);
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++) {
		if (ip->it_set[j].i_inst == in1[i].internal)
		    break;
	    }
	    if (j == ip->it_numinst)
		newinsts++;
	}
    } else if (s->version == MMV_VERSION2 || s->version == MMV_VERSION3) {
	in2 = (mmv_disk_instance2_t *)((char *)s->addr + offset);
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++) {
		if (ip->it_set[j].i_inst == in2[i].internal)
		    break;
	    }
	    if (j == ip->it_numinst)
		newinsts++;
	}
    }
    if (!newinsts)
	return 0;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "update_indom on %s: %u (%d new insts)",
			s->name, id->serial, newinsts);

    /* allocate memory, then append new instances to the known set */
    size = sizeof(pmdaInstid) * (ip->it_numinst + newinsts);
    iip = (pmdaInstid *)realloc(ip->it_set, size);
    if (iip == NULL) {
	pmNotifyErr(LOG_ERR, "%s: cannot get memory for instance list in %s",
			ap->prefix, s->name);
	ip->it_numinst = 0;
	free(ip->it_set);
	ip->it_set = NULL;
	return -ENOMEM;
    }
    ip->it_set = iip;

    if (s->version == MMV_VERSION1) {
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++)
		if (ip->it_set[j].i_inst == in1[i].internal)
		    break;
	    if (j == ip->it_numinst) {
		ip->it_set[j].i_inst = in1[i].internal;
		ip->it_set[j].i_name = in1[i].external;
		ip->it_numinst++;
	    }
	}
    } else if (s->version == MMV_VERSION2 || s->version == MMV_VERSION3) {
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++)
		if (ip->it_set[j].i_inst == in2[i].internal)
		    break;
	    if (j == ip->it_numinst) {
		string = (mmv_disk_string_t *)
				((char *)s->addr + in2[i].external);
		ip->it_set[j].i_inst = in2[i].internal;
		ip->it_set[j].i_name = string->payload;
		ip->it_numinst++;
	    }
	}
    }
    return 0;
}

static int
create_indom(pmdaExt *pmda, stats_t *s, __uint64_t offset, __uint32_t count,
		mmv_disk_indom_t *id, pmInDom indom)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    pmdaIndom		*ip;
    mmv_disk_instance_t	*in1;
    mmv_disk_instance2_t *in2;
    mmv_disk_string_t	*string;
    int			i;

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "%s: %s: create_indom: serial=%u", ap->prefix, s->name, id->serial);

    ip = realloc(ap->indoms, sizeof(pmdaIndom) * (ap->intot + 1));
    if (ip == NULL) {
	pmNotifyErr(LOG_ERR, "%s: %s: realloc indom list failed",
			ap->prefix, s->name);
	return -ENOMEM;
    }
    ap->indoms = ip;
    ip = &ap->indoms[ap->intot++];
    ip->it_indom = indom;
    ip->it_set = (pmdaInstid *)calloc(count, sizeof(pmdaInstid));
    if (ip->it_set == NULL) {
	pmNotifyErr(LOG_ERR, "%s: %s: indom serial=%u alloc inst list failed",
			ap->prefix, s->name, id->serial);
	ip->it_numinst = 0;
	return -ENOMEM;
    }

    if (count == 0 && pmDebugOptions.appl0) {
	pmNotifyErr(LOG_WARNING, "%s: %s:"
		   " indom serial=%u has no instances",
		   ap->prefix, s->name, id->serial);
    }

    if (s->version == MMV_VERSION1) {
	in1 = (mmv_disk_instance_t *)((char *)s->addr + offset);
	ip->it_numinst = count;
	for (i = 0; i < count; i++) {
	    ip->it_set[i].i_inst = in1[i].internal;
	    ip->it_set[i].i_name = in1[i].external;
	}
    } else if (s->version == MMV_VERSION2 || s->version == MMV_VERSION3) {
	in2 = (mmv_disk_instance2_t *)((char *)s->addr + offset);
	ip->it_numinst = count;
	for (i = 0; i < count; i++) {
	    string = (mmv_disk_string_t *)
				((char *)s->addr + in2[i].external);
	    ip->it_set[i].i_inst = in2[i].internal;
	    ip->it_set[i].i_name = string->payload;
	}
    }
    return 0;
}

static void
map_stats(pmdaExt *pmda)
{
    struct dirent	**files;
    struct stat		statbuf;
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    char		path[MAXPATHLEN], name[64], *client;
    int			need_reload = 0, sep = pmPathSeparator();
    int			i, j, k, sts, num;

    if (ap->pmns) {
	pmdaTreeRelease(ap->pmns);
	ap->notify |= PMDA_EXT_NAMES_CHANGE;
    }

    if ((sts = pmdaTreeCreate(&ap->pmns)) < 0) {
	pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			ap->prefix, pmErrStr(sts));
	ap->pmns = NULL;
	return;
    }

    /* hard-coded metrics (not from mmap'd files) */
    ap->mtot = 0;
    pmsprintf(name, sizeof(name), "%s.control.reload", ap->prefix);
    pmdaTreeInsert(ap->pmns, pmID_build(pmda->e_domain, 0, 0), name);
    ap->mtot++;
    pmsprintf(name, sizeof(name), "%s.control.debug", ap->prefix);
    pmdaTreeInsert(ap->pmns, pmID_build(pmda->e_domain, 0, 1), name);
    ap->mtot++;
    pmsprintf(name, sizeof(name), "%s.control.files", ap->prefix);
    pmdaTreeInsert(ap->pmns, pmID_build(pmda->e_domain, 0, 2), name);
    ap->mtot++;
    pmsprintf(name, sizeof(name), "%s.control.metrics", ap->prefix);
    pmdaTreeInsert(ap->pmns, pmID_build(pmda->e_domain, 0, 3), name);
    ap->mtot++;

    if (ap->indoms != NULL) {
	for (i = 0; i < ap->intot; i++)
	    free(ap->indoms[i].it_set);
	free(ap->indoms);
	ap->indoms = NULL;
	ap->intot = 0;
    }

    if (ap->slist != NULL) {
	for (i = 0; i < ap->scnt; i++) {
	    free(ap->slist[i].name);
	    __pmMemoryUnmap(ap->slist[i].addr, ap->slist[i].len);
	}
	free(ap->slist);
	ap->slist = NULL;
	ap->scnt = 0;
    }

    num = scandir(ap->statsdir, &files, NULL, alphasort);
    for (i = 0; i < num; i++) {
	if (files[i]->d_name[0] == '.')
	    continue;

	client = files[i]->d_name;
	pmsprintf(path, sizeof(path), "%s%c%s", ap->statsdir, sep, client);

	if (stat(path, &statbuf) >= 0 && S_ISREG(statbuf.st_mode))
	    if (create_client_stat(ap, client, path, statbuf.st_size) == -EAGAIN)
		need_reload = 1;
    }

    for (i = 0; i < num; i++)
	free(files[i]);
    if (num > 0)
	free(files);

    for (i = 0; ap->slist && i < ap->scnt; i++) {
	stats_t	*s = ap->slist + i;
	mmv_disk_indom_t *id;
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)s->addr;
	mmv_disk_toc_t *toc = (mmv_disk_toc_t *)
			((char *)s->addr + sizeof(mmv_disk_header_t));

	for (j = 0; j < hdr->tocs; j++) {
	    __uint64_t offset = toc[j].offset;
	    __uint32_t count = toc[j].count;
	    __uint32_t type = toc[j].type;

	    switch (type) {
	    case MMV_TOC_METRICS:
		if (count > MAX_MMV_ITEMS) {
		    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " metrics count: %u > %d",
				    ap->prefix, s->name, count, MAX_MMV_ITEMS);
		    continue;
		}
	        else if (count == 0 && pmDebugOptions.appl0) {
		    pmNotifyErr(LOG_WARNING, "%s: %s:"
			       " metrics count: 0", ap->prefix, s->name);
		}
		if (s->version == MMV_VERSION1) {
		    mmv_disk_metric_t *ml = (mmv_disk_metric_t *)
					((char *)s->addr + offset);

		    offset += (count * sizeof(mmv_disk_metric_t));
		    if (s->len < offset) {
			if (pmDebugOptions.appl0) {
			    pmNotifyErr(LOG_INFO, "%s: %s:"
					" metrics offset: %"PRIu64" < %"PRIu64,
					ap->prefix, s->name, s->len, (int64_t)offset);
			}
			continue;
		    }

		    s->metrics1 = ml;
		    s->mcnt1 = count;

		    for (k = 0; k < count; k++) {
			mmv_disk_metric_t *mp = &ml[k];
			pmID pmid;

			/* build name, check its legitimate and unique */
			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    pmsprintf(path, sizeof(path), "%s.", ap->prefix);
			else
			    pmsprintf(path, sizeof(path), "%s.%s.", ap->prefix, s->name);
			strcat(path, mp->name);
			if (verify_metric_name(ap, path, k, s) != 0)
			    continue;
			if (verify_metric_item(ml, k, path, s) != 0)
			    continue;

			pmid = pmID_build(pmda->e_domain, s->cluster, mp->item);
			create_metric(pmda, s, path, pmid, mp->indom,
					mp->type, mp->semantics, mp->dimension);
		    }
		}
		else if (s->version == MMV_VERSION2 || s->version == MMV_VERSION3) {
		    mmv_disk_metric2_t *ml = (mmv_disk_metric2_t *)
					((char *)s->addr + offset);

		    offset += (count * sizeof(mmv_disk_metric2_t));
		    if (s->len < offset) {
			if (pmDebugOptions.appl0) {
			    pmNotifyErr(LOG_INFO, "%s: %s:"
					" metrics offset: %"PRIu64" < %"PRIu64,
					ap->prefix, s->name, s->len, (int64_t)offset);
			}
			continue;
		    }

		    s->metrics2 = ml;
		    s->mcnt2 = count;

		    for (k = 0; k < count; k++) {
			mmv_disk_metric2_t *mp = &ml[k];
			mmv_disk_string_t *string;
			char buf[MMV_STRINGMAX];
			__uint64_t mname;
			pmID pmid;

			mname = mp->name;
			if (s->len < mname + sizeof(mmv_disk_string_t)) {
			    if (pmDebugOptions.appl0) {
				pmNotifyErr(LOG_INFO, "%s: %s:"
					" metrics2 name: %"PRIu64" < %"PRIu64,
					ap->prefix, s->name, s->len, mname);
			    }
			    continue;
			}
			string = (mmv_disk_string_t *)((char *)s->addr + mname);
			memcpy(buf, string->payload, sizeof(buf));
			buf[sizeof(buf)-1] = '\0';

			/* build name, check its legitimate and unique */
			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    pmsprintf(path, sizeof(path), "%s.", ap->prefix);
			else
			    pmsprintf(path, sizeof(path), "%s.%s.", ap->prefix, s->name);
			strcat(path, buf);

			if (verify_metric_name(ap, path, k, s) != 0)
			    continue;
			if (verify_metric_item2(ml, k, path, s) != 0)
			    continue;

			pmid = pmID_build(pmda->e_domain, s->cluster, mp->item);
			create_metric(pmda, s, path, pmid, mp->indom,
					mp->type, mp->semantics, mp->dimension);
		    }
		}
		break;

	    case MMV_TOC_INDOMS:
		if (count > MAX_MMV_SERIAL) {
		    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " indoms count: %u > %d",
				    ap->prefix, s->name, count, MAX_MMV_SERIAL);
		    continue;
		}
	        else if (count == 0 && pmDebugOptions.appl0) {
		    pmNotifyErr(LOG_WARNING, "%s: %s:"
			       " indoms count: 0", ap->prefix, s->name);
		}
		id = (mmv_disk_indom_t *)((char *)s->addr + offset);

		offset += (count * sizeof(mmv_disk_indom_t));
		if (s->len < offset) {
		    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " indoms offset: %"PRIu64" < %"PRIu64,
				    ap->prefix, s->name, s->len, offset);
		    continue;
		}

		for (k = 0; k < count; k++) {
		    int serial = id[k].serial;
		    pmInDom pmindom;
		    pmdaIndom *ip;
		    __uint64_t ioffset = id[k].offset;
		    __uint32_t icount = id[k].count;

		    if (s->version == MMV_VERSION1) {
			ioffset += (icount * sizeof(mmv_disk_instance_t));
			if (s->len < ioffset) {
			    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " indom[%d] offset: %"PRIu64" < %"PRIu64,
				    ap->prefix, s->name, k, s->len, ioffset);
			    continue;
			}
		    } else {
			ioffset += (icount * sizeof(mmv_disk_instance2_t));
			if (s->len < ioffset) {
			    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " indom[%d] offset: %"PRIu64" < %"PRIu64,
				    ap->prefix, s->name, k, s->len, ioffset);
			    continue;
			}
		    }
		    ioffset = id[k].offset;
		    sts = verify_indom_serial(pmda, serial, s, &pmindom, &ip);
		    if (sts == -EINVAL)
			continue;
		    else if (sts == -EEXIST)
			/* see if we have new instances to add here */
			update_indom(pmda, s, ioffset, icount, &id[k], ip);
		    else
			/* first time we've observed this indom */
			create_indom(pmda, s, ioffset, icount, &id[k], pmindom);
		}
		break;

	    case MMV_TOC_VALUES:
		offset += (count * sizeof(mmv_disk_value_t));
		if (s->len < offset) {
		    pmNotifyErr(LOG_ERR, "%s: %s:"
				    " values offset: %"PRIu64" < %"PRIu64,
				    ap->prefix, s->name, s->len, offset);
		    continue;
		}
		offset -= (count * sizeof(mmv_disk_value_t));

		s->vcnt = count;
		s->values = (mmv_disk_value_t *)((char *)s->addr + offset);
		break;

	    case MMV_TOC_INSTANCES:
	    case MMV_TOC_STRINGS:
		break;
		
	    case MMV_TOC_LABELS:
	        if (count > MAX_MMV_LABELS) {
		    pmNotifyErr(LOG_ERR, "%s: %s: "
			       " labels count: %u > %d",
				ap->prefix, s->name, count, MAX_MMV_LABELS);
		    continue;
		}
	        else if (count == 0 && pmDebugOptions.appl0) {
		    pmNotifyErr(LOG_WARNING, "%s: %s:"
			       " labels count: 0", ap->prefix, s->name);
		}
		mmv_disk_label_t *lb = (mmv_disk_label_t *)
					((char *)s->addr + offset);

		offset += (count * sizeof(mmv_disk_label_t));
		if (s->len < offset) {
		    if (pmDebugOptions.appl0) {
		        pmNotifyErr(LOG_INFO, "%s: %s:"
				" labels offset: %"PRIu64" < %"PRIu64,
				ap->prefix, s->name, s->len, (int64_t)offset);
		    }
		    continue;
		}

		s->labels = lb;
		s->lcnt = count;
	    	break;

	    default:
		pmNotifyErr(LOG_ERR, "%s: %s: bad TOC type (%x)",
				    ap->prefix, s->name, type);
		break;
	    }
	}
    }

    pmdaTreeRebuildHash(ap->pmns, ap->mtot); /* for reverse (pmid->name) lookups */
    ap->reload = need_reload;
}

static int
mmv_lookup_item1(int item, unsigned int inst,
	stats_t *s, mmv_disk_value_t **value,
	__uint64_t *shorttext, __uint64_t *helptext)
{
    mmv_disk_metric_t	*m1 = s->metrics1;
    mmv_disk_value_t	*v = s->values;
    int			mi, vi, sts = PM_ERR_PMID;

    for (mi = 0; mi < s->mcnt1; mi++) {
	if (m1[mi].item != item)
	    continue;

	sts = PM_ERR_INST;
	for (vi = 0; vi < s->vcnt; vi++) {
	    mmv_disk_metric_t *mt = (mmv_disk_metric_t *)
			((char *)s->addr + v[vi].metric);
	    mmv_disk_instance_t *is = (mmv_disk_instance_t *)
			((char *)s->addr + v[vi].instance);

	    if ((mt == &m1[mi]) &&
		(mt->indom == PM_INDOM_NULL || mt->indom == 0 ||
		inst == PM_IN_NULL || is->internal == inst)) {
		if (shorttext)
		    *shorttext = m1[mi].shorttext;
		if (helptext)
		    *helptext = m1[mi].helptext;
		*value = &v[vi];
		return m1[mi].type;
	    }
	}
    }
    return sts;
}

static int
mmv_lookup_item2(int item, unsigned int inst,
	stats_t *s, mmv_disk_value_t **value,
	__uint64_t *shorttext, __uint64_t *helptext)
{
    mmv_disk_metric2_t	*m2 = s->metrics2;
    mmv_disk_value_t	*v = s->values;
    int			mi, vi, sts = PM_ERR_PMID;

    for (mi = 0; mi < s->mcnt2; mi++) {
	if (m2[mi].item != item)
	    continue;

	sts = PM_ERR_INST;
	for (vi = 0; vi < s->vcnt; vi++) {
	    mmv_disk_metric2_t *mt = (mmv_disk_metric2_t *)
			((char *)s->addr + v[vi].metric);
	    mmv_disk_instance2_t *is = (mmv_disk_instance2_t *)
			((char *)s->addr + v[vi].instance);

	    if ((mt == &m2[mi]) &&
		(mt->indom == PM_INDOM_NULL || mt->indom == 0 ||
		inst == PM_IN_NULL || is->internal == inst)) {
		if (shorttext)
		    *shorttext = m2[mi].shorttext;
		if (helptext)
		    *helptext = m2[mi].helptext;
		*value = &v[vi];
		return m2[mi].type;
	    }
	}
    }
    return sts;
}

static int
mmv_lookup_stat_metric(agent_t *agent, pmID pmid, unsigned int inst,
	stats_t **stats, mmv_disk_value_t **value,
	__uint64_t *shorttext, __uint64_t *helptext)
{
    int			si, sts = PM_ERR_PMID;

    for (si = 0; si < agent->scnt; si++) {
	stats_t *s = &agent->slist[si];

	if (s->cluster != pmID_cluster(pmid))
	    continue;

	sts = (s->version == MMV_VERSION1) ?
	    mmv_lookup_item1(pmID_item(pmid), inst, s, value, shorttext, helptext):
	    mmv_lookup_item2(pmID_item(pmid), inst, s, value, shorttext, helptext);
	if (sts == MMV_TYPE_NOSUPPORT)
	    sts = PM_ERR_APPVERSION;
	if (sts >= 0) {
	    *stats = s;
	    break;
	}
    }
    return sts;
}

static int
mmv_lookup_stat_metric_value(agent_t *agent, pmID pmid, unsigned int inst,
	stats_t **stats, mmv_disk_value_t **value)
{
    return mmv_lookup_stat_metric(agent, pmid, inst, stats, value, NULL, NULL);
}

/*
 * callback provided to pmdaFetch
 */
static int
mmv_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    mmv_disk_string_t	*str;
    mmv_disk_value_t	*v;
    __uint64_t		offset;
    agent_t		*ap = (agent_t *)mdesc->m_user;
    stats_t		*s;
    pmID		pmid = mdesc->m_desc.pmid;
    int			sts, flags;

    if (pmID_cluster(pmid) == 0) {
	switch(pmID_item(pmid)) {
	    case 0:
		atom->l = ap->reload;
		return PMDA_FETCH_STATIC;
	    case 1:
		atom->l = pmDebug;
		return PMDA_FETCH_STATIC;
	    case 2:
		atom->l = ap->scnt;
		return PMDA_FETCH_STATIC;
	    case 3:
		/*
		 * control.metrics ... number of scanned metrics less
		 * control ones
		 */
		atom->l = ap->mtot - 4;
		return PMDA_FETCH_STATIC;
	    default:
		break;
	}
	return PM_ERR_PMID;
    }

    if (ap->scnt > 0) {	/* We have at least one source of metrics */
	if ((sts = mmv_lookup_stat_metric_value(ap, pmid, inst, &s, &v)) < 0)
	    return sts;
	flags = ((mmv_disk_header_t *)s->addr)->flags;

	switch (sts) {
	    case MMV_TYPE_I32:
	    case MMV_TYPE_U32:
	    case MMV_TYPE_I64:
	    case MMV_TYPE_U64:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((flags & MMV_FLAG_SENTINEL) &&
		    (memcmp(atom, &aNaN, sizeof(*atom)) == 0))
		    return PMDA_FETCH_NOVALUES;
		break;
	    case MMV_TYPE_FLOAT:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((flags & MMV_FLAG_SENTINEL) && isnan(atom->f))
		    return PMDA_FETCH_NOVALUES;
		break;
	    case MMV_TYPE_DOUBLE:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((flags & MMV_FLAG_SENTINEL) && isnan(atom->d))
		    return PMDA_FETCH_NOVALUES;
		break;
	    case MMV_TYPE_ELAPSED: {
		atom->ll = v->value.ll;
		if ((flags & MMV_FLAG_SENTINEL) &&
		    (memcmp(atom, &aNaN, sizeof(*atom)) == 0))
		    return 0;
		if (v->extra < 0) {	/* inside a timed section */
		    struct timeval tv; 
		    pmtimevalNow(&tv); 
		    atom->ll += (tv.tv_sec * 1e6 + tv.tv_usec) + v->extra;
		}
		break;
	    }
	    case MMV_TYPE_STRING: {
		offset = v->extra;
		if (s->len < offset + sizeof(MMV_STRINGMAX)) {
		    pmNotifyErr(LOG_ERR, "%s: %s:"
			    " bad string value offset: %"PRIu64" < %"PRIu64,
			    ap->prefix, s->name, s->len,
			    offset + sizeof(MMV_STRINGMAX));
		    return PM_ERR_GENERIC;
		}
		str = (mmv_disk_string_t *)((char *)s->addr + offset);
		memcpy(ap->buffer, str->payload, sizeof(ap->buffer));
		if ((flags & MMV_FLAG_SENTINEL) && ap->buffer[0] == '\0')
		    return PMDA_FETCH_NOVALUES;
		ap->buffer[sizeof(ap->buffer)-1] = '\0';
		atom->cp = ap->buffer;
		break;
	    }
	}
	return PMDA_FETCH_STATIC;
    }

    return PMDA_FETCH_NOVALUES;
}

static void
mmv_reload_maybe(pmdaExt *pmda)
{
    struct stat		s;
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    int			i, need_reload = ap->reload;

    /* check if generation numbers changed or monitored process exited */
    for (i = 0; i < ap->scnt; i++) {
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)ap->slist[i].addr;
	if (hdr->g1 != ap->slist[i].gen || hdr->g2 != ap->slist[i].gen) {
	    need_reload++;
	    break;
	}
	if (ap->slist[i].pid && !__pmProcessExists(ap->slist[i].pid)) {
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
    if (stat(ap->statsdir, &s) >= 0) {
#if defined(HAVE_ST_MTIME_WITH_E)
	if (s.st_mtime != ap->statsdir_stat.st_mtime)
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	if (s.st_mtimespec.tv_sec != ap->statsdir_stat.st_mtimespec.tv_sec ||
	    s.st_mtimespec.tv_nsec != ap->statsdir_stat.st_mtimespec.tv_nsec)
#else
	if (s.st_mtim.tv_sec != ap->statsdir_stat.st_mtim.tv_sec ||
	    s.st_mtim.tv_nsec != ap->statsdir_stat.st_mtim.tv_nsec)
#endif
	{
	    need_reload++;
	    ap->statsdir_code = 0;
	    ap->statsdir_stat = s;
	}
    } else {
	i = oserror();
	if (ap->statsdir_code != i) {
	    ap->statsdir_code = i;
	    memset(&ap->statsdir_stat, 0, sizeof(ap->statsdir_stat));
	    need_reload++;
	}
    }

    if (need_reload) {
	if (pmDebugOptions.appl0)
	    pmNotifyErr(LOG_DEBUG, "reloading");
	map_stats(pmda);

	pmda->e_indoms = ap->indoms;
	pmda->e_nindoms = ap->intot;
	pmdaRehash(pmda, ap->metrics, ap->mtot);

	if (pmDebugOptions.appl0)
	    pmNotifyErr(LOG_INFO, 
		      "%s: %d metrics and %d indoms after reload", 
		      pmGetProgname(), ap->mtot, ap->intot);
    }
}

/* Intercept request for descriptor and check if we'd have to reload */
static int
mmv_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaDesc(pmid, desc, pmda);
}

static int
mmv_lookup_metric_helptext(agent_t *ap, pmID pmid, int type, char **text)
{
    mmv_disk_string_t	*str;
    mmv_disk_value_t	*v;
    __uint64_t		st, lt;
    size_t		offset;
    stats_t		*s;

    if (mmv_lookup_stat_metric(ap, pmid, PM_IN_NULL, &s, &v, &st, &lt) < 0)
	return PM_ERR_PMID;

    if ((type & PM_TEXT_ONELINE) && st) {
	offset = st + sizeof(mmv_disk_string_t);
	if (s->len < offset) {
	    pmNotifyErr(LOG_ERR, "%s: %s:"
			    " bad shorttext offset: %"PRIu64" < %"PRIu64,
			    ap->prefix, s->name, s->len, (uint64_t)offset);
	    return PM_ERR_GENERIC;
	}
	offset -= sizeof(mmv_disk_string_t);

	str = (mmv_disk_string_t *)((char *)s->addr + offset);
	memcpy(ap->buffer, str->payload, sizeof(ap->buffer));
	ap->buffer[sizeof(ap->buffer)-1] = '\0';
	*text = ap->buffer;
	return 0;
    }

    if ((type & PM_TEXT_HELP) && lt) {
	offset = lt + sizeof(mmv_disk_string_t);
	if (s->len < offset) {
	    pmNotifyErr(LOG_ERR, "%s: %s:"
			    " bad helptext offset: %"PRIu64" < %"PRIu64,
			    ap->prefix, s->name, s->len, (uint64_t)offset);
	    return PM_ERR_GENERIC;
	}
	offset -= sizeof(mmv_disk_string_t);

	str = (mmv_disk_string_t *)((char *)s->addr + offset);
	memcpy(ap->buffer, str->payload, sizeof(ap->buffer));
	ap->buffer[sizeof(ap->buffer)-1] = '\0';
	*text = ap->buffer;
	return 0;
    }

    return PM_ERR_TEXT;
}

static int
mmv_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    agent_t		*agent = (agent_t *)pmdaExtGetData(pmda);

    if (type & PM_TEXT_INDOM)
	return PM_ERR_TEXT;

    mmv_reload_maybe(pmda);
    if (pmID_cluster(ident) == 0) {
	switch (pmID_item(ident)) {
	    case 0: {
		static char reloadoneline[] = "Control maps reloading";
		static char reloadtext[] = 
"Writing anything other then 0 to this metric will result in\n"
"re-reading directory and re-mapping files.\n";

		*buffer = (type & PM_TEXT_ONELINE) ? reloadoneline : reloadtext;
		return 0;
	    }
	    case 1: {
		static char debugoneline[] = "Debug flag";
		static char debugtext[] =
"See pmdbg(1).  pmstore into this metric to change the debug value.\n";

		*buffer = (type & PM_TEXT_ONELINE) ? debugoneline : debugtext;
		return 0;
	    }
	    case 2: {
		static char filesoneline[] = "Memory mapped file count";
		static char filestext[] =
"Count of currently mapped and exported statistics files.\n";

		*buffer = (type & PM_TEXT_ONELINE) ? filesoneline : filestext;
		return 0;
	    }
	    case 3: {
		static char metricsoneline[] = "Memory mapped metric count";
		static char metricstext[] =
"Count of currently mapped and exported metrics.\n\
Excludes the mmv.control.* metrics.\n";

		*buffer = (type & PM_TEXT_ONELINE) ? metricsoneline : metricstext;
		return 0;
	    }
	}
	return PM_ERR_PMID;
    }

    return mmv_lookup_metric_helptext(agent, ident, type, buffer);
}

static int
mmv_instance(pmInDom indom, int inst, char *name, 
	     pmInResult **result, pmdaExt *pmda)
{
    mmv_reload_maybe(pmda);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
mmv_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);

    mmv_reload_maybe(pmda);
    if (ap->notify) {
	pmdaExtSetFlags(pmda, ap->notify);
	ap->notify = 0;
    }
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
mmv_store(pmResult *result, pmdaExt *pmda)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    int			i, m;

    mmv_reload_maybe(pmda);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet * vsp = result->vset[i];

	if (pmID_cluster(vsp->pmid) == 0) {
	    for (m = 0; m < ap->mtot; m++) {

		if (pmID_cluster(ap->metrics[m].m_desc.pmid) == 0 &&
		    pmID_item(ap->metrics[m].m_desc.pmid) == pmID_item(vsp->pmid)) {
		    pmAtomValue atom;
		    int sts;

		    if (vsp->numval != 1 )
			return PM_ERR_BADSTORE;

		    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
					PM_TYPE_32, &atom, PM_TYPE_32)) < 0)
			return sts;
		    if (pmID_item(vsp->pmid) == 0)
			ap->reload = atom.l;
		    else if (pmID_item(vsp->pmid) == 1)
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
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);

    mmv_reload_maybe(pmda);
    return pmdaTreePMID(ap->pmns, name, pmid);
}

static int
mmv_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);

    mmv_reload_maybe(pmda);
    return pmdaTreeName(ap->pmns, pmid, nameset);
}

static int
mmv_children(const char *name, int traverse, char ***kids, int **sts, pmdaExt *pmda)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);

    mmv_reload_maybe(pmda);
    return pmdaTreeChildren(ap->pmns, name, traverse, kids, sts);
}

static int
mmv_label_lookup(agent_t *ap, int ident, int type, pmLabelSet **lp)
{
    mmv_disk_label_t	lb;
    stats_t		*s;
    int			i, j, id, cluster;

    if (type & PM_LABEL_INDOM) {
	cluster = 0;	/* indoms can span mappings */
	ident = pmInDom_serial(ident);
    } else if (type & PM_LABEL_ITEM) {
	cluster = pmID_cluster(ident);
	ident = pmID_item(ident);
    } else if (type & PM_LABEL_CLUSTER) {
	cluster = pmID_cluster(ident);
	ident = cluster;
    } else {
	return 0;
    }

    /* search for labels with requested identifier for given type */
    for (i = 0; i < ap->scnt; i++) {
	s = &ap->slist[i];
	if (type & (PM_LABEL_CLUSTER|PM_LABEL_ITEM))
	    if (s->cluster != cluster)
		continue;
	for (j = 0; j < s->lcnt; j++) {
	    lb = s->labels[j];
	    if (type & PM_LABEL_INDOM)
		id = ((s->cluster << 11) | lb.identity);
	    else
		id = lb.identity;
	    if ((lb.flags & type) && id == ident)
		__pmAddLabels(lp, lb.payload, lb.flags);
	}
    }
    return 0;
}

static int
mmv_label(int ident, int type, pmLabelSet **lp, pmdaExt *pmda)
{
    agent_t		*ap = (agent_t *)pmdaExtGetData(pmda);
    int			sts = 0;

    if ((sts = mmv_label_lookup(ap, ident, type, lp)) < 0)
	return sts;
    privdata = (void *)ap;
    sts = pmdaLabel(ident, type, lp, pmda);
    return sts;
}

static int
mmv_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    mmv_disk_label_t	lb;
    stats_t		*s;
    agent_t		*ap = (agent_t *)privdata;
    int			i, j, count = 0;

    /* search for labels with requested indom and instance identifier */
    for (i = 0; i < ap->scnt; i++) {
	s = &ap->slist[i];
	for (j = 0; j < s->lcnt; j++) {
	    lb = s->labels[j];
	    if (!(lb.flags & PM_LABEL_INSTANCES))
		continue;
	    if (lb.internal != inst)
		continue;
	    if (((s->cluster << 11) | lb.identity) != pmInDom_serial(indom))
		continue;
	    if (__pmAddLabels(lp, lb.payload, lb.flags) < 0)
		continue;
	    count++;
	}
    }
    return count;
}

static void
init_pmda(pmdaInterface *dp, agent_t *ap)
{
    int		m;
    int		sep = pmPathSeparator();

    if (!setup) {
	setup++;
	memset(&aNaN, -1, sizeof(pmAtomValue));
    }

    if (isDSO) {
	pmdaDSO(dp, PMDA_INTERFACE_7, (char *)ap->prefix, NULL);
    } else {
	pmSetProcessIdentity(username);
    }

    ap->pcptmpdir = pmGetConfig("PCP_TMP_DIR");
    ap->pcpvardir = pmGetConfig("PCP_VAR_DIR");
    ap->pcppmdasdir = pmGetConfig("PCP_PMDAS_DIR");

    pmsprintf(ap->statsdir, MAXPATHLEN, "%s%c%s", ap->pcptmpdir, sep, ap->prefix);
    pmsprintf(ap->pmnsdir, MAXPATHLEN, "%s%c" "pmns", ap->pcpvardir, sep);

    /* Initialize internal dispatch table */
    if (dp->status == 0) {
	/*
	 * number of hard-coded metrics here has to match initializer
	 * cases below, and pmns initialization in map_stats()
	 */
	ap->mtot = 4;
	if ((ap->metrics = malloc(ap->mtot * sizeof(pmdaMetric))) != NULL) {
	    /*
	     * all the hard-coded metrics have the same semantics
	     */
	    for (m = 0; m < ap->mtot; m++) {
		ap->metrics[m].m_user = (void *)ap;
		ap->metrics[m].m_desc.pmid = pmID_build(dp->domain, 0, m);
		ap->metrics[m].m_desc.type = PM_TYPE_32;
		ap->metrics[m].m_desc.indom = PM_INDOM_NULL;
		ap->metrics[m].m_desc.sem = PM_SEM_INSTANT;
		memset(&ap->metrics[m].m_desc.units, 0, sizeof(pmUnits));
	    }
	} else {
	    pmNotifyErr(LOG_ERR, "%s: pmdaInit - out of memory\n",
				pmGetProgname());
	    if (isDSO)
		return;
	    exit(0);
	}

	dp->version.seven.fetch = mmv_fetch;
	dp->version.seven.store = mmv_store;
	dp->version.seven.desc = mmv_desc;
	dp->version.seven.text = mmv_text;
	dp->version.seven.instance = mmv_instance;
	dp->version.seven.pmid = mmv_pmid;
	dp->version.seven.name = mmv_name;
	dp->version.seven.children = mmv_children;
	dp->version.seven.label = mmv_label;
	pmdaSetFetchCallBack(dp, mmv_fetchCallBack);
	pmdaSetLabelCallBack(dp, mmv_labelCallBack);

	pmdaSetData(dp, (void *)ap);
	pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
	pmdaInit(dp, ap->indoms, ap->intot, ap->metrics, ap->mtot);
    }
}

void
__PMDA_INIT_CALL
pmproxy_init(pmdaInterface *dp)
{
    agent_t	*agent = (agent_t *)calloc(1, sizeof(agent_t));

    if (agent) {
	agent->prefix = pmproxy_prefix;
	init_pmda(dp, agent);
    } else {
	dp->status = -ENOMEM;
    }
}

void
__PMDA_INIT_CALL
mmv_init(pmdaInterface *dp)
{
    agent_t	*agent = (agent_t *)calloc(1, sizeof(agent_t));

    if (agent) {
	agent->prefix = mmv_prefix;
	init_pmda(dp, agent);
    } else {
	dp->status = -ENOMEM;
    }
}

int
main(int argc, char **argv)
{
    int		isMMV = 1;
    int		domain = MMV;
    char	*progname;
    char	logfile[32];
    const char	*prefix = mmv_prefix;
    pmdaInterface dispatch = { 0 };

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    progname = pmGetProgname();
    if (strncmp(progname, "pmda", 4) == 0 && strlen(progname) > 4 &&
	(isMMV = (strcmp(progname + 4, pmproxy_prefix) != 0)) == 0) {
	prefix = pmproxy_prefix;
	domain = PMPROXY;
    }
    pmsprintf(logfile, sizeof(logfile), "%s.log", prefix);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, progname, domain, logfile, NULL);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    if (isMMV)
	mmv_init(&dispatch);
    else
	pmproxy_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
