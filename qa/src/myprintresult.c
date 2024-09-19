/*
 * dump _everything_ about a __pmResult
 *
 * Generously borrowed from pminfo/pminfo.c (for -fx) and
 * libpcp/src/util.c (for __pmPrintResult).
 *
 * Copyright (c) 2013-2018 Red Hat.
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmID	pmid_flags;
static pmID	pmid_missed;

static void print_valueset(FILE *, pmValueSet *, char *);

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
print_nparams(FILE *f, int numpmid)
{
    if (numpmid == 0) {
	fprintf(f, " ---\n");
	fprintf(f, "	No parameters\n");
	return -1;
    }
    if (numpmid < 0) {
	fprintf(f, " ---\n");
	fprintf(f, "	Error: illegal number of parameters (%d)\n", numpmid);
	return -1;
    }
    return 0;
}

static void
print_parameter(FILE *f, pmValueSet *xvsp, int index, int *flagsp)
{
    int		sts, flags = *flagsp;
    pmDesc	desc;
    char	**names;

    if ((sts = pmNameAll(xvsp->pmid, &names)) >= 0) {
	if (index == 0) {
	    if (xvsp->pmid == pmid_flags) {
		flags = *flagsp = xvsp->vlist[0].value.lval;
		fprintf(f, " flags 0x%x", flags);
		fprintf(f, " (%s) ---\n", pmEventFlagsStr(flags));
		free(names);
		return;
	    }
	    else
		fprintf(f, " ---\n");
	}
	if ((flags & PM_EVENT_FLAG_MISSED) &&
	    (index == 1) &&
	    (xvsp->pmid == pmid_missed)) {
	    fprintf(f, "        ==> %d missed event records\n",
			xvsp->vlist[0].value.lval);
	    free(names);
	    return;
	}
	//fprintf(f, "    ");
	//__pmPrintMetricNames(f, sts, names, " or ");
	//fprintf(f, " (%s)\n", pmIDStr(xvsp->pmid));
	free(names);
    }
    else
	fprintf(f, "	PMID: %s\n", pmIDStr(xvsp->pmid));
    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0)
	fprintf(f, "	pmLookupDesc: %s\n", pmErrStr(sts));
    else
	print_valueset(f, xvsp, "  ");
}

static void
print_events(FILE *f, pmValueSet *vsp, int inst, int highres)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;

    setup_event_derived_metrics();

    if (highres) {
	pmHighResResult	**hr;

	if ((nrecords = pmUnpackHighResEventRecords(vsp, inst, &hr)) < 0) {
	    fprintf(stderr, "%s: pmUnpackHighResEventRecords: %s\n",
		    pmGetProgname(), pmErrStr(nrecords));
	    return;
	}
	for (r = 0; r < nrecords; r++) {
	    fprintf(f, "    --- highres event record [%d] timestamp ", r);
	    pmPrintHighResStamp(f, &hr[r]->timestamp);
	    if (print_nparams(f, hr[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < hr[r]->numpmid; p++)
		print_parameter(f, hr[r]->vset[p], p, &flags);
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
	for (r = 0; r < nrecords; r++) {
	    fprintf(f, "    --- event record [%d] timestamp ", r);
	    pmPrintStamp(f, &res[r]->timestamp);
	    if (print_nparams(f, res[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < res[r]->numpmid; p++)
		print_parameter(f, res[r]->vset[p], p, &flags);
	}
	pmFreeEventResult(res);
    }
}

static void
print_valueset(FILE *f, pmValueSet *vsp, char *indent)
{
    char	*pmid;
    char	**names;
    int		j;
    pmDesc	desc;
    int		have_desc = 1;
    char	*p;
    int		n;
    char	errmsg[PM_MAXERRMSGLEN];
    char	strbuf[20];

    if (indent != NULL)
	fprintf(f, "%s", indent);

    pmid = pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf));
    if ((n = pmNameAll(vsp->pmid, &names)) < 0)
	fprintf(f, "  %s (%s):", pmid, "<noname>");
    else {
	fprintf(f, "  %s (", pmid);
	for (j = 0; j < n; j++) {
	    if (j > 0)
		fprintf(f, ", ");
	    fprintf(f, "%s", names[j]);
	}
	fprintf(f, "):");
	free(names);
    }
    if (vsp->numval == 0) {
	fprintf(f, " No values returned!\n");
	return;
    }
    if (vsp->numval < 0) {
	fprintf(f, " %s\n", pmErrStr_r(vsp->numval, errmsg, sizeof(errmsg)));
	return;
    }
    if (pmLookupDesc(vsp->pmid, &desc) < 0) {
	/* don't know, so punt on the most common cases */
	desc.indom = PM_INDOM_NULL;
	have_desc = 0;
    }

    fprintf(f, " numval: %d", vsp->numval);
    fprintf(f, " valfmt: %d vlist[]:\n", vsp->valfmt);

    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];

	if (indent != NULL)
	   fprintf(f, "%s", indent);
	if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
	    fprintf(f, "    inst [%d", vp->inst);
	    if (have_desc &&
		pmNameInDom(desc.indom, vp->inst, &p) >= 0) {
		fprintf(f, " or \"%s\"]", p);
		free(p);
	    }
	    else {
		fprintf(f, " or ???]");
	    }
	    fputc(' ', f);
	}
	else
	    fprintf(f, "   ");

	if (indent != NULL)
	   fprintf(f, "%s", indent);
	fprintf(f, "value ");

	if (have_desc) {
	    pmPrintValue(f, vsp->valfmt, desc.type, vp, 1);
	    fputc('\n', f);
	    if (desc.type == PM_TYPE_HIGHRES_EVENT)
		print_events(f, vsp, j, 1);
	    else if (desc.type == PM_TYPE_EVENT)
		print_events(f, vsp, j, 0);
	}
	else {
	    if (vsp->valfmt == PM_VAL_INSITU)
		pmPrintValue(f, vsp->valfmt, PM_TYPE_UNKNOWN, vp, 1);
	    else if (vsp->valfmt == PM_VAL_DPTR || vsp->valfmt == PM_VAL_SPTR)
		pmPrintValue(f, vsp->valfmt, (int)vp->value.pval->vtype, vp, 1);
	    else
		fprintf(f, "bad valfmt %d", vsp->valfmt);
	    fputc('\n', f);
	}
    }
}

void
myprintresult(FILE *f, __pmResult *rp)
{
    int		i;
    __pmCtlDebug(PM_CTL_DEBUG_SAVE);
    fprintf(f, "__pmResult timestamp: %" FMT_INT64 ".%09d ",
	rp->timestamp.sec, rp->timestamp.nsec);
    __pmPrintTimestamp(f, &rp->timestamp);
    fprintf(f, " numpmid: %d\n", rp->numpmid);
    for (i = 0; i < rp->numpmid; i++) {
	print_valueset(f, rp->vset[i], NULL);
    }
    __pmCtlDebug(PM_CTL_DEBUG_RESTORE);
}

