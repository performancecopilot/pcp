/*
 * Schizo PMDA (Version B)
 *
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "domain.h"

/*
 * Schizo PMDA
 *
 * This PMDA has several metadata changes between Version A and Version B.
 */

static pmdaInstid	myindom[] = { { 0, "one" } };

static pmdaIndom indomtab[] = {
#define MYINDOM	0
    { MYINDOM, sizeof(myindom)/sizeof(myindom[0]), myindom }
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* version */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* foo */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_STRING, MYINDOM, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* mumble */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_STRING, MYINDOM, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* data1 */
    { NULL, 
      { PMDA_PMID(1,1), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* data2 */
    { NULL, 
      { PMDA_PMID(1,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* data3 */
    { NULL, 
      { PMDA_PMID(1,3), PM_TYPE_32, MYINDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
/* data4 */
    { NULL, 
      { PMDA_PMID(1,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
};

/*
 * callback provided to pmdaFetch
 */
static int
schizo_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);

    pmNotifyErr(LOG_DEBUG, "schizo_fetch: %d.%d[%d]\n",
		  cluster, item, inst);

    if (cluster == 0) {
	switch (item) {
	case 0:					/* version */
	    atom->cp = "B";
	    break;

	case 2:					/* mumble */
	    atom->cp = "mumble";
	    break;

	case 3:					/* foo */
	    atom->cp = "foo";
	    break;

	default:
	    return PM_ERR_PMID;
	}
    }
    else if (cluster == 1) {
	switch(item) {
	case 1:					/* data1 */
	    atom->ll = (__int64_t)(inst*item);
	    break;

	case 2:					/* data2 */
	case 3:					/* data3 */
	case 4:					/* data4 */
	    atom->l = inst*item;
	    break;

	default:
	    return PM_ERR_PMID;
	}
    }
    else
	return PM_ERR_PMID;

    return 1;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
schizo_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, schizo_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmGetProgname());
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = pmPathSeparator();
    int			err = 0;
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath),
		"%s%c" "testsuite" "%c" "pmdas" "%c" "schizo" "%c" "help",
		pmGetConfig("PCP_VAR_DIR"), sep, sep, sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmGetProgname(), SCHIZO,
		"schizo.log", helppath);

    if (pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:6:?", &dispatch, &err) != EOF)
    	err++;

    if (err)
    	usage();

    pmdaOpenLog(&dispatch);
    schizo_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
