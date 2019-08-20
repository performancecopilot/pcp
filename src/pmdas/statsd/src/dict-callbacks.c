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
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-labels.h"
#include "dict-callbacks.h"
#include "utils.h"

void
metric_label_free_callback(void* privdata, void* val) 
{
    struct agent_config* config = ((struct pmda_metrics_dict_privdata*)privdata)->config;
    free_metric_label(config, (struct metric_label*)val);
}

void
metric_free_callback(void* privdata, void* val)
{
    struct agent_config* config = ((struct pmda_metrics_dict_privdata*)privdata)->config;
    free_metric(config, (struct metric*)val);
}

void
str_hash_free_callback(void* privdata, void* key) {
    if (key != NULL) {
        free(key);
    }
}

void*
str_duplicate_callback(void* privdata, const void* key)
{
    (void)privdata;
    size_t length = strlen(key) + 1;
    char* duplicate = malloc(length);
    ALLOC_CHECK("Unable to duplicate key.");
    memcpy(duplicate, key, length);
    return duplicate;
}

int
str_compare_callback(void* privdata, const void* key1, const void* key2)
{
    (void)privdata;
    return strcmp((char*)key1, (char*)key2) == 0;
}

uint64_t
str_hash_callback(const void* key)
{
    return dictGenCaseHashFunction((unsigned char*)key, strlen((char*)key));
}
