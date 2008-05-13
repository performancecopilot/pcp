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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

/*
 * Generic driver for a daemon-based PMDA
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "../domain.h"

extern void sample_init(pmdaInterface *);

static pmdaInterface	dispatch;

/*
 * simulate PMDA busy (not responding to PDUs)
 */
int
limbo(void)
{
    extern int not_ready;

    __pmSendError(dispatch.version.two.ext->e_outfd, PDU_BINARY, PM_ERR_PMDANOTREADY);
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
    else
	return 0;
}

/*
 * callback from pmdaMain
 */
static void
done(void)
{
    extern int	sample_done;

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
	  "  -u socket    expect PMCD to connect on given unix domain socket\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			errflag = 0;
    char		*p;
    char		helppath[MAXPATHLEN];
    extern char		*optarg;
    extern int		optind;
    extern int		_isDSO;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    _isDSO = 0;		/* if we get here, we are a daemon PMDA */

    snprintf(helppath, sizeof(helppath), "%s/pmdas/sample/help", pmGetConfig("PCP_VAR_DIR"));
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, SAMPLE, "sample.log", helppath);

    if (pmdaGetOpt(argc, argv, "D:d:i:l:pu:?", &dispatch, &errflag) != EOF)
	errflag++;

    if (errflag) {
	usage();
	/*NOTREACHED*/
    }

    pmdaOpenLog(&dispatch);
    sample_init(&dispatch);
    pmdaSetCheckCallBack(&dispatch, check);
    pmdaSetDoneCallBack(&dispatch, done);
    pmdaConnect(&dispatch);

    /*
     * Non-DSO agents should ignore gratuitous SIGHUPs, e.g. from xwsh
     * when launched by the PCP Tutorial!
     */
    signal(SIGHUP, SIG_IGN);

    pmdaMain(&dispatch);

    exit(0);
    /*NOTREACHED*/
}
