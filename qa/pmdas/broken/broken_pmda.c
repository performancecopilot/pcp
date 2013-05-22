/*
 * Broken, a PMDA which is broken in several ways
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <time.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"

/*
 * Broken PMDA
 *
 * This PMDA is broken in several ways. This is used to test libpcp_pmda
 * error recovery and messages to the user. Not all the metrics are invalid,
 * and the metric id's are intentionally sparse to force non-direct mapping.
 *
 * The macro BUG_? can be used to turn certain bad things on which may
 * be fatal.
 *
 * BUG  Metrics
 *  *	broken.valid.one       	- returns num calls to fetch callback
 *  *   broken.valid.two	- has instance domain, fixed values
 *  *	broken.bad.type		- illegal data type
 *  *	broken.valid.three	- normal, fixed value
 *  *   broken.no.fetch		- no fetch method implemented for this metric
 *  *   broken.no.help		- no help text for this metric
 *  *   broken.no.shorthelp	- no short help on metric
 *  *   broken.no.longhelp	- no long help on metric
 *  *   broken.bad.semantics	- illegal semantics
 *  *   broken.bad.scale       	- illegal scale
 *  *	broken.no.pmns		- not defined in pmns
 *  *   broken.no.instfetch	- one instance not handled by fetch
 *  *	broken.no.instances	- instance domain with no instances
 *
 *  1   broken.bad.indom	- metric with undefined instance
 *  2                           - no instance domain supplied
 *  3                           - no metrics supplied
 *  4				- bad help file path
 *
 * The macro VERSION_? can be used to force this to be a PCP 1.X or 2.0 PMDA
 *
 *  1				- use pcp1.X headers and link with compat libs
 *  2				- use pcp2.o headers and latest libs
 */

/*
 * list of instances
 */

#ifndef BUG_2
static pmdaInstid _indom0[] = {
    { 0, "a" }, { 1, "b" }, { 2, "c" }
};
#endif

static pmdaInstid _indom1[] = {
    { 50, "x" }, { 10, "y" }, { 9, "z" }
};

/*
 * list of instace domains
 */

static pmdaIndom indomtab[] = {
#define INDOM_0		0
#ifndef BUG_2
    { INDOM_0, 3, _indom0 },
#endif
#define INDOM_1		7
    { INDOM_1, 3, _indom1 },
#define INDOM_2		8
    { INDOM_2, 0, (pmdaInstid *)0 }
};



#ifndef BUG_3

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* valid.one */
    { (void *)0, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* valid.two */
    { (void *)0, 
      { PMDA_PMID(0,1), PM_TYPE_U32, INDOM_0, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* bad.type */
    { (void *)0,
      { PMDA_PMID(0,5), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, PM_SEM_COUNTER,
	{ 0, 0, 0, 0, 0, 0 } }, },
/* valid.three */
    { (void *)0, 
      { PMDA_PMID(0,7), PM_TYPE_U32, INDOM_1, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* no.fetch */
    { (void *)0, 
      { PMDA_PMID(0,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* no.help */
    { (void *)0, 
      { PMDA_PMID(0,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* no.shorthelp */
    { (void *)0, 
      { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* no.longhelp */
    { (void *)0, 
      { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* no.isntfetch */
    { (void *)0, 
      { PMDA_PMID(0,13), PM_TYPE_U32, INDOM_1, PM_SEM_INSTANT, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* bad.semantics */
    { (void *)0, 
      { PMDA_PMID(1,1), PM_TYPE_U32, PM_INDOM_NULL, -1, 
        { 0, 1, 0, 0, PM_TIME_SEC, 0 } }, },
/* bad.scale */
    { (void *)0,
      { PMDA_PMID(1,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	{ 1, 0, 0, 10, 0, 0 } }, },
/* no.pmns */
    { (void *)0,
      { PMDA_PMID(1,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	{ 1, 0, 0, 10, 0, 0 } }, },
#ifdef BUG_1
/* bad.indom */
    { (void *)0,
      { PMDA_PMID(1,4), PM_TYPE_U32, 17, PM_SEM_COUNTER,
	{ 1, 0, 0, 10, 0, 0 } }, },
#endif
/* no.instances */
    { (void *)0,
      { PMDA_PMID(0,14), PM_TYPE_U32, INDOM_2, PM_SEM_COUNTER,
	{ 1, 0, 0, 10, 0, 0 } }, },
};

#endif

static int		_isDSO = 1;	/* =0 I am a daemon */
static char		*_logFile = "broken.log";
#if defined(VERSION_1)
static char		*_helpText = "pmdas/broken/broken_v1";
#else
static char		*_helpText = "pmdas/broken/broken_v2";
#endif

/*
 * callback provided to pmdaFetch
 */

static int
broken_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    static int	count = 0;
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    switch (idp->cluster) {
    case 0:
	switch (idp->item) {
	case 0:
	    atom->l = ++count;
	    break;
	case 1:
	    switch (inst) {
	    case 0:
		atom->l = 1;
		break;
	    case 1:
		atom->l = 2;
		break;
	    case 2:
		atom->l = 3;
		break;
	    default:
		return PM_ERR_INST;
	    }
	    break;
	    /* broken.bogus.one: metric not defined in table but is in pmns */
	case 2:
	    fprintf(stderr, 
		    "%s: Fetching metric 0.2 which does not exist\n",
		    pmProgname);
	    atom->l = 42;
	    break;
	    /* metric not defined in table or pmns */
	case 3:
	    fprintf(stderr, 
		    "%s: Fetching metric 0.3 which does not exist\n",
		    pmProgname);
	    atom->l = -1;
	    break;
	case 5:
	    atom->d = 3.14;
	    break;
	case 7:
	    switch (inst) {
	    case 50:
		atom->l = 44;
		break;
	    case 10:
		atom->l = 45;
		break;
	    case 9:
		atom->l = 46;
		break;
	    default:
		return PM_ERR_INST;
	    }
	    break;
	case 10:
	    atom->l = 55;
	    break;
	case 11:
	    atom->l = 66;
	    break;
	case 12:
	    atom->l = 77;
	    break;
	case 13:
	    switch (inst) {
	    case 50:
		atom->l = 44;
		break;
	    case 9:
		atom->l = 46;
		break;
	    default:
		return PM_ERR_INST;
	    }
	    break;
	case 14:
	    fprintf(stderr, "PMID %s requested with inst = %d\n", 
		    pmIDStr(mdesc->m_desc.pmid), inst);
	    return PM_ERR_VALUE;
	default:
	    return PM_ERR_PMID;
	}
	break;
    case 1:
	switch (idp->item) {
	case 1:
	    atom->l = 333;
	    break;
	case 2:
	    atom->l = 12345;
	    break;
	case 3:
	    atom->l = 4321;
	    break;
#ifdef BUG_4
	case 4:
	    fprintf(stderr, "Eeek! Should not be trying to get PMID %s\n",
		    pmIDStr(mdesc->m_desc.pmid));
	    break;
#endif
	default:
	    return PM_ERR_PMID;
	}
	break;
    default:
	return PM_ERR_PMID;
    }

    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */

void 
broken_init(pmdaInterface *dp)
{
#if defined(BUG_5)
    if (_isDSO)
	/*
	 * we don't grok PMDA_INTERFACE_77 ... 77 is arbitrary, just needs
	 * to be bigger than currently validand supported
	 */
#if defined(VERSION_1)
	pmdaDSO(dp, 77, "broken DSO", BROKEN, _helpText);
#else
	pmdaDSO(dp, 77, "broken DSO", _helpText);
#endif
#elif defined(VERSION_1)
    if (_isDSO)
	pmdaDSO(dp, PMDA_PROTOCOL_2, "broken DSO", BROKEN, _helpText);
#else
    if (_isDSO)
	pmdaDSO(dp, PMDA_INTERFACE_2, "broken DSO", _helpText);
#endif

    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, broken_fetchCallBack);

#if defined(BUG_2)
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
             metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
#elif defined(BUG_3)
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     (pmdaMetric *)0, -1);
#elif defined(BUG_4)
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
#else
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
#endif
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d N       set pmDebug debugging flag to N\n"
	  "  -D domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -h helpfile  get help text from helpfile rather then default path\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port    expect PMCD to connect on given inet port (number or name)\n"
	  "  -p         expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket  expect PMCD to connect on given unix domain socket\n"
	  "  -6 port    expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int			err = 0;
    pmdaInterface	desc;

    __pmSetProgname(argv[0]);
    _isDSO = 0;

#if defined(BUG_5)
    /*
     * we don't grok PMDA_INTERFACE_77 ... 77 is arbitrary, just needs
     * to be bigger than currently valid and supported
     */
    pmdaDaemon(&desc, 77, pmProgname, BROKEN, _logFile,
	       _helpText);
#elif defined(VERSION_1)
    pmdaDaemon(&desc, PMDA_PROTOCOL_2, pmProgname, BROKEN, _logFile,
	       _helpText);
#else
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, BROKEN, _logFile,
	       _helpText);
#endif
    
    if (desc.status != 0) {
	fprintf(stderr, "pmdaDaemon() failed!\n");
	exit(1);
    }

    if (pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:6:", &desc, &err) != EOF)
    	err++;
   
    if (err)
    	usage();

    pmdaOpenLog(&desc);
    broken_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
}
