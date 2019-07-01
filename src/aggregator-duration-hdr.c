#include <hdr/hdr_histogram.h>
#include <stdio.h>

#include "utils.h"
#include "aggregators.h"
#include "aggregator-duration-hdr.h"
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
