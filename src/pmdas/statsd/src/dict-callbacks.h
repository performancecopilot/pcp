/*
 * Copyright (c) 2020 Red Hat.
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
#ifndef AGGREGATOR_METRIC_DICT_CALLBACKS_
#define AGGREGATOR_METRIC_DICT_CALLBACKS_

extern void
metric_label_free_callback(void* val);

extern void
metric_free_callback(void* val);

extern void
str_hash_free_callback(void* key);

extern void*
str_duplicate_callback(const void* key);

extern int
str_compare_callback(const void* key1, const void* key2);

extern uint64_t
str_hash_callback(const void* key);

#endif
