/*
 * perfevent PMDA
 *
 * Copyright (c) 2013 Joe White
 * Copyright (c) 2012,2016,2018,2019,2021 Red Hat.
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
#include "pmapi.h"
#include "pmda.h"
#include "domain.h"
#include <sys/stat.h>
#include <sys/resource.h>
#include <ctype.h>

#include "perfmanager.h"
#include "perfinterface.h"

/*
 * perfevent PMDA
 *
 * Metrics:
 *	perfevent.version
 *	        version number of this pmda
 *
 *	perfevent.active
 *	        number of active hardware counters
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
 *	perfevent.derived.active
 *	        number of derived counters
 *
 *	perfevent.derived.{DHWCOUNTER}.value
 *		similar to the above, but derivations based on values of the
 *		HWCOUNTER metrics as (optionally) specified in perfevent.conf
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
static perf_derived_counter *derived_counters;
static int nderivedcounters;
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
 * Performance metrics namespace
 */
static pmdaNameSpace *pmns;

typedef struct dynamic_metric_info
{
    perf_counter *hwcounter;
    perf_derived_counter *derived_counter;
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

static pmdaMetric static_derived_metrictab[] =
{
    /* perfevent.derived.active */
    { NULL, { PMDA_PMID(1,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } }
};

#define NUM_STATIC_DERIVED_METRICS (sizeof(static_derived_metrictab)/sizeof(static_derived_metrictab))
#define NUM_STATIC_DERIVED_INDOMS 0
#define NUM_STATIC_DERIVED_CLUSTERS 1

static pmdaMetric derived_metric_settings[] =
{
    /* perfevent.derived.{DERIVEDCOUNTER} */
    {   NULL, /* m_user */ { 0 /*pmid */, PM_TYPE_DOUBLE, 0 /* instance domain */,
            PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
        },
    },
};

#define METRICSPERCOUNTER (sizeof(default_metric_settings)/sizeof(default_metric_settings[0]))
#define METRICSPERDERIVED (sizeof(derived_metric_settings)/sizeof(derived_metric_settings[0]))

static const char *dynamic_nametab[] =
{
    /* perfevent.hwcounters.{HWCOUNTER,DERIVED}.value */
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

static const char *dynamic_derived_helptab[] =
{
    /* perfevent.hwcounters.{DERIVED}.value */
    "The values of the derived events"
};

static const char *dynamic_indom_helptab[] =
{
    /* per-CPU instance domains */
    "set of all processors"
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
    unsigned int cluster;
    unsigned int item;
    if( NULL == mdesc )
    {
        return PM_ERR_PMID;
    }

    cluster = pmID_cluster(mdesc->m_desc.pmid);
    item = pmID_item(mdesc->m_desc.pmid);

    if(cluster == 0)
    {
        if(item == 0)
        {
            atom->cp = VERSION;
            return 1;
        }
        else if( item == 1)
        {
            atom->l = activecounters;
            return 1;
        }
        else
        {
            return PM_ERR_PMID;
        }
    }
    if (cluster == 1)
    {
        if (item == 0)
        {
            atom->l = nderivedcounters;
            return 1;
        }
        else
        {
            return PM_ERR_PMID;
        }
    }
    else if(cluster >= (nderivedcounters + nhwcounters + NUM_STATIC_CLUSTERS + NUM_STATIC_DERIVED_CLUSTERS)  )
    {
        return PM_ERR_PMID;
    }

    dynamic_metric_info_t *pinfo = mdesc->m_user;

    if (pinfo == NULL)
    {
        return PM_ERR_PMID;
    }

    const perf_data *pdata = NULL;
    const perf_derived_data *pddata = NULL;

    if (cluster >= NUM_STATIC_DERIVED_CLUSTERS + NUM_STATIC_CLUSTERS + nhwcounters)
        pddata = &(pinfo->derived_counter->data[inst]);
    else if (pinfo->hwcounter->counter_disabled)
	return PM_ERR_VALUE;
    else
        pdata = &(pinfo->hwcounter->data[inst]);

    switch(pinfo->pmid_index)
    {
    case 0:
        if (pddata)
            atom->d = pddata->value;
        else
            atom->ll = pdata->value;
        break;
    case 1:
        if (pdata && pdata->time_enabled > 0)
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
static int perfevent_profile(pmProfile *prof, pmdaExt *pmda)
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
    activecounters = perf_get_r(perfif, &hwcounters, &nhwcounters, &derived_counters, &nderivedcounters);

    pmdaEventNewClient(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int perfevent_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    if (indom == PM_INDOM_NULL)
	return 0;
    return pmdaAddLabels(lp, "{\"cpu\":%u}", inst);
}

static int perfevent_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    if (type == PM_LABEL_INDOM && ident != PM_INDOM_NULL) {
	pmdaAddLabels(lpp, "{\"device_type\":\"cpu\"}");
	pmdaAddLabels(lpp, "{\"indom_name\":\"per cpu\"}");
    }
    pmdaEventNewClient(pmda->e_context);
    return pmdaLabel(ident, type, lpp, pmda);
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
        int i;

	/* Single static metric below the dynamic 'derived' namespace */
	if (pmID_cluster(ident) == 1 && pmID_item(ident) == 0) {
            *buffer = "The number of derived metrics configured";
            return 0;
	}

        /* Lookup pmid in the metric table. */
	for (i = 0; i < nummetrics; i++)
	{
            dynamic_metric_info_t *pinfo = metrictab[i].m_user;

	    if (!pinfo || metrictab[i].m_desc.pmid != (pmID)ident)
		continue;

            *buffer = (char *)pinfo->help_text;
            return 0;
        }
    }

    if ((type & PM_TEXT_INDOM) == PM_TEXT_INDOM)
    {
	if ((pmInDom)ident != PM_INDOM_NULL) {
	    *buffer = (char *)dynamic_indom_helptab[0];
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
        pmsprintf(cpuname, sizeof(cpuname), "cpu%d", counter->data[i].id);
        pindom->it_set[i].i_inst = i;
        pindom->it_set[i].i_name = strdup(cpuname);
    }
}

static void config_indom_derived(pmdaIndom *pindom, int index, perf_derived_counter *derived_counter)
{
    int i;
    char cpuname[32];

    pindom->it_indom = index;
    pindom->it_numinst = derived_counter->ninstances;
    pindom->it_set = calloc(derived_counter->ninstances, sizeof(pmdaInstid) );

    for(i = 0; i < derived_counter->ninstances; ++i)
    {
        pmsprintf(cpuname, sizeof(cpuname), "cpu%d", derived_counter->counter_list->counter->data[i].id);
        pindom->it_set[i].i_inst = i;
        pindom->it_set[i].i_name = strdup(cpuname);
    }
}

/* \brief set number of open files allowed to maximum possible.
 *
 * Note this PMDA may open one file per-event per-CPU so attempt
 * transparently increasing the allowed open files to the kernel
 * enforced hard limit.
 *
 * \returns void
 */
static void set_rlimit_maxfiles()
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
	limit.rlim_cur = limit.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
	    pmNotifyErr(LOG_ERR, "Cannot %s open file limits\n", "adjust");
    } else {
	pmNotifyErr(LOG_ERR, "Cannot %s open file limits\n", "get");
    }
}

static long get_rlimit_maxfiles()
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == 0)
	return limit.rlim_cur;
    return -1;
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
    int	sep = pmPathSeparator();
    pmsprintf(buffer, sizeof(buffer), "%s%c" PMDANAME "%c" PMDANAME ".conf", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    set_rlimit_maxfiles();

    perfif = manager_init(buffer);
    if( 0 == perfif )
    {
        pmNotifyErr(LOG_ERR, "Unable to create perf instance\n");
        return -1;
    }

    ret = perf_get_r(perfif, &hwcounters, &nhwcounters, &derived_counters, &nderivedcounters);
    if( ret < 0 )
    {
        err_desc = perf_strerror(ret);
        pmNotifyErr(LOG_ERR, "Error reading event counters perf_get returned %s\n",err_desc);
        return -1;
    }

    return 0;
}

static void teardown_perfevents()
{
    manager_destroy(perfif);
    perfif = 0;
    perf_counter_destroy(hwcounters, nhwcounters, derived_counters, nderivedcounters);
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
    nummetrics += (nderivedcounters * METRICSPERDERIVED) + NUM_STATIC_DERIVED_METRICS;
    numindoms = nderivedcounters + nhwcounters + NUM_STATIC_INDOMS + NUM_STATIC_DERIVED_INDOMS;

    dynamic_metric_infotab = malloc( ((nhwcounters * METRICSPERCOUNTER)
                                      + (nderivedcounters * METRICSPERDERIVED))
                                     * sizeof(*dynamic_metric_infotab) );
    metrictab              = malloc( nummetrics * sizeof(*metrictab) );
    indomtab               = malloc( numindoms * sizeof(*indomtab) );

    if( (NULL == dynamic_metric_infotab) || (NULL == metrictab) || (NULL == indomtab) )
    {
        pmNotifyErr(LOG_ERR, "Error allocating memory for %d metrics (%d counters)\n",
                      nummetrics, nhwcounters);
        free(dynamic_metric_infotab);
        free(metrictab);
        free(indomtab);
        return -1;
    }

    memcpy(metrictab, static_metrictab, sizeof(static_metrictab) );
    pmdaMetric *pmetric = &metrictab[NUM_STATIC_METRICS];

    memcpy(pmetric, static_derived_metrictab, sizeof(static_derived_metrictab));
    pmetric += NUM_STATIC_DERIVED_METRICS;

    dynamic_metric_info_t *pinfo = dynamic_metric_infotab;

    for(i = 0; i < nhwcounters; ++i)
    {
        cluster = i + NUM_STATIC_CLUSTERS + NUM_STATIC_DERIVED_CLUSTERS;

        /* For simplicity, a separate instance domain is setup for each hardware
         * counter */
        indom = i + NUM_STATIC_INDOMS + NUM_STATIC_DERIVED_INDOMS;

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

    for (i = 0; i < nderivedcounters; i++)
    {
        cluster = i + nhwcounters + NUM_STATIC_CLUSTERS + NUM_STATIC_DERIVED_CLUSTERS;
        indom = i + nhwcounters + NUM_STATIC_INDOMS + NUM_STATIC_DERIVED_INDOMS;

        config_indom_derived( &indomtab[indom], indom, &derived_counters[i]);

        memcpy(pmetric, derived_metric_settings, sizeof(derived_metric_settings));
        for(index = 0; index < METRICSPERDERIVED; index++)
        {
            /* Setup metrics info (used within this PMDA) */
            pinfo->derived_counter = &derived_counters[i];
            pinfo->pmid_index = index;
            pinfo->help_text = dynamic_derived_helptab[index];

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
     * characters with underscore. Allow the old way for backwards compatibility
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
            if( !isalnum((int)*p) && *p != '_' && *p != '.')
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

    if ((sts = pmdaTreeCreate(&pmns)) < 0)
    {
        pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n", pmGetProgname(), pmErrStr(sts));
        pmns = NULL;
        return -1;
    }

    pmetric = &metrictab[NUM_STATIC_METRICS];

    /* Setup for derived static metrics */
    pmsprintf(name, sizeof(name), PMDANAME ".derived.%s", "active");
    pmdaTreeInsert(pmns, pmetric[0].m_desc.pmid, name);

    pmetric += NUM_STATIC_DERIVED_METRICS;

    /* Now setup the dynamic metrics */
    for (i = 0; i < nhwcounters; ++i)
    {
        char *id = normalize_metric_name(hwcounters[i].name);

        for (j = 0; j < METRICSPERCOUNTER; j++)
        {
            pmsprintf(name, sizeof(name),
                     PMDANAME ".hwcounters.%s.%s", id, dynamic_nametab[j]);
            pmdaTreeInsert(pmns, pmetric[j].m_desc.pmid, name);
        }
        pmetric += METRICSPERCOUNTER;
        free(id);
    }

    for (i = 0; i < nderivedcounters; ++i)
    {
        char *id = normalize_metric_name(derived_counters[i].name);
        for (j = 0; j < METRICSPERDERIVED; j++)
        {
            pmsprintf(name, sizeof(name),
                     PMDANAME ".derived.%s.%s", id, dynamic_nametab[j]);
            pmdaTreeInsert(pmns, pmetric[j].m_desc.pmid, name);
        }
        pmetric += METRICSPERDERIVED;
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
__PMDA_INIT_CALL
perfevent_init(pmdaInterface *dp)
{
    if (isDSO)
    {
        int sep = pmPathSeparator();
        pmsprintf(mypath, sizeof(mypath), "%s%c" PMDANAME "%c" "help", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_7, PMDANAME " DSO", mypath);
    }

    if (dp->status != 0)
    {
        return;
    }

    if(setup_perfevents() < 0 )
    {
        return;
    }

    if(!isDSO)
    {
        pmSetProcessIdentity(username);
    }

    if(setup_metrics() < 0 )
    {
        return;
    }

    dp->version.seven.profile = perfevent_profile;
    dp->version.seven.fetch = perfevent_fetch;
    dp->version.seven.label = perfevent_label;
    dp->version.seven.text = perfevent_text;
    dp->version.seven.pmid = perfevent_pmid;
    dp->version.seven.name = perfevent_name;
    dp->version.seven.children = perfevent_children;

    pmdaSetFetchCallBack(dp, perfevent_fetchCallBack);
    pmdaSetLabelCallBack(dp, perfevent_labelCallBack);
    pmdaSetEndContextCallBack(dp, perfevent_end_contextCallBack);

    pmdaInit(dp, indomtab, nhwcounters + nderivedcounters, metrictab, nummetrics);

    if(setup_pmns() < 0)
    {
        return;
    }

    pmNotifyErr(LOG_INFO, "perfevent version " VERSION
			  " (maxfiles=%ld)\n", get_rlimit_maxfiles());
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmGetProgname());
    fputs("Options:\n"
          "  -C           maintain compatibility to (possibly) nonconforming metric names\n"
	  "  -D debug     set debug options, see pmdbg(1)\n"
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
    int			sep = pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "perfevent" "%c" "help",
             pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), PERFEVENT,
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
