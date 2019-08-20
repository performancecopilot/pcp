/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
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
#include <math.h>

#include "utils.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-duration-exact.h"
#include "aggregator-metric-duration.h"
#include "config-reader.h"

/**
 * Creates exact duration value
 * @arg value - initial value
 * @return hdr_histogram
 */
void
create_exact_duration_value(long long unsigned int value, void** out) {
    struct exact_duration_collection* collection = (struct exact_duration_collection*) malloc(sizeof(struct exact_duration_collection));
    ALLOC_CHECK("Unable to assign memory for duration values collection.");
    *collection = (struct exact_duration_collection) { 0 };
    update_exact_duration_value(value, collection);
    *out = collection;
}

/**
 * Adds item to duration collection, no ordering happens on add
 * @arg collection - Collection to which value should be added
 * @arg value - New value
 */
void
update_exact_duration_value(double value, struct exact_duration_collection* collection) {
    long int new_length = collection->length + 1;
    double** new_values = realloc(collection->values, sizeof(double*) * new_length);
    ALLOC_CHECK("Unable to allocate memory for collection value.");
    collection->values = new_values;
    collection->values[collection->length] = (double*) malloc(sizeof(double*));
    ALLOC_CHECK("Unable to allocate memory for duration collection value.");
    *(collection->values[collection->length]) = value;
    collection->length = new_length;
}

/**
 * Removes item from duration collection
 * @arg collection - Target collection
 * @arg value - Value to be removed, assuming primitive type
 * @return 1 on success
 */
int
remove_exact_duration_item(struct exact_duration_collection* collection, double value) {
    if (collection == NULL || collection->length == 0 || collection->values == NULL) {
        return 0;
    }
    int removed = 0;
    size_t i;
    for (i = 0; i < collection->length; i++) {
        if (removed) {
            collection->values[i - 1] = collection->values[i];
        } else {
            if (*(collection->values[i]) == value) {
                free(collection->values[i]);
                removed = 1;
            }
        }
    }
    if (!removed) {
        return 0;
    }
    collection = realloc(collection, sizeof(double*) * collection->length - 1);
    ALLOC_CHECK("Unable to resize exact duration collection.");
    collection->length -= 1;
    return 1;
}

static int
exact_duration_values_comparator(const void* x, const void* y) {
    int res = **(double**)x > **(double**)y;
    return res;
}

/**
 * Gets duration values meta data from given collection, as a sideeffect it sorts the values
 * @arg collection - Target collection
 * @arg instance - What information to extract
 * @return duration instance value
 */
double
get_exact_duration_instance(struct exact_duration_collection* collection, enum DURATION_INSTANCE instance) {
    if (collection == NULL || collection->length == 0 || collection->values == NULL) {
        return 0;
    }
    size_t i;
    switch (instance) {
        case DURATION_MIN: 
        {
            double min = *(collection->values[0]);
            for (i = 0; i < collection->length; i++) {
                double current = *(collection->values[i]);
                if (current < min) {
                    min = current;
                }
            }
            return min;
        }
        case DURATION_MAX:
        {
            double max = *(collection->values[0]);
            for (i = 0; i < collection->length; i++) {
                double current = *(collection->values[i]);
                if (current > max) {
                    max = current;
                }
            }
            return max;
        }
        case DURATION_AVERAGE:
        {
            long double accumulator = 0;
            for (i = 0; i < collection->length; i++) {
                accumulator += *(collection->values[i]);
            }
            return accumulator / collection->length;
        }
        case DURATION_COUNT:
            return (double)collection->length;
        case DURATION_STANDARD_DEVIATION:
        {
            double accumulator = 0;
            for (i = 0; i < collection->length; i++) {
                accumulator += *(collection->values[i]);
            }
            double average = accumulator / collection->length;
            accumulator = 0;
            for (i = 0; i < collection->length; i++) {
                double x = *(collection->values[i]) - average;
                accumulator += x * x; 
            }
            return sqrt(accumulator / (double)collection->length);
        }
        case DURATION_MEDIAN:
        case DURATION_PERCENTILE90:
        case DURATION_PERCENTILE95:
        case DURATION_PERCENTILE99:
        {
            qsort(collection->values, collection->length, sizeof(double*), exact_duration_values_comparator);
            if (instance == DURATION_MEDIAN) {
                return *(collection->values[(int)ceil((collection->length / 2.0) - 1)]);
            }
            if (instance == DURATION_PERCENTILE90) {
                return *(collection->values[((int)round((90.0 / 100.0) * (double)collection->length)) - 1]);
            }
            if (instance == DURATION_PERCENTILE95) {
                return *(collection->values[((int)round((95.0 / 100.0) * (double)collection->length)) - 1]);
            }
            if (instance == DURATION_PERCENTILE99) {
                return *(collection->values[((int)round((99.0 / 100.0) * (double)collection->length)) - 1]);
            }
            return 0;
        }
        default:
            return 0;
    }
}

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void
print_exact_duration_value(FILE* f, struct exact_duration_collection* collection) {
    fprintf(f, "min             = %lf\n", get_exact_duration_instance(collection, DURATION_MIN));
    fprintf(f, "max             = %lf\n", get_exact_duration_instance(collection, DURATION_MAX));
    fprintf(f, "median          = %lf\n", get_exact_duration_instance(collection, DURATION_MEDIAN));
    fprintf(f, "average         = %lf\n", get_exact_duration_instance(collection, DURATION_AVERAGE));
    fprintf(f, "percentile90    = %lf\n", get_exact_duration_instance(collection, DURATION_PERCENTILE90));
    fprintf(f, "percentile95    = %lf\n", get_exact_duration_instance(collection, DURATION_PERCENTILE95));
    fprintf(f, "percentile99    = %lf\n", get_exact_duration_instance(collection, DURATION_PERCENTILE99));
    fprintf(f, "count           = %lf\n", get_exact_duration_instance(collection, DURATION_COUNT));
    fprintf(f, "std deviation   = %lf\n", get_exact_duration_instance(collection, DURATION_STANDARD_DEVIATION));
}

/**
 * Frees exact duration metric value
 * @arg config
 * @arg value - value value to be freed
 */
void
free_exact_duration_value(struct agent_config* config, void* value) {
    (void)config;
    struct exact_duration_collection* collection = (struct exact_duration_collection*)value;
    if (collection != NULL) {
        if (collection->values != NULL) {
            size_t i;
            for (i = 0; i < collection->length; i++) {
                if (collection->values[i] != NULL) {
                    free(collection->values[i]);
                }
            }
            free(collection->values);
        }
        free(collection);
    }
}
