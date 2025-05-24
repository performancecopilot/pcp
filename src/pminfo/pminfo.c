/*
 * Copyright (c) 2013-2018 Red Hat.
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Debug flags
 * -Dappl4	report downsizing of -b
 * -Dappl5	dump PDU stats at end if context is a host
 */

#include <ctype.h>
#include <limits.h>
#include "pmapi.h"
#include "libpcp.h"
#include "sha1.h"

static void myeventdump(pmValueSet *, int, int);
static int  myoverrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_CONTAINER,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_NAMESPACE,
    PMOPT_UNIQNAMES,
    PMOPT_ORIGIN,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Protocol options"),
    { "batch",    1, 'b', "N", "fetch N metrics at a time for -f and -v [128]" },
    { "desc",     0, 'd', 0, "get and print metric description" },
    { "fetch",    0, 'f', 0, "fetch and print values for all instances" },
    { "fetchall", 0, 'F', 0, "fetch and print values for non-enumerable indoms" },
    { "fullindom",0, 'I', 0, "print InDom in verbose format" },
    { "labels",   0, 'l', 0, "print InDom, metric and instance labels" },
    { "pmid",     0, 'm', 0, "print PMID" },
    { "fullpmid", 0, 'M', 0, "print PMID in verbose format" },
    { "series",   0, 's', 0, "print source, metric, instance series identifiers" },
    { "oneline",  0, 't', 0, "get and display (terse) oneline text" },
    { "helptext", 0, 'T', 0, "get and display (verbose) help text" },
    PMAPI_OPTIONS_HEADER("Metrics options"),
    { "derived",  1, 'c', "FILE", "load global derived metric definitions from FILE(s)" },
    { "register", 1, 'r', "name=expr", "register a per-context derived metric" },
    { "events",   0, 'x', 0, "unpack and report on any fetched event records" },
    { "verify",   0, 'v', 0, "verify mode, be quiet and only report errors" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:b:c:dD:Ffh:IK:lLMmN:n:O:r:stTvVxzZ:?",
    .long_options = longopts,
    .short_usage = "[options] [metricname | pmid | indom]...",
    .override = myoverrides,
};

static int	p_mid;		/* Print metric IDs of leaf nodes */
static int	p_fullmid;	/* Print verbose metric IDs of leaf nodes */
static int	p_fulliid;	/* Print verbose indom IDs in descriptions */
static int	p_desc;		/* Print descriptions for metrics */
static int	p_label;	/* Print labels for InDoms,, metrics indoms and instances */
static int	p_series;	/* Print metrics series identifiers */
static int	p_oneline;	/* fetch oneline text? */
static int	p_help;		/* fetch help text? */
static int	p_value;	/* pmFetch and print value(s)? */
static int	p_force;	/* pmFetch and print value(s)? for non-enumerable indoms */

static int	need_context;	/* set if need a pmapi context */
static int	need_labels;	/* set if need to lookup labels */
static int	need_pmid;	/* set if need to lookup names */
static char	**namelist;
static pmID	*pmidlist;
static pmDesc	*desclist;
static int	contextid;
static int	batchsize = 128;
static int	pdusize;	/* guess at size of PDU_PMNS_NAMES PDU */
static int	batchidx;
static int	verify;		/* Only print error messages */
static int	events;		/* Decode event metrics */
static pmID	pmid_flags;
static pmID	pmid_missed;
static int	num_reg;	/* count of the number of -r arguments */
static char	**reg;		/* the -r arguments */

/*
 * InDom cache (icache) state - multiple accessors, so no longer using
 * local variables like most other caching in pminfo.  Caches recently
 * requested instance names and labels for a given pmInDom, since they
 * are closely related - labels/series reporting needs instance names.
 */
static pmInDom		icache_nameindom = PM_INDOM_NULL;
static int		icache_numinst = -1;
static int		*icache_instlist;
static char		**icache_namelist;
static pmInDom		icache_labelindom = PM_INDOM_NULL;
static pmLabelSet	*icache_labelset;
static int		icache_nlabelset = -1;

static void
icache_update_name(pmInDom indom)
{
    if (indom == icache_nameindom)
	return;
    if (icache_numinst > 0) {
	free(icache_instlist);
	free(icache_namelist);
    }
    icache_numinst = pmGetInDom(indom, &icache_instlist, &icache_namelist);
    icache_nameindom = indom;
}

static int
lookup_instance_numinst(pmInDom indom)
{
    icache_update_name(indom);
    return icache_numinst;
}

static char *
lookup_instance_name(pmInDom indom, int inst)
{
    int			i;

    icache_update_name(indom);
    for (i = 0; i < icache_numinst; i++)
	if (icache_instlist[i] == inst)
	    return icache_namelist[i];
    return NULL;
}

static int
lookup_instance_inum(pmInDom indom, int idx)
{
    icache_update_name(indom);
    if (idx >= 0 && idx < icache_numinst)
	return icache_instlist[idx];
    return PM_IN_NULL;
}

static void
icache_update_label(pmInDom indom)
{
    icache_update_name(indom);
    if (indom == icache_labelindom)
	return;
    if (icache_nlabelset > 0)
	pmFreeLabelSets(icache_labelset, icache_nlabelset);
    icache_nlabelset = pmGetInstancesLabels(indom, &icache_labelset);
    icache_labelindom = indom;
}

static int
lookup_instance_nlabelset(pmInDom indom)
{
    icache_update_label(indom);
    return icache_nlabelset;
}

static pmLabelSet *
lookup_instance_labels(pmInDom indom, int idx)
{
    icache_update_label(indom);
    if (idx >= 0 && idx < icache_nlabelset)
	return &icache_labelset[idx];
    return NULL;
}

/*
 * Cache all of the most recently requested labels for a given pmInDom
 */
static pmLabelSet *
lookup_indom_labels(pmInDom indom)
{
    static pmInDom	last = PM_INDOM_NULL;
    static pmLabelSet	*labels;

    if (indom != last) {
	if (labels)
	    pmFreeLabelSets(labels, 1);
	labels = NULL;
	pmGetInDomLabels(indom, &labels);
	last = indom;
    }
    return labels;
}

/*
 * Cache all of the most recently requested labels for a given pmID item
 */
static pmLabelSet *
lookup_cluster_labels(pmID pmid)
{
    static pmID		last = PM_ID_NULL;
    static pmLabelSet	*labels;

    if (pmID_domain(pmid) != pmID_domain(last) ||
	pmID_cluster(pmid) != pmID_cluster(last)) {
	if (labels)
	    pmFreeLabelSets(labels, 1);
	labels = NULL;
	pmGetClusterLabels(pmid, &labels);
	last = pmid;
    }
    return labels;
}

/*
 * Cache all of the most recently requested labels for a given pmID item
 */
static pmLabelSet *
lookup_item_labels(pmID pmid)
{
    static pmID		last = PM_ID_NULL;
    static pmLabelSet	*labels;

    if (pmid != last) {
	if (labels)
	    pmFreeLabelSets(labels, 1);
	labels = NULL;
	pmGetItemLabels(pmid, &labels);
	last = pmid;
    }
    return labels;
}

/*
 * Cache all of the most recently requested labels for a given domain
 */
static pmLabelSet *
lookup_domain_labels(int domain)
{
    static int		last = -1;
    static pmLabelSet	*labels;

    if (domain != last) {
	if (labels)
	    pmFreeLabelSets(labels, 1);
	labels = NULL;
	pmGetDomainLabels(domain, &labels);
	last = domain;
    }
    return labels;
}

/*
 * Produce a set of fallback context labels when no others are available
 * (this is from older versions of pmcd or older archives, typically).
 */
static int
defaultlabels(pmLabelSet **sets)
{
    pmLabelSet		*lp = NULL;
    char		buf[PM_MAXLABELJSONLEN];
    char		host[MAXHOSTNAMELEN];
    int			sts;

    if ((pmGetContextHostName_r(contextid, host, sizeof(host))) == NULL)
	return PM_ERR_GENERIC;
    pmsprintf(buf, sizeof(buf), "{\"hostname\":\"%s\"}", host);
    /*
     * __pmAddLabels() will return 1, else error (and lp is not alloc'd)
     * ... so no leak on the error path
     */
    if ((sts = __pmAddLabels(&lp, buf, PM_LABEL_CONTEXT)) > 0) {
	*sets = lp;
	return 0;
    }
    return sts;
}

/*
 * Cache the global labels for the current PMAPI context
 */
static pmLabelSet *
lookup_context_labels(void)
{
    static pmLabelSet	*labels;
    static int		setup;

    if (!setup) {
	if (pmGetContextLabels(&labels) <= 0)
	    defaultlabels(&labels);
	setup = 1;
    }
    return labels;
}

/* 
 * we only ever have one metric
 */
static void
mydump(pmDesc *dp, pmValueSet *vsp, char *indent)
{
    int		j;
    char	*p;

    if (indent != NULL)
	printf("%s", indent);
    if (vsp->numval == 0) {
	printf("No value(s) available!\n");
	return;
    }
    else if (vsp->numval < 0) {
	printf("Error: %s\n", pmErrStr(vsp->numval));
	return;
    }

    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (dp->indom != PM_INDOM_NULL) {
	    if ((p = lookup_instance_name(dp->indom, vp->inst)) == NULL) {
		if (p_force) {
		    /* the instance disappeared; ignore it */
		    printf("    inst [%d \"%s\"]\n", vp->inst, "DISAPPEARED");
		    continue;
		}
		else {
		    /* report the error and give up */
		    printf("pmNameIndom: indom=%s inst=%d: %s\n",
			    pmInDomStr(dp->indom), vp->inst, pmErrStr(PM_ERR_INST));
		    printf("    inst [%d]", vp->inst);
		}
	    }
	    else
		printf("    inst [%d or \"%s\"]", vp->inst, p);
	}
	else
	    printf("   ");
	printf(" value ");
	pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	putchar('\n');
	if (!events)
	    continue;
	if (dp->type == PM_TYPE_HIGHRES_EVENT)
	    myeventdump(vsp, j, 1);
	else if (dp->type == PM_TYPE_EVENT)
	    myeventdump(vsp, j, 0);
    }
}

static void
setup_event_derived_metrics(void)
{
    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anonymous metrics
	 * should now be in the PMNS
	 */
	const char	*name_flags = "event.flags";
	const char	*name_missed = "event.missed";
	int		sts;

	sts = pmLookupName(1, &name_flags, &pmid_flags);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_flags, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    pmid_flags = pmID_build(pmID_domain(pmid_flags), pmID_cluster(pmid_flags), 1);
	}
	sts = pmLookupName(1, &name_missed, &pmid_missed);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_missed, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    pmid_missed = pmID_build(pmID_domain(pmid_missed), pmID_cluster(pmid_missed), 1);
	}
    }
}

static int
dump_nparams(int numpmid)
{
    if (numpmid == 0) {
	printf(" ---\n");
	printf("	No parameters\n");
	return -1;
    }
    if (numpmid < 0) {
	printf(" ---\n");
	printf("	Error: illegal number of parameters (%d)\n", numpmid);
	return -1;
    }
    return 0;
}

static void
dump_parameter(pmValueSet *xvsp, int idx, int *flagsp)
{
    int		sts, flags = *flagsp;
    pmDesc	desc;
    char	**names;

    if ((sts = pmNameAll(xvsp->pmid, &names)) >= 0) {
	if (idx == 0) {
	    if (xvsp->pmid == pmid_flags) {
		flags = *flagsp = xvsp->vlist[0].value.lval;
		printf(" flags 0x%x", flags);
		printf(" (%s) ---\n", pmEventFlagsStr(flags));
		free(names);
		return;
	    }
	    else
		printf(" ---\n");
	}
	if ((flags & PM_EVENT_FLAG_MISSED) &&
	    (idx == 1) &&
	    (xvsp->pmid == pmid_missed)) {
	    printf("        ==> %d missed event records\n",
			xvsp->vlist[0].value.lval);
	    free(names);
	    return;
	}
	printf("    ");
	__pmPrintMetricNames(stdout, sts, names, " or ");
	printf(" (%s)\n", pmIDStr(xvsp->pmid));
	free(names);
    }
    else
	printf("	PMID: %s\n", pmIDStr(xvsp->pmid));
    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0)
	printf("	pmLookupDesc: %s\n", pmErrStr(sts));
    else
	mydump(&desc, xvsp, "    ");
}

static void
myeventdump(pmValueSet *vsp, int inst, int highres)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;

    if (highres) {
	pmHighResResult	**hr;

	if ((nrecords = pmUnpackHighResEventRecords(vsp, inst, &hr)) < 0) {
	    fprintf(stderr, "%s: pmUnpackHighResEventRecords: %s\n",
		    pmGetProgname(), pmErrStr(nrecords));
	    return;
	}
	setup_event_derived_metrics();
	for (r = 0; r < nrecords; r++) {
	    printf("    --- event record [%d] timestamp ", r);
	    pmPrintHighResStamp(stdout, &hr[r]->timestamp);
	    if (dump_nparams(hr[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < hr[r]->numpmid; p++)
		dump_parameter(hr[r]->vset[p], p, &flags);
	}
	pmFreeHighResEventResult(hr);
    }
    else {
	pmResult	**res;

	if ((nrecords = pmUnpackEventRecords(vsp, inst, &res)) < 0) {
	    fprintf(stderr, "%s: pmUnpackEventRecords: %s\n",
			pmGetProgname(), pmErrStr(nrecords));
	    return;
	}
	setup_event_derived_metrics();
	for (r = 0; r < nrecords; r++) {
	    printf("    --- event record [%d] timestamp ", r);
	    pmPrintStamp(stdout, &res[r]->timestamp);
	    if (dump_nparams(res[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < res[r]->numpmid; p++)
		dump_parameter(res[r]->vset[p], p, &flags);
	}
	pmFreeEventResult(res);
    }
}

static void
myhelptext(unsigned int ident, int type)
{
    char	*buffer, *p;
    int		sts;

    sts = (type == PM_TEXT_PMID) ?
	pmLookupText(ident, PM_TEXT_HELP, &buffer) :
	pmLookupInDomText(ident, PM_TEXT_HELP, &buffer);
    if (sts == 0) {
	for (p = buffer; *p; p++)
	    ;
	while (p > buffer && p[-1] == '\n') {
	    p--;
	    *p = '\0';
	}
	if (*buffer != '\0') {
	    printf("Help:\n");
	    printf("%s", buffer);
	    putchar('\n');
	}
	else
	    printf("Help: <empty entry>\n");
	free(buffer);
    }
    else
	printf("Full Help: Error: %s\n", pmErrStr(sts));
}

static void
myoneline(unsigned int ident, int type)
{
    char	*buffer;
    int		sts;

    sts = (type == PM_TEXT_PMID) ?
	pmLookupText(ident, PM_TEXT_ONELINE, &buffer) :
	pmLookupInDomText(ident, PM_TEXT_ONELINE, &buffer);
    if (sts == 0) {
	if ((p_fullmid && type == PM_TEXT_PMID) ||
	    (p_fulliid && type == PM_TEXT_INDOM))
	    printf("\n    ");
	else
	    putchar(' ');
	printf("[%s]", buffer);
	free(buffer);
    }
    else
	printf(" One-line Help: Error: %s", pmErrStr(sts));
}

static void
myindomlabels(pmInDom indom)
{
    pmLabelSet	*labels[3] = {0}; /* context+domain+indom */
    char	buf[PM_MAXLABELJSONLEN];
    int		sts;

    labels[0] = lookup_context_labels();
    labels[1] = lookup_domain_labels(pmInDom_domain(indom));
    labels[2] = lookup_indom_labels(indom);
    sts = pmMergeLabelSets(labels, 3, buf, sizeof(buf), NULL, NULL);

    if (sts > 0)
	printf("    labels %s\n", buf);
    else if (sts < 0)
	fprintf(stderr, "%s: indom %s labels merge failed: %s\n",
		pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
}

static void
myinstancelabels(pmInDom indom, pmDesc *dp)
{
    pmLabelSet	*labels[6] = {0}; /* context+domain+indom+cluster+item+insts */
    pmLabelSet	*ilabels = NULL;
    char	*iname, buf[PM_MAXLABELJSONLEN];
    int		i = 0, j, n, inst, count, sts = 0;

    if (indom == PM_INDOM_NULL)
	return;

    /* prime the cache (if not done already) and get the instance count */
    if ((n = lookup_instance_nlabelset(indom)) <= 0)
	n = lookup_instance_numinst(indom);

    labels[i++] = lookup_context_labels();
    labels[i++] = lookup_domain_labels(pmInDom_domain(indom));
    labels[i++] = lookup_indom_labels(indom);
    labels[i++] = lookup_cluster_labels(dp->pmid);
    labels[i++] = lookup_item_labels(dp->pmid);

    for (j = 0; j < n; j++) {
	count = i;
	if ((ilabels = lookup_instance_labels(indom, j)) != NULL)
	    labels[count++] = ilabels;
	/* merge all the labels down to each leaf instance */
	sts = pmMergeLabelSets(labels, count, buf, sizeof(buf), 0, 0);
	if (sts > 0) {
	    inst = ilabels? ilabels->inst : lookup_instance_inum(indom, j);
	    if ((iname = lookup_instance_name(indom, inst)) == NULL)
		iname = "DISAPPEARED";
	    printf("    inst [%d or \"%s\"] labels %s\n", inst, iname, buf);
	} else if (sts < 0) {
	    fprintf(stderr, "%s: %s instances labels merge failed: %s\n",
			pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	}
    }
}

static void
mymetriclabels(pmDesc *dp)
{
    pmLabelSet	*labels[5] = {0}; /* context+domain+indom+cluster+item */
    char	buf[PM_MAXLABELJSONLEN];
    int		i = 0, sts = 0;

    labels[i++] = lookup_context_labels();
    labels[i++] = lookup_domain_labels(pmID_domain(dp->pmid));
    if (dp->indom != PM_INDOM_NULL)
	labels[i++] = lookup_indom_labels(dp->indom);
    labels[i++] = lookup_cluster_labels(dp->pmid);
    labels[i++] = lookup_item_labels(dp->pmid);
    sts = pmMergeLabelSets(labels, i, buf, sizeof(buf), NULL, NULL);
    if (sts > 0)
	printf("    labels %s\n", buf);
    else if (sts < 0)
	fprintf(stderr, "%s: metric %s labels merge failed: %s\n",
		    pmGetProgname(), pmIDStr(dp->pmid), pmErrStr(sts));
}

/* Input: 20-byte SHA1 hash, output: 40-byte representation */
static const char *
myhash(const unsigned char *hash, char *buffer, size_t buflen)
{
    int		nbytes, off;

    for (nbytes = off = 0; nbytes < 20; nbytes++)
	off += pmsprintf(buffer + off, buflen - off, "%02x", hash[nbytes]);
    buffer[40] = '\0';
    return buffer;
}

static unsigned char *
mysourcehash(unsigned char *hash, const char *labels)
{
    SHA1_CTX		shactx;
    const unsigned char	prefix[] = "{\"series\":\"source\",\"labels\":";
    const unsigned char	suffix[] = "}";

    SHA1Init(&shactx);
    SHA1Update(&shactx, prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)labels, strlen(labels));
    SHA1Update(&shactx, suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);
    return hash;
}

static unsigned char *
mymetrichash(unsigned char *hash, const char *name, pmDesc *desc, const char *labels)
{
    SHA1_CTX		shactx;
    char		buffer[PM_MAXLABELJSONLEN+256];

    pmsprintf(buffer, sizeof(buffer),
		"{\"series\":\"metric\",\"name\":\"%s\",\"labels\":%s,"
		 "\"semantics\":\"%s\",\"type\":\"%s\",\"units\":\"%s\"}",
		name, labels, pmSemStr(desc->sem), pmTypeStr(desc->type),
		pmUnitsStr(&desc->units));
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)buffer, strlen(buffer));
    SHA1Final(hash, &shactx);
    return hash;
}

unsigned char *
myinstancehash(unsigned char *hash, const char *labels, const char *instance)
{
    SHA1_CTX		shactx;
    char		buffer[PM_MAXLABELJSONLEN+512];

    pmsprintf(buffer, sizeof(buffer),
		"{\"series\":\"instance\",\"name\":\"%s\",\"labels\":%s}",
		instance ? instance : "???", labels ? labels : "null");
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)buffer, strlen(buffer));
    SHA1Final(hash, &shactx);
    return hash;
}

/* extract only the identifying labels (cull optional labels) */
static int
intrinsics(const pmLabel *label, const char *json, void *arg)
{
    if ((label->flags & PM_LABEL_OPTIONAL) != 0)
	return 0;
    return 1;
}

static void
mymetricseries(const char *name, pmDesc *dp)
{
    pmLabelSet		*labels[5] = {0};
    char		buf[PM_MAXLABELJSONLEN];
    char		hash[40+2];
    unsigned char	id[20], *idhash;
    int			sts;

    labels[0] = lookup_context_labels();
    sts = pmMergeLabelSets(labels, 1, buf, sizeof(buf), intrinsics, NULL);
    if (sts > 0) {
	idhash = mysourcehash(id, buf);
	printf("    Source: %s\n", myhash(idhash, hash, sizeof(hash)));
    } else if (sts < 0) {
	fprintf(stderr, "%s: context labels merge failed: %s\n",
		pmGetProgname(), pmErrStr(sts));
    }

    labels[0] = lookup_context_labels();
    labels[1] = lookup_domain_labels(pmID_domain(dp->pmid));
    labels[2] = lookup_indom_labels(dp->indom);
    labels[3] = lookup_cluster_labels(dp->pmid);
    labels[4] = lookup_item_labels(dp->pmid);
    sts = pmMergeLabelSets(labels, 5, buf, sizeof(buf), intrinsics, NULL);
    if (sts > 0) {
	idhash = mymetrichash(id, name, dp, buf);
	printf("    Series: %s\n", myhash(idhash, hash, sizeof(hash)));
    } else if (sts < 0) {
	fprintf(stderr, "%s: metric %s labels merge failed: %s\n",
		pmGetProgname(), pmIDStr(dp->pmid), pmErrStr(sts));
    }
}

static void
myinstanceseries(pmInDom indom)
{
    unsigned char	id[20], *idhash;
    pmLabelSet		*labels[4] = {0}, *ilabels = NULL;
    char		buffer[PM_MAXLABELJSONLEN], hash[64], *iname;
    int			i, n, sts, inst, count;

    if (indom == PM_INDOM_NULL)
	return;

    labels[0] = lookup_context_labels();
    labels[1] = lookup_domain_labels(pmInDom_domain(indom));
    labels[2] = lookup_indom_labels(indom);

    /* prime the cache (if not done already) and get the instance count */
    if ((n = lookup_instance_nlabelset(indom)) <= 0)
	n = lookup_instance_numinst(indom);

    for (i = 0; i < n; i++) {
	count = 3;
	if ((ilabels = lookup_instance_labels(indom, i)) != NULL)
	    labels[count++] = ilabels;
	/* merge all the labels down to each leaf instance */
	sts = pmMergeLabelSets(labels, count, buffer, sizeof(buffer), 0, 0);
	if (sts > 0) {
	    inst = ilabels ? ilabels->inst : lookup_instance_inum(indom, i);
	    iname = lookup_instance_name(indom, inst);
	    idhash = myinstancehash(id, buffer, iname);
	    printf("    inst [%d or \"%s\"] series %s\n",
			inst, iname ? iname : "DISAPPEARED",
			myhash(idhash, hash, sizeof(hash)));
	} else if (sts < 0) {
	    fprintf(stderr, "%s: %s instances labels merge failed: %s\n",
		    pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	}
    }
}

static const char *
mytypestr(int type)
{
    switch (type) {
    case PM_TYPE_32:
	return "32-bit int";
    case PM_TYPE_U32:
	return "32-bit unsigned int";
    case PM_TYPE_64:
	return "64-bit int";
    case PM_TYPE_U64:
	return "64-bit unsigned int";
    case PM_TYPE_FLOAT:
	return "float";
    case PM_TYPE_DOUBLE:
	return "double";
    case PM_TYPE_STRING:
	return "string";
    case PM_TYPE_AGGREGATE:
	return "aggregate";
    case PM_TYPE_AGGREGATE_STATIC:
	return "static aggregate";
    case PM_TYPE_EVENT:
	return "event record array";
    case PM_TYPE_HIGHRES_EVENT:
	return "highres event record array";
    case PM_TYPE_NOSUPPORT:
	return "Not Supported";
    default:
	break;
    }
    return "???";

}

static void
mydesc(pmDesc *desc)
{
    const char          *type;
    const char          *sem;
    const char          *units;
    char                strbuf[60];

    if (desc->pmid == PM_ID_NULL) {
	/* no metadata, reason reported earlier */
	printf("    Metadata unavailable\n");
	return;
    }

    printf("    Data Type: %s", (type = mytypestr(desc->type)));
    if (strcmp(type, "???") == 0)
	printf(" (%d)", desc->type);

    printf("  InDom: %s", pmInDomStr_r(desc->indom, strbuf, sizeof(strbuf)));
    if (p_fulliid)
	printf(" = %u =", desc->indom);
    printf(" 0x%x\n", desc->indom);

    printf("    Semantics: %s", (sem = pmSemStr(desc->sem)));
    if (strcmp(sem, "???") == 0)
        printf(" (%d)", desc->sem);

    units = pmUnitsStr_r(&desc->units, strbuf, sizeof(strbuf));
    if (*units == '\0')
        pmsprintf(strbuf, sizeof(strbuf), "none");
    printf("  Units: %s\n", units);
}

static void
report(void)
{
    int		i;
    int		sts;
    pmResult	*result = NULL;
    pmResult	*xresult = NULL;
    pmValueSet	*vsp = NULL;
    int		all_count;
    int		*all_inst;
    char	**all_names;
    int		batch = batchidx;

    if (batchidx == 0)
	return;

    /* Lookup names. 
     * Cull out names that were unsuccessfully looked up. 
     * However, it is unlikely to fail because names come from a
     * traverse PMNS, however dynamic metrics can mean the non-leaf
     * name is in the PMNS, but there are no descendent names.
     */
    if (need_pmid) {
	int	j;
	int	lsts;
        if ((sts = pmLookupName(batchidx, (const char **)namelist, pmidlist)) < 0) {
	    if (batchidx > 1)
		printf("%s...%s: pmLookupName: %s\n", namelist[0], namelist[batchidx-1], pmErrStr(sts));
	    else
		printf("%s: pmLookupName: %s\n", namelist[0], pmErrStr(sts));
	}

	/*
	 * cull any names with no PMID
	 */
	j = 0;
	for (i = 0; i < batchidx; i++) {
	    if (pmidlist[i] == PM_ID_NULL) {
		if (sts >= 0)  {
		    /*
		     * not reported above, get the real reason for this one
		     * not having a valid PMID
		     */
		    lsts = pmLookupName(1, (const char **)&namelist[i], &pmidlist[i]);
		    printf("%s: pmLookupName: %s\n", namelist[i], pmErrStr(lsts));
		}
	    }
	    else {
		/* assert(j <= i); */
		if (j != i) {
		    /*
		     * swap names, so that free() works at the end
		     * and shuffle PMIDs
		     */
		    char	*tmp;
		    tmp = namelist[j];
		    namelist[j] = namelist[i];
		    namelist[i] = tmp;
		    pmidlist[j] = pmidlist[i];
		}
		j++;
	    }
	}
	batch = j;
    }

    if (p_value || p_label || verify) {
	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    if ((sts = pmSetMode(PM_MODE_FORW, &opts.origin, NULL)) < 0) {
		fprintf(stderr, "%s: pmSetMode failed: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
	if ((sts = pmFetch(batch, pmidlist, &result)) < 0) {
	    for (i = 0; i < batch; i++)
		printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
	    goto done;
	}
    }

    if (p_desc || p_value || p_label || p_series || verify) {
	if (batch > 1) {
	    int		j;
	    int		lsts;
	    if ((sts = pmLookupDescs(batch, pmidlist, desclist)) < 0) {
		printf("%s...%s: pmLookupDescs: %s\n", namelist[0], namelist[batch-1], pmErrStr(sts));
		goto done;
	    }
	    /*
	     * optionally report any metrics with no metadata and remove
	     * them from the list ... for -f and -v reporting will be
	     * done after pmFetch() with error codes per metric
	     */
	    if (!p_value && !verify) {
		j = 0;
		for (i = 0; i < batch; i++) {
		    if (pmidlist[i] != PM_ID_NULL && desclist[i].pmid == PM_ID_NULL) {
			/* no metadata, find out why ...  */
			lsts = pmLookupDesc(pmidlist[i], &desclist[i]);
			printf("%s: pmLookupDesc: %s\n", namelist[i], pmErrStr(lsts));
			/* assert(j <= i); */
			if (j != i) {
			    /*
			     * swap names, so that free() works at the end
			     * and shuffle PMIDs
			     */
			    char	*tmp;
			    tmp = namelist[j];
			    namelist[j] = namelist[i];
			    namelist[i] = tmp;
			    pmidlist[j] = pmidlist[i];
			}
		    }
		    else
			j++;
		}
		batch = j;
	    }
	}
	else {
	    if (pmidlist[0] == PM_ID_NULL)
		goto done;
	    if ((sts = pmLookupDesc(pmidlist[0], &desclist[0])) < 0) {
		printf("%s: pmLookupDesc: %s\n", namelist[0], pmErrStr(sts));
		goto done;
	    }
	}
    }

    for (i = 0; i < batch; i++) {

	if (p_desc || p_help || p_value || p_label)
	    /* Not doing verify, output separator  */
	    putchar('\n');

	if (p_value || verify) {
	    vsp = result->vset[i];
	    if (p_force) {
		if (result->vset[i]->numval == PM_ERR_PROFILE) {
		    /* indom is non-enumerable; try harder */
		    if ((all_count = pmGetInDom(desclist[i].indom, &all_inst, &all_names)) > 0) {
			pmDelProfile(desclist[i].indom, 0, NULL);
			pmAddProfile(desclist[i].indom, all_count, all_inst);
			if (xresult != NULL) {
			    pmFreeResult(xresult);
			    xresult = NULL;
			}
			free(all_inst);
			free(all_names);
			if (opts.context == PM_CONTEXT_ARCHIVE) {
			    if ((sts = pmSetMode(PM_MODE_FORW, &opts.origin, NULL)) < 0) {
				fprintf(stderr, "%s: pmSetMode failed: %s\n", pmGetProgname(), pmErrStr(sts));
				exit(1);
			    }
			}
			if ((sts = pmFetch(1, &pmidlist[i], &xresult)) < 0) {
			    printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
			    continue;
			}
			vsp = xresult->vset[0];
			/* leave the profile in the default state */
			pmDelProfile(desclist[i].indom, 0, NULL);
			pmAddProfile(desclist[i].indom, 0, NULL);
		    }
		    else if (all_count == 0) {
			printf("%s: pmGetIndom: No instances?\n", namelist[i]);
			continue;
		    }
		    else {
			printf("%s: pmGetIndom: %s\n", namelist[i], pmErrStr(all_count));
			continue;
		    }
		}
	    }
	}

	if (verify) {
	    if (desclist[i].type == PM_TYPE_NOSUPPORT)
		printf("%s: Not Supported\n", namelist[i]);
	    else if (vsp->numval < 0)
		printf("%s: %s\n", namelist[i], pmErrStr(vsp->numval));
	    else if (vsp->numval == 0)
		printf("%s: No value(s) available\n", namelist[i]);
	    continue;
	}

	/* not verify mode - detailed reporting */
	printf("%s", namelist[i]);
	if (p_mid)
	    printf(" PMID: %s", pmIDStr(pmidlist[i]));
	if (p_fullmid)
	    printf(" = %u = 0x%x", pmidlist[i], pmidlist[i]);
	if (p_oneline)
	    myoneline(pmidlist[i], PM_TEXT_PMID);
	putchar('\n');
	if (p_desc)
	    mydesc(&desclist[i]);
	if (p_series)
	    mymetricseries(namelist[i], &desclist[i]);
	if (p_label)
	    mymetriclabels(&desclist[i]);
	if (p_help)
	    myhelptext(pmidlist[i], PM_TEXT_PMID);
	if (p_value)
	    mydump(&desclist[i], vsp, NULL);
	if (p_series)
	    myinstanceseries(desclist[i].indom);
	if (p_label)
	    myinstancelabels(desclist[i].indom, &desclist[i]);
    }

    if (result != NULL) {
	pmFreeResult(result);
	result = NULL;
    }
    if (xresult != NULL) {
	pmFreeResult(xresult);
	xresult = NULL;
    }

done:
    for (i = 0; i < batchidx; i++)
	free(namelist[i]);
    batchidx = 0;

    if (opts.context == PM_CONTEXT_HOST)
	pdusize = 5 * sizeof(__pmPDU);
}

static void
dometric(const char *name)
{
    if (*name == '\0') {
	printf("PMNS appears to be empty!\n");
	return;
    }

    namelist[batchidx]= strdup(name);
    if (namelist[batchidx] == NULL) {
	fprintf(stderr, "%s: namelist string malloc: %s\n", pmGetProgname(), osstrerror());
	exit(1);
    }

    batchidx++;
    if (opts.context == PM_CONTEXT_HOST) {
	/*
	 * pdu encodes length and name (rounded up to a __pmPDU boundary)
	 * ... 64K is the hard limit, so being conservative here
	 */
	pdusize += sizeof(__pmPDU) + PM_PDU_SIZE_BYTES(strlen(name));
	if (pdusize > 63*1024) {
	    if (pmDebugOptions.appl4)
		fprintf(stderr, "pdusize=%d reduce -b from %d to %d\n", pdusize, batchsize, batchidx);
	    batchsize = batchidx;
	}
    }
    if (batchidx >= batchsize)
	report();
}

static int
dopmid(pmID pmid)
{
    char	*name;
    int		sts;

    if ((sts = pmNameID(pmid, &name)) < 0)
	return sts;
    sts = pmTraversePMNS(name, dometric);
    free(name);
    return sts;
}

static int
doindom(pmInDom indom)
{
    if (verify)
	return 0;
    putchar('\n');
    printf("InDom: %s", pmInDomStr(indom));
    if (p_fulliid)
	printf(" = %u =", indom);
    printf(" 0x%x", indom);
    if (p_oneline)
	myoneline(indom, PM_TEXT_INDOM);
    putchar('\n');
    if (p_label)
	myindomlabels(indom);
    if (p_help)
	myhelptext(indom, PM_TEXT_INDOM);
    return 0;
}

static int
dodigit(const char *arg)
{
    int		domain, cluster, item, serial;

    if (sscanf(arg, "%u.%u.%u", &domain, &cluster, &item) == 3)
	return dopmid(pmID_build(domain, cluster, item));
    if (sscanf(arg, "%u.%u", &domain, &serial) == 2)
	return doindom(pmInDom_build(domain, serial));
    return PM_ERR_NAME;
}

/*
 * pminfo has a few options which do not follow the defacto standards
 */
static int
myoverrides(int opt, pmOptions *optsp)
{
    if (opt == 's' || opt == 't' || opt == 'T')
	return 1;	/* we've claimed these, inform pmGetOptions */
    return 0;
}

int
main(int argc, char **argv)
{
    int		a, c;
    int		sts;
    int		exitsts = 0;
    char	*source;
    char	*endnum;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'b':		/* batchsize */
		batchsize = (int)strtol(opts.optarg, &endnum, 10);
		if (*endnum != '\0') {
		    pmprintf("%s: -b requires numeric argument\n", pmGetProgname());
		    opts.errors++;
		}
		break;

	    case 'c':		/* global derived metrics config file */
		sts = pmLoadDerivedConfig(opts.optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: derived configuration(s) error: %s\n",
			    pmGetProgname(), pmErrStr(sts));
		    /* errors are not necessarily fatal ... */
		}
		break;

	    case 'd':
		p_desc = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'F':
		p_force = p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'f':
		p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'l':
		p_label = 1;
		need_context = 1;
		need_labels = 1;
		need_pmid = 1;
		break;

	    case 'M':
		p_fullmid = 1;
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'I':
		p_fulliid = 1;
		p_desc = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'm':
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'r':		/* per-context derived metric */
		num_reg++;
		if ((reg = (char **)realloc(reg, num_reg * sizeof(reg[0]))) == NULL) {
		    fprintf(stderr, "%s: reg[] realloc: %s\n", pmGetProgname(), osstrerror());
		    exit(1);
		}
		reg[num_reg-1] = opts.optarg;
		need_context = 1;
		break;

	    case 's':
		p_series = 1;
		need_context = 1;
		need_labels = 1;
		need_pmid = 1;
		break;

	    case 't':
		p_oneline = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'T':
		p_help = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'v':
		verify = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'x':
		events = p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;
	}
    }
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if (opts.context)
	need_context = 1;

    if (opts.context == PM_CONTEXT_ARCHIVE)
	/*
	 * for archives, one metric per batch and start at beginning of
	 * archive for each batch so metric will be found if it is in
	 * the archive
	 */
	batchsize = 1;

    if (opts.context == PM_CONTEXT_HOST)
	/*
	 * need to keep track of expected PDU size for names sent
	 * to pmcd ... if this exceeds 64K pmcd will refuse to answer
	 * and close the conneection (DoS protection)
	 * initially have header (2) + 1 each for # bytes in total,
	 * # status values and # names
	 */
	pdusize = 5 * sizeof(__pmPDU);

    if (verify)
	p_desc = p_mid = p_fullmid = p_help = p_oneline = p_value = p_force = p_label = 0;


    if ((namelist = (char **)malloc(batchsize * sizeof(char *))) == NULL) {
	fprintf(stderr, "%s: namelist malloc: %s\n", pmGetProgname(), osstrerror());
	exit(1);
    }

    if ((pmidlist = (pmID *)malloc(batchsize * sizeof(pmID))) == NULL) {
	fprintf(stderr, "%s: pmidlist malloc: %s\n", pmGetProgname(), osstrerror());
	exit(1);
    }

    if ((desclist = (pmDesc *)malloc(batchsize * sizeof(pmDesc))) == NULL) {
	fprintf(stderr, "%s: desclist malloc: %s\n", pmGetProgname(), osstrerror());
	exit(1);
    }

    if (!opts.nsflag)
	need_context = 1; /* for distributed PMNS as no PMNS file given */

    if (need_context) {
	if (opts.context == PM_CONTEXT_ARCHIVE)
	    source = opts.archives[0];
	else if (opts.context == PM_CONTEXT_HOST)
	    source = opts.hosts[0];
	else if (opts.context == PM_CONTEXT_LOCAL)
	    source = NULL;
	else {
	    opts.context = PM_CONTEXT_HOST;
	    source = "local:";
	}
	if ((sts = pmNewContext(opts.context, source)) < 0) {
	    if (opts.context == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmGetProgname(), source, pmErrStr(sts));
	    else if (opts.context == PM_CONTEXT_LOCAL)
		fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
			pmGetProgname(), pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
			pmGetProgname(), source, pmErrStr(sts));
	    exit(1);
	}
	contextid = sts;

	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    if (opts.nsflag) {
		/*
		 * loaded -n (or -N) namespace from the command line,
		 * so cull metrics not in the archive
		 */
		pmTrimNameSpace();
	    }
	    /* complete TZ and time window option (origin) setup */
	    if (pmGetContextOptions(contextid, &opts)) {
		pmflush();
		exit(1);
	    }
	}

	/*
	 * do any -r args now we have a PMAPI context
	 */
	if (num_reg > 0) {
	    int		i;
	    char	*errmsg;
	    char	*name;
	    char	*expr;
	    int		bad = 0;
	    int		seen_eq = 0;

	    for (i = 0; i < num_reg; i++) {
		expr = name = reg[i];
		while (*expr && *expr != '=' && *expr != ' ' && *expr != '\t')
		    expr++;
		if (*expr == '=')
		    seen_eq = 1;
		*expr++ = '\0';
		if (seen_eq == 0) {
		    while (*expr && (*expr == ' ' || *expr == '\t'))
			expr++;
		    if (*expr != '=') {
			fprintf(stderr, "%s: bad register string, no =\n", pmGetProgname());
			bad++;
			continue;
		    }
		    expr++;
		}
		while (*expr && (*expr == ' ' || *expr == '\t'))
		    expr++;
		if (*expr == '\0') {
		    fprintf(stderr, "%s: bad register string, no expr after =\n", pmGetProgname());
		    bad++;
		    continue;
		}
		if ((sts = pmAddDerivedMetric(name, expr, &errmsg)) < 0) {
		    fprintf(stderr, "%s: per-context derived metric registration error:\n%s",
			    pmGetProgname(), errmsg);
		    bad++;
		    free(errmsg);
		}
	    }
	    if (bad)
		exit(1);
	}
    }

    if (opts.optind >= argc) {
    	sts = pmTraversePMNS("", dometric);
	if (sts < 0) {
	    fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		exitsts = 1;
	}
    }
    else {
	for (a = opts.optind; a < argc; a++) {
	    if (isdigit((int)(argv[a][0])))
		sts = dodigit(argv[a]);
	    else
		sts = pmTraversePMNS(argv[a], dometric);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s: %s\n", argv[a], pmErrStr(sts));
		exitsts = 1;
	    }
	}
    }
    report();

    if (pmDebugOptions.appl5)
	__pmDumpPDUCnt(stderr);

//    if (need_context)
//	pmDestroyContext(contextid);

    exit(exitsts);
}
