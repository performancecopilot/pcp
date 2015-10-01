/*
 * perfevent PMDA
 *
 * Copyright (c) 2013 
 * Copyright (c) 2012 Red Hat.
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

#include "perfmanager.h"
#include "perfinterface.h"

/*
 * perfevent PMDA
 *
 * Metrics:
 *	perfevent.version
 *	        version number of this pmda
 *
 *	perfevent.hwcounters.{HWCOUNTER}.value
 *	        the value of the counter. Per-cpu counters have mulitple instances,
 *	        one for each CPU. Uncore/Northbridge counters only have one
 *	        instance.
 *
 *	perfevent.hwcounters.{HWCOUNTER}.dutycycle
 *	        the ratio of time that the counter configured to the time it was
 *	        active. This value will typically be 1.00, but could be less if
 *	        more counters were configured than available in the hardware and
 *	        the kernel module is time-multiplexing them.
 *
 */

/*
 * Name definitions
 */

#define PMDANAME "perfevent"

/*
 * Pointers to the perf_event instance and hardware counters
 */
static perfmanagerhandle_t *perfif;
static perf_counter *hwcounters;
static int nhwcounters;
static int activecounters;

/*
 * metrics information
 */
static int nummetrics;
static pmdaMetric *metrictab;

/*
 * Instance domain table
 */
static int numindoms;
static pmdaIndom *indomtab;

/*
 * PM namespace
 */
static __pmnsTree *pmns;

typedef struct dynamic_metric_info
{
    perf_counter *hwcounter;
    int		pmid_index;
    const char *help_text;
} dynamic_metric_info_t;
static dynamic_metric_info_t *dynamic_metric_infotab;

/*
 * Static metrics are always present independent of the number
 * of hardware counters configured
 */
static pmdaMetric static_metrictab[] =
{
    /* perfevent.version */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* perfevent.active */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } }
};

#define NUM_STATIC_METRICS (sizeof(static_metrictab)/sizeof(static_metrictab[0]))
#define NUM_STATIC_INDOMS 0
#define NUM_STATIC_CLUSTERS 1

/*
 * Default settings for the metric information. The values with comments next
 * to them are altered in the setup_metrictable() function
 */
static pmdaMetric default_metric_settings[] =
{
    /* perfevent.hwcounters.{HWCOUNTER}.value */
    {   NULL, /* m_user */ { 0 /* pmid */, PM_TYPE_64, 0 /* instance domain */,
            PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
        },
    },
    /* perfevent.hwcounters.{HWCOUNTER}.dutycycle */
    {   NULL, /* m_user */ { 0 /* pmid */, PM_TYPE_DOUBLE, 0 /* instance domain */,
            PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)
        },
    },
};

#define METRICSPERCOUNTER (sizeof(default_metric_settings)/sizeof(default_metric_settings[0]))

static const char *dynamic_nametab[] =
{
    /* perfevent.hwcounters.{HWCOUNTER}.value */
    "value",
    /* perfevent.hwcounters.{HWCOUNTER}.dutycycle */
    "dutycycle"
};

static const char *dynamic_helptab[] =
{
    /* perfevent.hwcounters.{HWCOUNTER}.value */
    "The values of the counter",
    /* perfevent.hwcounters.{HWCOUNTER}.dutycycle */
    "The ratio of the time that the hardware counter was enabled to the total run time"
};

static char mypath[MAXPATHLEN];
static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;
static int	compat_names = 0;

/*
 * \brief callback function that retrieves the metric value.
 *
 * \returns PM_ERR_PMID on error, 1 on success and 0 if the metric
 * was not found
 */
static int perfevent_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    if( NULL == mdesc )
    {
        return PM_ERR_PMID;
    }
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if(idp->cluster == 0)
    {
        if(idp->item == 0)
        {
            atom->cp = VERSION;
            return 1;
        }
        else if( idp->item == 1)
        {
            atom->l = activecounters;
            return 1;
        }
        else
        {
            return PM_ERR_PMID;
        }
    }
    else if(idp->cluster >= (nhwcounters + NUM_STATIC_CLUSTERS)  )
    {
        return PM_ERR_PMID;
    }

    dynamic_metric_info_t *pinfo = mdesc->m_user;

    if (pinfo == NULL)
    {
        return PM_ERR_PMID;
    }

    const perf_data *pdata = &(pinfo->hwcounter->data[inst]);

    switch(pinfo->pmid_index)
    {
    case 0:
        atom->ll = pdata->value;
        break;
    case 1:
        if(pdata->time_enabled > 0)
            atom->d = (pdata->time_running * 1.0) / pdata->time_enabled;
        else
            atom->d = 0.0;
        break;
    default:
        return 0;
    }

    return 1;
}

/*
 * store the instance profile away for the next fetch
 */
static int perfevent_profile(__pmProfile *prof, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return 0;
}

/*
 * This routine is called once for each pmFetch(3) operation, so this
 * is where the hardware counters are read.
 */
static int perfevent_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    activecounters = perf_get_r(perfif, &hwcounters, &nhwcounters);

    pmdaEventNewClient(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static void perfevent_end_contextCallBack(int context)
{
    pmdaEventEndClient(context);
}

static int perfevent_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreePMID(pmns, name, pmid);
}

static int perfevent_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeName(pmns, pmid, nameset);
}

static int perfevent_children(const char *name, int traverse, char ***kids, int **sts,
                              pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

/*
 * return the help text for the metric
 */
static int perfevent_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);

    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID)
    {
        /* Lookup pmid in the metric table. */
        int item = pmid_item(ident);

        /* bounds check item, ensure PMID matches and user data present */
        if (item >= 0 && item < nummetrics
                && metrictab[item].m_desc.pmid == (pmID)ident
                && metrictab[item].m_user != NULL)
        {
            dynamic_metric_info_t *pinfo = metrictab[item].m_user;

            *buffer = (char *)pinfo->help_text;
            return 0;
        }
    }

    return pmdaText(ident, type, buffer, pmda);
}

/* \brief Setup an instance domain
 * \param pindom pointer to the pmdaIndom struct
 * \param index the index of the instance domain
 * \param instances - how many instances
 * \returns void
 */
static void config_indom(pmdaIndom *pindom, int index, perf_counter *counter)
{
    int i;
    char cpuname[32];

    pindom->it_indom = index;
    pindom->it_numinst = counter->ninstances;
    pindom->it_set = calloc(counter->ninstances, sizeof(pmdaInstid) );

    for(i = 0; i < counter->ninstances; ++i)
    {
        sprintf(cpuname, "cpu%d", counter->data[i].id);
        pindom->it_set[i].i_inst = i;
        pindom->it_set[i].i_name = strdup(cpuname);
    }
}

/* \brief Initialise the perf events interface and read the counters
 *
 * Note this function needs the correct OS permissions to succeed. Either
 * run this when the UID is root or with the cap_sys_admin capability set
 *
 * \returns 0 on success -1 otherwise
 */
static int setup_perfevents()
{
    char buffer[MAXPATHLEN];
    const char *err_desc;
    int ret;
    int	sep = __pmPathSeparator();
    snprintf(buffer, sizeof(buffer), "%s%c" PMDANAME "%c" PMDANAME ".conf", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    perfif = manager_init(buffer);
    if( 0 == perfif )
    {
        __pmNotifyErr(LOG_ERR, "Unable to create perf instance\n");
        return -1;
    }

    ret = perf_get_r(perfif, &hwcounters, &nhwcounters);
    if( ret < 0 )
    {
        err_desc = perf_strerror(ret);
        __pmNotifyErr(LOG_ERR, "Error reading event counters perf_get returned %s\n",err_desc);
        return -1;
    }

    return 0;
}

static void teardown_perfevents()
{
    manager_destroy(perfif);
    perfif = 0;
    perf_counter_destroy(hwcounters, nhwcounters);
    hwcounters = 0;
    nhwcounters = 0;
}

/* \brief Initialise the metrics table.
 * This function must be called after setup_perfevents has enumerated the hardware
 * counter information.
 *
 * \returns 0 on success -1 otherwise
 */
static int setup_metrics()
{
    int i;
    int indom;
    int cluster;
    int index;

    nummetrics = (nhwcounters * METRICSPERCOUNTER) + NUM_STATIC_METRICS;
    numindoms = nhwcounters + NUM_STATIC_INDOMS;

    dynamic_metric_infotab = malloc( nhwcounters * METRICSPERCOUNTER * sizeof(*dynamic_metric_infotab) );
    metrictab              = malloc( nummetrics * sizeof(*metrictab) );
    indomtab               = malloc( numindoms * sizeof(*indomtab) );

    if( (NULL == dynamic_metric_infotab) || (NULL == metrictab) || (NULL == indomtab) )
    {
        __pmNotifyErr(LOG_ERR, "Error allocating memory for %d metrics (%d counters)\n",
                      nummetrics, nhwcounters);
        free(dynamic_metric_infotab);
        free(metrictab);
        free(indomtab);
        return -1;
    }

    memcpy(metrictab, static_metrictab, sizeof(static_metrictab) );

    dynamic_metric_info_t *pinfo = dynamic_metric_infotab;
    pmdaMetric *pmetric = &metrictab[NUM_STATIC_METRICS];

    for(i = 0; i < nhwcounters; ++i)
    {
        cluster = i + NUM_STATIC_CLUSTERS;

        /* For simplicity, a separate instance domain is setup for each hardware
         * counter */
        indom = i + NUM_STATIC_INDOMS;

        config_indom( &indomtab[indom], indom, &hwcounters[i]);

        /* Copy metric template settings. */
        memcpy(pmetric, default_metric_settings, sizeof(default_metric_settings));

        for(index = 0; index < METRICSPERCOUNTER; ++index)
        {
            /* Setup metric information (used within this PMDA) */
            pinfo->hwcounter = &hwcounters[i];
            pinfo->pmid_index = index;
            pinfo->help_text = dynamic_helptab[index];

            /* Initialize pmdaMetric settings (required by API) */
            pmetric->m_desc.pmid = PMDA_PMID( cluster, index);
            pmetric->m_desc.indom = indom;
            pmetric->m_user = pinfo;

            ++pinfo;
            ++pmetric;
        }
    }

    return 0;
}

/* \brief return a c-string that only contains allowable metrics characters.
 * The string is allocated by this function and must be freed by the caller.
 */
static char *normalize_metric_name(const char *name)
{
    char *res;
    char *p;

    res = strdup(name);

    /*
     * We can't control the names that libpfm returns. Replace any invalid
     * characters with underscore. Allow the old way for backwards compatability
     */

    if(compat_names)
    {
        for(p = strchr(res, ':'); p != NULL; p = strchr(p, ':') )
        {
            *p = '-'; /* "dash" - old name */
        }
    }
    else
    {
        for(p = res; *p != '\0'; p++)
        {
            if( !isalnum((int)*p) && *p != '_')
            {
                *p = '_'; /* "underscore" - new name */
            }
        }
    }
    return res;
}

/* \brief Create the dynamic PMNS tree and populate it.
 * This function should be called after all the metrics are configured and the
 * hardware events have be setup.
 *
 * \returns 0 on success -1 otherwise
 */
static int setup_pmns()
{
    int sts, i, j;
    char name[MAXPATHLEN * 2];
    pmdaMetric *pmetric;

    if ((sts = __pmNewPMNS(&pmns)) < 0)
    {
        __pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n", pmProgname, pmErrStr(sts));
        pmns = NULL;
        return -1;
    }

    pmetric = &metrictab[NUM_STATIC_METRICS];
    for (i = 0; i < nhwcounters; ++i)
    {
        char *id = normalize_metric_name(hwcounters[i].name);

        for (j = 0; j < METRICSPERCOUNTER; j++)
        {
            snprintf(name, sizeof(name),
                     PMDANAME ".hwcounters.%s.%s", id, dynamic_nametab[j]);
            __pmAddPMNSNode(pmns, pmetric[j].m_desc.pmid, name);
        }
        pmetric += METRICSPERCOUNTER;

        free(id);
    }

    /* for reverse (pmid->name) lookups */
    pmdaTreeRebuildHash(pmns, nummetrics);

    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void
perfevent_init(pmdaInterface *dp)
{
    if (isDSO)
    {
        int sep = __pmPathSeparator();
        snprintf(mypath, sizeof(mypath), "%s%c" PMDANAME "%c" "help", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_5, PMDANAME " DSO", mypath);
    }

    if (dp->status != 0)
    {
        return;
    }

    pmdaOpenLog(dp);

    if(setup_perfevents() < 0 )
    {
        return;
    }

    if(!isDSO)
    {
        __pmSetProcessIdentity(username);
    }

    if(setup_metrics() < 0 )
    {
        return;
    }

    dp->version.five.profile = perfevent_profile;
    dp->version.five.fetch = perfevent_fetch;
    dp->version.five.text = perfevent_text;
    dp->version.five.pmid = perfevent_pmid;
    dp->version.five.name = perfevent_name;
    dp->version.five.children = perfevent_children;

    pmdaSetFetchCallBack(dp, perfevent_fetchCallBack);
    pmdaSetEndContextCallBack(dp, perfevent_end_contextCallBack);

    pmdaInit(dp, indomtab, nhwcounters, metrictab, nummetrics);

    if(setup_pmns() < 0)
    {
        return;
    }

    __pmNotifyErr(LOG_INFO, "perfevent version " VERSION " initialised\n");
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
          "  -C           maintain compatability to (possibly) nonconforming metric names\n"
          "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
          "  -l logfile   write log into logfile rather than using default log name\n"
          "  -U username  user account to run under (default \"pcp\")\n"
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
int main(int argc, char **argv)
{
    int			c, err = 0;
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "perfevent" "%c" "help",
             pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmProgname, PERFEVENT,
               "perfevent.log", mypath);

    while ((c = pmdaGetOpt(argc, argv, "CD:d:i:l:pu:U:6:?", &dispatch, &err)) != EOF)
    {
        switch(c)
        {
        case 'C':
            compat_names = 1;
            break;
        case 'U':
            username = optarg;
            break;
        default:
            err++;
        }
    }
    if (err)
        usage();

    pmdaOpenLog(&dispatch);
    perfevent_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    teardown_perfevents();

    exit(0);
}
