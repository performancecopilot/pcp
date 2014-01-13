/*
 * GFS2 sbstats sysfs file statistics.
 *
 * Copyright (c) 2013 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "pmdagfs2.h"

#include <ctype.h>

static const char *stattype[] = {
	[LOCKSTAT_SRTT]		= "srtt",
	[LOCKSTAT_SRTTVAR]	= "srttvar",
	[LOCKSTAT_SRTTB]	= "srttb",
	[LOCKSTAT_SRTTVARB]	= "srttvarb",
	[LOCKSTAT_SIRT]		= "sirt",
	[LOCKSTAT_SIRTVAR]	= "sirtvar",
	[LOCKSTAT_DCOUNT]	= "dlm",
	[LOCKSTAT_QCOUNT]	= "queue",
};

static const char *stattext[] = {
	[LOCKSTAT_SRTT]		= "Non-blocking smoothed round trip time",
	[LOCKSTAT_SRTTVAR]	= "Non-blocking smoothed variance",
	[LOCKSTAT_SRTTB]	= "Blocking smoothed round trip time",
	[LOCKSTAT_SRTTVARB]	= "Blocking smoothed variance",
	[LOCKSTAT_SIRT]		= "Smoothed inter-request time",
	[LOCKSTAT_SIRTVAR]	= "Smoothed inter-request variance",
	[LOCKSTAT_DCOUNT]	= "Count of Distributed Lock Manager requests",
	[LOCKSTAT_QCOUNT]	= "Count of gfs2_holder queues",
};

static const char *locktype[] = {
	[LOCKTYPE_RESERVED]	= "reserved",
	[LOCKTYPE_NONDISK]	= "nondisk",
	[LOCKTYPE_INODE]	= "inode",
	[LOCKTYPE_RGRB]		= "rgrp",
	[LOCKTYPE_META]		= "meta",
	[LOCKTYPE_IOPEN]	= "iopen",
	[LOCKTYPE_FLOCK]	= "flock",
	[LOCKTYPE_PLOCK]	= "plock",
	[LOCKTYPE_QUOTA]	= "quota",
	[LOCKTYPE_JOURNAL]	= "journal",
};

int
gfs2_sbstats_fetch(int item, struct sbstats *fs, pmAtomValue *atom)
{
    /* Check for valid metric count */
    if (item < 0 || item >= SBSTATS_COUNT)
	return PM_ERR_PMID;

    /* Check for no values recorded */
    if(fs->values[item] == UINT64_MAX)
        return 0;

    atom->ull = fs->values[item];
    return 1;
}

int
gfs2_refresh_sbstats(const char *sysfs, const char *name, struct sbstats *sb)
{
    unsigned int id = 0;
    char buffer[4096];
    FILE *fp;

    /* Reset all counter for this fs */
    memset(sb, 0, sizeof(*sb));

    snprintf(buffer, sizeof(buffer), "%s/%s/sbstats", sysfs, name);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL){
        /*
         * We set the values to UINT64_MAX to signify we have no
         * current values (no metric support or debugfs not mounted)
         *
         */
        memset(sb, -1, sizeof(*sb));
	return -oserror();
    }

    /*
     * Read through sbstats file one line at a time.  This is called on each
     * and every fetch request, so should be as quick as we can make it.
     *
     * Once we've skipped over the heading, the format is fixed so to make this
     * faster (we expect the kernel has a fixed set of types/stats) we go right
     * ahead and index directly into the stats structure for each line.  We do
     * check validity too - if there's a mismatch, we throw our toys.
     *
     * Also, we aggregate the per-CPU data for each statistic - we could go for
     * separate per-CPU metrics as well, but for now just keep it simple until
     * there's a real need for that extra complexity.
     *
     */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	char *typestr, *statstr, *end;
	char *p = buffer;

	if (strncmp(p, "type", 4) == 0)
	    continue;
	if (id > SBSTATS_COUNT)
	    break;

	typestr = p;
	for (typestr = p; !isspace((int)*p); p++) { }	/* skip lock type */
	for (; isspace((int)*p); p++) { *p = '\0'; }	/* eat whitespace */
	for (statstr = p; *p != ':'; p++) { }		/* skip stat type */
	*p = '\0';

	/* verify that the id we are up to matches what we see in the file */
	unsigned int type = id / NUM_LOCKSTATS;
	unsigned int stat = id % NUM_LOCKSTATS;
	if (strcmp(typestr, locktype[type]) != 0) {
	    __pmNotifyErr(LOG_ERR,
			"unexpected sbstat type \"%s\" (want %s at line %u)",
			typestr, locktype[type], id);
	    break;	/* eh? */
	}
	if (strcmp(statstr, stattype[stat]) != 0) {
	    __pmNotifyErr(LOG_ERR,
			"unexpected sbstat stat \"%s\" (want %s at line %u)",
			statstr, stattype[stat], id);
	    break;	/* wha? */
	}

	/* decode all of the (per-CPU) values until the end of line */
	for (p++; *p != '\0'; p++) {
	    __uint64_t value;

	    value = strtoull(p, &end, 10);
	    if (end == p)
		break;
	    sb->values[id] += value;
	    p = end;
	}

	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_INFO,
			"got expected sbstat type \"%s\", stat \"%s\" at line %u",
			typestr, statstr, id);

	/* now we can move on to the next metric (on the next line) */
	id++;
    }
    fclose(fp);
    return (id == SBSTATS_COUNT) ? 0 : -EINVAL;
}

static void
add_pmns_node(__pmnsTree *tree, int domain, int cluster, int lock, int stat)
{
    char entry[64];
    pmID pmid = pmid_build(domain, cluster, (lock * NUM_LOCKSTATS) + stat);

    snprintf(entry, sizeof(entry),
	     "gfs2.sbstats.%s.%s", locktype[lock], stattype[stat]);
    __pmAddPMNSNode(tree, pmid, entry);

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "GFS2 sbstats added %s (%s)", entry, pmIDStr(pmid));
}

static int
refresh_sbstats(pmdaExt *pmda, __pmnsTree **tree)
{
    int t, s, sts;
    static __pmnsTree *sbstats_tree;

    if (sbstats_tree) {
	*tree = sbstats_tree;
    } else if ((sts = __pmNewPMNS(&sbstats_tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create sbstats names: %s\n",
			pmProgname, pmErrStr(sts));
	*tree = NULL;
    } else {
        for (t = 0; t < NUM_LOCKTYPES; t++)
	    for (s = 0; s < NUM_LOCKSTATS; s++)
		add_pmns_node(sbstats_tree, pmda->e_domain, CLUSTER_SBSTATS, t, s);
	*tree = sbstats_tree;
	return 1;
    }
    return 0;
}

/*
 * Create a new metric table entry based on an existing one.
 *
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int lock)
{
    int item = pmid_item(source->m_desc.pmid);
    int domain = pmid_domain(source->m_desc.pmid);
    int cluster = pmid_cluster(source->m_desc.pmid);

    memcpy(dest, source, sizeof(pmdaMetric));
    item += lock * NUM_LOCKSTATS;
    dest->m_desc.pmid = pmid_build(domain, cluster, item);

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "GFS2 sbstats refresh_metrictable: (%p -> %p) "
			"metric ID dup: %d.%d.%d -> %d.%d.%d\n",
			source, dest, domain, cluster,
			pmid_item(source->m_desc.pmid), domain, cluster, item);
}

/*
 * Used to answer the question: how much extra space needs to be
 * allocated in the metric table for (dynamic) sbstats metrics?
 *
 */
static void
size_metrictable(int *total, int *trees)
{
    *total = NUM_LOCKSTATS;
    *trees = NUM_LOCKTYPES;
}

static int
sbstats_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    int item = pmid_item(pmid);
    static char text[128];

    if (pmid_cluster(pmid) != CLUSTER_SBSTATS)
	return PM_ERR_PMID;
    if (item < 0 || item >= SBSTATS_COUNT)
	return PM_ERR_PMID;
    snprintf(text, sizeof(text), "%s for %s glocks",
	     stattext[item % NUM_LOCKSTATS], locktype[item / NUM_LOCKSTATS]);

    *buf = text;

    return 0;
}

void
gfs2_sbstats_init(pmdaMetric *metrics, int nmetrics)
{
    int set[] = { CLUSTER_SBSTATS };

    pmdaDynamicPMNS("gfs2.sbstats",
		    set, sizeof(set)/sizeof(int),
		    refresh_sbstats, sbstats_text,
		    refresh_metrictable, size_metrictable,
		    metrics, nmetrics);
}
