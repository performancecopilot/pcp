#ifndef AGGREGATOR_DURATION_EXACT_
#define AGGREGATOR_DURATION_EXACT_

#include <stdio.h>

#include "config-reader.h"

/**
 * Represents basic duration aggregation unit
 */
typedef struct exact_duration_collection {
    double** values;
    long int length;
} exact_duration_collection;

/**
 * Collection of metadata of some duration collection 
 */
typedef struct duration_values_meta {
    double min;
    double max;
    double median;
    double average;
    double percentile90;
    double percentile95;
    double percentile99;
    double count;
    double std_deviation;
} duration_values_meta;

/**
 * Creates exact duration value
 * @arg value - initial value
 * @return hdr_histogram
 */
exact_duration_collection* create_exact_duration_value(long long unsigned int value);

/**
 * Adds item to duration collection, no ordering happens on add
 * @arg collection - Collection to which value should be added
 * @arg value - New value
 */
void update_exact_duration_value(exact_duration_collection* collection, double value);

/**
 * Removes item from duration collection
 * @arg collection - Target collection
 * @arg value - Value to be removed, assuming primitive type
 * @return 1 on success
 */
int remove_exact_duration_item(exact_duration_collection* collection, double value);

/**
 * Gets duration values meta data from given collection, as a sideeffect it sorts the values
 * @arg collection - Target collection
 * @arg out - Placeholder for data population
 * @return 1 on success
 */
int get_exact_duration_values_meta(exact_duration_collection* collection, duration_values_meta* out);

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void print_exact_durations(FILE* f, exact_duration_collection* collection);

/**
 * Frees exact duration metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void free_exact_duration_value(agent_config* config, metric* item);

#endif
