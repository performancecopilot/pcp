#include <math.h>

#include "utils.h"
#include "aggregators.h"
#include "aggregator-metric-duration-exact.h"
#include "aggregator-metric-duration.h"
#include "config-reader.h"

/**
 * Creates exact duration value
 * @arg value - initial value
 * @return hdr_histogram
 */
struct exact_duration_collection*
create_exact_duration_value(long long unsigned int value) {
    struct exact_duration_collection* collection = (struct exact_duration_collection*) malloc(sizeof(struct exact_duration_collection));
    ALLOC_CHECK("Unable to assign memory for duration values collection.");
    *collection = (struct exact_duration_collection) { 0 };
    update_exact_duration_value(collection, value);
    return collection;
}

/**
 * Adds item to duration collection, no ordering happens on add
 * @arg collection - Collection to which value should be added
 * @arg value - New value
 */
void
update_exact_duration_value(struct exact_duration_collection* collection, double value) {
    long int new_length = collection->length + 1;
    collection->values = realloc(collection->values, sizeof(double*) * new_length);
    ALLOC_CHECK("Unable to allocate memory for collection value.");
    collection->values[collection->length] = malloc(sizeof(double*));
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
 * @arg out - Placeholder for data population
 * @return 1 on success
 */
int
get_exact_duration_values_meta(struct exact_duration_collection* collection, struct duration_values_meta** out) {
    if (collection == NULL || collection->length == 0 || collection->values == NULL) {
        return 0;
    }
    qsort(collection->values, collection->length, sizeof(double*), exact_duration_values_comparator);
    double accumulator = 0;
    double min = *(collection->values[0]);
    double max = *(collection->values[0]);
    size_t i;
    for (i = 0; i < collection->length; i++) {
        double current = *(collection->values[i]);
        if (i == 0) {
            min = current;
            max = current;            
        }
        if (current > max) {
            max = current;
        }
        if (current < min) {
            min = current;
        }
        accumulator += current;
    }
    (*out)->min = min;
    (*out)->max = max;
    (*out)->median = *(collection->values[(int)ceil((collection->length / 2.0) - 1)]);
    (*out)->average = accumulator / collection->length;
    (*out)->percentile90 = *(collection->values[((int)round((90.0 / 100.0) * (double)collection->length)) - 1]);
    (*out)->percentile95 = *(collection->values[((int)round((95.0 / 100.0) * (double)collection->length)) - 1]);
    (*out)->percentile99 = *(collection->values[((int)round((99.0 / 100.0) * (double)collection->length)) - 1]);
    (*out)->count = collection->length;
    accumulator = 0;
    for (i = 0; i < collection->length; i++) {
        double x = *(collection->values[i]) - (*out)->average;
        accumulator += x * x; 
    }
    (*out)->std_deviation = sqrt(accumulator / (*out)->count);
    return 1;
}

/**
 * Prints duration collection metadata in human readable way
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg collection - Target collection
 */
void
print_exact_durations(FILE* f, struct exact_duration_collection* collection) {
    struct duration_values_meta* meta = 
        (struct duration_values_meta*) malloc(sizeof(struct duration_values_meta));
    get_exact_duration_values_meta(collection, &meta);
    fprintf(f, "min             = %lf\n", meta->min);
    fprintf(f, "max             = %lf\n", meta->max);
    fprintf(f, "median          = %lf\n", meta->median);
    fprintf(f, "average         = %lf\n", meta->average);
    fprintf(f, "percentile90    = %lf\n", meta->percentile90);
    fprintf(f, "percentile95    = %lf\n", meta->percentile95);
    fprintf(f, "percentile99    = %lf\n", meta->percentile99);
    fprintf(f, "count           = %lf\n", meta->count);
    fprintf(f, "std deviation   = %lf\n", meta->std_deviation);
}

/**
 * Frees exact duration metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void
free_exact_duration_value(struct agent_config* config, struct metric* item) {
    (void)config;
    struct exact_duration_collection* collection = (struct exact_duration_collection*)item->value;
    if (collection != NULL) {
        size_t i;
        for (i = 0; i < collection->length; i++) {
            if (collection->values[i] != NULL) {
                free(collection->values[i]);
            }
        }
        free(collection);
    }
}
