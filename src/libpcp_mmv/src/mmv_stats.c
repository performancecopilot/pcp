/*
 * Memory Mapped Values PMDA Client API
 *
 * Copyright (C) 2001,2009 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (C) 2009 Aconex.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */
#include "pmapi.h"
#include <sys/stat.h>
#include "mmv_stats.h"
#include "mmv_dev.h"
#include "impl.h"

static void
mmv_stats_path(const char *fname, char *fullpath, size_t pathlen)
{
    int sep = __pmPathSeparator();

    snprintf(fullpath, pathlen, "%s%c" "mmv" "%c%s",
		pmGetConfig("PCP_TMP_DIR"), sep, sep, fname);
}

static void *
mmv_mapping_init(const char *fname, size_t size)
{
    char path[MAXPATHLEN];
    void *addr = NULL;
    mode_t cur_umask;
    int fd, sts = 0;

    /* unlink+creat will cause the pmda to reload on next fetch */
    mmv_stats_path(fname, path, sizeof(path));
    unlink(path);
    cur_umask = umask(S_IWGRP | S_IWOTH);
    fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    umask(cur_umask);
    if (fd < 0)
	return NULL;

    if (ftruncate(fd, size) != -1)
	addr = __pmMemoryMap(fd, size, 1);
    else
	sts = oserror();

    close(fd);
    setoserror(sts);
    return addr;
}

static int
mmv_singular(__int32_t indom)
{
    return (indom == 0 || indom == PM_INDOM_NULL);
}

static mmv_disk_indom_t *
mmv_lookup_disk_indom(__int32_t indom, mmv_disk_indom_t *in, int nindoms)
{
    int i;

    for (i = 0; i < nindoms; i++)
	if (in[i].serial == indom)
	    return &in[i];
    return NULL;
}

static const mmv_indom_t *
mmv_lookup_indom(__int32_t indom, const mmv_indom_t *in, int nindoms)
{
    int i;

    for (i = 0; i < nindoms; i++)
	if (in[i].serial == indom)
	    return &in[i];
    return NULL;
}

static __uint64_t
mmv_generation(void)
{
    struct timeval now;
    __uint32_t gen1, gen2;

    __pmtimevalNow(&now);
    gen1 = now.tv_sec;
    gen2 = now.tv_usec;
    return (((__uint64_t)gen1 << 32) | (__uint64_t)gen2);
}

void * 
mmv_stats_init(const char *fname,
		int cluster, mmv_stats_flags_t fl,
		const mmv_metric_t *st, int nmetrics,
		const mmv_indom_t *in, int nindoms)
{
    mmv_disk_instance_t *inlist;
    mmv_disk_indom_t *domlist;
    mmv_disk_metric_t *mlist;
    mmv_disk_string_t *slist;
    mmv_disk_value_t *vlist;
    mmv_disk_header_t *hdr;
    mmv_disk_toc_t *toc;
    __uint64_t indoms_offset;		/* anchor start of indoms section */
    __uint64_t instances_offset;	/* anchor start of instances section */
    __uint64_t metrics_offset;		/* anchor start of metrics section */
    __uint64_t values_offset;		/* anchor start of values section */
    __uint64_t strings_offset;		/* anchor start of any/all strings */
    void *addr;
    size_t size;
    int i, j, k, tocidx, stridx;
    int ninstances = 0;
    int nstrings = 0;
    int nvalues = 0;

    for (i = 0; i < nindoms; i++) {
	if (mmv_singular(in[i].serial)) {
	    setoserror(ESRCH);
	    return NULL;
	}
	ninstances += in[i].count;
	if (in[i].shorttext)
	    nstrings++;
	if (in[i].helptext)
	    nstrings++;
    }

    for (i = 0; i < nmetrics; i++) {
	if ((st[i].type < MMV_TYPE_NOSUPPORT) || 
	    (st[i].type > MMV_TYPE_ELAPSED) || strlen(st[i].name) == 0) {
	    setoserror(EINVAL);
	    return NULL;
	}

	if (st[i].helptext)
	    nstrings++;
	if (st[i].shorttext)
	    nstrings++;

	if (!mmv_singular(st[i].indom)) {
	    const mmv_indom_t * mi;

	    if ((mi = mmv_lookup_indom(st[i].indom, in, nindoms)) == NULL) {
		setoserror(ESRCH);
		return NULL;
	    }
	    if (st[i].type == MMV_TYPE_STRING)
		nstrings += mi->count;
	    nvalues += mi->count;
	} else {
	    if (st[i].type == MMV_TYPE_STRING)
		nstrings++;
	    nvalues++;
	}
    }

    /* TOC follows header, with enough entries to hold */
    /* indoms, instances, metrics, values, and strings */
    size = sizeof(mmv_disk_toc_t) * 2;
    if (nindoms)
	size += sizeof(mmv_disk_toc_t) * 2;
    if (nstrings)
	size += sizeof(mmv_disk_toc_t) * 1;
    indoms_offset = sizeof(mmv_disk_header_t) + size;

    /* Following the indom definitions are the actual instances */
    size = sizeof(mmv_disk_indom_t) * nindoms;
    instances_offset = indoms_offset + size;

    /* Following the instances are the metric definitions */
    for (size = 0, i = 0; i < nindoms; i++)
	size += in[i].count * sizeof(mmv_disk_instance_t);
    metrics_offset = instances_offset + size;

    /* Following metric definitions are the actual values */
    size = nmetrics * sizeof(mmv_disk_metric_t);
    values_offset = metrics_offset + size;

    /* Following the values are the string values and/or help text */
    size = nvalues * sizeof(mmv_disk_value_t);
    strings_offset = values_offset + size;

    /* End of file follows all of the actual strings */
    size = strings_offset + nstrings * sizeof(mmv_disk_string_t);

    if ((addr = mmv_mapping_init(fname, size)) == NULL)
	return NULL;

    /*
     * We unconditionally clobber the stats file on each restart;
     * easier this way and the clients deal with counter wraps.
     * We also write from the start going forward through the file
     * with an occassional step back to (re)write the TOC page -
     * this gives the kernel a decent shot at laying out the file
     * contiguously ondisk (hopefully we dont do much disk I/O on
     * this file, but it will be written to disk at times so lets
     * try to minimise that I/O traffic, eh?).
     */

    hdr = (mmv_disk_header_t *) addr;
    strncpy(hdr->magic, "MMV", 4);
    hdr->version = MMV_VERSION;
    hdr->g1 = mmv_generation();
    hdr->g2 = 0;
    hdr->tocs = 2;
    if (nindoms)
	hdr->tocs += 2;
    if (nstrings)
	hdr->tocs += 1;
    hdr->flags = fl;
    hdr->cluster = cluster;
    hdr->process = (__int32_t)getpid();

    toc = (mmv_disk_toc_t *)((char *)addr + sizeof(mmv_disk_header_t));
    tocidx = 0;

    if (nindoms) {
	toc[tocidx].type = MMV_TOC_INDOMS;
	toc[tocidx].count = nindoms;
	toc[tocidx].offset = indoms_offset;
	tocidx++;
	toc[tocidx].type = MMV_TOC_INSTANCES;
	toc[tocidx].count = ninstances;
	toc[tocidx].offset = instances_offset;
	tocidx++;
    }
    toc[tocidx].type = MMV_TOC_METRICS;
    toc[tocidx].count = nmetrics;
    toc[tocidx].offset = metrics_offset;
    tocidx++;
    toc[tocidx].type = MMV_TOC_VALUES;
    toc[tocidx].count = nvalues;
    toc[tocidx].offset = values_offset;
    tocidx++;
    if (nstrings) {
	toc[tocidx].type = MMV_TOC_STRINGS;
	toc[tocidx].count = nstrings;
	toc[tocidx].offset = strings_offset;
	tocidx++;
    }

    /* Indom section */
    domlist = (mmv_disk_indom_t *)((char *)addr + indoms_offset);
    for (i = 0; i < nindoms; i++) {
	domlist[i].serial = in[i].serial;
	domlist[i].count = in[i].count;
	domlist[i].offset = 0;		/* filled in below */
	domlist[i].shorttext = 0;	/* filled in later */
	domlist[i].helptext = 0;	/* filled in later */
    }

    /* Instances section */
    inlist = (mmv_disk_instance_t *)((char *)addr + instances_offset);
    for (i = 0; i < nindoms; i++) {
	mmv_instances_t *insts = in[i].instances;
	domlist[i].offset = ((char *)inlist - (char *)addr);
	for (j = 0; j < domlist[i].count; j++) {
	    inlist->indom = indoms_offset + (i * sizeof(mmv_disk_indom_t));
	    inlist->padding = 0;
	    inlist->internal = insts[j].internal;
	    strncpy(inlist->external, insts[j].external, MMV_NAMEMAX);
	    inlist->external[MMV_NAMEMAX-1] = '\0';
	    inlist++;
	}
    }

    /* Metrics section */
    mlist = (mmv_disk_metric_t *)((char *)addr + metrics_offset);
    for (i = 0; i < nmetrics; i++) {
	strncpy(mlist[i].name, st[i].name, MMV_NAMEMAX);
	mlist[i].name[MMV_NAMEMAX-1] = '\0';
	mlist[i].item = st[i].item;
	mlist[i].type = st[i].type;
	mlist[i].indom = st[i].indom;
	mlist[i].dimension = st[i].dimension;
	mlist[i].semantics = st[i].semantics;
	mlist[i].shorttext = 0;		/* filled in later */
	mlist[i].helptext = 0;		/* filled in later */
	mlist[i].padding = 0;
    }

    /* Values section */
    vlist = (mmv_disk_value_t *)((char *)addr + values_offset);
    for (i = j = 0; i < nmetrics; i++) {
	__uint64_t off = metrics_offset + i * sizeof(mmv_disk_metric_t);

	if (mmv_singular(st[i].indom)) {
	    memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
	    vlist[j].metric = off;
	    j++;
	} else {
	    __uint64_t ioff;
	    mmv_disk_indom_t *indom;

	    indom = mmv_lookup_disk_indom(st[i].indom, domlist, nindoms);
	    for (k = 0; k < indom->count; k++) {
		ioff = indom->offset + sizeof(mmv_disk_instance_t) * k;
		memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
		vlist[j].metric = off;
		vlist[j].instance = ioff;
		j++;
	    }
	}
    }

    /* Strings section */
    slist = (mmv_disk_string_t *)((char *)addr + strings_offset);
    stridx = 0;

    /*
     * 3 phases: all string values, any metric help, any indom help.
     */
    for (i = 0; i < nvalues; i++) {
	mmv_disk_metric_t * metric = (mmv_disk_metric_t *)
			((char *)(addr + vlist[i].metric));
	if (metric->type == MMV_TYPE_STRING) {
	    vlist[i].extra = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    stridx++;
	}
    }
    for (i = 0; i < nmetrics; i++) {
	if (st[i].shorttext) {
	    mlist[i].shorttext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, st[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (st[i].helptext) {
	    mlist[i].helptext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, st[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }
    for (i = 0; i < nindoms; i++) {
	if (in[i].shorttext) {
	    domlist[i].shorttext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (in[i].helptext) {
	    domlist[i].helptext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }

    /* Complete - unlock the header, PMDA can read now */
    hdr->g2 = hdr->g1;

    return addr;
}

void
mmv_stats_stop(const char *fname, void *addr)
{
    mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
    char path[MAXPATHLEN];
    struct stat sbuf;

    mmv_stats_path(fname, path, sizeof(path));
    if (stat(path, &sbuf) < 0)
	sbuf.st_size = (size_t)-1;
    else if (hdr->flags & MMV_FLAG_PROCESS)
	unlink(path);
    __pmMemoryUnmap(addr, sbuf.st_size);
}

pmAtomValue *
mmv_lookup_value_desc(void *addr, const char *metric, const char *inst)
{
    if (addr != NULL && metric != NULL) {
	int i;
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
	mmv_disk_toc_t *toc = (mmv_disk_toc_t *)
			((char *)addr + sizeof(mmv_disk_header_t));

	for (i = 0; i < hdr->tocs; i++) {
	    if (toc[i].type == MMV_TOC_VALUES) {
		int j;
		mmv_disk_value_t *v = (mmv_disk_value_t *)
				((char *)addr + toc[i].offset);

		for (j = 0; j < toc[i].count; j++) {
		    mmv_disk_metric_t *m = (mmv_disk_metric_t *)
					((char *)addr + v[j].metric);
		    if (strcmp(m->name, metric) == 0) {
			if (mmv_singular(m->indom)) {  /* Singular metric */
			    return &v[j].value;
 			} else {
			    if (inst == NULL) {
				/* Metric has multiple instances, but
				 * we don't know which one to return,
				 * so return an error
				 */
				return NULL;
			    } else {
				mmv_disk_instance_t * in = 
				    (mmv_disk_instance_t *)
					((char *)addr + v[j].instance);
				if (strcmp(in->external, inst) == 0)
				    return &v[j].value;
			    }
			}
		    }
		}
	    }
	}
    }

    return NULL;
}

void
mmv_inc_value(void *addr, pmAtomValue *av, double inc)
{
    if (av != NULL && addr != NULL) {
	mmv_disk_value_t * v = (mmv_disk_value_t *) av;
	mmv_disk_metric_t * m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
	switch (m->type) {
	case MMV_TYPE_I32:
	    v->value.l += (__int32_t)inc;
	    break;
	case MMV_TYPE_U32:
	    v->value.ul += (__uint32_t)inc;
	    break;
	case MMV_TYPE_I64:
	    v->value.ll += (__int64_t)inc;
	    break;
	case MMV_TYPE_U64:
	    v->value.ull += (__uint64_t)inc;
	    break;
	case MMV_TYPE_FLOAT:
	    v->value.f += (float)inc;
	    break;
	case MMV_TYPE_DOUBLE:
	    v->value.d += inc;
	    break;
	case MMV_TYPE_ELAPSED:
	    if (inc < 0)
		v->extra = (__int64_t)inc;
	    else {
		v->value.ll += v->extra + (__int64_t)inc;
		v->extra = 0;
	    }
	    break;
	default:
	    break;
	}
    }
}

void
mmv_set_value(void *addr, pmAtomValue *av, double val)
{
    if (av != NULL && addr != NULL) {
	mmv_disk_value_t * v = (mmv_disk_value_t *) av;
	mmv_disk_metric_t * m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
	switch (m->type) {
	case MMV_TYPE_I32:
	    v->value.l = (__int32_t)val;
	    break;
	case MMV_TYPE_U32:
	    v->value.ul = (__uint32_t)val;
	    break;
	case MMV_TYPE_I64:
	    v->value.ll = (__int64_t)val;
	    break;
	case MMV_TYPE_U64:
	    v->value.ull = (__uint64_t)val;
	    break;
	case MMV_TYPE_FLOAT:
	    v->value.f = (float)val;
	    break;
	case MMV_TYPE_DOUBLE:
	    v->value.d = val;
	    break;
	case MMV_TYPE_ELAPSED:
	    v->value.ll = (__int64_t)val;
	    v->extra = 0;
	    break;
	default:
	    break;
	}
    }
}

void
mmv_set_string(void *addr, pmAtomValue *av, const char *string, int size)
{
    if (av != NULL && addr != NULL && string != NULL) {
	mmv_disk_value_t * v = (mmv_disk_value_t *) av;
	mmv_disk_metric_t * m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
 
	if (m->type == MMV_TYPE_STRING &&
	    (size >= 0 && size < MMV_STRINGMAX - 1)) {
	    __uint64_t soffset = v->extra;
	    mmv_disk_string_t * s;

	    s = (mmv_disk_string_t *)((char *)addr + soffset);
	    strncpy(s->payload, string, size);
	    s->payload[size] = '\0';
	    v->value.l = size;
	}
    }
}

/*
 * Simple wrapper routines
 */

void
mmv_stats_add(void *addr,
	const char *metric, const char *instance, double count)
{
    if (addr) {
	pmAtomValue * mmv_metric;
	mmv_metric = mmv_lookup_value_desc(addr, metric, instance);
	if (mmv_metric)
	    mmv_inc_value(addr, mmv_metric, count);
    }
}

void
mmv_stats_inc(void *addr, const char *metric, const char *instance)
{
    mmv_stats_add(addr, metric, instance, 1);
}

void
mmv_stats_set(void *addr,
	const char *metric, const char *instance, double value)
{
    if (addr) {
	pmAtomValue * mmv_metric;
	mmv_metric = mmv_lookup_value_desc(addr, metric, instance);
	if (mmv_metric)
	    mmv_set_value(addr, mmv_metric, value);
    }
}

void
mmv_stats_add_fallback(void *addr, const char *metric,
	const char *instance, const char *instance2, double count)
{
    if (addr) {
	pmAtomValue * mmv_metric;
	mmv_metric = mmv_lookup_value_desc(addr, metric, instance);
	if (mmv_metric == NULL)
	    mmv_metric = mmv_lookup_value_desc(addr,metric,instance2);
	if (mmv_metric)
	    mmv_inc_value(addr, mmv_metric, count);
    }
}

void
mmv_stats_inc_fallback(void *addr, const char *metric,
	const char *instance, const char *instance2)
{
    mmv_stats_add_fallback(addr, metric, instance, instance2, 1);
}

pmAtomValue *
mmv_stats_interval_start(void *addr, pmAtomValue *value,
	const char *metric, const char *instance)
{
    if (addr) {
	if (value == NULL)
	    value = mmv_lookup_value_desc(addr, metric, instance);
	if (value) {
	    struct timeval tv;
	    __pmtimevalNow(&tv);
	    mmv_inc_value(addr, value, -(tv.tv_sec*1e6 + tv.tv_usec));
	}
    }
    return value;
}

void
mmv_stats_interval_end(void *addr, pmAtomValue *value)
{
    if (value && addr) {
	struct timeval tv;
	__pmtimevalNow(&tv);
	mmv_inc_value(addr, value, (tv.tv_sec*1e6 + tv.tv_usec));
    }
}

void
mmv_stats_set_string(void *addr, const char *metric,
	const char *instance, const char *string)
{
    if (addr) {
	size_t len = strlen(string);
	pmAtomValue *mmv_metric;
	mmv_metric = mmv_lookup_value_desc(addr, metric, instance);
	mmv_set_string(addr, mmv_metric, string, len);
    }
}

void
mmv_stats_set_strlen(void *addr, const char *metric,
	const char *instance, const char *string, size_t len)
{
    if (addr) {
	pmAtomValue *mmv_metric;
	mmv_metric = mmv_lookup_value_desc(addr, metric, instance);
	mmv_set_string(addr, mmv_metric, string, len);
    }
}
