#include <stddef.h>
#include <pcp/dict.h>
#include <chan/chan.h>

#include "utils.h"
#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-counter.h"
#include "aggregator-metric-gauge.h"
#include "aggregator-metric-duration.h"

/**
 * Metric names that won't be saved into hashtable 
 * - these names are already taken by metric stats
 */
static const char* const g_blacklist[] = {
    "pmda.received",
    "pmda.parsed",
    "pmda.aggregated",
    "pmda.thrown_away",
    "pmda.time_spent_aggregating",
    "pmda.time_spent_parsing"
};

static void
metric_free_callback(void *privdata, void *val)
{
    struct agent_config* config = ((struct pmda_metrics_dict_privdata*)privdata)->config;
    struct pmda_metrics_container* container = ((struct pmda_metrics_dict_privdata*)privdata)->container;
    free_metric(config, container, (struct metric*)val);
}

static void*
metric_key_duplicate_callback(void *privdata, const void *key)
{
    (void)privdata;
    char* duplicate = malloc(strlen(key));
    strcpy(duplicate, key);
    return duplicate;
}

static int
metric_compare_callback(void* privdata, const void* key1, const void* key2)
{
    (void)privdata;
    return strcmp((char*)key1, (char*)key2) == 0;
}

static uint64_t
metric_hash_callback(const void *key)
{
    return dictGenCaseHashFunction((unsigned char *)key, strlen((char *)key));
}

/**
 * Callbacks for metrics hashtable
 */
static dictType metric_dict_callbacks = {
    .hashFunction	= metric_hash_callback,
    .keyCompare		= metric_compare_callback,
    .keyDup		    = metric_key_duplicate_callback,
    .keyDestructor	= metric_free_callback,
    .valDestructor	= metric_free_callback,
};

/**
 * Creates new pmda_metrics_container structure, initializes all stats to 0
 */
struct pmda_metrics_container*
init_pmda_metrics(struct agent_config* config) {
    struct pmda_metrics_container* container =
        (struct pmda_metrics_container*) malloc(sizeof(struct pmda_metrics_container));
    ALLOC_CHECK("Unable to create PMDA metrics container.");
    pthread_mutex_init(&container->mutex, NULL);
    struct pmda_metrics_dict_privdata* dict_data = 
        (struct pmda_metrics_dict_privdata*) malloc(sizeof(struct pmda_metrics_dict_privdata));    
    dict_data->config = config;
    dict_data->container = container;
    metrics* m = dictCreate(&metric_dict_callbacks, NULL);
    container->metrics = m;
    return container;
}

static char*
create_metric_dict_key(struct statsd_datagram* datagram) {
    int maximum_key_size = 4096;
    char buffer[maximum_key_size]; // maximum key size
    int key_size = pmsprintf(
        buffer,
        maximum_key_size,
        "%s&%s&%s&%d",
        datagram->name,
        datagram->tags != NULL ? datagram->tags : "-",
        datagram->instance != NULL ? datagram->instance : "-",
        datagram->type
    );
    char* result = malloc(key_size + 1);
    ALLOC_CHECK("Unable to allocate memory for hashtable key");
    memcpy(result, &buffer, key_size + 1);
    return result;
}

/**
 * Processes datagram struct into metric 
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 * @return status - 1 when successfully saved or updated, 0 when thrown away
 */
int
process_datagram(struct agent_config* config, struct pmda_metrics_container* container, struct statsd_datagram* datagram) {
    struct metric* item;
    char* key = create_metric_dict_key(datagram);
    if (key == NULL) {
        verbose_log("Throwing away datagram. REASON: unable to create hashtable key for metric record.");        
        return 0;
    }
    int metric_exists = find_metric_by_name(container, key, &item);
    if (metric_exists) {
        int res = update_metric(config, container, item, datagram);
        if (res == 0) {
            verbose_log("Throwing away datagram. REASON: semantically incorrect values.");
            return 0;
        }
        return 1;
    } else {
        int name_available = check_metric_name_available(container, key);
        if (name_available) {
            int correct_semantics = create_metric(config, datagram, &item);
            if (correct_semantics) {
                add_metric(container, key, item);
                return 1;
            } else {
                verbose_log("Throwing away datagram. REASON: semantically incorrect values.");
                return 0;
            }
        } else {
            verbose_log("Throwing away datagram. REASON: name is not available. (blacklisted?)");
            return 0;
        }
    }
}

/**
 * Frees metric
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper (optional)
 * @arg metric - Metric to be freed
 * 
 * Synchronized by mutex on pmda_metrics_container (if any passed)
 */
void
free_metric(struct agent_config* config, struct pmda_metrics_container* container, struct metric* item) {
    if (container != NULL) {
        pthread_mutex_lock(&container->mutex);
    }
    if (item->name != NULL) {
        free(item->name);
    }
    if (item->meta != NULL) {
        free_metric_metadata(item->meta);
    }
    switch (item->type) {
        case METRIC_TYPE_COUNTER:
            free_counter_value(config, item);
            break;
        case METRIC_TYPE_GAUGE:
            free_gauge_value(config, item);
            break;
        case METRIC_TYPE_DURATION:
            free_duration_value(config, item);
            break;
        case METRIC_TYPE_NONE:
            // not an actualy metric
            break;
    }
    if (item != NULL) {
        free(item);
    }
    if (container != NULL) {
        pthread_mutex_unlock(&container->mutex);
    }
}

/**
 * Prints metadata 
 * @arg f - Opened file handle, doesn't close it after finishing
 * @arg meta - Metric metadata
 */
void
print_metric_meta(FILE* f, struct metric_metadata* meta) {
    if (meta != NULL) {
        if (meta->tags != NULL) {
            fprintf(f, "tags = %s\n", meta->tags);
        }
        fprintf(f, "sampling = %f\n", meta->sampling);
        if (meta->instance != NULL) {
            fprintf(f, "instance = %s\n", meta->instance);
        }
    }
}

/**
 * Writes information about recorded metrics into file
 * @arg config - Config containing information about where to output
 * @arg container - Metrics struct acting as metrics wrapper
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
write_metrics_to_file(struct agent_config* config, struct pmda_metrics_container* container) {
    pthread_mutex_lock(&container->mutex);
    metrics* m = container->metrics;
    if (strlen(config->debug_output_filename) == 0) return; 
    FILE* f;
    f = fopen(config->debug_output_filename, "a+");
    if (f == NULL) {
        return;
    }
    dictIterator* iterator = dictGetSafeIterator(m);
    dictEntry* current;
    long int count = 0;
    while ((current = dictNext(iterator)) != NULL) {
        struct metric* item = (struct metric*)current->v.val;
        switch (item->type) {
            case METRIC_TYPE_COUNTER:
                print_counter_metric(config, f, item);
                break;
            case METRIC_TYPE_GAUGE:
                print_gauge_metric(config, f, item);
                break;
            case METRIC_TYPE_DURATION:
                print_duration_metric(config, f, item);
                break;
            case METRIC_TYPE_NONE:
                // not an actualy metric error case
                break;
        }
        count++;
    }
    dictReleaseIterator(iterator);
    fprintf(f, "-----------------\n");
    fprintf(f, "Total number of records: %lu \n", count);
    fclose(f);
    pthread_mutex_unlock(&container->mutex);
}

/**
 * Finds metric by name
 * @arg container - Metrics container
 * @arg name - Metric name to search for
 * @arg out - Placeholder metric
 * @return 1 when any found
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
find_metric_by_name(struct pmda_metrics_container* container, char* name, struct metric** out) {
    pthread_mutex_lock(&container->mutex);
    metrics* m = container->metrics;
    dictEntry* result = dictFind(m, name);
    if (result == NULL) {
        pthread_mutex_unlock(&container->mutex);
        return 0;
    }
    if (out != NULL) {
        struct metric* item = (struct metric*)result->v.val;
        *out = item;
    }
    pthread_mutex_unlock(&container->mutex);
    return 1;
}

/**
 * Creates metric
 * @arg config - Agent config
 * @arg datagram - Datagram with data that should populate new metric
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail - fails before allocating anything to "out"
 */
int
create_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out) {
    struct metric* item = (struct metric*) malloc(sizeof(struct metric));
    ALLOC_CHECK("Unable to allocate memory for metric.");
    *out = item;
    switch (datagram->type) {
        case METRIC_TYPE_COUNTER:
            return create_counter_metric(config, datagram, out);
        case METRIC_TYPE_GAUGE:
            return create_gauge_metric(config, datagram, out);
        case METRIC_TYPE_DURATION:
            return create_duration_metric(config, datagram, out);
        default:
            return 0;
    }
}

/**
 * Adds gauge record
 * @arg container - Metrics container
 * @arg gauge - Gauge metric to me added
 * @return all gauges
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
add_metric(struct pmda_metrics_container* container, char* key, struct metric* item) {
    pthread_mutex_lock(&container->mutex);
    dictAdd(container->metrics, key, item);
    pthread_mutex_unlock(&container->mutex);
}

/**
 * Updates counter record
 * @arg config - Agent config
 * @arg container - Metrics container
 * @arg counter - Metric to be updated
 * @arg datagram - Data with which to update
 * @return 1 on success
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
update_metric(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    struct metric* item,
    struct statsd_datagram* datagram
) {
    pthread_mutex_lock(&container->mutex);
    int status = 0;
    switch (item->type) {
        case METRIC_TYPE_COUNTER:
            status = update_counter_metric(config, item, datagram);
            break;
        case METRIC_TYPE_GAUGE:
            status = update_gauge_metric(config, item, datagram);
            break;
        case METRIC_TYPE_DURATION:
            status = update_duration_metric(config, item, datagram);
            break;
        case METRIC_TYPE_NONE:
            status = 0;
            break;
    }
    pthread_mutex_unlock(&container->mutex);
    return status;
}

/**
 * Checks if given metric name is available (it isn't recorded yet or is blacklisted)
 * @arg container - Metrics container
 * @arg name - Name to be checked
 * @return 1 on success else 0
 */
int
check_metric_name_available(struct pmda_metrics_container* container, char* name) {    
    size_t i;
    for (i = 0; i < sizeof(g_blacklist) / sizeof(g_blacklist[0]); i++) {
        if (strcmp(name, g_blacklist[i]) == 0) return 0;
    }
    if (!find_metric_by_name(container, name, NULL)) {
        return 1;
    }
    return 0;
}

/**
 * Creates metric metadata
 * @arg datagram - Datagram from which to build metadata
 * @return metric metadata
 */
struct metric_metadata*
create_metric_meta(struct statsd_datagram* datagram) {
    if (datagram->tags == NULL && datagram->instance == NULL) {
        return NULL;
    }
    struct metric_metadata* meta = (struct metric_metadata*) malloc(sizeof(struct metric_metadata));
    *meta = (struct metric_metadata) { 0 };
    meta->sampling = datagram->sampling;
    if (datagram->tags != NULL) {
        meta->tags = (char*) malloc(strlen(datagram->tags) + 1);
        memcpy(meta->tags, datagram->tags, strlen(datagram->tags) + 1);
    }
    if (datagram->instance != NULL) {
        meta->instance = (char*) malloc(strlen(datagram->instance) + 1);
        memcpy(meta->instance, datagram->instance, strlen(datagram->instance) + 1);
    }
    return meta;
}

/**
 * Frees metric metadata
 * @arg metadata - Metadata to be freed
 */ 
void
free_metric_metadata(struct metric_metadata* meta) {
    if (meta->tags != NULL) {
        free(meta->tags);
    }
    if (meta->instance != NULL) {
        free(meta->instance);
    }
    if (meta != NULL) {
        free(meta);
    }
}
