/*
 * Memory Mapped Values PMDA Client API
 *
 * Copyright (C) 2001,2009 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (C) 2009 Aconex.  All rights reserved.
 * Copyright (C) 2013,2016,2018 Red Hat.
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
#include <ctype.h>
#include "pmapi.h"
#include "mmv_stats.h"
#include "mmv_dev.h"
#include "libpcp.h"

struct mmv_registry {
    mmv_indom2_t *	indoms;
    __uint32_t		nindoms;
    mmv_metric2_t *	metrics;
    __uint32_t		nmetrics;
    mmv_instances2_t *	instances;
    __uint32_t		ninstances;
    mmv_label_t *	labels;
    __uint32_t		nlabels;
    __uint32_t		version;
    const char *	file;
    __uint32_t		cluster;
    mmv_stats_flags_t	flags;
    void *		addr;
};

static void
mmv_stats_path(const char *fname, char *fullpath, size_t pathlen)
{
    int sep = pmPathSeparator();

    if (fname[0] != sep)
	pmsprintf(fullpath, pathlen, "%s%c" "mmv" "%c%s",
		pmGetConfig("PCP_TMP_DIR"), sep, sep, fname);
    else /* full path given - use it directly */
	pmsprintf(fullpath, pathlen, "%s", fname);
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

static const mmv_indom2_t *
mmv_lookup_indom2(__int32_t indom, const mmv_indom2_t *in2, int nindoms)
{
    int i;

    for (i = 0; i < nindoms; i++)
	if (in2[i].serial == indom)
	    return &in2[i];
    return NULL;
}

static __uint64_t
mmv_generation(void)
{
    struct timeval now;
    __uint32_t gen1, gen2;

    pmtimevalNow(&now);
    gen1 = now.tv_sec;
    gen2 = now.tv_usec;
    return (((__uint64_t)gen1 << 32) | (__uint64_t)gen2);
}

static void * 
mmv_init(const char *fname, int version,
		int cluster, mmv_stats_flags_t fl,
		const mmv_metric_t *st1, int nmetric1,
		const mmv_indom_t *in1, int nindom1,
		const mmv_metric2_t *st2, int nmetric2,
		const mmv_indom2_t *in2, int nindom2,
		const mmv_label_t *lb, int nlabels)
{
    mmv_disk_instance2_t *inlist2;
    mmv_disk_instance_t *inlist1;
    mmv_disk_metric2_t *mlist2;
    mmv_disk_metric_t *mlist1;
    mmv_disk_string_t *slist;
    mmv_disk_indom_t *domlist;
    mmv_disk_value_t *vlist;
    mmv_disk_label_t *lblist;
    mmv_disk_header_t *hdr;
    mmv_disk_toc_t *toc;
    const mmv_indom_t *mi1;
    const mmv_indom2_t *mi2;
    __uint64_t indoms_offset;		/* anchor start of indoms section */
    __uint64_t instances_offset;	/* anchor start of instances section */
    __uint64_t metrics_offset;		/* anchor start of metrics section */
    __uint64_t values_offset;		/* anchor start of values section */
    __uint64_t strings_offset;		/* anchor start of any/all strings */
    __uint64_t labels_offset;		/* anchor start of any/all labels */
    void *addr;
    size_t size;
    __uint64_t offset;
    int i, j, k, tocidx, stridx;
    int ninstances = 0;
    int nstrings = 0;
    int nvalues = 0;

    for (i = 0; i < nindom1; i++) {
	ninstances += in1[i].count;
	if (in1[i].shorttext)
	    nstrings++;
	if (in1[i].helptext)
	    nstrings++;
    }
    for (i = 0; i < nindom2; i++) {
	ninstances += in2[i].count;
	if (version == MMV_VERSION2 || version == MMV_VERSION3)
	    nstrings += in2[i].count;	/* instance names */
	if (in2[i].shorttext)
	    nstrings++;
	if (in2[i].helptext)
	    nstrings++;
    }

    for (i = 0; i < nmetric1; i++) {
	if (st1[i].helptext)
	    nstrings++;
	if (st1[i].shorttext)
	    nstrings++;

	if (!mmv_singular(st1[i].indom)) {
	    mi1 = mmv_lookup_indom(st1[i].indom, in1, nindom1);
	    if (st1[i].type == MMV_TYPE_STRING)
		nstrings += mi1->count;
	    nvalues += mi1->count;
	} else {
	    if (st1[i].type == MMV_TYPE_STRING)
		nstrings++;
	    nvalues++;
	}
    }
    for (i = 0; i < nmetric2; i++) {
	if (version == MMV_VERSION2 || version == MMV_VERSION3)
	    nstrings++;		/* metric name */
	if (st2[i].helptext)
	    nstrings++;
	if (st2[i].shorttext)
	    nstrings++;

	if (!mmv_singular(st2[i].indom)) {
	    mi2 = mmv_lookup_indom2(st2[i].indom, in2, nindom2);
	    if (st2[i].type == MMV_TYPE_STRING)
		nstrings += mi2->count;
	    nvalues += mi2->count;
	} else {
	    if (st2[i].type == MMV_TYPE_STRING)
		nstrings++;
	    nvalues++;
	}
    }
    
    /* TOC follows header, with enough entries to hold */
    /* indoms, instances, metrics, values, strings, and labels */
    size = sizeof(mmv_disk_toc_t) * 2;
    if (nindom1 || nindom2)
	size += sizeof(mmv_disk_toc_t) * 2;
    if (nstrings)
	size += sizeof(mmv_disk_toc_t) * 1;
    if (nlabels) {
	size += sizeof(mmv_disk_toc_t) * 1;
    }
    indoms_offset = sizeof(mmv_disk_header_t) + size;

    /* Following the indom definitions are the actual instances */
    size = 0;
    if (nindom1)
	size = sizeof(mmv_disk_indom_t) * nindom1;
    else if (nindom2)
	size = sizeof(mmv_disk_indom_t) * nindom2;
    instances_offset = indoms_offset + size;

    /* Following the instances are the metric definitions */
    size = 0;
    if (nindom1) {
	for (i = 0; i < nindom1; i++)
	    size += in1[i].count * sizeof(mmv_disk_instance_t);
    } else if (nindom2) {
	for (i = 0; i < nindom2; i++)
	    if (version == MMV_VERSION1)
		size += in2[i].count * sizeof(mmv_disk_instance_t);
	    else
		size += in2[i].count * sizeof(mmv_disk_instance2_t);
    }
    metrics_offset = instances_offset + size;

    /* Following metric definitions are the actual values */
    size = 0;
    if (nmetric1)
	size += nmetric1 * sizeof(mmv_disk_metric_t);
    else if (nmetric2) {
	if (version == MMV_VERSION1)
	    size += nmetric2 * sizeof(mmv_disk_metric_t);
	else
	    size += nmetric2 * sizeof(mmv_disk_metric2_t);
    }
    values_offset = metrics_offset + size;

    /* Following the values are the string values and/or help text */
    size = nvalues * sizeof(mmv_disk_value_t);
    strings_offset = values_offset + size;

    /* Following the strings are the labels */
    size = nstrings * sizeof(mmv_disk_string_t);
    labels_offset = strings_offset + size;

    /* End of file follows all of the actual strings */
    size = labels_offset + nlabels * sizeof(mmv_disk_label_t);

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
    hdr->version = version;
    hdr->g1 = mmv_generation();
    hdr->g2 = 0;
    hdr->tocs = 2;
    if (nindom1 || nindom2)
	hdr->tocs += 2;
    if (nstrings)
	hdr->tocs += 1;
    if (nlabels)
	hdr->tocs += 1;    
    hdr->flags = fl;
    hdr->cluster = cluster;
    hdr->process = (__int32_t)getpid();

    toc = (mmv_disk_toc_t *)((char *)addr + sizeof(mmv_disk_header_t));
    tocidx = 0;

    if (nindom1 || nindom2) {
	toc[tocidx].type = MMV_TOC_INDOMS;
	toc[tocidx].count = nindom1 ? nindom1 : nindom2;
	toc[tocidx].offset = indoms_offset;
	tocidx++;
	toc[tocidx].type = MMV_TOC_INSTANCES;
	toc[tocidx].count = ninstances;
	toc[tocidx].offset = instances_offset;
	tocidx++;
    }
    toc[tocidx].type = MMV_TOC_METRICS;
    toc[tocidx].count = nmetric1 ? nmetric1 : nmetric2;
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
    if (nlabels) {
	toc[tocidx].type = MMV_TOC_LABELS;
	toc[tocidx].count = nlabels;
	toc[tocidx].offset = labels_offset;
	tocidx++;
    }

    /* Indom section */
    domlist = (mmv_disk_indom_t *)((char *)addr + indoms_offset);
    for (i = 0; i < nindom1; i++) {
	domlist[i].serial = in1[i].serial;
	domlist[i].count = in1[i].count;
	domlist[i].offset = 0;		/* filled in later */
	domlist[i].shorttext = 0;	/* filled in later */
	domlist[i].helptext = 0;	/* filled in later */
    }
    for (i = 0; i < nindom2; i++) {
	domlist[i].serial = in2[i].serial;
	domlist[i].count = in2[i].count;
	domlist[i].offset = 0;		/* filled in later */
	domlist[i].shorttext = 0;	/* filled in later */
	domlist[i].helptext = 0;	/* filled in later */
    }

    /* Instances section */
    inlist1 = (mmv_disk_instance_t *)((char *)addr + instances_offset);
    inlist2 = (mmv_disk_instance2_t *)((char *)addr + instances_offset);
    for (i = 0; i < nindom1; i++) {
	mmv_instances_t *insts = in1[i].instances;
	domlist[i].offset = ((char *)inlist1 - (char *)addr);
	for (j = 0; j < domlist[i].count; j++) {
	    inlist1->indom = indoms_offset + (i * sizeof(mmv_disk_indom_t));
	    inlist1->padding = 0;
	    inlist1->internal = insts[j].internal;
	    strncpy(inlist1->external, insts[j].external, MMV_NAMEMAX);
	    inlist1->external[MMV_NAMEMAX-1] = '\0';
	    inlist1++;
	}
    }
    for (i = 0; i < nindom2; i++) {
	mmv_instances2_t *insts = in2[i].instances;
	if (version == MMV_VERSION1) {
	    domlist[i].offset = ((char *)inlist1 - (char *)addr);
	    for (j = 0; j < domlist[i].count; j++) {
		inlist1->indom = indoms_offset + (i * sizeof(mmv_disk_indom_t));
		inlist1->padding = 0;
		inlist1->internal = insts[j].internal;
		strncpy(inlist1->external, insts[j].external, MMV_NAMEMAX);
		inlist1->external[MMV_NAMEMAX-1] = '\0';
		inlist1++;
	    }
	} else {
	    domlist[i].offset = ((char *)inlist2 - (char *)addr);
	    for (j = 0; j < domlist[i].count; j++) {
		inlist2->indom = indoms_offset + (i * sizeof(mmv_disk_indom_t));
		inlist2->padding = 0;
		inlist2->internal = insts[j].internal;
		inlist2->external = 0;	/* filled in later */
		inlist2++;
	    }
	}
    }

    /* Metrics section */
    mlist1 = (mmv_disk_metric_t *)((char *)addr + metrics_offset);
    mlist2 = (mmv_disk_metric2_t *)((char *)addr + metrics_offset);
    for (i = 0; i < nmetric1; i++) {
	strncpy(mlist1[i].name, st1[i].name, MMV_NAMEMAX);
	mlist1[i].name[MMV_NAMEMAX-1] = '\0';
	mlist1[i].item = st1[i].item;
	mlist1[i].type = st1[i].type;
	mlist1[i].indom = st1[i].indom;
	mlist1[i].dimension = st1[i].dimension;
	mlist1[i].semantics = st1[i].semantics;
	mlist1[i].shorttext = 0;	/* filled in later */
	mlist1[i].helptext = 0;		/* filled in later */
	mlist1[i].padding = 0;
    }
    for (i = 0; i < nmetric2; i++) {
	if (version == MMV_VERSION1) {
	    strncpy(mlist1[i].name, st2[i].name, MMV_NAMEMAX);
	    mlist1[i].name[MMV_NAMEMAX-1] = '\0';
	    mlist1[i].item = st2[i].item;
	    mlist1[i].type = st2[i].type;
	    mlist1[i].indom = st2[i].indom;
	    mlist1[i].dimension = st2[i].dimension;
	    mlist1[i].semantics = st2[i].semantics;
	    mlist1[i].shorttext = 0;	/* filled in later */
	    mlist1[i].helptext = 0;		/* filled in later */
	    mlist1[i].padding = 0;
	} else {
	    mlist2[i].name = 0;		/* filled in later */
	    mlist2[i].item = st2[i].item;
	    mlist2[i].type = st2[i].type;
	    mlist2[i].indom = st2[i].indom;
	    mlist2[i].dimension = st2[i].dimension;
	    mlist2[i].semantics = st2[i].semantics;
	    mlist2[i].shorttext = 0;	/* filled in later */
	    mlist2[i].helptext = 0;	/* filled in later */
	    mlist2[i].padding = 0;
	}
    }

    /* Values section */
    vlist = (mmv_disk_value_t *)((char *)addr + values_offset);
    for (i = j = 0; i < nmetric1; i++) {
	offset = metrics_offset + i * sizeof(mmv_disk_metric_t);

	if (mmv_singular(st1[i].indom)) {
	    memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
	    vlist[j].metric = offset;
	    j++;
	} else {
	    __uint64_t ioff;
	    mmv_disk_indom_t *indom;

	    indom = mmv_lookup_disk_indom(st1[i].indom, domlist, nindom1);
	    for (k = 0; k < indom->count; k++) {
		ioff = indom->offset + sizeof(mmv_disk_instance_t) * k;
		memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
		vlist[j].metric = offset;
		vlist[j].instance = ioff;
		j++;
	    }
	}
    }
    for (i = j = 0; i < nmetric2; i++) {
	if (version == MMV_VERSION1)
	    offset = metrics_offset + i * sizeof(mmv_disk_metric_t);
	else
	    offset = metrics_offset + i * sizeof(mmv_disk_metric2_t);

	if (mmv_singular(st2[i].indom)) {
	    memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
	    vlist[j].metric = offset;
	    j++;
	} else {
	    __uint64_t ioff;
	    mmv_disk_indom_t *indom;

	    indom = mmv_lookup_disk_indom(st2[i].indom, domlist, nindom2);
	    for (k = 0; k < indom->count; k++) {
		if (version == MMV_VERSION1)
		    ioff = indom->offset + sizeof(mmv_disk_instance_t) * k;
		else
		    ioff = indom->offset + sizeof(mmv_disk_instance2_t) * k;
		memset(&vlist[j], 0, sizeof(mmv_disk_value_t));
		vlist[j].metric = offset;
		vlist[j].instance = ioff;
		j++;
	    }
	}
    }

    /* Strings section */
    slist = (mmv_disk_string_t *)((char *)addr + strings_offset);
    stridx = 0;

    /*
     * 6 phases: v2 instance names, v2 metric names, all string values,
     *	   any metric help, any indom help, v3 metric labels.
     */
    if (version == MMV_VERSION2 || version == MMV_VERSION3) {
	inlist2 = (mmv_disk_instance2_t *)((char *)addr + instances_offset);
	for (i = 0; i < nindom2; i++) {
	    mmv_instances2_t *insts = in2[i].instances;

	    for (k = 0; k < in2[i].count; k++) {
		inlist2->external = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
		inlist2++;
		strncpy(slist[stridx].payload, insts[k].external, MMV_STRINGMAX);
		slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
		stridx++;
	    }
	}
	mlist2 = (mmv_disk_metric2_t *)((char *)addr + metrics_offset);
	for (i = 0; i < nmetric2; i++) {
	    mlist2->name = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    mlist2++;
	    strncpy(slist[stridx].payload, st2[i].name, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }

    for (i = 0; i < nvalues; i++) {
	mmv_metric_type_t type = MMV_TYPE_NOSUPPORT;

	if (version == MMV_VERSION1) {
	    mmv_disk_metric_t *m1 = (mmv_disk_metric_t *)
			((char *)(addr + vlist[i].metric));
	    type = m1->type;
	} else if (version == MMV_VERSION2 || version == MMV_VERSION3) {
	    mmv_disk_metric2_t *m2 = (mmv_disk_metric2_t *)
			((char *)(addr + vlist[i].metric));
	    type = m2->type;
	}
	if (type == MMV_TYPE_STRING) {
	    vlist[i].extra = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    stridx++;
	}
    }
    for (i = 0; i < nmetric1; i++) {
	if (st1[i].shorttext) {
	    mlist1[i].shorttext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, st1[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (st1[i].helptext) {
	    mlist1[i].helptext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, st1[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }
    for (i = 0; i < nmetric2; i++) {
	mlist2 = (mmv_disk_metric2_t *)((char *)addr + metrics_offset);
	if (st2[i].shorttext) {
	    offset = strings_offset + (stridx * sizeof(mmv_disk_string_t));
	    if (version == MMV_VERSION1)
		mlist1[i].shorttext = offset;
	    else
		mlist2[i].shorttext = offset;
	    strncpy(slist[stridx].payload, st2[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (st2[i].helptext) {
	    offset = strings_offset + (stridx * sizeof(mmv_disk_string_t));
	    if (version == MMV_VERSION1)
		mlist1[i].helptext = offset;
	    else
		mlist2[i].helptext = offset;
	    strncpy(slist[stridx].payload, st2[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }
    for (i = 0; i < nindom1; i++) {
	if (in1[i].shorttext) {
	    domlist[i].shorttext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in1[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (in1[i].helptext) {
	    domlist[i].helptext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in1[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }
    for (i = 0; i < nindom2; i++) {
	if (in2[i].shorttext) {
	    domlist[i].shorttext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in2[i].shorttext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
	if (in2[i].helptext) {
	    domlist[i].helptext = strings_offset +
				(stridx * sizeof(mmv_disk_string_t));
	    strncpy(slist[stridx].payload, in2[i].helptext, MMV_STRINGMAX);
	    slist[stridx].payload[MMV_STRINGMAX-1] = '\0';
	    stridx++;
	}
    }

    /* Labels section */
    lblist = (mmv_disk_label_t *)((char *)addr + labels_offset);
    for (i = 0; i < nlabels; i++) {
	lblist[i].flags = lb[i].flags;
	lblist[i].identity = lb[i].identity; 
	lblist[i].internal = lb[i].internal;
	memcpy(lblist[i].payload, lb[i].payload, MMV_LABELMAX);
    }

    /* Complete - unlock the header, PMDA can read now */
    hdr->g2 = hdr->g1;

    return addr;
}

static int
mmv_check(const mmv_metric_t *st, int nmetrics,
	  const mmv_indom_t *in, int nindoms)
{
    const mmv_metric_t *metric;
    const mmv_indom_t *indom;
    size_t size;
    int i;

    for (i = 0; i < nindoms; i++) {
	indom = &in[i];
	if (mmv_singular(indom->serial)) {
	    setoserror(ESRCH);
	    return -1;
	}
	if (indom->shorttext) {
	    size = strlen(indom->shorttext);
	    if (size >= MMV_STRINGMAX) {
		setoserror(E2BIG);
		return -1;
	    }
	}
	if (indom->helptext) {
	    size = strlen(indom->helptext);
	    if (size >= MMV_STRINGMAX) {
		setoserror(E2BIG);
		return -1;
	    }
	}
    }

    for (i = 0; i < nmetrics; i++) {
	metric = &st[i];
	size = strlen(metric->name);
	if (metric->type < MMV_TYPE_NOSUPPORT ||
	    metric->type > MMV_TYPE_ELAPSED || size == 0) {
	    setoserror(EINVAL);
	    return -1;
	}
	if (size >= MMV_STRINGMAX) {
	    setoserror(E2BIG);
	    return -1;
	}
	if (!mmv_singular(metric->indom) &&
	    !mmv_lookup_indom(metric->indom, in, nindoms)) {
	    setoserror(ESRCH);
	    return -1;
	}
    }
    return MMV_VERSION1;
}

void * 
mmv_stats_init(const char *fname,
		int cluster, mmv_stats_flags_t flags,
		const mmv_metric_t *st, int nmetrics,
		const mmv_indom_t *in, int nindoms)
{
    int	version;

    if ((version = mmv_check(st, nmetrics, in, nindoms)) < 0)
	return NULL;

    return mmv_init(fname, version, cluster, flags,
		    st, nmetrics, in, nindoms, 
		    NULL, 0, NULL, 0, NULL, 0);
}

static int
mmv_check2(const mmv_metric2_t *st, int nmetrics,
	  const mmv_indom2_t *in, int nindoms)
{
    const mmv_instances2_t *instance;
    const mmv_metric2_t *metric;
    const mmv_indom2_t *indom;
    size_t size;
    int i, j, version = MMV_VERSION1;

    for (i = 0; i < nindoms; i++) {
	indom = &in[i];
	if (mmv_singular(indom->serial)) {
	    setoserror(ESRCH);
	    return -1;
	}
	if (indom->shorttext) {
	    size = strlen(indom->shorttext);
	    if (size >= MMV_STRINGMAX) {
		setoserror(E2BIG);
		return -1;
	    }
	}
	if (indom->helptext) {
	    size = strlen(indom->helptext);
	    if (size >= MMV_STRINGMAX) {
		setoserror(E2BIG);
		return -1;
	    }
	}
	for (j = 0; j < indom->count; j++) {
	    instance = &indom->instances[j];
	    size = strlen(instance->external);
	    if (size == 0) {
		setoserror(EINVAL);
		return -1;
	    }
	    if (size >= MMV_STRINGMAX) {
		setoserror(E2BIG);
		return -1;
	    }
	    if (size >= MMV_NAMEMAX)
		version = MMV_VERSION2;
	}
    }

    for (i = 0; i < nmetrics; i++) {
	metric = &st[i];
	size = strlen(metric->name);
	if (metric->type < MMV_TYPE_NOSUPPORT ||
	    metric->type > MMV_TYPE_ELAPSED || size == 0) {
	    setoserror(EINVAL);
	    return -1;
	}
	if (size >= MMV_STRINGMAX) {
	    setoserror(E2BIG);
	    return -1;
	}
	if (size >= MMV_NAMEMAX)
	    version = MMV_VERSION2;
	if (!mmv_singular(metric->indom) &&
	    !mmv_lookup_indom2(metric->indom, in, nindoms)) {
	    setoserror(ESRCH);
	    return -1;
	}
    }
    return version;
}

void * 
mmv_stats2_init(const char *fname,
		int cluster, mmv_stats_flags_t flags,
		const mmv_metric2_t *st, int nmetrics,
		const mmv_indom2_t *in, int nindoms)
{
    int	version;

    if ((version = mmv_check2(st, nmetrics, in, nindoms)) < 0)
	return NULL;

    return mmv_init(fname, version, cluster, flags,
		    NULL, 0, NULL, 0, st, nmetrics, in, nindoms, NULL, 0);
}

mmv_registry_t *
mmv_stats_registry(const char *file,
		   int cluster,
		   mmv_stats_flags_t flags)
{
    mmv_registry_t * mr;

    /*
     * Allocate dynamic memory to hold the metric registry
     */
    mr = (mmv_registry_t *)calloc(1, sizeof(mmv_registry_t));
    if (mr == NULL) {
	setoserror(ENOMEM);
	return NULL;
    }
    /*
     * Initial version is 1, this increases to 2 if adding
     * long strings, and to 3 if adding any metric labels.
     */
    mr->version = MMV_VERSION1;
    mr->file = file;
    mr->cluster = cluster;
    mr->flags = flags;
    return mr;
}

int 
mmv_stats_add_metric(mmv_registry_t *registry, const char *name, int item,
		     mmv_metric_type_t type, mmv_metric_sem_t sem, pmUnits units,
		     int serial, const char *shorthelp, const char *longhelp)
{
    mmv_metric2_t * metric;
    size_t bytes;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }

    bytes = (registry->nmetrics + 1) * sizeof(mmv_metric2_t);
    metric = (mmv_metric2_t *) realloc(registry->metrics, bytes);
    if (metric == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->metrics = metric;
    
    metric[registry->nmetrics].name = (char *)name;
    metric[registry->nmetrics].item = item;
    metric[registry->nmetrics].type = type;
    
    metric[registry->nmetrics].semantics = sem;
    metric[registry->nmetrics].dimension = units;
    metric[registry->nmetrics].indom = serial;
    
    metric[registry->nmetrics].shorttext = (char *)shorthelp;
    metric[registry->nmetrics].helptext = (char *)longhelp;

    registry->nmetrics++;
    return 0;
}

int
mmv_stats_add_indom(mmv_registry_t *registry, int serial, 
		    const char *shorthelp, const char *longhelp) 
{
    mmv_indom2_t * indom;
    size_t bytes;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }

    bytes = (registry->nindoms + 1) * sizeof(mmv_indom2_t);
    indom = (mmv_indom2_t *) realloc(registry->indoms, bytes);
    if (indom == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->indoms = indom;

    indom[registry->nindoms].serial = serial;
    indom[registry->nindoms].count = 0;
    indom[registry->nindoms].instances = NULL;
    indom[registry->nindoms].shorttext = (char *)shorthelp;
    indom[registry->nindoms].helptext = (char *)longhelp;

    registry->nindoms++;
    return 0;
}

int
mmv_stats_add_instance(mmv_registry_t *registry, int serial,
			int instid, const char *instname) 
{
    mmv_instances2_t * instance;
    mmv_instances2_t * inst_aux;
    size_t bytes;
    int i;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }
    
    bytes = (registry->ninstances + 1) * sizeof(mmv_instances2_t);
    instance = (mmv_instances2_t *) realloc(registry->instances, bytes);
    if (instance == NULL) {
	setoserror(ENOMEM);
	return -1;
    }
    registry->instances = instance;
    instance[registry->ninstances].internal = instid;
    instance[registry->ninstances].external = (char *) instname;

    /* Look for the given indom and add a new instance */
    for (i = 0; i < registry->nindoms; i++) {
	if (registry->indoms[i].serial != serial)
	    continue;
	bytes = (registry->indoms[i].count + 1) * sizeof(mmv_instances2_t);
	inst_aux = realloc(registry->indoms[i].instances, bytes);
	if (inst_aux == NULL) {
	    setoserror(ENOMEM);
	    return -1;    
	}
	registry->indoms[i].instances = inst_aux;

	inst_aux[registry->indoms[i].count].internal = instid;
	inst_aux[registry->indoms[i].count].external = (char *) instname;

	registry->indoms[i].count++;

    }
    if (i == registry->nindoms) {
	/* indom with that serial number was not found */
	setoserror(EINVAL);
	return -1;
    }
    
    registry->ninstances++;
    return 0;
}

/*
 * Verify the user-supplied label.  Produce a JSONB form label in
 * the provided buffer (out) of length MMV_LABELMAX.
 */
static int
get_label(const char *name, const char *value, mmv_value_type_t type,
		int flags, char *buffer)
{
    pmLabelSet *set = NULL;
    char *endnum = NULL;
    int i, len, sts;

    if (name == NULL || value == NULL) {
	setoserror(EINVAL);
	return -1;
    }

    /* The +5 is for the characters we add next - {"":} */
    if (strlen(name) + strlen(value) + 5 > MMV_LABELMAX) {
	setoserror(E2BIG);
	return -1;
    }

    /* Verify the name meets pmLookupLabel(3) syntax rules */
    len = name ? strlen(name) : 0;
    if (len < 1 || !isalpha((int)(name[0]))) {
	setoserror(EINVAL);
	return -1;
    }
    for (i = 1; i < len; i++) {
	if (isalnum((int)(name[i])) || name[i] == '_')
	    continue;
	setoserror(EINVAL);
	return -1;
    }

    /* Verify values meet some (type-based) sanity checks */
    len = value ? strlen(value) : 0;
    switch (type) {
	case MMV_NULL_TYPE:
	    value = "null";
	    break;
	case MMV_BOOLEAN_TYPE:
	    if ((len < 4 || len > 5) ||
		(strcmp(value, "true") != 0 && strcmp(value, "false")) != 0) {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
	case MMV_MAP_TYPE:
	    if (len < 2 || value[0] != '{' || value[len-1] != '}') {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
	case MMV_ARRAY_TYPE:
	    if (len < 2 || value[0] != '[' || value[len-1] != ']') {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
	case MMV_STRING_TYPE:
	    if (len < 2 || value[0] != '\"' || value[len-1] != '\"') {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
	case MMV_NUMBER_TYPE:
	    if (value)
		(void)strtod(value, &endnum);
	    if (len < 1 || *endnum != '\0') {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
	default:
	    setoserror(EINVAL);
	    return -1;
    }

    len = pmsprintf(buffer, MMV_LABELMAX, "{\"%s\":%s}", name, value);
    if ((sts = __pmParseLabelSet(buffer, len, flags, &set)) < 0) {
	setoserror(-sts);
	return -1;
    }
    pmFreeLabelSets(set, 1);
    return len + 1;	/* include the null terminator */
}

/*
 * Create a new PM_LABEL_CLUSTER type label - i.e. a label for
 * all metrics from the instrumented application.
 */
int 
mmv_stats_add_registry_label(mmv_registry_t *registry,
			const char *name, const char *value,
			mmv_value_type_t type, int optional)
{
    mmv_label_t * label;
    size_t bytes;
    char buffer[MMV_LABELMAX];
    int buflen;
    int flags = PM_LABEL_CLUSTER;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }
    if (optional)
	flags |= PM_LABEL_OPTIONAL;
    if ((buflen = get_label(name, value, type, flags, buffer)) < 0)
	return -1;

    bytes = (registry->nlabels + 1) * sizeof(mmv_label_t);
    label = (mmv_label_t *) realloc(registry->labels, bytes);
    if (label == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->version = MMV_VERSION3;
    registry->labels = label;

    label[registry->nlabels].flags = flags;
    label[registry->nlabels].identity = registry->cluster;
    label[registry->nlabels].internal = PM_IN_NULL;
    memcpy(label[registry->nlabels].payload, buffer, buflen);

    registry->nlabels++;
    return 0;
}

/*
 * Create a new PM_LABEL_INDOM type label - i.e. a label for
 * the given set of instances.
 */
int 
mmv_stats_add_indom_label(mmv_registry_t *registry, int serial,
			const char *name, const char *value, 
			mmv_value_type_t type, int optional)
{
    mmv_label_t * label;
    size_t bytes;
    char buffer[MMV_LABELMAX];
    int buflen;
    int flags = PM_LABEL_INDOM;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }
    if (optional)
	flags |= PM_LABEL_OPTIONAL;
    if ((buflen = get_label(name, value, type, flags, buffer)) < 0)
	return -1;

    bytes = (registry->nlabels + 1) * sizeof(mmv_label_t);
    label = (mmv_label_t *) realloc(registry->labels, bytes);
    if (label == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->version = MMV_VERSION3;
    registry->labels = label;

    label[registry->nlabels].flags = flags;
    label[registry->nlabels].identity = serial;
    label[registry->nlabels].internal = PM_IN_NULL;
    memcpy(label[registry->nlabels].payload, buffer, buflen);

    registry->nlabels++;
    return 0;
}

/*
 * Create a new PM_LABEL_ITEM type label - i.e. a label for
 * an individual metric.
 */
int
mmv_stats_add_metric_label(mmv_registry_t *registry, int item,
			   const char *name, const char *value,
			   mmv_value_type_t type, int optional)
{
    mmv_label_t * label;
    size_t bytes;
    char buffer[MMV_LABELMAX];
    int buflen;
    int flags = PM_LABEL_ITEM;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }
    if (optional)
	flags |= PM_LABEL_OPTIONAL;
    if ((buflen = get_label(name, value, type, flags, buffer)) < 0)
	return -1;
 
    bytes = (registry->nlabels + 1) * sizeof(mmv_label_t);
    label = (mmv_label_t *) realloc(registry->labels, bytes);
    if (label == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->version = MMV_VERSION3;
    registry->labels = label;

    label[registry->nlabels].flags = flags;
    label[registry->nlabels].identity = item;
    label[registry->nlabels].internal = PM_IN_NULL;
    memcpy(label[registry->nlabels].payload, buffer, buflen);
    
    registry->nlabels++;
    return 0;
}

/*
 * Create a new PM_LABEL_INSTANCES type label - i.e. a label for
 * an individual instance of an instance domain.
 */
int 
mmv_stats_add_instance_label(mmv_registry_t *registry, int serial, int instid, 
			const char *name, const char *value,
			mmv_value_type_t type, int optional)
{
    mmv_label_t * label;
    size_t bytes;
    char buffer[MMV_LABELMAX];
    int buflen;
    int flags = PM_LABEL_INSTANCES;

    if (registry == NULL) {
	setoserror(EFAULT);
	return -1;
    }
    if (optional)
	flags |= PM_LABEL_OPTIONAL;
    if ((buflen = get_label(name, value, type, flags, buffer)) < 0)
	return -1;
    
    bytes = (registry->nlabels + 1) * sizeof(mmv_label_t);
    label = (mmv_label_t *) realloc(registry->labels, bytes);
    if (label == NULL) {
	setoserror(ENOMEM);
	return -1;
    }

    registry->version = MMV_VERSION3;
    registry->labels = label;

    label[registry->nlabels].flags = flags;
    label[registry->nlabels].identity = serial;
    label[registry->nlabels].internal = instid;
    memcpy(label[registry->nlabels].payload, buffer, buflen);

    registry->nlabels++;
    return 0;
}

void *
mmv_stats_start(mmv_registry_t *registry) 
{
    int version;

    if ((version = mmv_check2(registry->metrics, registry->nmetrics,
				registry->indoms, registry->nindoms)) < 0)
	return NULL;

    if (registry->version != MMV_VERSION3)
	registry->version = version;

    registry->addr = mmv_init(registry->file,
				registry->version, registry->cluster,
				registry->flags, NULL, 0, NULL, 0, 
				registry->metrics, registry->nmetrics, 
				registry->indoms, registry->nindoms,
				registry->labels, registry->nlabels);
    return registry->addr;
}

void
mmv_stats_stop(const char *fname, void *addr)
{
    mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
    char path[MAXPATHLEN];
    struct stat sbuf;
    int fd;

    mmv_stats_path(fname, path, sizeof(path));
    /*
     * If file has gone or is inaccessible, we just want to unmap the
     * region that starts at addr ... a length of 1 seems to suffice.
     */
    if ((fd = open(path, O_RDONLY)) < 0)
	sbuf.st_size = 1;
    else if (fstat(fd, &sbuf) < 0)
	sbuf.st_size = 1;
    else if (hdr && hdr->flags & MMV_FLAG_PROCESS)
	unlink(path);
    if (fd >= 0)
	close(fd);
    if (addr)
	__pmMemoryUnmap(addr, sbuf.st_size);
}

void
mmv_stats_free(mmv_registry_t *registry)
{
    int i;

    for (i = 0; i < registry->nindoms; i++)
	if (registry->indoms[i].instances)
	    free(registry->indoms[i].instances);
    if (registry->indoms)
	free(registry->indoms);
    if (registry->instances)
	free(registry->instances);
    if (registry->metrics)
	free(registry->metrics);
    if (registry->labels)
	free(registry->labels);

    mmv_stats_stop(registry->file, registry->addr);
    memset(registry, 0, sizeof(mmv_registry_t));
    free(registry);
}

static pmAtomValue *
mmv_lookup_value_desc1(void *addr, const char *metric, const char *inst,
			mmv_disk_toc_t *toc)
{
    int j;
    mmv_disk_value_t *v = (mmv_disk_value_t *)((char *)addr + toc->offset);

    for (j = 0; j < toc->count; j++) {
	mmv_disk_metric_t *m = (mmv_disk_metric_t *)
					((char *)addr + v[j].metric);
	if (strcmp(m->name, metric) == 0) {
	    if (mmv_singular(m->indom)) {  /* Singular metric */
		return &v[j].value;
	    } else {
		if (inst == NULL) {
		    /* Metric has multiple instances, but we don't know
		     * which one to return, so return an error.
		     */
		    return NULL;
		} else {
		    mmv_disk_instance_t *in = (mmv_disk_instance_t *)
					((char *)addr + v[j].instance);
		    if (strcmp(in->external, inst) == 0)
			return &v[j].value;
		}
	    }
	}
    }

    return NULL;
}

static pmAtomValue *
mmv_lookup_value_desc2(void *addr, const char *metric, const char *inst,
			mmv_disk_toc_t *toc)
{
    int j;
    mmv_disk_value_t *v = (mmv_disk_value_t *)((char *)addr + toc->offset);

    for (j = 0; j < toc->count; j++) {
	mmv_disk_metric2_t *m = (mmv_disk_metric2_t *)
					((char *)addr + v[j].metric);
	mmv_disk_string_t *s = (mmv_disk_string_t *)
					((char *)addr + m->name);
	if (strcmp(s->payload, metric) == 0) {
	    if (mmv_singular(m->indom)) {  /* Singular metric */
		return &v[j].value;
	    } else {
		if (inst == NULL) {
		    /* Metric has multiple instances, but we don't know
		     * which one to return, so return an error.
		     */
		    return NULL;
		} else {
		    mmv_disk_instance2_t *in = (mmv_disk_instance2_t *)
					((char *)addr + v[j].instance);
		    s = (mmv_disk_string_t *)((char *)addr + in->external);
		    if (strcmp(s->payload, inst) == 0)
			return &v[j].value;
		}
	    }
	}
    }
    return NULL;
}

pmAtomValue *
mmv_lookup_value_desc(void *addr, const char *metric, const char *inst)
{
    if (addr != NULL && metric != NULL) {
	int i;
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
	mmv_disk_toc_t *toc = (mmv_disk_toc_t *)
			((char *)addr + sizeof(mmv_disk_header_t));

	if (hdr->version == MMV_VERSION1) {
	    for (i = 0; i < hdr->tocs; i++)
		if (toc[i].type == MMV_TOC_VALUES)
		    return mmv_lookup_value_desc1(addr, metric, inst, &toc[i]);
	} else {
	    for (i = 0; i < hdr->tocs; i++)
		if (toc[i].type == MMV_TOC_VALUES)
		    return mmv_lookup_value_desc2(addr, metric, inst, &toc[i]);
	}
    }
    return NULL;
}

void
mmv_inc_value(void *addr, pmAtomValue *av, double inc)
{
    if (av != NULL && addr != NULL) {
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
	mmv_disk_value_t *v = (mmv_disk_value_t *)av;
	int type;

	if (hdr->version == MMV_VERSION1) {
	    mmv_disk_metric_t *m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
	    type = m->type;
	} else {
	    mmv_disk_metric2_t *m = (mmv_disk_metric2_t *)
					((char *)addr + v->metric);
	    type = m->type;
	}
	switch (type) {
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
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
	mmv_disk_value_t *v = (mmv_disk_value_t *)av;
	int type;

	if (hdr->version == MMV_VERSION1) {
	    mmv_disk_metric_t *m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
	    type = m->type;
	} else {
	    mmv_disk_metric2_t *m = (mmv_disk_metric2_t *)
					((char *)addr + v->metric);
	    type = m->type;
	}
	switch (type) {
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
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;
	mmv_disk_value_t *v = (mmv_disk_value_t *)av;
	int type;

	if (hdr->version == MMV_VERSION1) {
	    mmv_disk_metric_t *m = (mmv_disk_metric_t *)
					((char *)addr + v->metric);
	    type = m->type;
	} else {
	    mmv_disk_metric2_t *m = (mmv_disk_metric2_t *)
					((char *)addr + v->metric);
	    type = m->type;
	}
 
	if (type == MMV_TYPE_STRING &&
	    (size >= 0 && size < MMV_STRINGMAX - 1)) {
	    __uint64_t soffset = v->extra;
	    mmv_disk_string_t *s;

	    s = (mmv_disk_string_t *)((char *)addr + soffset);
	    /* clear original contents, preparing for overwriting */
	    memset(s->payload, 0, sizeof(s->payload));
	    /* swap in new value (note: value.l is cosmetic only) */
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
	pmAtomValue *mmv_metric;
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
	pmAtomValue *mmv_metric;
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
	    pmtimevalNow(&tv);
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
	pmtimevalNow(&tv);
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
