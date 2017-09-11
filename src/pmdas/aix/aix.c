/*
 * AIX PMDA
 *
 * Collect performance data from the AIX kernel using libperfstat for
 * the most part.
 *
 * Copyright (c) 2012,2014 Red Hat.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <time.h>
#include "common.h"

static int	_isDSO = 1;
static char	mypath[MAXPATHLEN];
static char	*username;

/*
 * wrapper for pmdaFetch which primes the methods ready for
 * the next fetch
 * ... real callback is fetch_callback()
 */
static int
aix_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i;

    // TODO: this should only fetch metrics from "pmidlist"
    for (i = 0; i < methodtab_sz; i++) {
	methodtab[i].m_prefetch();
    }

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
aix_fetch_callback(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    metricdesc_t	*mdp;

    mdp = (metricdesc_t *)mdesc->m_user;
    return methodtab[mdp->md_method].m_fetch(mdesc, inst, atom);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
aix_init(pmdaInterface *dp)
{
    if (_isDSO) {
	int sep = __pmPathSeparator();
	pmsprintf(mypath, sizeof(mypath), "%s%c" "aix" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_3, "AIX DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.two.fetch = aix_fetch;
    pmdaSetFetchCallBack(dp, aix_fetch_callback);
    init_data(dp->domain);
    pmdaInit(dp, indomtab, indomtab_sz, metrictab, metrictab_sz);
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "aix" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_3, pmProgname, AIX, "aix.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    aix_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
