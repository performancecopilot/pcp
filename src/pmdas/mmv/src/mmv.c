/*
 * Copyright (c) 2012-2017 Red Hat.
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

#include "pmapi.h"
#include "mmv_stats.h"
#include "mmv_dev.h"
#include "impl.h"
#include "pmda.h"
#include "./domain.h"
#include <sys/stat.h>
#include <inttypes.h>
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
static int mtot;
static pmdaIndom * indoms;
static int intot;

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
    mmv_disk_metric_t * metrics1;	/* v1 metric descs in mmap */
    mmv_disk_metric2_t * metrics2;	/* v2 metric descs in mmap */
    int		vcnt;			/* number of values */
    int		mcnt1;			/* number of metrics */
    int		mcnt2;			/* number of v2 metrics */
    int		version;		/* v1/v2 version number */
    int		cluster;		/* cluster identifier */
    pid_t	pid;			/* process identifier */
    __int64_t	len;			/* mmap region len */
    __uint64_t	gen;			/* generation number on open */
} stats_t;

static stats_t * slist;
static int scnt;

#define MAX_MMV_COUNT 10000		/* enforce reasonable limits */
#define MAX_MMV_CLUSTER ((1<<12)-1)

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
	if (!valid_cluster(next_cluster))
	    return -EAGAIN;

	return next_cluster;
    }

    if (!valid_cluster(requested))
	return -EINVAL;

    for (i = 0; i < scnt; i++) {
	if (slist[i].cluster == requested) {
	    if (pmDebugOptions.appl0)
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

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_client_stat: %s, %s", client, path);

    /* sanity check */
    if (size < sizeof(mmv_disk_header_t))
	return -EINVAL;

    if ((fd = open(path, O_RDONLY)) >= 0) {
	void *m = __pmMemoryMap(fd, size, 0);

	close(fd);
	if (m != NULL) {
	    mmv_disk_header_t header = *(mmv_disk_header_t *)m;
	    size_t offset;
	    int cluster;

	    if (strncmp(header.magic, "MMV", 4)) {
		__pmMemoryUnmap(m, size);
		return -EINVAL;
	    }

	    if (header.version != MMV_VERSION1 &&
		header.version != MMV_VERSION2) {
		if (pmDebugOptions.appl0)
		    __pmNotifyErr(LOG_ERR,
			"%s: %s client version %d unsupported (current is %d)",
			pmProgname, prefix, header.version, MMV_VERSION);
		__pmMemoryUnmap(m, size);
		return -ENOSYS;
	    }

	    if (header.g1 == 0 || header.g1 != header.g2) {
		/* still in flux, wait until next refresh */
		__pmMemoryUnmap(m, size);
		return -EAGAIN;
	    }

	    /* must have entries for at least metric descs and values */
	    if (header.tocs < 2) {
		__pmMemoryUnmap(m, size);
		return -EINVAL;
	    }
	    offset = header.tocs * sizeof(mmv_disk_toc_t);
	    if (size < sizeof(mmv_disk_header_t) + offset) {
		__pmMemoryUnmap(m, size);
		return -EINVAL;
	    }

	    /* optionally verify the creator PID is running */
	    if (header.process && (header.flags & MMV_FLAG_PROCESS) &&
		!__pmProcessExists((pid_t)header.process)) {
		__pmMemoryUnmap(m, size);
		return -ESRCH;
	    }

	    if ((cluster = choose_cluster(header.cluster, path)) < 0) {
		__pmMemoryUnmap(m, size);
		return cluster;
	    }

	    /* all checks out so far, we'll use this one */
	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_DEBUG, "MMV: loading %s client: %d \"%s\"",
				    prefix, cluster, path);

	    slist = realloc(slist, sizeof(stats_t) * (scnt + 1));
	    if (slist != NULL) {
		memset(&slist[scnt], 0, sizeof(stats_t));
		slist[scnt].name = strdup(client);
		slist[scnt].addr = m;
		if ((header.flags & MMV_FLAG_PROCESS) != 0)
		    slist[scnt].pid = (pid_t)header.process;
		slist[scnt].version = header.version;
		slist[scnt].cluster = cluster;
		slist[scnt].gen = header.g1;
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
    } else if (pmDebugOptions.appl0) {
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

    if (pmDebugOptions.appl0)
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
    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: verify_metric_item: %u - %s", item, name);

    if (pmid_item(item) != item) {
	__pmNotifyErr(LOG_WARNING, "invalid item %u (%s) in %s, ignored",
			item, name, s->name);
	return -EINVAL;
    }
    return 0;
}

static int
create_metric(pmdaExt *pmda, stats_t *s, char *name, pmID pmid, unsigned indom,
	mmv_metric_type_t type, mmv_metric_sem_t semantics, pmUnits units)
{
    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_metric: %s - %s", name, pmIDStr(pmid));

    metrics = realloc(metrics, sizeof(pmdaMetric) * (mtot + 1));
    if (metrics == NULL)  {
	__pmNotifyErr(LOG_ERR, "cannot grow MMV metric list: %s", s->name);
	return -ENOMEM;
    }

    metrics[mtot].m_user = NULL;
    metrics[mtot].m_desc.pmid = pmid;

    if (type == MMV_TYPE_ELAPSED) {
	pmUnits unit = PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0);
	metrics[mtot].m_desc.sem = PM_SEM_COUNTER;
	metrics[mtot].m_desc.type = MMV_TYPE_I64;
	metrics[mtot].m_desc.units = unit;
    } else {
	if (semantics)
	    metrics[mtot].m_desc.sem = semantics;
	else
	    metrics[mtot].m_desc.sem = PM_SEM_COUNTER;
	metrics[mtot].m_desc.type = type;
	memcpy(&metrics[mtot].m_desc.units, &units, sizeof(pmUnits));
    }
    if (!indom || indom == PM_INDOM_NULL)
	metrics[mtot].m_desc.indom = PM_INDOM_NULL;
    else
	metrics[mtot].m_desc.indom = 
		pmInDom_build(pmda->e_domain, (s->cluster << 11) | indom);

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG,
			"MMV: map_stats adding metric[%d] %s %s from %s\n",
			mtot, name, pmIDStr(pmid), s->name);

    mtot++;
    __pmAddPMNSNode(pmns, pmid, name);

    return 0;
}

/* check client serial number validity, and check for a duplicate */
static int
verify_indom_serial(pmdaExt *pmda, int serial, stats_t *s, pmInDom *p, pmdaIndom **i)
{
    int index;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: verify_indom_serial: %u", serial);

    if (pmInDom_serial(serial) != serial) {
	__pmNotifyErr(LOG_WARNING, "invalid serial %u in %s, ignored",
			serial, s->name);
	return -EINVAL;
    }

    *p = pmInDom_build(pmda->e_domain, (s->cluster << 11) | serial);
    for (index = 0; index < intot; index++) {
	*i = &indoms[index];
	if (indoms[index].it_indom == *p)
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
    mmv_disk_string_t *string;
    int i, j, size, newinsts = 0;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: update_indom: %u (%d insts)",
			id->serial, ip->it_numinst);

    if (s->version == MMV_VERSION1) {
	in1 = (mmv_disk_instance_t *)((char *)s->addr + offset);
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++) {
		if (ip->it_set[j].i_inst == in1[i].internal)
		    continue;
	    }
	    if (j == ip->it_numinst)
		newinsts++;
	}
    } else if (s->version == MMV_VERSION2) {
	in2 = (mmv_disk_instance2_t *)((char *)s->addr + offset);
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++) {
		if (ip->it_set[j].i_inst == in2[i].internal)
		    continue;
	    }
	    if (j == ip->it_numinst)
		newinsts++;
	}
    }
    if (!newinsts)
	return 0;

    /* allocate memory, then append new instances to the known set */
    size = sizeof(pmdaInstid) * (ip->it_numinst + newinsts);
    ip->it_set = (pmdaInstid *)realloc(ip->it_set, size);
    if (ip->it_set == NULL) {
	__pmNotifyErr(LOG_ERR, "%s: cannot get memory for instance list in %s",
			pmProgname, s->name);
	ip->it_numinst = 0;
	return -ENOMEM;
    }

    if (s->version == MMV_VERSION1) {
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++)
		if (ip->it_set[j].i_inst == in1[i].internal)
		    continue;
	    if (j == ip->it_numinst) {
		ip->it_set[j].i_inst = in1[i].internal;
		ip->it_set[j].i_name = in1[i].external;
		ip->it_numinst++;
	    }
	}
    } else if (s->version == MMV_VERSION2) {
	for (i = 0; i < count; i++) {
	    for (j = 0; j < ip->it_numinst; j++)
		if (ip->it_set[j].i_inst == in2[i].internal)
		    continue;
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
    int i;
    pmdaIndom *ip;
    mmv_disk_instance_t *in1;
    mmv_disk_instance2_t *in2;
    mmv_disk_string_t *string;

    if (pmDebugOptions.appl0)
	__pmNotifyErr(LOG_DEBUG, "MMV: create_indom: %u", id->serial);

    indoms = realloc(indoms, sizeof(pmdaIndom) * (intot + 1));
    if (indoms == NULL) {
	__pmNotifyErr(LOG_ERR, "%s: cannot grow indom list in %s",
			pmProgname, s->name);
	return -ENOMEM;
    }
    ip = &indoms[intot++];
    ip->it_indom = indom;
    ip->it_set = (pmdaInstid *)calloc(count, sizeof(pmdaInstid));
    if (ip->it_set == NULL) {
	__pmNotifyErr(LOG_ERR, "%s: cannot get memory for instance list in %s",
			pmProgname, s->name);
	ip->it_numinst = 0;
	return -ENOMEM;
    }

    if (s->version == MMV_VERSION1) {
	in1 = (mmv_disk_instance_t *)((char *)s->addr + offset);
	ip->it_numinst = count;
	for (i = 0; i < count; i++) {
	    ip->it_set[i].i_inst = in1[i].internal;
	    ip->it_set[i].i_name = in1[i].external;
	}
    } else if (s->version == MMV_VERSION2) {
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
    struct dirent **files;
    char name[64];
    int need_reload = 0;
    int i, j, k, sts, num;
    int sep = __pmPathSeparator();

    if (pmns)
	__pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	pmns = NULL;
	return;
    }

    /* hard-coded metrics (not from mmap'd files) */
    pmsprintf(name, sizeof(name), "%s.control.reload", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 0), name);
    pmsprintf(name, sizeof(name), "%s.control.debug", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 1), name);
    pmsprintf(name, sizeof(name), "%s.control.files", prefix);
    __pmAddPMNSNode(pmns, pmid_build(pmda->e_domain, 0, 2), name);
    mtot = 3;

    if (indoms != NULL) {
	for (i = 0; i < intot; i++)
	    free(indoms[i].it_set);
	free(indoms);
	indoms = NULL;
	intot = 0;
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
	pmsprintf(path, sizeof(path), "%s%c%s", statsdir, sep, client);

	if (stat(path, &statbuf) >= 0 && S_ISREG(statbuf.st_mode))
	    if (create_client_stat(client, path, statbuf.st_size) == -EAGAIN)
		need_reload = 1;
    }

    for (i = 0; i < num; i++)
	free(files[i]);
    if (num > 0)
	free(files);

    for (i = 0; slist && i < scnt; i++) {
	stats_t *s = slist + i;
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
		if (count > MAX_MMV_COUNT) {
		    if (pmDebugOptions.appl0) {
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"metrics count: %d > %d",
					s->name, count, MAX_MMV_COUNT);
		    }
		    continue;
		}
		if (s->version == MMV_VERSION1) {
		    mmv_disk_metric_t *ml = (mmv_disk_metric_t *)
					((char *)s->addr + offset);

		    offset += (count * sizeof(mmv_disk_metric_t));
		    if (s->len < offset) {
			if (pmDebugOptions.appl0) {
			    __pmNotifyErr(LOG_INFO, "MMV: %s - "
					"metrics offset: %"PRIu64" < %"PRIu64,
					s->name, s->len, (int64_t)offset);
			}
			continue;
		    }

		    s->metrics1 = ml;
		    s->mcnt1 = count;

		    for (k = 0; k < count; k++) {
			mmv_disk_metric_t *mp = &ml[k];
			char name[MAXPATHLEN];
			pmID pmid;

			/* build name, check its legitimate and unique */
			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    pmsprintf(name, sizeof(name), "%s.", prefix);
			else
			    pmsprintf(name, sizeof(name), "%s.%s.", prefix, s->name);
			strcat(name, mp->name);
			if (verify_metric_name(name, k, s) != 0)
			    continue;
			if (verify_metric_item(mp->item, name, s) != 0)
			    continue;

			pmid = pmid_build(pmda->e_domain, s->cluster, mp->item);
			create_metric(pmda, s, name, pmid, mp->indom,
					mp->type, mp->semantics, mp->dimension);
		    }
		}
		else if (s->version == MMV_VERSION2) {
		    mmv_disk_metric2_t *ml = (mmv_disk_metric2_t *)
					((char *)s->addr + offset);

		    offset += (count * sizeof(mmv_disk_metric2_t));
		    if (s->len < offset) {
			if (pmDebugOptions.appl0) {
			    __pmNotifyErr(LOG_INFO, "MMV: %s - "
					"metrics offset: %"PRIu64" < %"PRIu64,
					s->name, s->len, (int64_t)offset);
			}
			continue;
		    }

		    s->metrics2 = ml;
		    s->mcnt2 = count;

		    for (k = 0; k < count; k++) {
			mmv_disk_metric2_t *mp = &ml[k];
			mmv_disk_string_t *string;
			char buf[MMV_STRINGMAX];
			char name[MAXPATHLEN];
			__uint64_t mname;
			pmID pmid;

			mname = mp->name;
			if (s->len < mname + sizeof(mmv_disk_string_t)) {
			    if (pmDebugOptions.appl0) {
				__pmNotifyErr(LOG_INFO, "MMV: %s - "
					"metrics2 name: %"PRIu64" < %"PRIu64,
					s->name, s->len, mname);
			    }
			    continue;
			}
			string = (mmv_disk_string_t *)((char *)s->addr + mname);
			memcpy(buf, string->payload, sizeof(buf));
			buf[sizeof(buf)-1] = '\0';

			/* build name, check its legitimate and unique */
			if (hdr->flags & MMV_FLAG_NOPREFIX)
			    pmsprintf(name, sizeof(name), "%s.", prefix);
			else
			    pmsprintf(name, sizeof(name), "%s.%s.", prefix, s->name);
			strcat(name, buf);

			if (verify_metric_name(name, k, s) != 0)
			    continue;
			if (verify_metric_item(mp->item, name, s) != 0)
			    continue;

			pmid = pmid_build(pmda->e_domain, s->cluster, mp->item);
			create_metric(pmda, s, name, pmid, mp->indom,
					mp->type, mp->semantics, mp->dimension);
		    }
		}
		break;

	    case MMV_TOC_INDOMS:
		if (count > MAX_MMV_COUNT) {
		    if (pmDebugOptions.appl0) {
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"indoms count: %d > %d",
					s->name, count, MAX_MMV_COUNT);
		    }
		    continue;
		}
		id = (mmv_disk_indom_t *)((char *)s->addr + offset);

		offset += (count * sizeof(mmv_disk_indom_t));
		if (s->len < offset) {
		    if (pmDebugOptions.appl0) {
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"indoms offset: %"PRIu64" < %"PRIu64,
					s->name, s->len, offset);
		    }
		    continue;
		}

		for (k = 0; k < count; k++) {
		    int sts, serial = id[k].serial;
		    pmInDom pmindom;
		    pmdaIndom *ip;

		    offset = id[k].offset;
		    count = id[k].count;

		    if (count > MAX_MMV_COUNT) {
			if (pmDebugOptions.appl0) {
			    __pmNotifyErr(LOG_ERR, "MMV: %s - "
					"indom[%d] count: %d > %d",
					s->name, k, count, MAX_MMV_COUNT);
			}
			continue;
		    }

		    if (s->version == MMV_VERSION1) {
			offset += (count * sizeof(mmv_disk_instance_t));
			if (s->len < offset) {
			    if (pmDebugOptions.appl0) {
				__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"indom[%d] offset: %"PRIu64" < %"PRIu64,
					s->name, k, s->len, offset);
			    }
			    continue;
			}
			offset -= (count * sizeof(mmv_disk_instance_t));
		    } else {
			offset += (count * sizeof(mmv_disk_instance2_t));
			if (s->len < offset) {
			    if (pmDebugOptions.appl0) {
				__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"indom[%d] offset: %"PRIu64" < %"PRIu64,
					s->name, k, s->len, offset);
			    }
			    continue;
			}
		        offset -= (count * sizeof(mmv_disk_instance2_t));
		    }
		    sts = verify_indom_serial(pmda, serial, s, &pmindom, &ip);
		    if (sts == -EINVAL)
			continue;
		    else if (sts == -EEXIST)
			/* see if we have new instances to add here */
			update_indom(pmda, s, offset, count, &id[k], ip);
		    else
			/* first time we've observed this indom */
			create_indom(pmda, s, offset, count, &id[k], pmindom);
		}
		break;

	    case MMV_TOC_VALUES:
		if (count > MAX_MMV_COUNT) {
		    if (pmDebugOptions.appl0) {
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"values count: %d > %d",
					s->name, count, MAX_MMV_COUNT);
		    }
		    continue;
		}
		offset += (count * sizeof(mmv_disk_value_t));
		if (s->len < offset) {
		    if (pmDebugOptions.appl0) {
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
					"values offset: %"PRIu64" < %"PRIu64,
					s->name, s->len, offset);
		    }
		    continue;
		}
		offset -= (count * sizeof(mmv_disk_value_t));

		s->vcnt = count;
		s->values = (mmv_disk_value_t *)((char *)s->addr + offset);
		break;

	    default:
		if (pmDebugOptions.appl0) {
		    __pmNotifyErr(LOG_DEBUG, "MMV: %s - bad TOC type (%x)",
				    s->name, type);
		}
		break;
	    }
	}
    }

    pmdaTreeRebuildHash(pmns, mtot);	/* for reverse (pmid->name) lookups */
    reload = need_reload;
}

static int
mmv_lookup_item1(int item, unsigned int inst,
	stats_t *s, mmv_disk_value_t **value,
	__uint64_t *shorttext, __uint64_t *helptext)
{
    mmv_disk_metric_t *m1 = s->metrics1;
    mmv_disk_value_t *v = s->values;
    int mi, vi, sts = PM_ERR_PMID;

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
    mmv_disk_metric2_t *m2 = s->metrics2;
    mmv_disk_value_t *v = s->values;
    int mi, vi, sts = PM_ERR_PMID;

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
mmv_lookup_stat_metric(pmID pmid, unsigned int inst,
	stats_t **stats, mmv_disk_value_t **value,
	__uint64_t *shorttext, __uint64_t *helptext)
{
    __pmID_int *id = (__pmID_int *)&pmid;
    int si, sts = PM_ERR_PMID;

    for (si = 0; si < scnt; si++) {
	stats_t *s = &slist[si];

	if (s->cluster != id->cluster)
	    continue;

	sts = (s->version == MMV_VERSION1) ?
	    mmv_lookup_item1(id->item, inst, s, value, shorttext, helptext):
	    mmv_lookup_item2(id->item, inst, s, value, shorttext, helptext);
	if (sts >= 0) {
	    *stats = s;
	    break;
	}
    }
    return sts;
}

static int
mmv_lookup_stat_metric_value(pmID pmid, unsigned int inst,
	stats_t **stats, mmv_disk_value_t **value)
{
    return mmv_lookup_stat_metric(pmid, inst, stats, value, NULL, NULL);
}

/*
 * callback provided to pmdaFetch
 */
static int
mmv_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *id = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (id->cluster == 0) {
	if (id->item <= 2) {
	    atom->l = *(int *)mdesc->m_user;
	    return 1;
	}
	return PM_ERR_PMID;

    } else if (scnt > 0) {	/* We have at least one source of metrics */
	static int setup;
	static float fNaN;
	static double dNaN;
	static pmAtomValue aNaN;
	static char buffer[MMV_STRINGMAX];
	mmv_disk_string_t *str;
	mmv_disk_value_t *v;
	__uint64_t offset;
	stats_t *s;
	int rv, fl;

	rv = mmv_lookup_stat_metric_value(mdesc->m_desc.pmid, inst, &s, &v);
	if (rv < 0)
	    return rv;
	fl = ((mmv_disk_header_t *)s->addr)->flags;

	if (!setup) {
	    setup++;
	    fNaN = (float)0.0 / (float)0.0;
	    dNaN = (double)0.0 / (double)0.0;
	    memset(&aNaN, -1, sizeof(pmAtomValue));
	}

	switch (rv) {
	    case MMV_TYPE_I32:
	    case MMV_TYPE_U32:
	    case MMV_TYPE_I64:
	    case MMV_TYPE_U64:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((fl & MMV_FLAG_SENTINEL) &&
		    (memcmp(atom, &aNaN, sizeof(*atom)) == 0))
		    return 0;
		break;
	    case MMV_TYPE_FLOAT:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((fl & MMV_FLAG_SENTINEL) && atom->f == fNaN)
		    return 0;
		break;
	    case MMV_TYPE_DOUBLE:
		memcpy(atom, &v->value, sizeof(pmAtomValue));
		if ((fl & MMV_FLAG_SENTINEL) && atom->d == dNaN)
		    return 0;
		break;
	    case MMV_TYPE_ELAPSED: {
		atom->ll = v->value.ll;
		if ((fl & MMV_FLAG_SENTINEL) &&
		    (memcmp(atom, &aNaN, sizeof(*atom)) == 0))
		    return 0;
		if (v->extra < 0) {	/* inside a timed section */
		    struct timeval tv; 
		    __pmtimevalNow(&tv); 
		    atom->ll += (tv.tv_sec * 1e6 + tv.tv_usec) + v->extra;
		}
		break;
	    }
	    case MMV_TYPE_STRING: {
		offset = v->extra;
		if (s->len < offset + sizeof(MMV_STRINGMAX)) {
		    if (pmDebugOptions.appl0)
			__pmNotifyErr(LOG_ERR, "MMV: %s - "
				"bad string value offset: %"PRIu64" < %"PRIu64,
				s->name, s->len,
				offset + sizeof(MMV_STRINGMAX));
		    return PM_ERR_GENERIC;
		}
		str = (mmv_disk_string_t *)((char *)s->addr + offset);
		memcpy(buffer, str->payload, sizeof(buffer));
		if ((fl & MMV_FLAG_SENTINEL) && buffer[0] == '\0')
		    return 0;
		buffer[sizeof(buffer)-1] = '\0';
		atom->cp = buffer;
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
	if (pmDebugOptions.appl0)
	    __pmNotifyErr(LOG_DEBUG, "MMV: %s: reloading", pmProgname);
	map_stats(pmda);

	pmda->e_indoms = indoms;
	pmda->e_nindoms = intot;
	pmdaRehash(pmda, metrics, mtot);

	if (pmDebugOptions.appl0)
	    __pmNotifyErr(LOG_DEBUG, 
		      "MMV: %s: %d metrics and %d indoms after reload", 
		      pmProgname, mtot, intot);
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
mmv_lookup_metric_helptext(pmID pmid, int type, char **text)
{
    static char string[MMV_STRINGMAX];
    mmv_disk_string_t *str;
    mmv_disk_value_t *v;
    __uint64_t st, lt;
    size_t offset;
    stats_t *s;

    if (mmv_lookup_stat_metric(pmid, PM_IN_NULL, &s, &v, &st, &lt) < 0)
	return PM_ERR_PMID;

    if ((type & PM_TEXT_ONELINE) && st) {
	offset = st + sizeof(mmv_disk_string_t);
	if (s->len < offset) {
	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_ERR, "MMV: %s - "
				"bad shorttext offset: %"PRIu64" < %"PRIu64,
				s->name, s->len, (uint64_t)offset);
	    return PM_ERR_GENERIC;
	}
	offset -= sizeof(mmv_disk_string_t);

	str = (mmv_disk_string_t *)((char *)s->addr + offset);
	memcpy(string, str->payload, sizeof(string));
	string[sizeof(string)-1] = '\0';
	*text = string;
	return 0;
    }

    if ((type & PM_TEXT_HELP) && lt) {
	offset = lt + sizeof(mmv_disk_string_t);
	if (s->len < offset) {
	    if (pmDebugOptions.appl0)
		__pmNotifyErr(LOG_ERR, "MMV: %s - "
				"bad helptext offset: %"PRIu64" < %"PRIu64,
				s->name, s->len, (uint64_t)offset);
	    return PM_ERR_GENERIC;
	}
	offset -= sizeof(mmv_disk_string_t);

	str = (mmv_disk_string_t *)((char *)s->addr + offset);
	memcpy(string, str->payload, sizeof(string));
	string[sizeof(string)-1] = '\0';
	*text = string;
	return 0;
    }

    return PM_ERR_TEXT;
}

static int
mmv_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    if (type & PM_TEXT_INDOM)
	return PM_ERR_TEXT;

    mmv_reload_maybe(ep);
    if (pmid_cluster(ident) == 0) {
	switch (pmid_item(ident)) {
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
	}
	return PM_ERR_PMID;
    }

    return mmv_lookup_metric_helptext(ident, type, buffer);
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
	    for (m = 0; m < mtot; m++) {
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

    pmsprintf(statsdir, sizeof(statsdir), "%s%c%s", pcptmpdir, sep, prefix);
    pmsprintf(pmnsdir, sizeof(pmnsdir), "%s%c" "pmns", pcpvardir, sep);
    statsdir[sizeof(statsdir)-1] = '\0';
    pmnsdir[sizeof(pmnsdir)-1] = '\0';

    /* Initialize internal dispatch table */
    if (dp->status == 0) {
	/*
	 * number of hard-coded metrics here has to match initializer
	 * cases below, and pmns initialization in map_stats()
	 */
	mtot = 3;
	if ((metrics = malloc(mtot * sizeof(pmdaMetric))) != NULL) {
	    /*
	     * all the hard-coded metrics have the same semantics
	     */
	    for (m = 0; m < mtot; m++) {
		if (m == 0)
		    metrics[m].m_user = &reload;
		else if (m == 1)
		    metrics[m].m_user = &pmDebug;
		else if (m == 2)
		    metrics[m].m_user = &scnt;
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
	pmdaInit(dp, indoms, intot, metrics, mtot);
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
    pmsprintf(logfile, sizeof(logfile), "%s.log", prefix);
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
