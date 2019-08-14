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
    struct pmda_metrics_container* container = ((struct pmda_metrics_dict_privdata*)privdata)->container;
    pthread_mutex_lock(&container->mutex);
    free_metric_label(config, (struct metric_label*)val);
    pthread_mutex_unlock(&container->mutex);
}

void
metric_free_callback(void* privdata, void* val)
{
    struct agent_config* config = ((struct pmda_metrics_dict_privdata*)privdata)->config;
    struct pmda_metrics_container* container = ((struct pmda_metrics_dict_privdata*)privdata)->container;
    pthread_mutex_lock(&container->mutex);
    free_metric(config, (struct metric*)val);
    pthread_mutex_unlock(&container->mutex);
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
    char* duplicate = malloc(strlen(key));
    ALLOC_CHECK("Unable to duplicate key.");
    strcpy(duplicate, key);
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
