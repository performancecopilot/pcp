/*
 * Logger, configurable PMDA
 *
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
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
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include "percontext.h"
#include "event.h"

/*
 * Logger PMDA
 *
 * This PMDA is a sample that illustrates how a logger PMDA might be
 * constructed using libpcp_pmda.
 *
 * Although the metrics supported are logger, the framework is quite general,
 * and could be extended to implement a much more complex PMDA.
 *
 * Metrics
 *	logger.clients		- number of attached clients
 */

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
    { NULL, 
/* clients */
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* event.records */
    { NULL,
      { PMDA_PMID(PM_CLUSTER_EVENT,0), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* event.param_string */
    { NULL,
      { PMDA_PMID(0,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static char	mypath[MAXPATHLEN];
static int	isDSO = 1;		/* ==0 if I am a daemon */
char	       *monitor_path = NULL;

void
logger_end_contextCallBack(int ctx)
{
    __pmNotifyErr(LOG_INFO, "%s: saw context %d\n", __FUNCTION__, ctx);
    ctx_end(ctx);
}

static int
logger_profile(__pmProfile *prof, pmdaExt *ep)
{
//    (ep->e_context)
    __pmNotifyErr(LOG_INFO, "%s: saw context %d\n", __FUNCTION__, ep->e_context);
    ctx_start(ep->e_context);
    return 0;
}

/*
 * callback provided to pmdaFetch
 */
static int
logger_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int		rc;
    int		status = PMDA_FETCH_STATIC;

    __pmNotifyErr(LOG_INFO, "%s called\n", __FUNCTION__);
    if ((idp->cluster == 0 && (idp->item < 0 || idp->item > 1))
	|| (idp->cluster == PM_CLUSTER_EVENT && idp->item != 0)) {
	__pmNotifyErr(LOG_ERR, "%s: PM_ERR_PMID (cluster = %d, item = %d)\n",
		      __FUNCTION__, idp->cluster, idp->item);
	return PM_ERR_PMID;
    }
    else if (inst != PM_IN_NULL) {
	__pmNotifyErr(LOG_ERR, "%s: PM_ERR_INST (inst = %d)\n",
		      __FUNCTION__, inst);
	return PM_ERR_INST;
    }

    if (idp->cluster == 0) {
	switch(idp->item) {
	  case 0:
	    atom->ul = ctx_get_num();
	    break;
	  case 1:
	    status = PMDA_FETCH_NOVALUES;
	    break;
	  default:
	    __pmNotifyErr(LOG_ERR,
			  "%s: PM_ERR_PMID (cluster = %d, item = %d)\n",
			  __FUNCTION__, idp->cluster, idp->item);
	    return PM_ERR_PMID;
	}
    }
    else if (idp->cluster == PM_CLUSTER_EVENT) {
	switch(idp->item) {
	  case 0:
	    if ((rc = event_fetch(&atom->vbp)) != 0)
		return rc;
	    if (atom->vbp == NULL)
		status = PMDA_FETCH_NOVALUES;
	    break;
	  default:
	    __pmNotifyErr(LOG_ERR,
			  "%s: PM_ERR_PMID (cluster = %d, item = %d)\n",
			  __FUNCTION__, idp->cluster, idp->item);
	    return PM_ERR_PMID;
	}
    }

    return status;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
logger_init(pmdaInterface *dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "logger" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_5, "logger DSO", mypath);
    }

    if (dp->status != 0)
	return;

    dp->version.four.profile = logger_profile;

    pmdaSetFetchCallBack(dp, logger_fetchCallBack);
    pmdaSetEndContextCallBack(dp, logger_end_contextCallBack);

    pmdaInit(dp, NULL, 0, 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    event_init(dp, monitor_path);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -m logfile   logfile to monitor (required)\n",
	      stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			c;
    int			err = 0;
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);

    snprintf(mypath, sizeof(mypath), "%s%c" "logger" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_5, pmProgname, LOGGER,
		"logger.log", mypath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:m:?", &desc, &err)) != EOF) {
	switch (c) {
	  case 'm':
	    monitor_path = optarg;
	    break;
	  default:
	    err++;
	    break;
	}
    }
    if (err || monitor_path == NULL)
    	usage();

    pmdaOpenLog(&desc);
    logger_init(&desc);
    pmdaConnect(&desc);

#ifdef HAVE_SIGHUP
    /*
     * Non-DSO agents should ignore gratuitous SIGHUPs, e.g. from xwsh
     * when launched by the PCP Tutorial!
     */
    signal(SIGHUP, SIG_IGN);
#endif

    pmdaMain(&desc);

    exit(0);
}
