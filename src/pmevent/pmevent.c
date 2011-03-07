/*
 * pmevent - event record dumper
 * (based on pmval)
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 * TODO
 *   +	-g and -p - nothing has been checked
 *   +	semantic checks between -i and pmParseMetricSpec results
 *   +	conditional pmGetInDomArchive() in lookup()
 *   +	add proper cache for metrics found _within_ event records, to
 *   	avoid calls to pmNameID and pmLookupDesc in mydump()
 */

#include "pmevent.h"

static int		amode = PM_MODE_FORW;		/* archive scan mode */
static pmTime		*pmtime;

char		*host;				/* original host */
char		*archive = NULL;		/* archive source */
int		ahtype = -1;			/* archive or host or local context */
struct timeval	now;				/* current reporting time */
struct timeval	first;				/* start reporting time */
struct timeval	last = {INT_MAX, 999999};	/* end reporting time */
struct timeval	delta;				/* sample interval */
long		samples;			/* number of samples */
int		gui;				/* set if -g */
int		port = -1;			/* pmtime port number from -p */
char		*rpt_tz;			/* timezone for pmtime */
pmTimeControls	controls;

metric_t	*metrictab = NULL;		/* metrics from cmd line */
int		nmetric = 0;
pmID		*pmidlist;

/* Fetch metric values. */
static int
getvals(pmResult **result)
{
    pmResult	*rp;
    int		sts;
    int		i;
    int		m;

    if (archive != NULL) {
	/*
	 * for archives read until we find a pmResult with at least
	 * one of the pmids we are after
	 */
	for ( ; ; ) {
	    sts = pmFetchArchive(&rp);
	    if (sts < 0)
		break;

	    if (rp->numpmid == 0)
		/* skip mark records */
		continue;

	    for (i = 0; i < rp->numpmid; i++) {
		for (m = 0; m < nmetric; m++) {
		    if (rp->vset[i]->pmid == metrictab[m].pmid)
			break;
		}
		if (m < nmetric)
		    break;
	    }
	    if (i == rp->numpmid) {
		pmFreeResult(rp);
		continue;
	    }
	}
    }
    else
	sts = pmFetch(nmetric, pmidlist, &rp);

    if (sts >= 0)
	*result = rp;

    return sts;
}

static void
timestep(struct timeval newdelta)
{
    /* time moved, may need to wait for previous value again */
    // TODO ?
}


/***************************************************************************
 * output
 ***************************************************************************/

/* Print parameter values as output header. */
static void
printhdr(void)
{
    char		timebuf[26];

    if (archive == NULL)
	printf("host:      %s\n", host);
    else {
	printf("archive:   %s\n", archive);
	printf("host:      %s\n", host);
	printf("start:     %s", pmCtime(&first.tv_sec, timebuf));
	if (last.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime(&last.tv_sec, timebuf));
    }

    /* sample count and interval */
    if (samples == ALL_SAMPLES) printf("samples:   all\n");
    else printf("samples:   %ld\n", samples);
    if (samples != ALL_SAMPLES && samples > 1 &&
	(ahtype != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP))
	printf("interval:  %1.2f sec\n", __pmtimevalToReal(&delta));
}

/*
 * cache all of the most recently requested
 * pmInDom ...
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
	numinst = pmGetInDom(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

static void myeventdump(pmValueSet *vsp);

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
	    if ((p = lookup(dp->indom, vp->inst)) == NULL)
		printf("    inst [%d]", vp->inst);
	    else
		printf("    inst [%d or \"%s\"]", vp->inst, p);
	}
	else
	    printf("   ");
	printf(" value ");
	pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	putchar('\n');
	if (dp->type == PM_TYPE_EVENT)
	    myeventdump(vsp);
    }
}

static void
myeventdump(pmValueSet *vsp)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;
    pmResult	**res;
    static pmID	pmid_flags;
    static pmID	pmid_missed;

    nrecords = pmUnpackEventRecords(vsp, &res);
    if (nrecords < 0) {
	fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	return;
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
	    __pmid_int(&pmid_flags)->item = 1;
	}
	sts = pmLookupName(1, &name_missed, &pmid_missed);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_missed, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_missed)->item = 1;
	}
    }

    for (r = 0; r < nrecords; r++) {
	__pmPrintStamp(stdout, &res[r]->timestamp);
	printf(" --- event record [%d] ", r);
	if (res[r]->numpmid == 0) {
	    printf(" ---\n");
	    printf("	No parameters\n");
	    continue;
	}
	if (res[r]->numpmid < 0) {
	    printf(" ---\n");
	    printf("	Error: illegal number of parameters (%d)\n",
			res[r]->numpmid);
	    continue;
	}
	flags = 0;
	for (p = 0; p < res[r]->numpmid; p++) {
	    pmValueSet	*xvsp = res[r]->vset[p];
	    int		sts;
	    pmDesc	desc;
	    char	*name;

	    if (pmNameID(xvsp->pmid, &name) >= 0) {
		if (p == 0) {
		    if (xvsp->pmid == pmid_flags) {
			flags = xvsp->vlist[0].value.lval;
			printf(" flags 0x%x", flags);
			printf(" (%s) ---\n", pmEventFlagsStr(flags));
			free(name);
			continue;
		    }
		    else
			printf(" ---\n");
		}
		if ((flags & PM_EVENT_FLAG_MISSED) &&
		    (p == 1) &&
		    (xvsp->pmid == pmid_missed)) {
		    printf("    ==> %d missed event records\n",
				xvsp->vlist[0].value.lval);
		    free(name);
		    continue;
		}
		printf("    %s\n", name);
		free(name);
	    }
	    else
		printf("	PMID: %s\n", pmIDStr(xvsp->pmid));
	    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0) {
		printf("	pmLookupDesc: %s\n", pmErrStr(sts));
		continue;
	    }
	    mydump(&desc, xvsp, "    ");
	}
    }
    if (nrecords >= 0)
	pmFreeEventResult(res);
}


/***************************************************************************
 * main
 ***************************************************************************/
int
main(int argc, char **argv)
{
    pmResult		*rp;		/* current values */
    int			forever;
    int			sts;
    int			j;
    int			m;

    __pmSetProgname(argv[0]);
    setlinebuf(stdout);

    doargs(argc, argv);

    if ((pmidlist = (pmID *)malloc(nmetric*sizeof(pmidlist[0]))) == NULL) {
	__pmNoMem("metrictab", nmetric*sizeof(pmidlist[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (m = 0 ; m < nmetric; m++)
	pmidlist[m] = metrictab[m].pmid;

    if (gui || port != -1) {
	/* set up pmtime control */
	pmtime = pmTimeStateSetup(&controls, ahtype, port, delta, now,
				    first, last, rpt_tz, host);
	controls.stepped = timestep;
	gui = 1;	/* we're using pmtime control from here on */
    }
    else if (ahtype == PM_CONTEXT_ARCHIVE) /* no time control, go it alone */
	pmTimeStateMode(amode, delta, &now);

    forever = (samples == ALL_SAMPLES || gui);

    printhdr();

    /* wait till time for first sample */
    if (archive == NULL)
	__pmtimevalPause(now);

    /* main loop fetching and printing sample values */
    while (forever || (samples-- > 0)) {
	if (gui)
	    pmTimeStateVector(&controls, pmtime);

	/* wait till time for sample */
	if (!gui && archive == NULL)
	    __pmtimevalSleep(delta);

	/* next sample */
	sts = getvals(&rp);
	if (gui)
	    pmTimeStateAck(&controls, pmtime);

	if (sts < 0) {
	    if (sts == PM_ERR_EOL && gui) {
		pmTimeStateBounds(&controls, pmtime);
		continue;
	    }
	    if (sts == PM_ERR_EOL)
		break;
	    if (archive == NULL)
		fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    else
		fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}

	if ((double)rp->timestamp.tv_sec + (double)rp->timestamp.tv_usec/1000000 >
	    (double)last.tv_sec + (double)last.tv_usec/1000000)
	    break;

	for (j = 0; j < rp->numpmid; j++) {
	    int		first = 1;
	    for (m = 0; m < nmetric; m++) {
		metric_t	*mp = &metrictab[m];
		if (rp->vset[j]->pmid == mp->pmid) {
		    if (rp->vset[j]->numval == 0)
			printf("No values available\n");
		    else if (rp->vset[j]->numval < 0)
			printf("Error: %s\n", pmErrStr(rp->vset[i]->numval));
		    else {
			int		v;
			int		i;
			for (v = 0; v < rp->vset[j]->numval; v++) {
			    for (i = 0; i < mp->ninst; i++) {
				if (rp->vset[j]->
			    if (mp->ninst == 0
		    // TODO instance filtering and one-trip timestamp +
		    // metric header reporting
		    if (gui || archive != NULL) {
			__pmPrintStamp(stdout, &rp->timestamp);
			printf("  ");
		    }
		    printf("%s: ", metrictab[m].name);
			// TODO need instance stuff here also
			myeventdump(rp->vset[j]);
		    break;
		}
	    }
	}
	pmFreeResult(rp);
    }

    return 0;
}
