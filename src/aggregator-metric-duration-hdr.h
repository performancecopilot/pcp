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
struct hdr_histogram*
create_hdr_duration_value(long long unsigned int value);

/**
 * Updates hdr duration value
 * @arg histogram - Histogram to update 
 * @arg value - Value to record
 */
void
update_hdr_duration_value(struct hdr_histogram* histogram, long long unsigned int value);

/**
 * Gets duration values meta data from given collection, as a sideeffect it sorts the values
 * @arg collection - Target collection
 * @arg out - Placeholder for data population
 * @return 1 on success
 */
int
get_hdr_duration_values_meta(struct hdr_histogram* histogram, struct duration_values_meta** out);

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void
print_hdr_durations(FILE* f, struct hdr_histogram* histogram);

/**
 * Frees exact duration metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void
free_hdr_duration_value(struct agent_config* config, struct metric* item);

#endif
