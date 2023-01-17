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
#include "aggregators.h"
#include "parsers-utils.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-labels.h"
#include "dict-callbacks.h"
#include "aggregator-metric-counter.h"
#include "aggregator-metric-gauge.h"
#include "aggregator-metric-duration.h"
#include "utils.h"

/**
 * Creates new hashtable for labels of metric
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper
 * @arg item - Parent item
 * 
 * Synchronized by 
 */
static void
create_labels_dict(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    struct metric* item
) {
    pthread_mutex_lock(&container->mutex);
    /**
     * Callbacks for metrics hashtable
     */
    static dictType metric_label_dict_callbacks = {
        .hashFunction	= str_hash_callback,
        .keyCompare		= str_compare_callback,
        .keyDup		    = str_duplicate_callback,
        .keyDestructor	= str_hash_free_callback,
        .valDestructor	= metric_label_free_callback,
    };
    labels* children = dictCreate(&metric_label_dict_callbacks, container->metrics_privdata);
    item->children = children;
    pthread_mutex_unlock(&container->mutex);
}


/**
 * Futher processes datagram that also possesses labels
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper
 * @arg item - Metric serving as root
 * @arg datagram - Datagram to be processed
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
process_labeled_datagram(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    struct metric* item,
    struct statsd_datagram* datagram
) {
    char throwing_away_msg[] = "Throwing away metric:";
    int correct_semantics = item->type == datagram->type;
    if (!correct_semantics) {
        METRIC_PROCESSING_ERR_LOG("%s %s, REASON: metric type doesn't match with root record.", throwing_away_msg, datagram->name);
        return 0;
    }
    int labeled_children_dict_exists = item->children != NULL;
    if (!labeled_children_dict_exists) {
        create_labels_dict(config, container, item);
    }
    char* label_key = create_metric_dict_key(datagram->tags);
    if (label_key == NULL) {
        METRIC_PROCESSING_ERR_LOG("%s %s, REASON: unable to create hashtable key for labeled child.", throwing_away_msg, datagram->name);
    }
    struct metric_label* label;
    int label_exists = find_label_by_name(container, item, label_key, &label);
    int status = 0;
    if (label_exists) {
        int update_success = update_metric_value(config, container, label->type, datagram, &label->value);
        if (update_success != 1) {
            METRIC_PROCESSING_ERR_LOG("%s %s, REASON: semantically incorrect values.", throwing_away_msg, datagram->name);
            status = 0;
        } else {
            status = update_success;
        }
    } else {
        int create_success = create_label(config, item, datagram, &label);
        if (create_success) {
            add_label(container, item, label_key, label);
            status = create_success;
        } else {
            METRIC_PROCESSING_ERR_LOG("%s %s, REASON: unable to create label.", throwing_away_msg, datagram->name);
            status = 0;
        }
    }
    free(label_key);
    return status;
}

/**
 * Find label by name
 * @arg container - Metrics container
 * @arg metric - Within which metric to search for label
 * @arg key - Label key to find
 * @arg out - Placeholder label
 * @return 1 when any found, 0 when not
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
find_label_by_name(
    struct pmda_metrics_container* container,
    struct metric* item,
    char* key,
    struct metric_label** out
) {
    pthread_mutex_lock(&container->mutex);
    dictEntry* result = dictFind(item->children, key);
    if (result == NULL) {
        pthread_mutex_unlock(&container->mutex);
        return 0;
    }
    if (out != NULL) {
        struct metric_label* label = (struct metric_label*)result->v.val;
        *out = label;
    }
    pthread_mutex_unlock(&container->mutex);
    return 1;
}

static char*
create_instance_label_segment_str(char* tags) {
    char buffer[JSON_BUFFER_SIZE] = {'\0'};
    size_t tags_length = strlen(tags) + 1;
    if (tags_length > JSON_BUFFER_SIZE) {
        return NULL;
    }
    size_t tag_char_index;
    size_t buffer_char_index = 0;
    for (tag_char_index = 0; tag_char_index < tags_length; tag_char_index++) {
        if (tags[tag_char_index] == '{' ||
            tags[tag_char_index] == '}' ||
            tags[tag_char_index] == '"') {
            continue;
        }
        if (tags[tag_char_index] == ':') {
            // insert key and value separator '='
            buffer[buffer_char_index] = '=';
            buffer_char_index++;
            continue;
        }
        if (tags[tag_char_index] == ',') {
            buffer[buffer_char_index] = ':';
            buffer_char_index++;
            buffer[buffer_char_index] = ':';
            buffer_char_index++;
            continue;
        }
        buffer[buffer_char_index] = tags[tag_char_index];
        buffer_char_index++;
    }
    size_t label_segment_length = strlen(buffer) + 1;
    char* label_segment = (char*) malloc(sizeof(char) * label_segment_length);
    memcpy(label_segment, buffer, label_segment_length);
    return label_segment;
}

/**
 * Creates metric label child
 * @arg config - Agent config
 * @arg item - Parent metric
 * @arg datagram - Child's data
 * @arg out - Placholder metric label
 * @return 1 on success, 0 on fail
 */
int
create_label(
    struct agent_config* config,
    struct metric* item,
    struct statsd_datagram* datagram,
    struct metric_label** out
) {
    struct metric_label* label = (struct metric_label*) malloc(sizeof(struct metric_label));
    *out = label;
    size_t labels_length = strlen(datagram->tags) + 1;
    (*out)->labels = (char*) malloc(sizeof(char) * labels_length);
    ALLOC_CHECK((*out)->labels, "Unable to allocate memory for labels string in metric label record.");
    memcpy((*out)->labels, datagram->tags, labels_length);
    struct metric_label_metadata* meta = 
        (struct metric_label_metadata*) malloc(sizeof(struct metric_label_metadata));
    ALLOC_CHECK(meta, "Unable to allocate memory for metric label metadata.");
    (*out)->meta = meta;
    (*out)->type = METRIC_TYPE_NONE;
    meta->instance_label_segment_str = NULL;
    char* label_segment_identifier = create_instance_label_segment_str(datagram->tags);
    if (label_segment_identifier == NULL) {
        free_metric_label(config, label);
        return 0;
    }
    meta->instance_label_segment_str = label_segment_identifier;
    (*out)->pair_count = datagram->tags_pair_count;
    int status = 0;
    switch (item->type) {
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
    (*out)->type = item->type;
    if (!status) {
        free_metric_label(config, label);
    }    
    return status;
}

/**
 * Adds label to metric's children hashtable
 * @arg container - Metrics container 
 * @arg item - Metric containing children hashtable target
 * @arg key - Label key
 * @arg label - Label to be saved
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
add_label(struct pmda_metrics_container* container, struct metric* item, char* key, struct metric_label* label) {
    pthread_mutex_lock(&container->mutex);
    dictAdd(item->children, key, label);
    container->generation += 1;
    item->meta->pcp_instance_change_requested = 1;
    pthread_mutex_unlock(&container->mutex);
}

/**
 * Frees metric label value
 * @arg config - Contains info about aggregation used
 * @arg metadata - Metadata to be freed
 */
void
free_metric_label(struct agent_config* config, struct metric_label* label) {
    if (label != NULL) {
        if (label->labels != NULL) {
            free(label->labels);
        }
        free_metric_label_metadata(label->meta);
        switch (label->type) {
            case METRIC_TYPE_COUNTER:
                free_counter_value(config, label->value);
                break;
            case METRIC_TYPE_GAUGE:
                free_gauge_value(config, label->value);
                break;
            case METRIC_TYPE_DURATION:
                free_duration_value(config, label->value);
                break;
            case METRIC_TYPE_NONE:
                // not actually a metric
                break;
        }
        free(label);
    }
}

/**
 * Prints metric label meta information
 * @arg config
 * @arg f - Opened file handle
 * @arg item - Labels to print out
 */
static void
print_label_meta(struct agent_config* config, FILE* f, struct metric_label_metadata* meta) {
    (void)config;
    if (meta == NULL) return;
    if (meta->instance_label_segment_str != NULL) {
        fprintf(
            f,
            "instance segment = %s\n",
            meta->instance_label_segment_str
        );
    }
}

/**
 * Prints metric label information
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg item - Labels to print out
 */
void
print_labels(struct agent_config* config, FILE* f, labels* l) {
    if (l == NULL) return;
    dictIterator* iterator = dictGetSafeIterator(l);
    dictEntry* current;
    long int count = 1;
    while ((current = dictNext(iterator)) != NULL) {
        struct metric_label* item = (struct metric_label*)current->v.val;
        fprintf(f, "---\n");
        fprintf(f, "#%ld Label: \n", count);
        if (item->labels != NULL) {
            fprintf(f, "-> desc = %s\n", item->labels);
        }
        fprintf(f, "-> ");
        print_label_meta(config, f, item->meta);
        fprintf(f, "-> pair count = %d\n", item->pair_count);
        if (item->type != METRIC_TYPE_NONE) {
            fprintf(f, "-> ");
            switch (item->type) {
                case METRIC_TYPE_COUNTER:
                    print_counter_metric_value(config, f, item->value);
                    break;
                case METRIC_TYPE_GAUGE:
                    print_gauge_metric_value(config, f, item->value);
                    break;
                case METRIC_TYPE_DURATION:
                    print_duration_metric_value(config, f, item->value);
                    break;
                case METRIC_TYPE_NONE:
                    // not an actual metric error case
                    break;
            }
        }
        count++;
    }
    fprintf(f, "---\n");
    dictReleaseIterator(iterator);
}

/**
 * Frees metric label metadata
 * @arg metadata - Metadata to be freed
 */
void
free_metric_label_metadata(struct metric_label_metadata* meta) {
    if (meta != NULL) {
        if (meta->instance_label_segment_str != NULL) {
            free(meta->instance_label_segment_str);
        }
        free(meta);
    }
}
