/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Generic driver for a daemon-based PMDA
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "../domain.h"
#include "percontext.h"

extern void sample_init(pmdaInterface *);
extern int sample_done;
extern int not_ready;

static pmdaInterface	dispatch;

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

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -U username  run under specified user account\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			c, errflag = 0;
    int			sep = __pmPathSeparator();
    char		helppath[MAXPATHLEN];
    char		*username = "pcp";
    extern int		_isDSO;

    _isDSO = 0;
    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "sample" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_LATEST, pmProgname, SAMPLE,
		"sample.log", helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:i:l:pu:U:?",
				&dispatch, &errflag)) != EOF) {
	switch (c) {
	case 'U':
	    username = optarg;
	    break;
	default:
	    errflag++;
	    break;
	}
    }
    if (errflag)
	usage();

    pmdaOpenLog(&dispatch);
    __pmSetProcessIdentity(username);

    sample_init(&dispatch);
    pmdaSetCheckCallBack(&dispatch, check);
    pmdaSetDoneCallBack(&dispatch, done);
    pmdaConnect(&dispatch);

#ifdef HAVE_SIGHUP
    /*
     * Non-DSO agents should ignore gratuitous SIGHUPs, e.g. from xwsh
     * when launched by the PCP Tutorial!
     */
    signal(SIGHUP, SIG_IGN);
#endif

    pmdaMain(&dispatch);

    exit(0);
}
