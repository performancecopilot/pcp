/*
 * Simple, configurable PMDA
 *
 * Copyright (c) 2012-2014 Red Hat.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <sys/stat.h>

/*
 * Simple PMDA
 *
 * This PMDA is a sample that illustrates how a simple PMDA might be
 * constructed using libpcp_pmda.
 *
 * Although the metrics supported are simple, the framework is quite general,
 * and could be extended to implement a much more complex PMDA.
 *
 * Metrics
 *	simple.numfetch		- number of fetches from this PMDA,
 *				  may be re-set using pmStore
 *	simple.colors		- 3 instances ("red", "green" and "blue")
 *				  of a "saw-tooth" sequence
 *	simple.time.user	- time in seconds spent executing user code
 *	simple.time.sys		- time in seconds spent executing system code
 *	simple.now		- current time of day across a dynamically
 *				  re-configurable instance domain.
 */

/*
 * list of instances
 */

static pmdaInstid color[] = {
    { 0, "red" }, { 1, "green" }, { 2, "blue" }
};

/*
 * instance domains
 * COLOR_INDOM uses the classical indomtab[] method
 * NOW_INDOM uses the more recent pmdaCache methods, but also appears in
 * indomtab[] so that the initialization of the pmInDom and the pmDescs
 * in metrictab[] is completed by pmdaInit
 */

static pmdaIndom indomtab[] = {
#define COLOR_INDOM	0	/* serial number for "color" instance domain */
    { COLOR_INDOM, sizeof(color)/sizeof(color[0]), color },
#define NOW_INDOM	1	/* serial number for "now" instance domain */
    { NOW_INDOM, 0, NULL },
};

/* this is merely a convenience */
static pmInDom	*now_indom = &indomtab[NOW_INDOM].it_indom;

/*
 * All metrics supported in this PMDA - one table entry for each.
 * The 4th field specifies the serial number of the instance domain
 * for the metric, and must be either PM_INDOM_NULL (denoting a
 * metric that only ever has a single value), or the serial number
 * of one of the instance domains declared in the instance domain table
 * (i.e. in indomtab, above).
 */

static pmdaMetric metrictab[] = {
/* numfetch */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* color */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_32, COLOR_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* time.user */
    { NULL,
      { PMDA_PMID(1,2), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
/* time.sys */
    { NULL,
      { PMDA_PMID(1,3), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
      	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
/* now */
    { NULL,
      { PMDA_PMID(2,4), PM_TYPE_U32, NOW_INDOM, PM_SEM_INSTANT,
      	PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static int	numfetch = 0;		/* number of pmFetch operations */
static int	red = 0;		/* current red value */
static int	green = 100;		/* current green value */
static int	blue = 200;		/* current blue value */
static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;

/* data and function prototypes for dynamic instance domain handling */
static struct timeslice {
    int		tm_field;
    int		inst_id;
    char	*tm_name;
} timeslices[] = {
    { 0, 1, "sec" }, { 0, 60, "min" }, { 0, 3600, "hour" }
};
static int num_timeslices = sizeof(timeslices)/sizeof(timeslices[0]);

#define SIMPLE_BUFSIZE		256
static struct stat file_change;	/* has time of last configuration change */
static void simple_timenow_clear(void);
static void simple_timenow_init(void);
static void simple_timenow_refresh(void);
static void simple_timenow_check(void);

static char	mypath[MAXPATHLEN];

/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:i:l:pu:U:6:?",
    .long_options = longopts,
};

/*
 * callback provided to pmdaFetch
 */
static int
simple_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int			sts;
    static int		oldfetch;
    static double	usr, sys;
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (inst != PM_IN_NULL &&
	!(idp->cluster == 0 && idp->item == 1) &&
	!(idp->cluster == 2 && idp->item == 4))
	return PM_ERR_INST;

    if (idp->cluster == 0) {
	if (idp->item == 0) {			/* simple.numfetch */
	    atom->l = numfetch;
	}
	else if (idp->item == 1) {		/* simple.color */
	    switch (inst) {
	    case 0:				/* red */
		red = (red + 1) % 256;
	    	atom->l = red;
		break;
	    case 1:				/* green */
		green = (green + 1) % 256;
	    	atom->l = green;
		break;
	    case 2:				/* blue */
		blue = (blue + 1) % 256;
	    	atom->l = blue;
		break;
	    default:
		return PM_ERR_INST;
	    }
	}
	else
	    return PM_ERR_PMID;
    }
    else if (idp->cluster == 1) {		/* simple.time */
	if (oldfetch < numfetch) {
	    __pmProcessRunTimes(&usr, &sys);
	    oldfetch = numfetch;
	}
	if (idp->item == 2)			/* simple.time.user */
	    atom->d = usr;
	else if (idp->item == 3)      		/* simple.time.sys */
	    atom->d = sys;
	else
	    return PM_ERR_PMID;
     }
     else if (idp->cluster == 2) {
	if (idp->item == 4) {			/* simple.now */
	    struct timeslice *tsp;
	    if ((sts = pmdaCacheLookup(*now_indom, inst, NULL, (void *)&tsp)) != PMDA_CACHE_ACTIVE) {
		if (sts < 0)
		    __pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
		return PM_ERR_INST;
	    }
	    atom->l = tsp->tm_field;
	}
	else 
	    return PM_ERR_PMID;
    }
    else
	return PM_ERR_PMID;

    return 0;
}

/*
 * wrapper for pmdaFetch which increments the fetch count and checks for
 * a change to the NOW instance domain.
 *
 * This routine is called once for each pmFetch(3) operation, so is a
 * good place to do once-per-fetch functions, such as value caching or
 * instance domain evaluation (as we do in simple_timenow_check).
 */
static int
simple_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    numfetch++;
    simple_timenow_check();
    simple_timenow_refresh();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * wrapper for pmdaInstance which we need to ensure is called with the
 * _current_ contents of the NOW instance domain.
 */
static int
simple_instance(pmInDom indom, int foo, char *bar, __pmInResult **iresp, pmdaExt *pmda)
{
    simple_timenow_check();
    return pmdaInstance(indom, foo, bar, iresp, pmda);
}

/*
 * Re-evaluate the NOW instance domain.
 * 
 * Refer to the help text for simple.now for an explanation of how
 * this indom can be modified, or just read the code ...
 */
static void
simple_timenow_check(void)
{
    struct stat		statbuf;
    static int		last_error = 0;
    int			sep = __pmPathSeparator();

    /* stat the file & check modification time has changed */
    snprintf(mypath, sizeof(mypath), "%s%c" "simple" "%c" "simple.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    if (stat(mypath, &statbuf) == -1) {
	if (oserror() != last_error) {
	    last_error = oserror();
	    __pmNotifyErr(LOG_ERR, "stat failed on %s: %s\n",
			  mypath, pmErrStr(-last_error));
	}
	simple_timenow_clear();
    }
    else {
	last_error = 0;
#if defined(HAVE_ST_MTIME_WITH_E)
	if (statbuf.st_mtime != file_change.st_mtime) {
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	if (statbuf.st_mtimespec.tv_sec != file_change.st_mtimespec.tv_sec ||
		statbuf.st_mtimespec.tv_nsec != file_change.st_mtimespec.tv_nsec) {
#else
	if (statbuf.st_mtim.tv_sec != file_change.st_mtim.tv_sec ||
		statbuf.st_mtim.tv_nsec != file_change.st_mtim.tv_nsec) {
#endif
	    simple_timenow_clear();
	    simple_timenow_init();
	    file_change = statbuf;
	}
    }
}

/*
 * get values for time.now metric instances
 */
static void
simple_timenow_refresh(void)
{
    time_t	t = time(NULL);
    struct tm	*tptr;

    tptr = localtime(&t);
    timeslices[0].tm_field = tptr->tm_sec;
    timeslices[1].tm_field = tptr->tm_min;
    timeslices[2].tm_field = tptr->tm_hour;
}

/*
 * clear the time.now metric instance domain
 */
static void
simple_timenow_clear(void)
{
    int		sts;

    sts = pmdaCacheOp(*now_indom, PMDA_CACHE_INACTIVE);
    if (sts < 0)
	__pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	    pmInDomStr(*now_indom), pmErrStr(sts));
#ifdef DESPERATE
    __pmdaCacheDump(stderr, *now_indom, 1);
#endif
}

/* 
 * parse the configuration file for the time.now metric instance domain
 */
static void
simple_timenow_init(void)
{
    int		i;
    int		sts;
    int		sep = __pmPathSeparator();
    FILE	*fp;
    char	*p, *q;
    char	buf[SIMPLE_BUFSIZE];

    snprintf(mypath, sizeof(mypath), "%s%c" "simple" "%c" "simple.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    if ((fp = fopen(mypath, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "fopen on %s failed: %s\n",
		      mypath, pmErrStr(-oserror()));
	return;
    }
    if ((p = fgets(&buf[0], SIMPLE_BUFSIZE, fp)) == NULL) {
	__pmNotifyErr(LOG_ERR, "fgets on %s found no data\n", mypath);
	fclose(fp);
	return;
    }
    if ((q = strchr(p, '\n')) != NULL)
	*q = '\0';		/* remove eol character */

    q = strtok(p, ",");		/* and refresh using the updated file */
    while (q != NULL) {
	for (i = 0; i < num_timeslices; i++) {
	    if (strcmp(timeslices[i].tm_name, q) == 0) {
		sts = pmdaCacheStore(*now_indom, PMDA_CACHE_ADD, q, &timeslices[i]);
		if (sts < 0) {
		    __pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
		    fclose(fp);
		    return;
		}
		break;
	    }
	}
	if (i == num_timeslices)
	    __pmNotifyErr(LOG_WARNING, "ignoring \"%s\" in %s", q, mypath);
	q = strtok(NULL, ",");
    }
#ifdef DESPERATE
    __pmdaCacheDump(stderr, *now_indom, 1);
#endif
    if (pmdaCacheOp(*now_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
	__pmNotifyErr(LOG_WARNING, "\"timenow\" instance domain is empty");

    fclose(fp);
}

/*
 * support the storage of a value into the number of fetches count
 */
static int
simple_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		j;
    int		val;
    int		sts = 0;
    pmValueSet	*vsp = NULL;
    __pmID_int	*pmidp = NULL;

    /* a store request may affect multiple metrics at once */
    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster == 0) {	/* all storable metrics are cluster 0 */

	    switch (pmidp->item) {
	    	case 0:					/* simple.numfetch */
		    val = vsp->vlist[0].value.lval;
		    if (val < 0) {
			sts = PM_ERR_BADSTORE;
			val = 0;
		    }
		    numfetch = val;
		    break;

		case 1:					/* simple.color */
		    /* a store request may affect multiple instances at once */
		    for (j = 0; j < vsp->numval && sts == 0; j++) {

			val = vsp->vlist[j].value.lval;
			if (val < 0) {
			    sts = PM_ERR_BADSTORE;
			    val = 0;
			}
			if (val > 255) {
			    sts = PM_ERR_BADSTORE;
			    val = 255;
			}

			switch (vsp->vlist[j].inst) {
			    case 0:				/* red */
				red = val;
				break;
			    case 1:				/* green */
				green = val;
				break;
			    case 2:				/* blue */
				blue = val;
				break;
			    default:
				sts = PM_ERR_INST;
			}
		    }
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else if ((pmidp->cluster == 1 && 
		 (pmidp->item == 2 || pmidp->item == 3)) ||
		 (pmidp->cluster == 2 && pmidp->item == 4)) {
	    sts = PM_ERR_PERMISSION;
	    break;
	}
	else {
	    sts = PM_ERR_PMID;
	    break;
	}
    }
    return sts;
}


/*
 * Initialise the agent (both daemon and DSO).
 */
void 
simple_init(pmdaInterface *dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "simple" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_2, "simple DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.any.fetch = simple_fetch;
    dp->version.any.store = simple_store;
    dp->version.any.instance = simple_instance;

    pmdaSetFetchCallBack(dp, simple_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "simple" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, SIMPLE,
		"simple.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    simple_init(&dispatch);
    simple_timenow_check();
    pmdaMain(&dispatch);

    exit(0);
}
