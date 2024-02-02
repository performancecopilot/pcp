#ifndef MODULE_H
#define MODULE_H

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <math.h>
#include "dict.h"
#include "sds.h"

typedef int (*init_fn_t)(dict *cfg, char *module_name);
typedef void (*register_fn_t)(unsigned int cluster_id, pmdaMetric *metrics, pmdaIndom *indoms);
typedef void (*shutdown_fn_t)(void);
typedef unsigned int (*metric_count_fn_t)(void);
typedef void (*set_indom_serial_fn_t)(unsigned int local_indom_id, unsigned int global_id);
typedef unsigned int (*indom_count_fn_t)(void);
typedef void (*refresh_fn_t)(unsigned int item);
typedef int (*fetch_to_atom_fn_t)(unsigned int item, unsigned int inst, pmAtomValue *atom);
typedef char* (*metric_name_fn_t)(unsigned int metric);
typedef int (*metric_text_fn_t)(int item, int type, char **buf);

/**
 * Module layer interface struct.
 *
 * Modules should be shared object files (.so) and have a single well-known entry
 * point 'load_module'. The load_module call for each module should return a populated
 * module struct.
 *
 * Functions cannot be null. Modules need to be prepared to respond to all of these
 * calls; even if they are a noop they should provide default behaviour as they will
 * be called.
 */
typedef struct module {
    /**
     * Return the number of pmdaIndom this instance requires.
     *
     * This is used to allocate sufficient space for register_metrics call later.
     */
    indom_count_fn_t indom_count;

    /**
     * Set indom serial to support dynamic indom setup
     *
     * Will be called "indom_count" times, start ing from 0 to "indom_count - 1", to inform
     * this module what the global indom id is for the given local id.
     */
    set_indom_serial_fn_t set_indom_serial;

    /**
     * Return the number of pmdaMetric slots this instance requires.
     *
     * This is used to allocate sufficient space for register_metrics call later.
     */
    metric_count_fn_t metric_count;

    /**
     * Callback to have module fill in pmdaMetric and pmdaIndom records.
     *
     * The module is passed an array pointer for pmdaMetric and pmdaIndom records; the
     * module should fill the elements sequentially. It is important that indom_count()
     * and metric_count() calls return the correct number so that there is correct
     * space allocated.
     *
     * The values here must match the definitions in PMNS.
     */
    register_fn_t register_metrics;

    /**
     * Initialise a module.
     *
     * @return 0 if no issues, non-zero to indicate errors.
     */
    init_fn_t init;

    /**
     * Release any resources associated with the module.
     */
    shutdown_fn_t shutdown;

    /**
     * Pre-fetch refresh call issued by PMCD.
     *
     * This is a good time to refresh indom table, or load any metrics that are more
     * efficiently fetched in bulk.
     */
    refresh_fn_t refresh;

    /**
     * Fetch individual metric for a module, akin to the pmdaFetchCallback.
     *
     * A module can be called for fetch without being initialised, it should respond
     * with PMDA_FETCH_NOVALUES. This is the module's responsibility.
     */
    fetch_to_atom_fn_t fetch_to_atom;

    /**
     * Fetch name for a metric
     */
    metric_name_fn_t metric_name;

    /**
     * Fetch help text for a metric (oneline or full text)
     */
    metric_text_fn_t metric_text;
} module;

/**
 * List of all modules defined
 */
char *all_modules[] = {
    "bashreadline",
    "biolatency",
    "biosnoop",
    "execsnoop",
    "exitsnoop"
    "fsslower",
    "mountsnoop",
    "netatop",
    "oomkill",
    "opensnoop",
    "runqlat",
    "statsnoop",
    "tcpconnect",
    "tcpconnlat",
    "vfsstat",
};

/**
 * Fill a pmdaInstid table with log2 scaled values.
 *
 * The instance ID table will be filled sequentially with strings
 * "0-1", "2-3", "4-7", "8-15", "16-31", "32-63", etc
 *
 * if slot_count is > 63, it will be capped at 63
 */
void fill_instids_log2(unsigned int slot_count, pmdaInstid slots[]) {
    if (slot_count > 63)
        slot_count = 63;

    for(int i = 0; i < slot_count; i++) {
        char *string;
        unsigned long lower = round(pow(2, i));
        unsigned long upper = round(pow(2, i+1)-1);

        // fixup
        if (i == 0)
            lower = 0;

        int ret = asprintf(&string, "%lu-%lu", lower, upper);
        if (ret > 0) {
            slots[i].i_inst = i;
            slots[i].i_name = string;
        }
    }
}

void fill_instids(unsigned int slot_count, pmdaInstid **slots) {
    if ((*slots = malloc(slot_count * sizeof(pmdaInstid))) == NULL) {
        pmNotifyErr(LOG_ERR, "pmdaInstid: realloc err: %d", PM_FATAL_ERR);
        exit(1);
    }
    for (int i = 0; i < slot_count; i++) {
        char *string;

        int ret = asprintf(&string, "%d", i);
        if (ret > 0) {
            (*slots)[i].i_name = string;
            (*slots)[i].i_inst = i;
        }
    }
}

#endif
