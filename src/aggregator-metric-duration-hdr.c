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
struct hdr_histogram*
create_hdr_duration_value(long long unsigned int value) {
    struct hdr_histogram* histogram;
    hdr_init(1, INT64_C(3600000000), 3, &histogram);
    ALLOC_CHECK("Unable to allocate memory for histogram");
    hdr_record_value(histogram, value);
    return histogram;
}

/**
 * Updates hdr duration value
 * @arg histogram - Histogram to update 
 * @arg value - Value to record
 */
void
update_hdr_duration_value(struct hdr_histogram* histogram, long long unsigned int value) {
    hdr_record_value(histogram, value);
}

/**
 * Gets duration values meta data from given collection, as a sideeffect it sorts the values
 * @arg collection - Target collection
 * @arg out - Placeholder for data population
 * @return 1 on success
 */
int
get_hdr_duration_values_meta(struct hdr_histogram* histogram, struct duration_values_meta** out) {
    if (histogram == NULL) {
        return 0;
    }
    (*out)->min = hdr_min(histogram);
    (*out)->max = hdr_max(histogram);
    (*out)->count = histogram->total_count;
    (*out)->average = hdr_mean(histogram);
    (*out)->median = hdr_value_at_percentile(histogram, 50);
    (*out)->percentile90 = hdr_value_at_percentile(histogram, 90);
    (*out)->percentile95 = hdr_value_at_percentile(histogram, 95);
    (*out)->percentile99 = hdr_value_at_percentile(histogram, 99);
    (*out)->std_deviation = hdr_stddev(histogram);
    return 1;
}

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void
print_hdr_durations(FILE* f, struct hdr_histogram* histogram) {
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
 * @arg metric - Metric value to be freed
 */
void
free_hdr_duration_value(struct agent_config* config, struct metric* item) {
    (void)config;
    if (item->value != NULL) {
        hdr_close((struct hdr_histogram*)item->value);
    }
}
