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
#include <float.h>
#include <string.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metric-labels.h"
#include "aggregator-metric-gauge.h"
#include "errno.h"
#include "utils.h"

/**
 * Creates gauge value in given destination
 * @arg config - Config
 * @arg datagram - Data to extract value from
 * @arg out - Dest pointer
 * @return 1 on success, 0 on fail. Should NOT allocate "out" when 0 is returned
 */
int
create_gauge_value(struct agent_config* config, struct statsd_datagram* datagram, void** out) {
    (void)config;   
    double new_value = 0;
    switch (datagram->explicit_sign) {
        case SIGN_MINUS:
            new_value = -1.0 * datagram->value;
            break;
        default:
            new_value = datagram->value;
    }
    *out = (double*) malloc(sizeof(double));
    ALLOC_CHECK(*out, "Unable to allocate memory for copy of metric value.");
    *(double*)*out = new_value;
    return 1;
}

/**
 * Updates gauge metric record of value subtype
 * @arg config - Config from which we know what gauge
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int
update_gauge_value(struct agent_config* config, struct statsd_datagram* datagram, void* value) {
    (void)config;
    double increment;
    switch (datagram->explicit_sign) {
        case SIGN_MINUS:
            increment = -1.0 * datagram->value;
            break;
        default:
            increment = datagram->value;
    }
    if (datagram->explicit_sign == SIGN_NONE) {
        *(double*)(value) = increment;
    } else {
        *(double*)(value) += increment;
    }    
    return 1;
}

/**
 * Print gauge metric value
 * @arg config
 * @arg f - Opened file handle
 * @arg value
 */
void
print_gauge_metric_value(struct agent_config* config, FILE* f, void* value) {
    (void)config;
    if (value != NULL) {
        fprintf(f, "value = %f\n", *(double*)(value));
    }
}

/**
 * Prints gauge metric information
 * @arg config - Config where gauge subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_gauge_metric(struct agent_config* config, FILE* f, struct metric* item) {
    (void)config;
    fprintf(f, "----------------\n");
    fprintf(f, "name = %s\n", item->name);
    fprintf(f, "type = gauge\n");
    print_gauge_metric_value(config, f, item->value);
    print_metric_meta(f, item->meta);
    print_labels(config, f, item->children);
    fprintf(f, "\n");
}

/**
 * Frees gauge metric value
 * @arg config
 * @arg value - value to be freed
 */
void
free_gauge_value(struct agent_config* config, void* value) {
    (void)config;
    if (value != NULL) {
        free(value);
    }
}
