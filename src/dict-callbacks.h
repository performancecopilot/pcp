#ifndef AGGREGATOR_METRIC_DICT_CALLBACKS_
#define AGGREGATOR_METRIC_DICT_CALLBACKS_

void
metric_label_free_callback(void* privdata, void* val);

void
metric_free_callback(void* privdata, void* val);

void
str_hash_free_callback(void* privdata, void* key);

void*
str_duplicate_callback(void* privdata, const void* key);

int
str_compare_callback(void* privdata, const void* key1, const void* key2);

uint64_t
str_hash_callback(const void* key);

#endif
