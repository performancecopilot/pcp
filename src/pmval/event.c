/*
 * Copyright (c) 2011,2015 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmval.h"

static void myeventdump(pmValueSet *, int, int);

static pmID pmid_flags;
static pmID pmid_missed;

/*
 * Cache all of the most recently requested pmInDom
 */
static char *
lookup(pmInDom indom, int inst)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst = -1;
    static int		*instlist;
    static char		**namelist;
    int			i;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(namelist);
	}
	if (archive)
	    numinst = pmGetInDomArchive(indom, &instlist, &namelist);
	else
	    numinst = pmGetInDom(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

static void
mydump(const char *name, pmDesc *dp, pmValueSet *vsp)
{
    int		j;
    char	*p;

    if (vsp->numval == 0) {
	if (verbose)
	    printf("%s: No value(s) available!\n", name);
	return;
    }
    else if (vsp->numval < 0) {
	printf("%s: Error: %s\n", name, pmErrStr(vsp->numval));
	return;
    }

    printf("    %s", name);
    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (dp->indom != PM_INDOM_NULL) {
	    if (vsp->numval > 1)
		printf("\n        ");
	    if ((p = lookup(dp->indom, vp->inst)) == NULL)
		printf("[%d]", vp->inst);
	    else
		printf("[\"%s\"]", p);
	}
	putchar(' ');

	switch (dp->type) {
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC: {
	    /*
	     * pinched from pmPrintValue, just without the preamble of
	     * floating point values
	     */
	    char	*p;
	    int		i;
	    putchar('[');
	    p = &vp->value.pval->vbuf[0];
	    for (i = 0; i < vp->value.pval->vlen - PM_VAL_HDR_SIZE; i++, p++)
		printf("%02x", *p & 0xff);
	    putchar(']');
	    putchar('\n');
	    break;
	}
	case PM_TYPE_EVENT:
	case PM_TYPE_HIGHRES_EVENT:
	    /* odd, nested event type! */
	    myeventdump(vsp, j, dp->type != PM_TYPE_EVENT);
	    break;
	default:
	    pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	    putchar('\n');
	}
    }
}

static void
myvaluesetdump(pmValueSet *xvsp, int idx, int *flagsp)
{
    int			sts, flags = *flagsp;
    DescHash		*hp;
    __pmHashNode	*hnp;
    static __pmHashCtl	hash =  { 0, 0, NULL };

    if ((hnp = __pmHashSearch((unsigned int)xvsp->pmid, &hash)) == NULL) {
	/* first time for this pmid */
	hp = (DescHash *)malloc(sizeof(DescHash));
	if (hp == NULL) {
	    pmNoMem("DescHash", sizeof(DescHash), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	if ((sts = pmNameID(xvsp->pmid, &hp->name)) < 0) {
	    printf("	%s: pmNameID: %s\n", pmIDStr(xvsp->pmid), pmErrStr(sts));
	    free(hp);
	    return;
	}
	else {
	    if (xvsp->pmid != pmid_flags &&
		xvsp->pmid != pmid_missed &&
		(sts = pmLookupDesc(xvsp->pmid, &hp->desc)) < 0) {
		printf("	%s: pmLookupDesc: %s\n", hp->name, pmErrStr(sts));
		free(hp->name);
		free(hp);
		return;
	    }
	    if ((sts = __pmHashAdd((unsigned int)xvsp->pmid, (void *)hp, &hash)) < 0) {
		printf("	%s: __pmHashAdd: %s\n", hp->name, pmErrStr(sts));
		free(hp->name);
		free(hp);
		return;
	    }
	}
    }
    else
	hp = (DescHash *)hnp->data;

    if (idx == 0) {
	if (xvsp->pmid == pmid_flags) {
	    flags = *flagsp = xvsp->vlist[0].value.lval;
	    printf(" flags 0x%x", flags);
	    printf(" (%s) ---\n", pmEventFlagsStr(flags));
	    return;
	}
	else
	    printf(" ---\n");
    }
    if ((flags & PM_EVENT_FLAG_MISSED) &&
	(idx == 1) &&
	(xvsp->pmid == pmid_missed)) {
	printf("    ==> %d missed event records\n",
		    xvsp->vlist[0].value.lval);
	return;
    }
    mydump(hp->name, &hp->desc, xvsp);
}

static void
myeventdump(pmValueSet *vsp, int idx, int highres)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		flags;
    int		numpmid;
    int		nrecords;
    pmResult	**res = NULL;
    pmHighResResult **hres = NULL;

    if (highres) {
	if ((nrecords = pmUnpackHighResEventRecords(vsp, idx, &hres)) < 0) {
	    printf(" pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	    return;
	}
    }
    else {
	if ((nrecords = pmUnpackEventRecords(vsp, idx, &res)) < 0) {
	    printf(" pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	    return;
	}
    }
    printf(" %d event records\n", nrecords);

    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anonymous metrics
	 * should now be in the PMNS
	 */
	char	*name_flags = "event.flags";
	char	*name_missed = "event.missed";
	int	sts;

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

    for (r = 0; r < nrecords; r++) {
	printf("  ");
	if (highres) {
	    numpmid = hres[r]->numpmid;
	    pmPrintHighResStamp(stdout, &hres[r]->timestamp);
	}
	else {
	    numpmid = res[r]->numpmid;
	    pmPrintStamp(stdout, &res[r]->timestamp);
	}

	printf(" --- event record [%d]", r);
	if (numpmid == 0) {
	    printf(" ---\n");
	    printf("    ==> No parameters\n");
	    continue;
	}
	if (numpmid < 0) {
	    printf(" ---\n");
	    printf("	Error: illegal number of parameters (%d)\n", numpmid);
	    continue;
	}
	flags = 0;
	if (highres) {
	    for (p = 0; p < numpmid; p++)
		myvaluesetdump(hres[r]->vset[p], p, &flags);
	}
	else {
	    for (p = 0; p < numpmid; p++)
		myvaluesetdump(res[r]->vset[p], p, &flags);
	}
    }
    if (highres)
	pmFreeHighResEventResult(hres);
    if (res)
	pmFreeEventResult(res);
}

/* Print event performance metric values */
void
printevents(Context *x, pmValueSet *vset, int cols)
{
    int		i, sts, highres = (x->desc.type != PM_TYPE_EVENT);
    unsigned 	inst;

    for (i = 0; i < vset->numval; i++) {
	inst = (unsigned int)vset->vlist[i].inst;

	if (inst == PM_IN_NULL)
	    printf("%s:", x->metric);
	else {
	    int	k;
	    char *iname = NULL;

	    if (x->inum > 0) {
		for (k = 0; k < x->inum; k++) {
		    if (x->iids[k] == inst) {
			iname = x->inames[k];
			break;
		    }
		}
	    }
	    else {
		/* all instances selected */
		__pmHashNode	*hnp;

		hnp = __pmHashSearch(inst, &x->ihash);
		if (hnp == NULL) {
		    if (archive)
			sts = pmNameInDomArchive(x->desc.indom, inst, &iname);
		    else
			sts = pmNameInDom(x->desc.indom, inst, &iname);
		    if (sts < 0) {
			fprintf(stderr, "%s: pmNameInDom: %s[%u]: %s\n",
				pmGetProgname(), x->metric, inst, pmErrStr(sts));
			exit(EXIT_FAILURE);
		    }
		    if ((sts = __pmHashAdd(inst, (void *)iname, &x->ihash)) < 0) {
			fprintf(stderr, "%s: __pmHashAdd: %s[%s (%u)]: %s\n",
				pmGetProgname(), x->metric, iname, inst,
				pmErrStr(sts));
			exit(EXIT_FAILURE);
		    }
		}
		else
		    iname = (char *)hnp->data;
	    }
	    if (iname == NULL)
		continue;
	    printf("%s[%s]:", x->metric, iname);
	}
	myeventdump(vset, i, highres);
    }
}
