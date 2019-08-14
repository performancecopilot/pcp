#ifndef AGGREGATOR_DURATION_HDR_
#define AGGREGATOR_DURATION_HDR_

#include <hdr/hdr_histogram.h>
#include <stdio.h>

#include "aggregator-metric-duration.h"
#include "config-reader.h"

/**
 * Creates hdr duration value
 * @arg value - Initial value
 * @return hdr_histogram
 */
void
create_hdr_duration_value(long long unsigned int value, void** out);

/**
 * Updates hdr duration value
 * @arg histogram - Histogram to update 
 * @arg value - Value to record
 */
void
update_hdr_duration_value(long long unsigned int value, struct hdr_histogram* histogram);

/**
 * Gets duration values meta data histogram
 * @arg collection - Target collection
 * @arg instance - Placeholder for data population
 * @return duration instance value
 */
double
get_hdr_histogram_duration_instance(struct hdr_histogram* histogram, enum DURATION_INSTANCE instance);

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void
print_hdr_duration_value(FILE* f, struct hdr_histogram* histogram);

/**
 * Frees exact duration metric value
 * @arg config
 * @arg value - value value to be freed
 */
void
free_hdr_duration_value(struct agent_config* config, void* value);

#endif
