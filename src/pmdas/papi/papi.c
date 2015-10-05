/*
 * PAPI PMDA
 *
 * Copyright (c) 2014-2015 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 * for more details.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <papi.h>
#include <assert.h>
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif
#include <string.h>
#include <time.h>


enum {
    CLUSTER_PAPI = 0,	// hardware event counters
    CLUSTER_CONTROL,	// control variables
    CLUSTER_AVAILABLE,	// available hardware
};

enum {
    CONTROL_ENABLE = 0,	 // papi.control.enable
    CONTROL_RESET,	 // papi.control.reset
    CONTROL_DISABLE,	 // papi.control.disable
    CONTROL_STATUS,	 // papi.control.status
    CONTROL_AUTO_ENABLE, // papi.control.auto_enable
    CONTROL_MULTIPLEX,	 // papi.control.multiplex
};

enum {
    AVAILABLE_NUM_COUNTERS = 0, // papi.available.num_counters
    AVAILABLE_VERSION,		// papi.available.version
};

typedef struct {
    char papi_string_code[PAPI_HUGE_STR_LEN]; //same length as the papi 'symbol' or name of the event
    pmID pmid;
    int position; /* >=0 implies actively counting in EventSet, index into values[] */
    time_t metric_enabled; /* >=0: time until user desires this metric; -1 forever. */
    long_long prev_value;
    PAPI_event_info_t info;
} papi_m_user_tuple;

#define METRIC_ENABLED_FOREVER ((time_t)-1)
static __uint32_t auto_enable_time = 120; /* seconds; 0:disabled */
static int auto_enable_afid = -1; /* pmaf(3) identifier for periodic callback */
static int enable_multiplexing = 1; /* on by default */

static papi_m_user_tuple *papi_info;
static unsigned int number_of_events; /* cardinality of papi_info[] */

static int isDSO = 1; /* == 0 if I am a daemon */
static int EventSet = PAPI_NULL;
static long_long *values;
struct uid_tuple {
    int uid_flag; /* uid attribute received. */
    int uid; /* uid received from PCP_ATTR_* */
}; 
static struct uid_tuple *ctxtab;
static int ctxtab_size;
static int number_of_counters; // XXX: collapse into number_of_events
static char papi_version[15];
static unsigned int size_of_active_counters; // XXX: eliminate
static __pmnsTree *papi_tree;

static int refresh_metrics(int);
static void auto_enable_expiry_cb(int, void *);

static char helppath[MAXPATHLEN];

static int
permission_check(int context)
{
    if (ctxtab[context].uid_flag && ctxtab[context].uid == 0)
	return 1;
    return 0;
}

static void
expand_papi_info(int size)
{
    if (number_of_events <= size) {
	size_t new_size = (size + 1) * sizeof(papi_m_user_tuple);
	papi_info = realloc(papi_info, new_size);
	if (papi_info == NULL)
	    __pmNoMem("papi_info tuple", new_size, PM_FATAL_ERR);
	while (number_of_events <= size)
	    memset(&papi_info[number_of_events++], 0, sizeof(papi_m_user_tuple));
    }
}

static void
expand_values(int size)	 // XXX: collapse into expand_papi_info()
{
    if (size_of_active_counters <= size) {
	size_t new_size = (size + 1) * sizeof(long_long);
	values = realloc(values, new_size);
	if (values == NULL)
	    __pmNoMem("values", new_size, PM_FATAL_ERR);
	while (size_of_active_counters <= size) {
	    memset(&values[size_of_active_counters++], 0, sizeof(long_long));
	    if (pmDebug & DBG_TRACE_APPL0) {
		__pmNotifyErr(LOG_DEBUG, "memsetting to zero, %d counters\n",
				size_of_active_counters);
	    }
	}
    }
}

static void
enlarge_ctxtab(int context)
{
    /* Grow the context table if necessary. */
    if (ctxtab_size /* cardinal */ <= context /* ordinal */) {
	size_t need = (context + 1) * sizeof(struct uid_tuple);
	ctxtab = realloc(ctxtab, need);
	if (ctxtab == NULL)
	    __pmNoMem("papi ctx table", need, PM_FATAL_ERR);
	/* Blank out new entries. */
	while (ctxtab_size <= context)
	    memset(&ctxtab[ctxtab_size++], 0, sizeof(struct uid_tuple));
    }
}


static int
check_papi_state()
{
    int state = 0;
    int sts;

    sts = PAPI_state(EventSet, &state);
    if (sts != PAPI_OK)
	return sts;
    return state;
}

static void
papi_endContextCallBack(int context)
{
    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_DEBUG, "end context %d received\n", context);

    /* ensure clients re-using this slot re-authenticate */
    if (context >= 0 && context < ctxtab_size)
	ctxtab[context].uid_flag = 0;
}

static int
papi_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int sts;
    int i;
    int state;
    char local_string[PAPI_HUGE_STR_LEN+12];
    static char status_string[4096];
    int first_metric = 0;
    time_t now;

    now = time(NULL);

    switch (idp->cluster) {
    case CLUSTER_PAPI:
	if (idp->item >= 0 && idp->item <= number_of_events) {
	    // the 'case' && 'idp->item' value we get is the pmns_position
	    if (papi_info[idp->item].position >= 0) { // live counter?
		atom->ll = papi_info[idp->item].prev_value + values[papi_info[idp->item].position];
		return PMDA_FETCH_STATIC;
	    }
	    else { // inactive counter?
                if (papi_info[idp->item].metric_enabled) { // but requested?
                    papi_info[idp->item].metric_enabled = 0; // give up
                    return PM_ERR_VALUE; // i.e., expect no values ever
                }
                return PMDA_FETCH_NOVALUES;
            }
	}

	return PM_ERR_PMID;

    case CLUSTER_CONTROL:
	switch (idp->item) {
	case CONTROL_ENABLE:
	    atom->cp = "";
	    return PMDA_FETCH_STATIC;

	case CONTROL_RESET:
	    atom->cp = "";
	    return PMDA_FETCH_STATIC;

	case CONTROL_DISABLE:
	    atom->cp = "";
	    if ((sts = check_papi_state()) & PAPI_RUNNING)
		return PMDA_FETCH_STATIC;
	    return 0;

	case CONTROL_STATUS:
	    sts = PAPI_state(EventSet, &state);
	    if (sts != PAPI_OK)
		return PM_ERR_VALUE;
	    strcpy(status_string, "Papi ");
	    if(state & PAPI_STOPPED)
		strcat(status_string, "is stopped, ");
	    if (state & PAPI_RUNNING)
		strcat(status_string, "is running, ");
	    if (state & PAPI_PAUSED)
		strcat(status_string,"is paused, ");
	    if (state & PAPI_NOT_INIT)
		strcat(status_string, "is defined but not initialized, ");
	    if (state & PAPI_OVERFLOWING)
		strcat(status_string, "has overflowing enabled, ");
	    if (state & PAPI_PROFILING)
		strcat(status_string, "eventset has profiling enabled, ");
	    if (state & PAPI_MULTIPLEXING)
		strcat(status_string,"has multiplexing enabled, ");
	    if (state & PAPI_ATTACHED)
		strcat(status_string, "is attached to another process/thread, ");
	    if (state & PAPI_CPU_ATTACHED)
		strcat(status_string, "is attached to a specific CPU, ");

	    first_metric = 1;
	    for(i = 0; i < number_of_events; i++){
		if(papi_info[i].position < 0)
		    continue;
		sprintf(local_string, "%s%s(%d): %lld",
			(first_metric ? "" : ", "),
			papi_info[i].papi_string_code,
			(papi_info[i].metric_enabled == METRIC_ENABLED_FOREVER ? -1 :
			 (int)(papi_info[i].metric_enabled - now)), // number of seconds left
			(papi_info[i].prev_value + values[papi_info[i].position]));
		first_metric = 0;
		if ((strlen(status_string) + strlen(local_string) + 1) < sizeof(status_string))
		    strcat(status_string, local_string);
	    }
	    atom->cp = status_string;
	    return PMDA_FETCH_STATIC;

	case CONTROL_AUTO_ENABLE:
	    atom->ul = auto_enable_time;
	    return PMDA_FETCH_STATIC;

	case CONTROL_MULTIPLEX:
	    atom->ul = enable_multiplexing;
	    return PMDA_FETCH_STATIC;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_AVAILABLE:
	if (idp->item == AVAILABLE_NUM_COUNTERS) {
	    atom->ul = number_of_counters;
	    return PMDA_FETCH_STATIC;
	}
	if (idp->item == AVAILABLE_VERSION) {
	    atom->cp = papi_version;
	    return PMDA_FETCH_STATIC;
	}
	return PM_ERR_PMID;

    default:
	return PM_ERR_PMID;
    }

    return PMDA_FETCH_NOVALUES;
}


static int
papi_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int i, sts = 0;

    __pmAFblock();
    auto_enable_expiry_cb(0, NULL); // run auto-expiry

    /* In auto-enable mode, handle a mass-refresh of the papi counter
       state in a big batch here, ahead of individual attempts to 
       confirm the counters' activation & read (initial) values. */
    if (auto_enable_time) {
        int need_refresh_p = 0;
        time_t now = time (NULL);

        for (i=0; i<numpmid; i++) {
            __pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
            if (idp->cluster == CLUSTER_PAPI) {
                if (papi_info[idp->item].position < 0) { // new counter?
                    need_refresh_p = 1;
                }
                // update or initialize remaining lifetime
                if (papi_info[idp->item].metric_enabled != METRIC_ENABLED_FOREVER)
                    papi_info[idp->item].metric_enabled = now + auto_enable_time;
            }
        }
        if (need_refresh_p) {
            refresh_metrics(1);
            // NB: A non-0 sts here would not be a big problem; no
            // need to abort the whole fetch sequence just for that.
            // Each individual CLUSTER_PAPI fetch will get a
            // PM_ERR_VALUE to let the user know something's up.
        }
            
    }

    /* Update our copy of the papi counter values, so that we do so
       only once per pcp-fetch batch.  Though it's relatively cheap,
       and harmless even if the incoming pcp-fetch is for non-counter
       pcp metrics, we do this only for CLUSTER_PAPI pmids.  This is
       independent of auto-enable mode. */
    for (i=0; i<numpmid; i++) {
        __pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
        if (idp->cluster == CLUSTER_PAPI) {
            sts = check_papi_state();
            if (sts & PAPI_RUNNING) {
                sts = PAPI_read(EventSet, values);
                if (sts != PAPI_OK) {
                    __pmNotifyErr(LOG_ERR, "PAPI_read: %s\n", PAPI_strerror(sts));
                    return PM_ERR_VALUE;
                }
            }
            break; /* No need to look at other pmids. */
        }
    }
    sts = 0; /* clear out any PAPI remnant flags */

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster != CLUSTER_AVAILABLE)
	    sts = 1;
    }
    if (sts == 0 || permission_check(pmda->e_context))
	sts = pmdaFetch(numpmid, pmidlist, resp, pmda);
    else
	sts = PM_ERR_PERMISSION;

    __pmAFunblock();
    return sts;
}

static void
handle_papi_error(int error, int logged)
{
    if (logged || (pmDebug & DBG_TRACE_APPL0))
	__pmNotifyErr(LOG_ERR, "Papi error: %s\n", PAPI_strerror(error));
}

/*
 * Iterate across all papi_info[].  Some of them are presumed to have
 * changed metric_enabled states (we don't care which way).  Shut down
 * the PAPI eventset and collect the then-current values; create a new
 * PAPI eventset with the survivors; restart.  (These steps are
 * necessary because PAPI doesn't let one modify a PAPI_RUNNING
 * EventSet, nor (due to a bug) subtract even from a PAPI_STOPPED one.)
 *
 * NB: in case of a partial success (some counters enabled, some not),
 * the rc here will be 0 (success).
 *
 * "log" parameter indicates whether errors are to be recorded in the
 * papi.log file, or if there is a calling process we can send 'em to
 * (in which case, they are not logged).
 */

static int
refresh_metrics(int log)
{
    int sts = 0;
    int state = 0;
    int i;
    int number_of_active_counters = 0;
    time_t now;

    now = time(NULL);

    /* Shut down, save previous state. */
    state = check_papi_state();
    if (state & PAPI_RUNNING) {
	sts = PAPI_stop(EventSet, values);
	if (sts != PAPI_OK) {
	    /* futile to continue */
	    handle_papi_error(sts, log);
	    return PM_ERR_VALUE;
	}

	/* Save previous values */ 
	for (i = 0; i < number_of_events; i++){
	    if(papi_info[i].position >= 0) {
		papi_info[i].prev_value += values[papi_info[i].position];
		papi_info[i].position = -1;
	    }
	}

	/* Clean up eventset */
	sts = PAPI_cleanup_eventset(EventSet);
	if (sts != PAPI_OK) {
	    handle_papi_error(sts, log);
	    /* FALLTHROUGH */
	}
	
	sts = PAPI_destroy_eventset(&EventSet); /* sets EventSet=NULL */
	if (sts != PAPI_OK) {
	    handle_papi_error(sts, log);
	    /* FALLTHROUGH */
	}
    }

    /* Initialize new EventSet */
    EventSet = PAPI_NULL;
    if ((sts = PAPI_create_eventset(&EventSet)) != PAPI_OK) {
	handle_papi_error(sts, log);
	return PM_ERR_GENERIC;
    }
    if ((sts = PAPI_assign_eventset_component(EventSet, 0 /*CPU*/)) != PAPI_OK) {
	handle_papi_error(sts, log);
	return PM_ERR_GENERIC;
    }
    if (enable_multiplexing && (sts = PAPI_set_multiplex(EventSet)) != PAPI_OK) {
	handle_papi_error(sts, log);
	/* not fatal - FALLTHROUGH */
    }

    /* Add all survivor events to new EventSet */
    number_of_active_counters = 0;
    for (i = 0; i < number_of_events; i++) {
	if (papi_info[i].metric_enabled == METRIC_ENABLED_FOREVER ||
	    papi_info[i].metric_enabled >= now) {
	    sts = PAPI_add_event(EventSet, papi_info[i].info.event_code);
	    if (sts != PAPI_OK) {
		if (pmDebug & DBG_TRACE_APPL0) {
		    char eventname[PAPI_MAX_STR_LEN];
		    PAPI_event_code_to_name(papi_info[i].info.event_code, eventname);
		    __pmNotifyErr(LOG_DEBUG, "Unable to add: %s due to error: %s\n",
				  eventname, PAPI_strerror(sts));
		}
		handle_papi_error(sts, log);
                /*
                 * We may be called to make drastic changes to the PAPI
                 * eventset.  Partial successes/failures are quite reasonable,
                 * as a user may be asking to activate a mix of good and bad
                 * counters.  Diagnosing the partial failure is left to the 
                 * caller, by examining the papi_info[i].position value
                 * after this function returns. */
		continue;
	    }
	    papi_info[i].position = number_of_active_counters++;
	}
    }

    /* Restart counting. */
    if (number_of_active_counters > 0) {
	sts = PAPI_start(EventSet);
	if (sts != PAPI_OK) {
	    handle_papi_error(sts, log);
	    return PM_ERR_VALUE;
	}
    }
    return 0;
}

/* The pmaf(3)-based callback for auto-enabled metric expiry. */
static void
auto_enable_expiry_cb(int ignored1, void *ignored2)
{
    int i;
    time_t now;
    int must_refresh;

    /* All we need to do here is to scan through all the enabled
     * metrics, and if some have just expired, call refresh_metrics().
     * We don't want to call it unconditionally, since it's disruptive.
     */
    now = time(NULL);
    must_refresh = 0;
    for (i = 0; i < number_of_events; i++) {
	if (papi_info[i].position >= 0 && // enabled at papi level
	    papi_info[i].metric_enabled != METRIC_ENABLED_FOREVER &&
	    papi_info[i].metric_enabled < now) // just expired
	    must_refresh = 1;
    }
    if (must_refresh)
	refresh_metrics(1);
}

static int
papi_setup_auto_af(void)
{
    if (auto_enable_afid >= 0)
	__pmAFunregister(auto_enable_afid);
    auto_enable_afid = -1;

    if (auto_enable_time) {
	struct timeval t;

	t.tv_sec = (time_t) auto_enable_time;
	t.tv_usec = 0;
	auto_enable_afid = __pmAFregister(&t, NULL, auto_enable_expiry_cb);
	return auto_enable_afid < 0 ? auto_enable_afid : 0;
    }
    return 0;
}

static int
papi_store(pmResult *result, pmdaExt *pmda)
{
    int sts = 0;
    int sts2 = 0;
    int i, j;
    const char *delim = " ,";
    char *substring;

    if (!permission_check(pmda->e_context))
	return PM_ERR_PERMISSION;
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;

	if (idp->cluster != CLUSTER_CONTROL) {
	    sts2 = PM_ERR_PERMISSION;
	    continue;
	}
	switch (idp->item) {
	case CONTROL_ENABLE:
	case CONTROL_DISABLE: // NB: almost identical handling!
	    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				      PM_TYPE_STRING, &av, PM_TYPE_STRING)) < 0) {
		sts2 = sts;
		continue;
	    }
	    substring = strtok(av.cp, delim);
	    while (substring != NULL) {
		for (j = 0; j < number_of_events; j++) {
		    if (!strcmp(substring, papi_info[j].papi_string_code)) {
			papi_info[j].metric_enabled =
			    (idp->item == CONTROL_ENABLE) ? METRIC_ENABLED_FOREVER : 0;
			break;
		    }
		}
		if (j == number_of_events) {
		    if (pmDebug & DBG_TRACE_APPL0)
			__pmNotifyErr(LOG_DEBUG, "metric name %s does not match any known metrics\n", substring);
		    sts = 1;
		    /* NB: continue for other event names that may succeed */
		}
		substring = strtok(NULL, delim);
	    }
	    if (sts) { /* any unknown metric name encountered? */
		sts = refresh_metrics(0); /* still enable those that we can */
		if (sts == 0)
		    sts = PM_ERR_BADSTORE; /* but return overall error */
	    } else {
		sts = refresh_metrics(0);
	    }
            // NB: We could iterate the affected papi_info[j]'s again to see which
            // counters were successfully activated (position >= 0).  Then again,
            // the user will find out soon enough, when attempting to fetch them.
	    sts2 = sts;
	    continue;

	case CONTROL_RESET:
	    for (j = 0; j < number_of_events; j++)
		papi_info[j].metric_enabled = 0;
	    sts = refresh_metrics(0);
	    continue;

	case CONTROL_AUTO_ENABLE:
	    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				      PM_TYPE_U32, &av, PM_TYPE_U32)) < 0) {
		sts2 = sts;
		continue;
	    }
	    auto_enable_time = av.ul;
	    sts = papi_setup_auto_af();
	    sts2 = sts;
	    continue;

	case CONTROL_MULTIPLEX:
	    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				      PM_TYPE_U32, &av, PM_TYPE_U32)) < 0) {
		sts2 = sts;
		continue;
	    }
	    enable_multiplexing = av.ul;
	    sts = refresh_metrics(0);
	    sts2 = sts;
	    continue;

	default:
	    sts2 = PM_ERR_PMID;
	    continue;
	}
    }
    if (sts == 0)
	sts = sts2;
    return sts;
}

static int
papi_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int sts = PM_ERR_PMID; // presume fail; switch statements fall through to this
    __pmID_int *idp = (__pmID_int *)&(pmid);

    switch (idp->cluster) {
    case CLUSTER_PAPI:
	desc->pmid = pmid;
	desc->type = PM_TYPE_64;
	desc->indom = PM_INDOM_NULL;
	desc->sem = PM_SEM_COUNTER;
	desc->units = (pmUnits) PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE);
	sts = 0;
	break;

    case CLUSTER_CONTROL:
	switch (idp->item) {
	case CONTROL_ENABLE:
	case CONTROL_RESET:
	case CONTROL_DISABLE:
	case CONTROL_STATUS:
	    desc->pmid = pmid;
	    desc->type = PM_TYPE_STRING;
	    desc->indom = PM_INDOM_NULL;
	    desc->sem = PM_SEM_INSTANT;
	    desc->units = (pmUnits) PMDA_PMUNITS(0,0,0,0,0,0);
	    sts = 0;
	    break;
	case CONTROL_AUTO_ENABLE:
	    desc->pmid = pmid;
	    desc->type = PM_TYPE_U32;
	    desc->indom = PM_INDOM_NULL;
	    desc->sem = PM_SEM_DISCRETE;
	    desc->units = (pmUnits) PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0);
	    sts = 0;
	    break;
	case CONTROL_MULTIPLEX:
	    desc->pmid = pmid;
	    desc->type = PM_TYPE_U32;
	    desc->indom = PM_INDOM_NULL;
	    desc->sem = PM_SEM_DISCRETE;
	    desc->units = (pmUnits) PMDA_PMUNITS(0,0,0,0,0,0);
	    sts = 0;
	    break;
	}
	break;

    case CLUSTER_AVAILABLE:
	switch (idp->item) {
	case AVAILABLE_NUM_COUNTERS:
	    desc->pmid = pmid;
	    desc->type = PM_TYPE_U32;
	    desc->indom = PM_INDOM_NULL;
	    desc->sem = PM_SEM_DISCRETE;
	    desc->units = (pmUnits) PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE);
	    sts = 0;
	    break;
	case AVAILABLE_VERSION:
	    desc->pmid = pmid;
	    desc->type = PM_TYPE_STRING;
	    desc->indom = PM_INDOM_NULL;
	    desc->sem = PM_SEM_INSTANT;
	    desc->units = (pmUnits) PMDA_PMUNITS(0,0,0,0,0,0);
	    sts = 0;
	    break;
	}
	break;
    }

    return sts;
}


static int
papi_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    __pmID_int *pmidp = (__pmID_int *)&ident;

    /* no indoms - we only deal with metric help text */
    if ((type & PM_TEXT_PMID) != PM_TEXT_PMID)
	return PM_ERR_TEXT;

    if (pmidp->cluster == CLUSTER_PAPI) {
	if (pmidp->item < number_of_events) {
	    if (type & PM_TEXT_ONELINE)
		*buffer = papi_info[pmidp->item].info.short_descr;
	    else
		*buffer = papi_info[pmidp->item].info.long_descr;
	    return 0;
	}
    }

    /* delegate to "help" file */
    return pmdaText(ident, type, buffer, ep);
}

static int
papi_name_lookup(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return pmdaTreePMID(papi_tree, name, pmid);
}

static int
papi_children(const char *name, int traverse, char ***offspring, int **status, pmdaExt *pmda)
{
    return pmdaTreeChildren(papi_tree, name, traverse, offspring, status);
}

static int
papi_internal_init(pmdaInterface *dp)
{
    int ec;
    int sts;
    PAPI_event_info_t info;
    char entry[PAPI_HUGE_STR_LEN+12]; // the length papi uses for the symbol name
    unsigned int i = 0;
    pmID pmid;

    sts = sprintf(papi_version, "%d.%d.%d", PAPI_VERSION_MAJOR(PAPI_VERSION),
	    PAPI_VERSION_MINOR(PAPI_VERSION), PAPI_VERSION_REVISION(PAPI_VERSION));
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "%s failed to create papi version metric.\n",pmProgname);
	return PM_ERR_GENERIC;
    }

    if ((sts = __pmNewPMNS(&papi_tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s failed to create dynamic papi pmns: %s\n",
		      pmProgname, pmErrStr(sts));
	papi_tree = NULL;
	return PM_ERR_GENERIC;
    }

    number_of_counters = PAPI_num_counters();
    if (number_of_counters < 0) {
	__pmNotifyErr(LOG_ERR, "hardware does not support performance counters\n");
	return PM_ERR_APPVERSION;
    }

    sts = PAPI_library_init(PAPI_VER_CURRENT);
    if (sts != PAPI_VER_CURRENT) {
	__pmNotifyErr(LOG_ERR, "PAPI_library_init error (%d)\n", sts);
	return PM_ERR_GENERIC;
    }

    ec = PAPI_PRESET_MASK;
    PAPI_enum_event(&ec, PAPI_ENUM_FIRST);
    do {
	if (PAPI_get_event_info(ec, &info) == PAPI_OK) {
	    if (info.count && PAPI_PRESET_ENUM_AVAIL) {
		expand_papi_info(i);
		memcpy(&papi_info[i].info, &info, sizeof(PAPI_event_info_t));
		memcpy(&papi_info[i].papi_string_code, info.symbol + 5, strlen(info.symbol)-5);
		snprintf(entry, sizeof(entry),"papi.system.%s", papi_info[i].papi_string_code);
		pmid = pmid_build(dp->domain, CLUSTER_PAPI, i);
		papi_info[i].pmid = pmid;
		__pmAddPMNSNode(papi_tree, pmid, entry);
		memset(&entry[0], 0, sizeof(entry));
		papi_info[i].position = -1;
		papi_info[i].metric_enabled = 0;
		expand_values(i);
		i++;
	    }
	}
    } while(PAPI_enum_event(&ec, 0) == PAPI_OK);

#if defined(HAVE_PAPI_DISABLED_COMP)
    char *tokenized_string;
    int number_of_components;
    int component_id;
    int native;

    number_of_components = PAPI_num_components();
    native = 0 | PAPI_NATIVE_MASK;
    for (component_id = 0; component_id < number_of_components; component_id++) {
	const PAPI_component_info_t *component;
	component = PAPI_get_component_info(component_id);
	if (component->disabled || (strcmp("perf_event", component->name)
				    && strcmp("perf_event_uncore", component->name)))
	    continue;
	sts = PAPI_enum_cmp_event (&native, PAPI_ENUM_FIRST, component_id);
	if (sts == PAPI_OK)
	do {
	    if (PAPI_get_event_info(native, &info) == PAPI_OK) {
		char local_native_metric_name[PAPI_HUGE_STR_LEN] = "";
		int was_tokenized = 0;
		expand_papi_info(i);
		memcpy(&papi_info[i].info, &info, sizeof(PAPI_event_info_t));
		tokenized_string = strtok(info.symbol, "::: -");
		while (tokenized_string != NULL) {
		    size_t remaining = sizeof(local_native_metric_name) -
			strlen(local_native_metric_name) - 1;
		    if (remaining < 1)
			break;
		    strncat(local_native_metric_name, tokenized_string, remaining);
		    was_tokenized = 1;
		    tokenized_string=strtok(NULL, "::: -");
		    if (tokenized_string) {
			remaining = sizeof(local_native_metric_name) -
			    strlen(local_native_metric_name) - 1;
			if (remaining < 1)
			    break;
			strncat(local_native_metric_name, ".", remaining);
		    }
		}
		if (!was_tokenized) {
		    strncpy(papi_info[i].papi_string_code, info.symbol,
			    sizeof(papi_info[i].papi_string_code) - 1);
		}
		else {
		    strncpy(papi_info[i].papi_string_code,
			    local_native_metric_name,
			    sizeof(papi_info[i].papi_string_code) - 1);
		}
		snprintf(entry, sizeof(entry),"papi.system.%s", papi_info[i].papi_string_code);
		pmid = pmid_build(dp->domain, CLUSTER_PAPI, i);
		papi_info[i].pmid = pmid;
		__pmAddPMNSNode(papi_tree, pmid, entry);
		memset(&entry[0], 0, sizeof(entry));
		papi_info[i].position = -1;
		papi_info[i].metric_enabled = 0;
		expand_values(i);
		i++;
	    }
	} while (PAPI_enum_cmp_event(&native, PAPI_ENUM_EVENTS, component_id) == PAPI_OK);
    }
#endif
    pmdaTreeRebuildHash(papi_tree, number_of_events);

    /* Set one-time settings for all future EventSets. */
    if ((sts = PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK) {
	handle_papi_error(sts, 0);
	return PM_ERR_GENERIC;
    }
    if ((sts = PAPI_multiplex_init()) != PAPI_OK) {
	handle_papi_error(sts, 0);
	return PM_ERR_GENERIC;
    }

    sts = refresh_metrics(0);
    if (sts != PAPI_OK)
	return PM_ERR_GENERIC;
    return 0;
}

/* use documented in pmdaAttribute(3) */
static int
papi_contextAttributeCallBack(int context, int attr,
			      const char *value, int length, pmdaExt *pmda)
{
    int id = -1;

    enlarge_ctxtab(context);
    assert(ctxtab != NULL && context < ctxtab_size);

    if (attr != PCP_ATTR_USERID)
	return 0;

    ctxtab[context].uid_flag = 1;
    ctxtab[context].uid = id = atoi(value);
    if (id != 0) {
	if (pmDebug & DBG_TRACE_AUTH)
	    __pmNotifyErr(LOG_DEBUG, "access denied attr=%d id=%d\n", attr, id);
	return PM_ERR_PERMISSION;
    }

    if (pmDebug & DBG_TRACE_AUTH)
	__pmNotifyErr(LOG_DEBUG, "access granted attr=%d id=%d\n", attr, id);
    return 0;
}

void
__PMDA_INIT_CALL
papi_init(pmdaInterface *dp)
{
    int sts;

    if (isDSO) {
	int	sep = __pmPathSeparator();

	snprintf(helppath, sizeof(helppath), "%s%c" "papi" "%c" "help",
		 pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "papi DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->comm.flags |= PDU_FLAG_AUTH;

    if ((sts = papi_internal_init(dp)) < 0) {
	__pmNotifyErr(LOG_ERR, "papi_internal_init: %s\n", pmErrStr(sts));
	dp->status = PM_ERR_GENERIC;
	return;
    }

    if ((sts = papi_setup_auto_af()) < 0) {
	__pmNotifyErr(LOG_ERR, "papi_setup_auto_af: %s\n", pmErrStr(sts));
	dp->status = PM_ERR_GENERIC;
	return;
    }

    dp->version.six.fetch = papi_fetch;
    dp->version.six.store = papi_store;
    dp->version.six.attribute = papi_contextAttributeCallBack;
    dp->version.six.desc = papi_desc;
    dp->version.any.text = papi_text;
    dp->version.four.pmid = papi_name_lookup;
    dp->version.four.children = papi_children;
    pmdaSetFetchCallBack(dp, papi_fetchCallBack);
    pmdaSetEndContextCallBack(dp, papi_endContextCallBack);
    pmdaInit(dp, NULL, 0, NULL, 0);
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * Set up agent if running as daemon.
 */
int
main(int argc, char **argv)
{
    int sep = __pmPathSeparator();
    pmdaInterface dispatch;

    isDSO = 0;
    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "papi" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, PAPI, "papi.log", helppath);	
    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
 
    pmdaOpenLog(&dispatch);
    papi_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    free(ctxtab);
    free(papi_info);
    free(values);

    exit(0);
}
