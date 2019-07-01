#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <chan/chan.h>
#include <hdr/hdr_histogram.h>
#include <pcp/dict.h>
#include <pcp/pmapi.h>

#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"
#include "utils.h"
#include "aggregators.h"
#include "aggregator-duration-exact.h"
#include "aggregator-duration-hdr.h"
#include "aggregator-duration.h"
#include "aggregator-counter.h"
#include "aggregator-gauge.h"
#include "pcp.h"

/**
 * Flag to capture USR1 signal event, which is supposed mark request to output currently recorded data in debug file 
 */
static int g_output_requested = 0;
pthread_mutex_t g_output_requested_lock;

static void
metricFreeCallBack(void *privdata, void *val)
{
    struct agent_config* config = (struct agent_config*)privdata;
    free_metric(config, (struct metric*)val);
}

static void*
metricKeyDupCallBack(void *privdata, const void *key)
{
    (void)privdata;
    char* duplicate = malloc(strlen(key));
    strcpy(duplicate, key);
    return duplicate;
}

static int
metricCompareCallBack(void* privdata, const void* key1, const void* key2)
{
    (void)privdata;
    return strcmp((char*)key1, (char*)key2) == 0;
}

static uint64_t
metricHashCallBack(const void *key)
{
    return dictGenCaseHashFunction((unsigned char *)key, strlen((char *)key));
}

/**
 * Callbacks for metrics hashtable
 */
dictType metricDictCallBacks = {
    .hashFunction	= metricHashCallBack,
    .keyCompare		= metricCompareCallBack,
    .keyDup		    = metricKeyDupCallBack,
    .keyDestructor	= metricFreeCallBack,
    .valDestructor	= metricFreeCallBack,
};

/**
 * Initializes metrics struct to empty values
 * @arg config - Config (should there be need to pass detailed info into metrics)
 */
metrics*
init_metrics(struct agent_config* config) {
    (void)config;
    metrics* m = dictCreate(&metricDictCallBacks, config);
    return m;
}

/**
 * Thread startpoint - passes down given datagram to aggregator to record value it contains
 * @arg args - (aggregator_args), see ~/network-listener.h
 */
void*
aggregator_exec(void* args) {
    struct agent_config* config = ((struct aggregator_args*)args)->config;
    chan_t* parsed = ((struct aggregator_args*)args)->parsed_datagrams;
    chan_t* pcp_to_aggregator = ((struct aggregator_args*)args)->pcp_request_channel;
    chan_t* aggregator_to_pcp = ((struct aggregator_args*)args)->pcp_response_channel;
    metrics* m = ((struct aggregator_args*)args)->metrics_wrapper;
    struct statsd_datagram* datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram));
    struct pcp_request* request = (struct pcp_request*) malloc(sizeof(struct pcp_request));
    while(1) {
        switch(chan_select(&parsed, 1, (void *)&datagram, NULL, 0, NULL)) {
            case 0:
                process_datagram(config, m, datagram);
                break;
        }
        switch(chan_select(&pcp_to_aggregator, 1, (void *)&request, NULL, 0, NULL)) {
            case 0:
                process_pcp_request(config, m, request, aggregator_to_pcp);
                break;
        }
        pthread_mutex_lock(&g_output_requested_lock);
        if (g_output_requested) {
            verbose_log("Output of recorded values request caught.");
            print_metrics(config, m);
            verbose_log("Recorded values output.");
            g_output_requested = 0;
        }
        pthread_mutex_unlock(&g_output_requested_lock);
    }
    free_datagram(datagram);
}

/**
 * Sets flag notifying that output was requested
 */
void
aggregator_request_output() {
    pthread_mutex_lock(&g_output_requested_lock);
    g_output_requested = 1;
    pthread_mutex_unlock(&g_output_requested_lock);
}

static char*
create_metric_dict_key(struct statsd_datagram* datagram) {
    int maximum_key_size = 4096;
    char buffer[maximum_key_size]; // maximum key size
    int key_size = pmsprintf(
        buffer,
        maximum_key_size,
        "%s&%s&%s&%s",
        datagram->metric,
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
 * @arg m - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 */
void
process_datagram(struct agent_config* config, metrics* m, struct statsd_datagram* datagram) {
    struct metric* item;
    char* key = create_metric_dict_key(datagram);
    if (key == NULL) {
        verbose_log("Throwing away datagram. REASON: unable to create hashtable key for metric record.");
        return;
    }
    int metric_exists = find_metric_by_name(m, key, &item);
    if (metric_exists) {
        int res = update_metric(config, item, datagram);
        if (res == 0) {
            verbose_log("Throwing away datagram. REASON: semantically incorrect values.");
        }
    } else {
        int name_available = check_metric_name_available(m, key);
        if (name_available) {
            int correct_semantics = create_metric(config, datagram, &item);
            if (correct_semantics) {
                add_metric(m, key, item);
            } else {
                verbose_log("Throwing away datagram. REASON: semantically incorrect values.");
            }
        }
    }
}

/**
 * NOT IMPLEMENTED YET
 * Processes PCP PMDA request
 * @arg config - Agent config
 * @arg m - Metric struct acting as metrics wrapper
 * @arg request - PCP PMDA request to be processed
 * @arg out - Channel over which to send request response
 */
void
process_pcp_request(struct agent_config* config, metrics* m, struct pcp_request* request, chan_t* out) {
    (void)config;
    (void)m;
    (void)request;
    (void)out;
}

/**
 * Frees metric
 * @arg config
 * @arg metric - Metric to be freed
 */
void
free_metric(struct agent_config* config, struct metric* item) {
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
    }
    if (item != NULL) {
        free(item);
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
        if (meta->sampling != NULL) {
            fprintf(f, "sampling = %s\n", meta->sampling);
        }
        if (meta->instance != NULL) {
            fprintf(f, "instance = %s\n", meta->instance);
        }
    }
}

/**
 * Writes information about recorded metrics into file
 * @arg config - Config containing information about where to output
 * @arg m - Metrics struct (what values to print)
 */
void
print_metrics(struct agent_config* config, metrics* m) {
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
        }
        count++;
    }
    dictReleaseIterator(iterator);
    fprintf(f, "-----------------\n");
    fprintf(f, "Total number of records: %lu \n", count);
    fclose(f);
}

/**
 * Finds metric by name
 * @arg name - Metric name to search for
 * @arg out - Placeholder metric
 * @return 1 when any found
 */
int
find_metric_by_name(metrics* m, char* name, struct metric** out) {
    dictEntry* result = dictFind(m, name);
    if (result == NULL) {
        return 0;
    }
    if (out != NULL) {
        struct metric* item = (struct metric*)result->v.val;
        *out = item;
    }
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
    if (strcmp(datagram->type, "ms") == 0) {
        return create_duration_metric(config, datagram, out);
    } else if (strcmp(datagram->type, "c") == 0) {
        return create_counter_metric(config, datagram, out);
    } else if (strcmp(datagram->type, "g") == 0) {
        return create_gauge_metric(config, datagram, out);
    }
    return 0;
}

/**
 * Adds gauge record
 * @arg gauge - Gauge metric to me added
 * @return all gauges
 */
void
add_metric(metrics* m, char* key, struct metric* item) {
    dictAdd(m, key, item);
}

/**
 * Updates counter record
 * @arg config - Agent config
 * @arg counter - Metric to be updated
 * @arg datagram - Data with which to update
 * @return 1 on success
 */
int
update_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram) {
    switch (item->type) {
        case METRIC_TYPE_COUNTER:
            return update_counter_metric(config, item, datagram);
        case METRIC_TYPE_GAUGE:
            return update_gauge_metric(config, item, datagram);
        case METRIC_TYPE_DURATION:
            return update_duration_metric(config, item, datagram);
    }
    return 0;
}

/**
 * Checks if given metric name is available (it isn't recorded yet)
 * @arg m - Metrics struct (storage in which to check for name availability)
 * @arg name - Name to be checked
 * @return 1 on success else 0
 */
int
check_metric_name_available(metrics* m, char* name) {
    if (!find_metric_by_name(m, name, NULL)) {
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
    if (datagram->sampling == NULL && datagram->tags == NULL && datagram->instance == NULL) {
        return NULL;
    }
    struct metric_metadata* meta = (struct metric_metadata*) malloc(sizeof(struct metric_metadata));
    *meta = (struct metric_metadata) { 0 };
    if (datagram->sampling != NULL) {
        meta->sampling = (char*) malloc(strlen(datagram->sampling) + 1);
        memcpy(meta->sampling, datagram->sampling, strlen(datagram->sampling) + 1);
    }
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
    if (meta->sampling != NULL) {
        free(meta->sampling);
    }
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

/**
 * Creates arguments for Agregator thread
 * @arg config - Application config
 * @arg parsed_channel - Parser -> Aggregator channel
 * @arg pcp_request_channel - PCP -> Aggregator channel
 * @arg pcp_response_channel - Aggregator -> PCP channel
 * @return aggregator_args
 */
struct aggregator_args*
create_aggregator_args(
    struct agent_config* config,
    chan_t* parsed_channel,
    chan_t* pcp_request_channel,
    chan_t* pcp_response_channel,
    metrics* m
) {
    struct aggregator_args* aggregator_args = (struct aggregator_args*) malloc(sizeof(struct aggregator_args));
    ALLOC_CHECK("Unable to assign memory for parser aguments.");
    aggregator_args->config = config;
    aggregator_args->parsed_datagrams = parsed_channel;
    aggregator_args->pcp_request_channel = pcp_request_channel;
    aggregator_args->pcp_response_channel = pcp_response_channel;
    aggregator_args->metrics_wrapper = m;
    return aggregator_args;
}
