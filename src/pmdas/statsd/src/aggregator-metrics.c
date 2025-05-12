/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
#include <stddef.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/dict.h>
#include <chan/chan.h>
#include <string.h>

#include "utils.h"
#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-labels.h"
#include "aggregator-metric-counter.h"
#include "aggregator-metric-gauge.h"
#include "aggregator-metric-duration.h"
#include "dict-callbacks.h"
#include "pmdastatsd.h"
#include "domain.h"

/**
 * Creates new pmda_metrics_container structure, initializes all stats to 0
 */
struct pmda_metrics_container*
init_pmda_metrics(struct agent_config* config) {
    /**
     * Callbacks for metrics hashtable
     */
    static dictType metric_dict_callbacks = {
        .hashFunction	= str_hash_callback,
        .keyCompare		= str_compare_callback,
        .keyDup		    = str_duplicate_callback,
        .keyDestructor	= str_hash_free_callback,
        .valDestructor	= metric_free_callback,
    };
    struct pmda_metrics_container* container =
        (struct pmda_metrics_container*) malloc(sizeof(struct pmda_metrics_container));
    ALLOC_CHECK(container, "Unable to create PMDA metrics container.");
    pthread_mutex_init(&container->mutex, NULL);
    struct pmda_metrics_dict_privdata* dict_data = 
        (struct pmda_metrics_dict_privdata*) malloc(sizeof(struct pmda_metrics_dict_privdata));    
    ALLOC_CHECK(dict_data, "Unable to create priv PMDA metrics container data.");
    dict_data->config = config;
    dict_data->container = container;
    metrics* m = dictCreate(&metric_dict_callbacks, dict_data);
    container->metrics = m;
    container->generation = 0;
    container->metrics_privdata = dict_data;
    return container;
}

/**
 * Creates STATSD metric hashtable key for use in hashtable related functions (find_metric_by_name, check_metric_name_available)
 * @return new key
 */
char*
create_metric_dict_key(char* key) {
    size_t maximum_key_size = 2048;
    char buffer[maximum_key_size]; // maximum key size
    int key_size = pmsprintf(
        buffer,
        maximum_key_size,
        "%s",
        key
    ) + 1;
    char* result = malloc(key_size);
    ALLOC_CHECK(result, "Unable to allocate memory for hashtable key");
    memcpy(result, &buffer, key_size);
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
process_metric(struct agent_config* config, struct pmda_metrics_container* container, struct statsd_datagram* datagram) {
    struct metric* item;
    char throwing_away_msg[] = "Throwing away metric:";
    char* metric_key = create_metric_dict_key(datagram->name);
    if (metric_key == NULL) {
        METRIC_PROCESSING_ERR_LOG("%s %s, REASON: unable to create hashtable key for metric record.", throwing_away_msg, datagram->name);
        return 0;
    }
    int status = 0;
    int metric_exists = find_metric_by_name(container, metric_key, &item);
    if (metric_exists) {
        int datagram_contains_tags = datagram->tags != NULL;
        if (!datagram_contains_tags) {
            int res = update_metric_value(config, container, item->type, datagram, &item->value);
            if (res == 0) {
                METRIC_PROCESSING_ERR_LOG("%s %s, REASON: semantically incorrect values.", throwing_away_msg, datagram->name);
                status = 0;
            } else if (res == -1) {
                METRIC_PROCESSING_ERR_LOG("%s %s, REASON: metric of same name but different type is already recorded.", throwing_away_msg, datagram->name);
                status = 0;
            } else {
                status = 1;
            }
        } else {
            int label_success = process_labeled_datagram(config, container, item, datagram);
            status = label_success;
        }
    } else {
        int name_available = check_metric_name_available(container, metric_key);
        if (name_available) {
            int correct_semantics = create_metric(config, datagram, &item);
            if (correct_semantics) {
                add_metric(container, metric_key, item);
                int datagram_contains_tags = datagram->tags != NULL;
                status = 1;
                if (datagram_contains_tags) {
                    status = 0;
                    int label_success = process_labeled_datagram(config, container, item, datagram);
                    if (!label_success) {
                        remove_metric(container, metric_key);
                    } else {
                        mark_metric_as_committed(container, item);
                        status = 1;
                    }
                } else {
                    mark_metric_as_committed(container, item);
                }
            } else {
                METRIC_PROCESSING_ERR_LOG("%s %s, REASON: semantically incorrect values.", throwing_away_msg, datagram->name);
                status = 0;
            }
        } else {
            METRIC_PROCESSING_ERR_LOG("%s %s, REASON: name is not available. (blocklisted?)", throwing_away_msg, datagram->name);
            status = 0;
        }
    }
    free(metric_key);
    return status;
}

/**
 * Frees metric
 * @arg config - Agent config
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
    if (item->children != NULL) {
        dictRelease(item->children);
    }
    switch (item->type) {
        case METRIC_TYPE_COUNTER:
            free_counter_value(config, item->value);
            break;
        case METRIC_TYPE_GAUGE:
            free_gauge_value(config, item->value);
            break;
        case METRIC_TYPE_DURATION:
            free_duration_value(config, item->value);
            break;
        case METRIC_TYPE_NONE:
            // not actually a metric
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
        if (meta->pcp_name) {
            fprintf(f, "pcp_name = %s\n", meta->pcp_name);
        }
        fprintf(f, "pmid = %s\n", pmIDStr(meta->pmid));
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
    VERBOSE_LOG(0, "Writing metrics to file...");
    pthread_mutex_lock(&container->mutex);
    metrics* m = container->metrics;
    if (strlen(config->debug_output_filename) == 0) {
        pthread_mutex_unlock(&container->mutex);
        return; 
    }
    int sep = pmPathSeparator();
    char debug_output[MAXPATHLEN];
    pmsprintf(
        debug_output,
        MAXPATHLEN,
        "%s" "%c" "pmcd" "%c" "statsd_%s",
        pmGetConfig("PCP_LOG_DIR"),
        sep, sep, config->debug_output_filename);
    FILE* f;
    f = fopen(debug_output, "a+");
    if (f == NULL) {
        pthread_mutex_unlock(&container->mutex);
        VERBOSE_LOG(0, "Unable to open file for output.");
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
                // not actually a metric error case
                break;
        }
        count++;
    }
    dictReleaseIterator(iterator);
    fprintf(f, "----------------\n");
    fprintf(f, "Total number of records: %lu \n", count);
    fclose(f);    
    pthread_mutex_unlock(&container->mutex);
    VERBOSE_LOG(0, "Wrote metrics to debug file.");
}

/**
 * Finds metric by name
 * @arg container - Metrics container
 * @arg key - Metric key to find
 * @arg out - Placeholder metric
 * @return 1 when any found, 0 when not
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
find_metric_by_name(struct pmda_metrics_container* container, char* key, struct metric** out) {
    pthread_mutex_lock(&container->mutex);
    metrics* m = container->metrics;
    dictEntry* result = dictFind(m, key);
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
 * @return 1 on success, 0 on fail
 */
int
create_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out) {
    struct metric* item = (struct metric*) malloc(sizeof(struct metric));
    ALLOC_CHECK(item, "Unable to allocate memory for metric.");
    *out = item;
    size_t len = strlen(datagram->name) + 1;
    (*out)->name = (char*) malloc(len);
    ALLOC_CHECK((*out)->name, "Unable to allocate memory for copy of metric name.");
    memcpy((*out)->name, datagram->name, len);
    (*out)->meta = create_metric_meta(datagram);
    (*out)->children = NULL;
    (*out)->committed = 0;
    int status = 0; 
    (*out)->value = NULL;
    // this metric doesn't have root value
    if (datagram->tags != NULL) {
        (*out)->value = NULL;
        status = 1;
    } else {
        switch (datagram->type) {
            case METRIC_TYPE_COUNTER:
                status = create_counter_value(config, datagram, &(*out)->value);
                break;
            case METRIC_TYPE_GAUGE:
                status = create_gauge_value(config, datagram, &(*out)->value);
                break;
            case METRIC_TYPE_DURATION:
                status = create_duration_value(config, datagram, &(*out)->value);
                break;
            default:
                status = 0;
        }
    }
    if (status) {
        (*out)->type = datagram->type;
    } else {
        free_metric(config, item);
    }
    return status;
}

/**
 * Adds metric to hashtable
 * @arg container - Metrics container 
 * @arg item - Metric to be saved
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
add_metric(struct pmda_metrics_container* container, char* key, struct metric* item) {
    pthread_mutex_lock(&container->mutex);
    dictAdd(container->metrics, key, item);
    container->generation += 1;
    pthread_mutex_unlock(&container->mutex);
}

/**
 * Removes metric from hashtable
 * @arg container - Metrics container
 * @arg key - Metric's hashtable key
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
remove_metric(struct pmda_metrics_container* container, char* key) {
    pthread_mutex_lock(&container->mutex);
    dictDelete(container->metrics, key);
    container->generation += 1;
    pthread_mutex_unlock(&container->mutex);
}

/**
 * Updates metric record
 * @arg config - Agent config
 * @arg container - Metrics container
 * @arg type - What type the metric value is
 * @arg datagram - Data with which to update
 * @arg value - Dest value
 * @return 1 on success, 0 when update itself fails, -1 when metric with same name but different type is already recorded
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
update_metric_value(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    enum METRIC_TYPE type,
    struct statsd_datagram* datagram,
    void** value
) {
    pthread_mutex_lock(&container->mutex);
    int status = 0;
    if (datagram->type != type) {
        status = -1;
    } else {
        switch (type) {
            case METRIC_TYPE_COUNTER:
                if (*value == NULL) {
                    status = create_counter_value(config, datagram, value);
                } else {
                    status = update_counter_value(config, datagram, *value);
                }
                break;
            case METRIC_TYPE_GAUGE:
                if (*value == NULL) {
                    status = create_gauge_value(config, datagram, value);
                } else {
                    status = update_gauge_value(config, datagram, *value);
                }
                break;
            case METRIC_TYPE_DURATION:
                if (*value == NULL) {
                    status = create_duration_value(config, datagram, value);
                } else {
                    status = update_duration_value(config, datagram, *value);
                }
                break;
            case METRIC_TYPE_NONE:
                status = 0;
                break;
        }
    }
    pthread_mutex_unlock(&container->mutex);
    return status;
}

/**
 * Checks if given metric name is available (it isn't recorded yet or is blocklisted)
 * @arg container - Metrics container
 * @arg key - Key of metric
 * @return 1 on success else 0
 */
int
check_metric_name_available(struct pmda_metrics_container* container, char* key) {
    /**
     * Metric names that won't be saved into hashtable 
     * - these names are already taken by metric stats
     */
    static const char* const g_blocklist[] = {
        "pmda.received",
        "pmda.parsed",
        "pmda.aggregated",
        "pmda.dropped",
        "pmda.metrics_tracked",
        "pmda.time_spent_aggregating",
        "pmda.time_spent_parsing",
        "pmda.settings.max_udp_packet_size",
        "pmda.settings.max_unprocessed_packets",
        "pmda.settings.verbose",
        "pmda.settings.debug",
        "pmda.settings.debug_output_filename",
        "pmda.settings.port",
        "pmda.settings.parser_type",
        "pmda.settings.duration_aggregation_type"
    };
    size_t i;
    for (i = 0; i < sizeof(g_blocklist) / sizeof(g_blocklist[0]); i++) {
        if (strcmp(key, g_blocklist[i]) == 0) return 0;
    }
    if (!find_metric_by_name(container, key, NULL)) {
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
    struct metric_metadata* meta = (struct metric_metadata*) malloc(sizeof(struct metric_metadata));
    ALLOC_CHECK(meta, "Unable to allocate memory for metric metadata.");
    *meta = (struct metric_metadata) { 0 };
    meta->pmid = PM_ID_NULL;
    if (datagram->type == METRIC_TYPE_DURATION) {
        meta->pmindom = pmInDom_build(STATSD, STATSD_METRIC_DEFAULT_DURATION_INDOM);
    } else {
        meta->pmindom = pmInDom_build(STATSD, STATSD_METRIC_DEFAULT_INDOM);
    }
    char name[1024];
    size_t len = pmsprintf(name, 1024, "statsd.%s", datagram->name) + 1;
    meta->pcp_name = (char*) malloc(sizeof(char) * len);
    ALLOC_CHECK(meta->pcp_name, "Unable to allocate memory for metric pcp name");
    memcpy((char*)meta->pcp_name, name, len);
    meta->pcp_metric_index = 0;
    meta->pcp_instance_map = NULL;
    meta->pcp_instance_change_requested = 0;
    return meta;
}

/**
 * Frees metric metadata
 * Doesn't free individual pointers of meta->pcp_instance_map->labels as those are also pointed at
 * by metric_label* labels field
 * @arg meta - Metadata to be freed
 */
void
free_metric_metadata(struct metric_metadata* meta) {
    if (meta != NULL) {
        if (meta->pcp_instance_map != NULL) {
            if (meta->pcp_instance_map->labels != NULL) {
                free(meta->pcp_instance_map->labels);
            }
            free(meta->pcp_instance_map);
        }
        if (meta->pcp_name != NULL) {
            free((char*)meta->pcp_name);
        }
        free(meta);
    }
}

/**
 * Special case handling - this confirms that label was also added to metric before it actually is processed
 * @arg container - Metrics container
 * @arg item - Metric to be updated
 * 
 * Synchronized by mutex on pmda_metrics_container struct
 */
void
mark_metric_as_committed(struct pmda_metrics_container* container, struct metric* item) {
    pthread_mutex_lock(&container->mutex);
    item->committed = 1;
    pthread_mutex_unlock(&container->mutex);
}
