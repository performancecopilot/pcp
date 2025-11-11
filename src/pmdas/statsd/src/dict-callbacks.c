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
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-labels.h"
#include "dict-callbacks.h"
#include "utils.h"

/* Static variable to store config for callbacks that need it */
static struct agent_config* current_dict_config = NULL;

void
metric_label_free_callback(void* val) 
{
    if (current_dict_config != NULL) {
	free_metric_label(current_dict_config, (struct metric_label*)val);
    }
}

void
metric_free_callback(void* val)
{
    if (current_dict_config != NULL) {
	free_metric(current_dict_config, (struct metric*)val);
    }
}

void
str_hash_free_callback(void* key) {
    if (key != NULL) {
        free(key);
    }
}

void*
str_duplicate_callback(const void* key)
{
    size_t length = strlen(key) + 1;
    char* duplicate = malloc(length);
    ALLOC_CHECK(duplicate, "Unable to duplicate key.");
    memcpy(duplicate, key, length);
    return duplicate;
}

int
str_compare_callback(const void* key1, const void* key2)
{
    return strcmp((char*)key1, (char*)key2) == 0;
}

/* Helper function to set config for callbacks */
void
dict_set_config(struct agent_config* config)
{
    current_dict_config = config;
}

/* Helper function to clear config */
void
dict_clear_config(void)
{
    current_dict_config = NULL;
}

uint64_t
str_hash_callback(const void* key)
{
    return dictGenHashFunction((unsigned char*)key, strlen((char*)key));
}
