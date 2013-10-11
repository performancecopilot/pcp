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
 */

#include "pmevent.h"

static int		amode = PM_MODE_FORW;		/* archive scan mode */
static pmTime		*pmtime;

char		*host;				/* hostname according to pmGetContextHostName */
char		*archive;			/* archive source */
int		ahtype = -1;			/* archive or host or local context */
int		ctxhandle = -1;			/* handle for the active context */
int		verbose;			/* verbose diagnostic output */
struct timeval	now;				/* current reporting time */
struct timeval	first;				/* start reporting time */
struct timeval	last = {INT_MAX, 999999};	/* end reporting time */
struct timeval	delta;				/* sample interval */
long		samples;			/* number of samples */
int		gui;				/* set if -g */
int		port = -1;			/* pmtime port number from -p */
pmTimeControls	controls;

metric_t	*metrictab;			/* metrics from cmd line */
int		nmetric;
pmID		*pmidlist;

/* performance metrics in the hash list */
typedef struct {
    char	*name;		/* name of metric */
    pmDesc	desc;		/* metric description */
} hash_t;

/* Fetch metric values. */
static int
getvals(pmResult **result)
{
    pmResult	*rp = NULL;
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

	    /*
	     * scan for any of the metrics of interest ... keep skipping
	     * archive records until one found
	     */
	    for (i = 0; i < rp->numpmid; i++) {
		for (m = 0; m < nmetric; m++) {
		    if (rp->vset[i]->pmid == metrictab[m].pmid) {
			/* match */
			goto done;
		    }
		}
	    }
	    pmFreeResult(rp);
	    rp = NULL;
	}
    }
    else
	sts = pmFetch(nmetric, pmidlist, &rp);

done:
    if (sts >= 0)
	*result = rp;
    else if (rp)
    	pmFreeResult(rp);

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

    if (archive == NULL) {
        printf("host:      %s\n", host);
    } else {
	printf("archive:   %s\n", archive);
	printf("host:      %s\n", host);
	printf("start:     %s", pmCtime(&first.tv_sec, timebuf));
	if (last.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime(&last.tv_sec, timebuf));
    }

    /* sample count and interval */
    if (samples == ALL_SAMPLES) printf("samples:   all\n");
    else printf("samples:   %ld\n", samples);
    if (samples != ALL_SAMPLES && samples > 1 && ahtype != PM_CONTEXT_ARCHIVE)
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
	if (archive == NULL)
	    numinst = pmGetInDom(indom, &instlist, &namelist);
	else
	    numinst = pmGetInDomArchive(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

static void myeventdump(pmValueSet *, int);

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
	if (dp->type == PM_TYPE_AGGREGATE ||
	    dp->type == PM_TYPE_AGGREGATE_STATIC) {
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
	}
	else if (dp->type == PM_TYPE_EVENT)
	    /* odd, nested event type! */
	    myeventdump(vsp, j);
	else {
	    pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	    putchar('\n');
	}
    }
}

static void
myeventdump(pmValueSet *vsp, int idx)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;
    pmResult	**res;
    static pmID	pmid_flags;
    static pmID	pmid_missed;
    static __pmHashCtl	hash =  { 0, 0, NULL };

    nrecords = pmUnpackEventRecords(vsp, idx, &res);
    if (nrecords < 0) {
	printf(" pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
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
	printf("  ");
	__pmPrintStamp(stdout, &res[r]->timestamp);
	printf(" --- event record [%d]", r);
	if (res[r]->numpmid == 0) {
	    printf(" ---\n");
	    printf("    ==> No parameters\n");
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
	    pmValueSet		*xvsp = res[r]->vset[p];
	    int			sts;
	    __pmHashNode	*hnp;
	    hash_t		*hp;

	    if ((hnp = __pmHashSearch((unsigned int)xvsp->pmid, &hash)) == NULL) {
		/* first time for this pmid */
		hp = (hash_t *)malloc(sizeof(hash_t));
		if (hp == NULL) {
		    __pmNoMem("hash_t", sizeof(hash_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		if ((sts = pmNameID(xvsp->pmid, &hp->name)) < 0) {
		    printf("	%s: pmNameID: %s\n", pmIDStr(xvsp->pmid), pmErrStr(sts));
		    free(hp);
		    continue;
		}
		else {
		    if (xvsp->pmid != pmid_flags &&
			xvsp->pmid != pmid_missed &&
			(sts = pmLookupDesc(xvsp->pmid, &hp->desc)) < 0) {
			printf("	%s: pmLookupDesc: %s\n", hp->name, pmErrStr(sts));
			free(hp->name);
			free(hp);
			continue;
		    }
		    if ((sts = __pmHashAdd((unsigned int)xvsp->pmid, (void *)hp, &hash)) < 0) {
			printf("	%s: __pmHashAdd: %s\n", hp->name, pmErrStr(sts));
			free(hp->name);
			free(hp);
			continue;
		    }
		}
	    }
	    else
		hp = (hash_t *)hnp->data;

	    if (p == 0) {
		if (xvsp->pmid == pmid_flags) {
		    flags = xvsp->vlist[0].value.lval;
		    printf(" flags 0x%x", flags);
		    printf(" (%s) ---\n", pmEventFlagsStr(flags));
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
		continue;
	    }
	    mydump(hp->name, &hp->desc, xvsp);
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
    pmResult		*rp = NULL;	/* current values */
    int			forever;
    int			sts;
    int			j;
    int			m;

    __pmSetProgname(argv[0]);
    setlinebuf(stdout);

    doargs(argc, argv);

    pmidlist = (pmID *)malloc(nmetric*sizeof(pmidlist[0]));
    if (pmidlist == NULL) {
	__pmNoMem("metrictab", nmetric*sizeof(pmidlist[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (m = 0 ; m < nmetric; m++)
	pmidlist[m] = metrictab[m].pmid;

    if (gui || port != -1) {
	char	*rpt_tz;
	/* set up pmtime control */
	pmWhichZone(&rpt_tz);
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
	    for (m = 0; m < nmetric; m++) {
		metric_t	*mp = &metrictab[m];
		if (rp->vset[j]->pmid == mp->pmid) {
		    if (gui || archive != NULL) {
			__pmPrintStamp(stdout, &rp->timestamp);
			printf("  ");
		    }
		    if (rp->vset[j]->numval == 0) {
			if (verbose)
			    printf("%s: No values available\n", mp->name);
		    } else if (rp->vset[j]->numval < 0) {
			printf("%s: Error: %s\n", mp->name, pmErrStr(rp->vset[j]->numval));
		    } else {
			int	i;
			for (i = 0; i < rp->vset[j]->numval; i++) {
			    if (rp->vset[j]->vlist[i].inst == PM_IN_NULL)
				printf("%s:", mp->name);
			    else {
				int	k;
				char	*iname = NULL;
				if (mp->ninst > 0) {
				    for (k = 0; k < mp->ninst; k++) {
					if (mp->inst[k] == rp->vset[j]->vlist[i].inst) {
					    iname = mp->iname[k];
					    break;
					}
				    }
				}
				else {
				    /* all instances selected */
				    __pmHashNode	*hnp;
				    hnp = __pmHashSearch((unsigned int)rp->vset[j]->vlist[i].inst, &mp->ihash);
				    if (hnp == NULL) {
					if (archive != NULL)
					    sts = pmNameInDomArchive(mp->desc.indom, rp->vset[j]->vlist[i].inst, &iname);
					else
					    sts = pmNameInDom(mp->desc.indom, rp->vset[j]->vlist[i].inst, &iname);
					if (sts < 0) {
					    fprintf(stderr, "%s: pmNameInDom: %s[%d]: %s\n", pmProgname, mp->name, rp->vset[j]->vlist[i].inst, pmErrStr(sts));
					    exit(EXIT_FAILURE);
					}
					if ((sts = __pmHashAdd((unsigned int)rp->vset[j]->vlist[i].inst, (void *)iname, &mp->ihash)) < 0) {
					    printf("%s: __pmHashAdd: %s[%s (%d)]: %s\n", pmProgname, mp->name, iname, rp->vset[j]->vlist[i].inst, pmErrStr(sts));
					    exit(EXIT_FAILURE);
					}
				    }
				    else
					iname = (char *)hnp->data;
				}
				if (iname == NULL)
				    continue;
				printf("%s[%s]:", mp->name, iname);
			    }
			    myeventdump(rp->vset[j], i);
			}
		    }
		    break;
		}
	    }
	}
	pmFreeResult(rp);
    }

    return 0;
}
