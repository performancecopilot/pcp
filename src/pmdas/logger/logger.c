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
#include <ctype.h>
#include <string.h>
#include "domain.h"
#include "percontext.h"
#include "event.h"
#include "util.h"

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
 *	logger.numclients			- number of attached clients
 *	logger.numlogfiles			- number of monitored logfiles
 *	logger.param_string			- string event data
 *	logger.perfile.{LOGFILE}.numclients	- number of attached
 *						  clients/logfile
 *	logger.perfile.{LOGFILE}.records	- event records/logfile
 */

static struct LogfileData *logfiles = NULL;
static int numlogfiles = 0;
static int nummetrics = 0;
static __pmnsTree *pmns;

struct dynamic_metric_info {
    int logfile;
    int pmid_index;
};
static struct dynamic_metric_info *dynamic_metric_infotab = NULL;

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric dynamic_metrictab[] = {
/* perfile.{LOGFILE}.numclients */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* perfile.{LOGFILE}.records */
    { NULL, 				/* m_user gets filled in later */
      { 0 /* pmid gets filled in later */, PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static char *dynamic_nametab[] = {
/* perfile.numclients */
    "numclients",
/* perfile.records */
    "records",
};

static pmdaMetric static_metrictab[] = {
/* numclients */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* numlogfiles */
    { NULL,
      { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* param_string */
    { NULL,
      { PMDA_PMID(0,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static pmdaMetric *metrictab = NULL;

static char	mypath[MAXPATHLEN];
static int	isDSO = 1;		/* ==0 if I am a daemon */
char	       *configfile = NULL;

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
    if (idp->cluster != 0 || (idp->item < 0 || idp->item > nummetrics)) {
	__pmNotifyErr(LOG_ERR, "%s: PM_ERR_PMID (cluster = %d, item = %d)\n",
		      __FUNCTION__, idp->cluster, idp->item);
	return PM_ERR_PMID;
    }

    if (idp->item < 3) {
	switch(idp->item) {
	  case 0:			/* logger.numclients */
	    atom->ul = ctx_get_num();
	    break;
	  case 1:			/* logger.numlogfiles */
	    atom->ul = numlogfiles;
	    break;
	  case 2:			/* logger.param_string */
	    status = PMDA_FETCH_NOVALUES;
	    break;
	  default:
	    __pmNotifyErr(LOG_ERR,
			  "%s: PM_ERR_PMID (inst = %d, cluster = %d, item = %d)\n",
			  __FUNCTION__, inst, idp->cluster, idp->item);
	    return PM_ERR_PMID;
	}
    }
    else {
	struct dynamic_metric_info *pinfo = ((mdesc != NULL) ? mdesc->m_user
					     : NULL);
	if (pinfo == NULL) {
	    __pmNotifyErr(LOG_ERR,
			  "%s: PM_ERR_PMID - bad pinfo (item = %d)\n",
			  __FUNCTION__, idp->item);
	    return PM_ERR_PMID;
	}

	switch(pinfo->pmid_index) {
	  case 0:	     /* logger.perfile.{LOGFILE}.numclients */
	    atom->ul = event_get_clients_per_logfile(pinfo->logfile);
	    break;
	  case 1:		/* logger.perfile.{LOGFILE}.records */
	    if ((rc = event_fetch(&atom->vbp, pinfo->logfile)) != 0)
		return rc;
	    if (atom->vbp == NULL)
		status = PMDA_FETCH_NOVALUES;
	    break;
	  default:
	    __pmNotifyErr(LOG_ERR,
			  "%s: PM_ERR_PMID (item = %d)\n", __FUNCTION__,
			  idp->item);
	    return PM_ERR_PMID;
	}
    }
    return status;
}

/* Ensure potential PMNS name can be used as a PCP namespace entry. */
static int
valid_pmns_name(char *name)
{
    if (!isalpha(name[0]))
	return 0;
    for (; *name != '\0'; name++)
	if (!isalnum(*name) && *name != '_')
	    return 0;
    return 1;
}

/*
 * Handle the config file.
 */
static int
read_config(const char *filename)
{
    FILE	       *configFile;
    struct LogfileData *data;
    int			rc = 0;
    size_t		len;
    char		line[MAXPATHLEN * 2];
    char	       *ptr, *name;

    configFile = fopen(filename, "r");
    if (configFile == NULL) {
	fprintf(stderr, "%s: %s: %s\n", __FUNCTION__, filename,
		strerror(errno));
	return -1;
    }

    while (! feof(configFile)) {
	if (fgets(line, sizeof(line), configFile) == NULL) {
	    if (feof(configFile)) {
		break;
	    }
	    else {
		fprintf(stderr, "%s: fgets failed: %s\n", __FUNCTION__,
			strerror(errno));
		rc = -1;
		break;
	    }
	}

	/* fgets() puts the '\n' at the end of the buffer.  Remove
	 * it.  If it isn't there, that must mean that the line is
	 * longer than our buffer. */
	len = strlen(line);
	if (len == 0) {			/* Ignore empty string. */
	    continue;
	}
	else if (line[len - 1] != '\n') { /* String must be too long */
	    fprintf(stderr, "%s: config file line too long: %s\n",
		    __FUNCTION__, line);
	    rc = -1;
	    break;
	}
	line[len - 1] = '\0';		/* Remove the '\n'. */

	/* Strip all trailing whitespace. */
	rstrip(line);

	/* If the string is now empty, just ignore the line. */
	len = strlen(line);
	if (len == 0) {
	    continue;
	}
	
	/* Skip past all leading whitespace to find the start of
	 * NAME. */
	name = lstrip(line);

	/* Now we need to split the line into 2 parts: NAME and
	 * PATHNAME.  NAME can't have whitespace in it, so look for
	 * the first non-whitespace. */
	ptr = name;
	while (*ptr != '\0' && ! isspace(*ptr)) {
	    ptr++;
	}
	/* If we're at the end, we never found any whitespace, so
	 * we've only got a NAME, with no PATHNAME. */
	if (*ptr == '\0') {
	    fprintf(stderr, "%s: badly formatted config file line: %s\n",
		    __FUNCTION__, line);
	    rc = -1;
	    break;
	}
	/* Terminate NAME at the 1st whitespace. */
	*ptr++ = '\0';

	/* Make sure NAME isn't too long. */
	if (strlen(name) > MAXPATHLEN) {
	    fprintf(stderr, "%s: NAME is too long: %s\n",
		    __FUNCTION__, name);
	    rc = -1;
	    break;
	}

	/* Make sure NAME is valid. */
	if (valid_pmns_name(name) == 0) {
	    fprintf(stderr, "%s: NAME isn't a valid PMNS name: %s\n",
		    __FUNCTION__, name);
	    rc = -1;
	    break;
	}

	/* Skip past any extra whitespace between NAME and PATHNAME */
	ptr = lstrip(ptr);

	/* Make sure PATHNAME (the rest of the line) isn't too long. */
	if (strlen(ptr) > MAXPATHLEN) {
	    fprintf(stderr, "%s: PATHNAME is too long: %s\n",
		    __FUNCTION__, ptr);
	    rc = -1;
	    break;
	}

	/* Now we've got a reasonable NAME and PATHNAME.  Save them. */
	numlogfiles++;
	logfiles = realloc(logfiles, numlogfiles * sizeof(struct LogfileData));
	if (logfiles == NULL) {
	    fprintf(stderr, "%s: realloc failed: %s\n", __FUNCTION__,
		    strerror(errno));
	    rc = -1;
	    break;
	}
	data = &logfiles[numlogfiles - 1];
	strncpy(data->pmns_name, name, sizeof(data->pmns_name));
	strncpy(data->pathname, ptr, sizeof(data->pathname));
	/* data->pmid_string gets filled in after pmdaInit() is called. */

	__pmNotifyErr(LOG_INFO, "%s: saw logfile %s (%s)\n", __FUNCTION__,
		      data->pathname, data->pmns_name);
    }
    if (rc != 0) {
	free(logfiles);
	logfiles = NULL;
	numlogfiles = 0;
    }

    fclose(configFile);
    return rc;
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

static int
logger_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmNotifyErr(LOG_INFO, "%s: name %s\n", __FUNCTION__,
		  (name == NULL) ? "NULL" : name);
    return pmdaTreePMID(pmns, name, pmid);
}

static int
logger_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmNotifyErr(LOG_INFO, "%s: pmid 0x%x\n", __FUNCTION__, pmid);
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
logger_children(const char *name, int traverse, char ***kids, int **sts,
		pmdaExt *pmda)
{
    __pmNotifyErr(LOG_INFO, "%s: name %s\n", __FUNCTION__,
		  (name == NULL) ? "NULL" : name);
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
logger_init(pmdaInterface *dp)
{
    int i, j, rc;
    int numstatics = sizeof(static_metrictab)/sizeof(static_metrictab[0]);
    int numdynamics = sizeof(dynamic_metrictab)/sizeof(dynamic_metrictab[0]);
    pmdaMetric *pmetric;
    int pmid_num;
    char name[MAXPATHLEN * 2];
    struct dynamic_metric_info *pinfo;

    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "logger" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_5, "logger DSO", mypath);
    }

    /* Read and parse config file. */
    if (read_config(configfile) != 0) {
	exit(1);
    }
    if (numlogfiles == 0) {
	usage();
    }

    /* Create the dynamic metric info table based on the logfile
     * table. */
    dynamic_metric_infotab = malloc(sizeof(struct dynamic_metric_info)
				    * numdynamics * numlogfiles);
    if (dynamic_metric_infotab == NULL) {
	fprintf(stderr, "%s: allocation error: %s\n", __FUNCTION__,
		strerror(errno));
	return;
    }
    pinfo = dynamic_metric_infotab;
    for (i = 0; i < numlogfiles; i++) {
	for (j = 0; j < numdynamics; j++) {
	    pinfo->logfile = i;
	    pinfo->pmid_index = j;
	    pinfo++;
	}
    }

    /* Create the metric table based on the static and dynamic metric
     * tables. */
    nummetrics = numstatics + (numlogfiles * numdynamics);
    metrictab = malloc(sizeof(pmdaMetric) * nummetrics);
    if (metrictab == NULL) {
	free(dynamic_metric_infotab);
	fprintf(stderr, "%s: allocation error: %s\n", __FUNCTION__,
		strerror(errno));
	return;
    }
    memcpy(metrictab, static_metrictab, sizeof(static_metrictab));
    pmetric = &metrictab[numstatics];
    pmid_num = numstatics;
    pinfo = dynamic_metric_infotab;
    for (i = 0; i < numlogfiles; i++) {
	memcpy(pmetric, dynamic_metrictab, sizeof(dynamic_metrictab));
	for (j = 0; j < numdynamics; j++) {
	    pmetric[j].m_desc.pmid = PMDA_PMID(0, pmid_num);
	    pmetric[j].m_user = pinfo++;
	    pmid_num++;
	}
	pmetric += numdynamics;
    }

    if (dp->status != 0)
	return;
    dp->version.four.profile = logger_profile;

    /* Dynamic PMNS handling. */
    dp->version.four.pmid = logger_pmid;
    dp->version.four.name = logger_name;
    dp->version.four.children = logger_children;
    /* FIXME: if we want to generate help text for the dynamic
     * metrics, we'll have to override 'four.text'. */

    pmdaSetFetchCallBack(dp, logger_fetchCallBack);
    pmdaSetEndContextCallBack(dp, logger_end_contextCallBack);

    pmdaInit(dp, NULL, 0, metrictab, nummetrics);

    /* Create the dynamic PMNS tree and populate it. */
    if ((rc = __pmNewPMNS(&pmns)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(rc));
	pmns = NULL;
	return;
    }
    pmetric = &metrictab[numstatics];
    for (i = 0; i < numlogfiles; i++) {
	for (j = 0; j < numdynamics; j++) {
	    snprintf(name, sizeof(name), "logger.perfile.%s.%s",
		     logfiles[i].pmns_name, dynamic_nametab[j]);
	    __pmAddPMNSNode(pmns, pmetric[j].m_desc.pmid, name);
	}
	pmetric += numdynamics;
    }
    pmdaTreeRebuildHash(pmns, (numlogfiles * numdynamics)); /* for reverse (pmid->name) lookups */

    /* Now that the metric table has been fully filled in, update
     * each LogfileData with the proper string pmid to use. */
    for (i = 0; i < numlogfiles; i++) {
	logfiles[i].pmid_string = metrictab[2].m_desc.pmid;
    }

    event_init(dp, logfiles, numlogfiles);
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

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:?", &desc, &err)) != EOF) {
	switch (c) {
	  default:
	    err++;
	    break;
	}
    }
    if (err || optind != argc -1) {
    	usage();
    }

    configfile = argv[optind];

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

    event_shutdown();
    exit(0);
}
