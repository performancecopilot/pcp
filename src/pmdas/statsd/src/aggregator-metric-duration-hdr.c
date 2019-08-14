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
#include <hdr/hdr_histogram.h>
#include <stdio.h>

#include "utils.h"
#include "aggregators.h"
#include "aggregator-metric-duration.h"
#include "aggregator-metric-duration-hdr.h"
#include "config-reader.h"

/**
 * Creates hdr duration value
 * @arg value - Initial value
 * @return hdr_histogram
 */
void
create_hdr_duration_value(long long unsigned int value, void** out) {
    struct hdr_histogram* histogram;
    hdr_init(1, INT64_C(3600000000), 3, &histogram);
    ALLOC_CHECK("Unable to allocate memory for histogram");
    hdr_record_value(histogram, value);
    *out = histogram;
}

/**
 * Updates hdr duration value
 * @arg histogram - Histogram to update 
 * @arg value - Value to record
 */
void
update_hdr_duration_value(long long unsigned int value, struct hdr_histogram* histogram) {
    hdr_record_value(histogram, value);
}

/**
 * Gets duration values meta data histogram
 * @arg collection - Target collection
 * @arg instance - Placeholder for data population
 * @return duration instance value
 */
double
get_hdr_histogram_duration_instance(struct hdr_histogram* histogram, enum DURATION_INSTANCE instance) {
    if (histogram == NULL) {
        return 0;
    }
    switch (instance) {
        case DURATION_MIN:
            return hdr_min(histogram);
        case DURATION_MAX:
            return hdr_max(histogram);     
        case DURATION_COUNT:
            return histogram->total_count;
        case DURATION_AVERAGE:
            return hdr_mean(histogram);
        case DURATION_MEDIAN:
            return hdr_value_at_percentile(histogram, 50);
        case DURATION_PERCENTILE90:
            return hdr_value_at_percentile(histogram, 90);
        case DURATION_PERCENTILE95:
            return hdr_value_at_percentile(histogram, 95);
        case DURATION_PERCENTILE99:
            return hdr_value_at_percentile(histogram, 99);
        case DURATION_STANDARD_DEVIATION:
            return hdr_stddev(histogram);
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
print_hdr_duration_value(FILE* f, struct hdr_histogram* histogram) {
    hdr_percentiles_print(
        histogram,
        f,
        5,
        1.0,
        CLASSIC
    );
}

/**
 * Frees exact duration metric value
 * @arg config
 * @arg value - value value to be freed
 */
void
free_hdr_duration_value(struct agent_config* config, void* value) {
    (void)config;
    if (value != NULL) {
        hdr_close((struct hdr_histogram*)value);
    }
}
