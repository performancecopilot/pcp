/*
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
 *
 * Memory Mapped Values PMDA Client API
 */
#include "pmapi.h"
#include "impl.h"
#include "mmv_stats.h"

#define ROUNDUP_64(i) ((((i) + 63) >> 6) << 6)

typedef struct {
    mmv_stats_inst_t *inst;
    int ninst;
} indom_t;

static void *
mmv_mapping(const char *fname, size_t size)
{
    static int initialised;
    static char statsdir[MAXPATHLEN];
    char fullpath[MAXPATHLEN];
    void *addr = NULL;
    int fd;

    if (!initialised) {
	sprintf(statsdir, "%s%c" "mmv",
		pmGetConfig("PCP_TMP_DIR"), __pmPathSeparator());
	initialised = 1;
    }

    /* unlink+creat will cause the pmda to reload on next fetch */
    sprintf(fullpath, "%s%c%s", statsdir, __pmPathSeparator(), fname);
    unlink(fullpath);
    fd = open(fullpath, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0)
	return NULL;

    if (lseek(fd, size - 1, SEEK_SET) < 0)
	goto finish;
    if (write(fd, " ", 1) <= 0)
	goto finish;
    addr = __pmMemoryMap(fd, size, 1);
finish:
    close(fd);
    return addr;
}

static __uint64_t
mmv_string(void *addr, int tocindex, int strindex, __uint64_t values_offset, char *data)
{
    mmv_stats_toc_t *	toc;
    mmv_stats_string_t	*str;
    __uint64_t		string_offset;

    toc = (mmv_stats_toc_t *)((char *)addr + sizeof(mmv_stats_hdr_t)) + tocindex;
    string_offset = values_offset + strindex * sizeof(mmv_stats_string_t);

    toc->typ = MMV_TOC_STRING;
    toc->cnt = 1;
    toc->offset = string_offset;

    if (data) {		/* optional initial value (help text) */
	str = (mmv_stats_string_t *)((char *)addr + string_offset);
	strncpy(str->payload, data, MMV_STRINGMAX);
    }
    return string_offset;
}

void * 
mmv_stats_init(const char *fname, const mmv_stats_t *st, int nstats, mmv_stats_flags_t fl, int cl)
{
    mmv_stats_metric_t *mlist;
    mmv_stats_value_t *vlist;
    mmv_stats_hdr_t *hdr;
    mmv_stats_toc_t *toc;
    __uint64_t soffset;
    __uint64_t offset;
    indom_t *indoms;
    void *addr;
    int i, j, sz, tocidx;
    int nstrings = 0;
    int nvalues = 0;
    int nindoms = 0;

    mlist = (mmv_stats_metric_t *)calloc(nstats, sizeof(mmv_stats_metric_t));
    if (mlist == NULL)
	return NULL;
    if ((indoms = (indom_t *)calloc(nstats, sizeof(indom_t*))) == NULL) {
	free(mlist);
	return NULL;
    }

    for (i = 0; i < nstats; i++) {
	if ((st[i].type < MMV_ENTRY_NOSUPPORT) || 
	    (st[i].type > MMV_ENTRY_INTEGRAL) ||
	    (strlen(st[i].name) == 0)) {
	    free(mlist);
	    free(indoms);
	    return NULL;
	}

	strcpy(mlist[i].name, st[i].name);
	mlist[i].item = st[i].item;
	mlist[i].type = st[i].type;
	mlist[i].dimension = st[i].dimension;
	mlist[i].semantics = st[i].semantics;
	if (st[i].semantics != MMV_SEM_INSTANT && st[i].semantics != MMV_SEM_DISCRETE)
	    mlist[i].semantics = MMV_SEM_COUNTER;

	if (st[i].helptext)
	    nstrings++;
	if (st[i].shorttext)
	    nstrings++;

	if (st[i].indom != NULL) {
	    /* Lookup an indom */
	    for (j = 0; j < nindoms; j++) {
		if (indoms[j].inst == st[i].indom) {
		    sz = indoms[j].ninst;
		    mlist[i].indom = j;
		    break;
		}
	    }

	    if (j == nindoms) {
		indoms[nindoms].inst = st[i].indom;
		indoms[nindoms].ninst = 0;

		for (j = 0; st[i].indom[j].internal != -1; j++)
		    indoms[nindoms].ninst++;

		sz = indoms[nindoms].ninst;
		mlist[i].indom = nindoms++;
	    }
	    if (mlist[i].type == MMV_ENTRY_STRING)
		nstrings += sz;
	    nvalues += sz;
	} else {
	    mlist[i].indom = -1;
	    if (mlist[i].type == MMV_ENTRY_STRING)
		nstrings++;
	    nvalues++;
	}
    }

    if (nvalues == 0) {
	free(mlist);
	free(indoms);
	return NULL;
    }

    /* Size of the header + TOC with enough instances to accomodate
     * nindoms instance lists plus metric list and value list
     */
    sz = ROUNDUP_64(sizeof(mmv_stats_hdr_t) + 
		    sizeof(mmv_stats_toc_t) * (nindoms + nstrings + 2));

    /* Size of all indoms */
    for (i = 0; i < nindoms; i++)
	sz += ROUNDUP_64(indoms[i].ninst * sizeof(mmv_stats_inst_t));

    /* Size of metrics list */
    sz += ROUNDUP_64(nstats * sizeof(mmv_stats_metric_t));

    /* Size of values list */
    sz += nvalues * sizeof(mmv_stats_value_t);

    /* Size of all string sections (values, short/long help text) */
    sz += nstrings * sizeof(mmv_stats_string_t);

    if ((addr = mmv_mapping(fname, sz)) == NULL) {
	free(mlist);
	free(indoms);
	return NULL;
    }

    hdr = (mmv_stats_hdr_t *) addr;
    toc = (mmv_stats_toc_t *)((char *)addr + sizeof(mmv_stats_hdr_t));
    offset = ROUNDUP_64(sizeof(mmv_stats_hdr_t) + 
			(nindoms + nstrings + 2) * sizeof(mmv_stats_toc_t));

    /* We unconditionally clobber the stat file on each restart -
     * easier this way and the clients deal with counter wraps.
     */
    memset(hdr, 0, sizeof(mmv_stats_hdr_t));
    strncpy(hdr->magic, "MMV", 4);
    hdr->version = MMV_VERSION;
    hdr->g1 = (__uint64_t) time(NULL);
    hdr->tocs = nindoms + nstrings + 2;
    hdr->flags = fl;
    hdr->cluster = cl;
    hdr->process = getpid();

    for (tocidx = 0; tocidx < nindoms; tocidx++) {
	toc[tocidx].typ = MMV_TOC_INDOM;
	toc[tocidx].cnt = indoms[tocidx].ninst;
	toc[tocidx].offset = offset;

	memcpy((char *)addr + offset, indoms[tocidx].inst, 
		sizeof(mmv_stats_inst_t) * indoms[tocidx].ninst);

	offset += sizeof(mmv_stats_inst_t) * indoms[tocidx].ninst;
	offset = ROUNDUP_64(offset);
    }

    toc[tocidx].typ = MMV_TOC_METRICS;
    toc[tocidx].cnt = nstats;
    toc[tocidx].offset = offset;

    memcpy((char *)addr + offset, mlist,  sizeof(mmv_stats_metric_t) * nstats);

    offset += sizeof(mmv_stats_metric_t) * nstats;
    offset = ROUNDUP_64(offset);

    tocidx++;

    toc[tocidx].typ = MMV_TOC_VALUES;
    toc[tocidx].cnt = nvalues;
    toc[tocidx].offset = offset;

    vlist = (mmv_stats_value_t *)((char *)addr + offset);
    soffset = offset + nvalues * sizeof(mmv_stats_value_t);

    for (--nvalues, i = 0; i < nstats; i++) {
	__uint64_t moffset = toc[nindoms].offset + i * sizeof(mmv_stats_metric_t);
	mmv_stats_metric_t *mp = addr + moffset;
	mmv_stats_value_t *value;

	if (st[i].indom == NULL) {
	    value = &vlist[nvalues];
	    memset(value, 0, sizeof(mmv_stats_value_t));
	    value->metric = moffset;
	    value->instance = -1;
	    if (st[i].type == MMV_ENTRY_STRING)
		value->extra = mmv_string(addr, ++tocidx, --nstrings, soffset, NULL);
	    nvalues--;
	} else {
	    int j, idx = mlist[i].indom;

	    for (j = indoms[idx].ninst - 1; j >= 0; j-- ) {
		value = &vlist[nvalues];
		memset(value, 0, sizeof(mmv_stats_value_t));
		value->metric = moffset;
		value->instance = toc[idx].offset + j * sizeof(mmv_stats_inst_t);
		if (st[i].type == MMV_ENTRY_STRING)
		    value->extra = mmv_string(addr, ++tocidx, --nstrings, soffset, NULL);
		nvalues--;
	    }
	}
	if (st[i].shorttext)
	    mp->shorttext = mmv_string(addr, ++tocidx, --nstrings, soffset, st[i].shorttext);
	if (st[i].helptext)
	    mp->helptext = mmv_string(addr, ++tocidx, --nstrings, soffset, st[i].helptext);
    }

    hdr->g2 = hdr->g1; /* Unlock the header - PMDA can read now */

    free(mlist);
    free(indoms);
    return addr;
}

mmv_stats_value_t *
mmv_lookup_value_desc(void *addr, const char *metric, const char *inst)
{
    if (addr != NULL && metric != NULL) {
	int i;
	mmv_stats_hdr_t *hdr = (mmv_stats_hdr_t *)addr;
	mmv_stats_toc_t *toc = 
	    (mmv_stats_toc_t *)((char *)addr + sizeof(mmv_stats_hdr_t));

	for (i = 0; i < hdr->tocs; i++) {
	    if (toc[i].typ ==  MMV_TOC_VALUES) {
		int j;
		mmv_stats_value_t *v = 
		    (mmv_stats_value_t *)((char *)addr + toc[i].offset);

		for (j = 0; j < toc[i].cnt; j++) {
		    mmv_stats_metric_t *m = 
			(mmv_stats_metric_t *)((char *)addr + v[j].metric);
		    if (!strcmp(m->name, metric)) {
			if (m->indom < 0) {  /* Singular metric */
			    return v+j;
 			} else {
			    if (inst == NULL) {
				/* Metric has multiple instances, but
				 * we don't know which one to return,
				 * so return an error
				 */
				return NULL;
			    } else {
				mmv_stats_inst_t * in = 
				    (mmv_stats_inst_t *)((char *)addr + 
							 v[j].instance);
				if (!strcmp(in->external, inst))
				    return v+j;
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
mmv_inc_value(void *addr, mmv_stats_value_t *v, double inc)
{
    if (v != NULL && addr != NULL) {
	mmv_stats_metric_t * m = 
	    (mmv_stats_metric_t *)((char *)addr + v->metric);
    
	switch (m->type) {
	case MMV_ENTRY_I32:
	    v->val.i32 += (__int32_t)inc;
	    break;
	case MMV_ENTRY_U32:
	    v->val.u32 += (__uint32_t)inc;
	    break;
	case MMV_ENTRY_I64:
	    v->val.i64 += (__int64_t)inc;
	    break;
	case MMV_ENTRY_U64:
	    v->val.i32 += (__uint64_t)inc;
	    break;
	case MMV_ENTRY_FLOAT:
	    v->val.f += (float)inc;
	    break;
	case MMV_ENTRY_DOUBLE:
	    v->val.d += inc;
	    break;
	case MMV_ENTRY_INTEGRAL:
	    v->val.i64 +=  (__int64_t)inc;
	    if (inc < 0)
		v->extra++;
	    else
		v->extra--;
	    break;
	default:
	    break;
	}
    }
}

void
mmv_set_string(void *addr, mmv_stats_value_t *v, const char *string, int size)
{
    if (v != NULL && addr != NULL && string != NULL) {
	__uint64_t		soffset = v->extra;
	mmv_stats_metric_t *	m = (mmv_stats_metric_t *)((char *)addr + v->metric);
	mmv_stats_string_t	*str;
    
	if (m->type == MMV_ENTRY_STRING && size >= 0 && size < MMV_STRINGMAX-1) {
	    str = (mmv_stats_string_t *)((char *)addr + soffset);
	    strncpy(str->payload, string, size + 1);
	    v->val.i32 = size;
	}
    }
}
