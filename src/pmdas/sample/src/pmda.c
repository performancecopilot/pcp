/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * Generic driver for a daemon-based PMDA
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include "percontext.h"

extern void sample_init(pmdaInterface *);
extern int sample_done;
extern int not_ready;
extern int _isDSO;

static pmdaInterface dispatch;
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
 * simulate PMDA busy (not responding to PDUs)
 */
int
limbo(void)
{
    __pmSendError(dispatch.version.two.ext->e_outfd, FROM_ANON, PM_ERR_PMDANOTREADY);
    while (not_ready > 0)
	not_ready = sleep(not_ready);
    return PM_ERR_PMDAREADY;
}

/*
 * callback from pmdaMain
 */
static int
check(void)
{
    if (access("/tmp/sample.unavail", F_OK) == 0)
	return PM_ERR_AGAIN;
    return 0;
}

/*
 * callback from pmdaMain
 */
static void
done(void)
{
    if (sample_done)
	exit(0);
}

int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    char		helppath[MAXPATHLEN];
    char		*username;

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    pmsprintf(helppath, sizeof(helppath), "%s%c" "sample" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_LATEST, pmProgname, SAMPLE,
		"sample.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    pmdaOpenLog(&dispatch);
    if (opts.username)
	username = opts.username;
    __pmSetProcessIdentity(username);

    sample_init(&dispatch);
    pmdaSetCheckCallBack(&dispatch, check);
    pmdaSetDoneCallBack(&dispatch, done);
    pmdaConnect(&dispatch);

#ifdef HAVE_SIGHUP
    /*
     * Non-DSO agents should ignore gratuitous SIGHUPs, e.g. from a
     * terminal when launched by the PCP Tutorial!
     */
    signal(SIGHUP, SIG_IGN);
#endif

    pmdaMain(&dispatch);

    exit(0);
}
