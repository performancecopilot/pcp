/*
 * Trivial, configurable PMDA
 *
 * Copyright (c) 2014 Red Hat.
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

/*
 * Trivial PMDA
 *
 * This PMDA is a sample that illustrates how a trivial PMDA might be
 * constructed using libpcp_pmda.
 *
 * Although the metrics supported are trivial, the framework is quite general,
 * and could be extended to implement a much more complex PMDA.
 *
 * Metrics
 *	trivial.time		- time in seconds since the 1st of Jan, 1970.
 */

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* time */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) } }
};

static char	*username;
static char	mypath[MAXPATHLEN];
static int	isDSO = 1;		/* ==0 if I am a daemon */

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * callback provided to pmdaFetch
 */
static int
trivial_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->cluster != 0 || idp->item != 0)
	return PM_ERR_PMID;
    else if (inst != PM_IN_NULL)
	return PM_ERR_INST;

    atom->ul = time(NULL);
    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
trivial_init(pmdaInterface *dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	pmsprintf(mypath, sizeof(mypath), "%s%c" "trivial" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_2, "trivial DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, trivial_fetchCallBack);

    pmdaInit(dp, NULL, 0, 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "trivial" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, TRIVIAL,
		"trivial.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    trivial_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
}
