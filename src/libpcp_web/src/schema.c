/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include <assert.h>
#include "schema.h"
#include "util.h"
#include "maps.h"
#include "sha1.h"
#include <pcp/pmda.h>

#define STRINGIFY(s)	#s
#define TO_STRING(s)	STRINGIFY(s)
#define SCHEMA_VERSION	2
#define SHA1SZ		20

#define reloadfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define reloadmsg(baton, level, message)	\
	((baton)->info((level), (message), (baton)->userdata), sdsfree(msg))

typedef struct redisScript {
    sds			hash;
    const char		*text;
} redisScript;

static redisScript scripts[] = {
/* Script HASH_MAP_ID pcp:map:<name> , <string> -> ID
	returns a map identifier from a given key (hash) and
	value (string).  If the string has not been observed
	previously a new identifier is assigned and the hash
	updated.  This is a cache-internal identifier only.
 */
    { .text = \
	"local ID = redis.pcall('HGET', KEYS[1], ARGV[1])\n"
	"local NEW = 0\n"
	"if (ID == false) then\n"
	"    ID = redis.pcall('HLEN', KEYS[1]) + 1\n"
	"    redis.call('HSETNX', KEYS[1], ARGV[1], tostring(ID))\n"
	"    NEW = 1\n"
	"end\n"
	"return {tonumber(ID), NEW}\n"
    },
};

typedef enum {
    HASH_MAP_ID = 0,
    NSCRIPTS
} redisScriptID;

static void
redisScriptsInit(void)
{
    const unsigned char	*text;
    unsigned char	hash[20];
    redisScript		*script;
    SHA1_CTX		shactx;
    int			i;

    for (i = 0; i < NSCRIPTS; i++) {
	script = &scripts[i];
	text = (const unsigned char *)script->text;

	/* Calculate unique script identifier from its contents */
	SHA1Init(&shactx);
	SHA1Update(&shactx, text, strlen((char *)text));
	SHA1Final(hash, &shactx);
	scripts->hash = sdsnew(pmwebapi_hash_str(hash));
    }
}

static redisScript *
redisGetScript(redisScriptID id)
{
    assert(id < NSCRIPTS);
    return &scripts[HASH_MAP_ID];
}

static int
testReplyError(redisReply *reply, const char *server_message)
{
    return (reply && reply->type == REDIS_REPLY_ERROR &&
	    strcmp(reply->str, server_message) == 0);
}

static void
reportReplyError(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, va_list argp)
{
    sds			msg = sdsnew("Error: ");

    msg = sdscatvprintf(msg, format, argp);
    if (reply && reply->type == REDIS_REPLY_ERROR)
	msg = sdscatfmt(msg, "\nRedis: %s\n", reply->str);
    else
	msg = sdscat(msg, "\n");
    info(PMLOG_RESPONSE, msg, userdata);
    sdsfree(msg);
}

static int
checkStatusReplyOK(redisInfoCallBack info, void *userdata,
		redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply->type == REDIS_REPLY_STATUS &&
	(strcmp("OK", reply->str) == 0 || strcmp("QUEUED", reply->str) == 0))
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

static int
checkStatusReplyString(redisInfoCallBack info, void *userdata,
	redisReply *reply, sds s, const char *format, ...)
{
    va_list		argp;

    if (reply->type == REDIS_REPLY_STATUS && strcmp(s, reply->str) == 0)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

static int
checkArrayReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_ARRAY)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

static int
checkMapScriptReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, long long *mapid, sds s, const char *format, ...)
{
    va_list		argp;

    /* on success, map script script returns two integer values via array */
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements == 2 &&
	reply->element[0]->type == REDIS_REPLY_INTEGER &&
	reply->element[1]->type == REDIS_REPLY_INTEGER) {
	*mapid = reply->element[0]->integer;	/* the actual string mapid */
	return reply->element[1]->integer;	/* is this newly allocated */
    }
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

static long long
checkIntegerReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_INTEGER)
	return reply->integer;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

static void
initRedisMapBaton(redisMapBaton *baton, redisSlots *slots,
	redisMap *mapping, sds mapKey, long long *mapID,
	redisDoneCallBack on_done, redisInfoCallBack on_info,
	void *userdata, void *arg)
{
    baton->magic = MAGIC_MAPPING;
    baton->mapping = mapping;
    baton->mapKey = mapKey;
    baton->mapID = mapID;
    baton->slots = slots;
    baton->info = on_info;
    baton->mapped = on_done;
    baton->userdata = userdata;
    baton->arg = arg;
}

static void
doneRedisMapBaton(redisMapBaton *baton)
{
    assert(baton->magic == MAGIC_MAPPING);
    if (baton->mapped)
	baton->mapped(baton->arg);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static void redisMapRequest(redisMapBaton *);

static void
redis_map_loadscript_callback(redisAsyncContext *redis, redisReply *reply, void *arg)
{
    redisMapBaton	*baton = (redisMapBaton *)arg;
    redisScript		*script;
    sds			msg;

    assert(baton->magic == MAGIC_MAPPING);
    script = redisGetScript(baton->scriptID);
    if (reply->type != REDIS_REPLY_STRING) {
	reloadfmt(msg, "Failed to LOAD Redis LUA script[%d]: %s\n[%s]\n",
			baton->scriptID, reply->str, script->text);
	reloadmsg(baton, PMLOG_WARNING, msg);
	*baton->mapID = -1;
	doneRedisMapBaton(baton);
    } else {
	if (strcmp(script->hash, reply->str) != 0) {
	    reloadfmt(msg, "Hash mismatch on loaded script[%d]: %s/%s\n",
			    baton->scriptID, script->hash, reply->str);
	    reloadmsg(baton, PMLOG_WARNING, msg);
	    *baton->mapID = -1;
	    doneRedisMapBaton(baton);
	} else {
	    if (pmDebugOptions.series)
		fprintf(stderr, "Registered LUA script[%d] as %s, retrying\n",
			    baton->scriptID, script->hash);
	    /* retry the original mapping request */
	    redisMapRequest(baton);
	}
    }
}

static void
redis_map_publish_callback(redisAsyncContext *redis, redisReply *reply, void *arg)
{
    redisMapBaton	*baton = (redisMapBaton *)arg;
    const char		*mapname;

    assert(baton->magic == MAGIC_MAPPING);

    mapname = redisMapName(baton->mapping);
    checkIntegerReply(baton->info, baton->userdata, reply,
		    "%s: %s", PUBLISH, "new %s mapping", mapname);

    doneRedisMapBaton(baton);
}

static void
redis_map_evalsha_callback(redisAsyncContext *redis, redisReply *reply, void *arg)
{
    redisMapBaton	*baton = (redisMapBaton *)arg;
    redisSlots		*slots = (redisSlots *)redis->data;
    redisScript		*script;
    const char		*mapname;
    sds			cmd, msg, key;
    int			added = 0;

    assert(baton->magic == MAGIC_MAPPING);

    if (testReplyError(reply, REDIS_ENOSCRIPT)) {
	if (baton->loaded) {
	    /* we've attempted to load the script once, error out now */
	    *baton->mapID = -1;
	    doneRedisMapBaton(baton);
	} else {
	    baton->loaded++;

	    /*
	     * An attempt to call the script on the given node failed.
	     * Load script, and then retry the earlier EVAL call.
	     */
	    key = sdscatfmt(sdsempty(), "pcp:map:%s", redisMapName(baton->mapping));
	    script = redisGetScript(HASH_MAP_ID);
	    cmd = redis_command(3);
	    cmd = redis_param_str(cmd, "SCRIPT", sizeof("SCRIPT")-1);
	    cmd = redis_param_str(cmd, "LOAD", sizeof("LOAD")-1);
	    cmd = redis_param_str(cmd, script->text, strlen(script->text));

	    redisSlotsRequest(slots, EVALSHA, key, cmd, redis_map_loadscript_callback, baton);
	}
    } else {
	mapname = redisMapName(baton->mapping);
	added = checkMapScriptReply(baton->info, baton->userdata, reply,
			baton->mapID, "%s: %s (%s)" EVALSHA,
			"string mapping script", mapname);

	redisMapInsert(baton->mapping, baton->mapKey, *baton->mapID);

	/* publish any newly created name mapping */
	if (added <= 0) {
	    doneRedisMapBaton(baton);
	} else {
	    msg = sdscatfmt(sdsempty(), "%I:%s", *baton->mapID, baton->mapKey);
	    key = sdscatfmt(sdsempty(), "pcp:channel:%s", mapname);
	    cmd = redis_command(3);
	    cmd = redis_param_str(cmd, PUBLISH, PUBLISH_LEN);
	    cmd = redis_param_sds(cmd, key);
	    cmd = redis_param_sds(cmd, msg);
	    sdsfree(msg);

	    redisSlotsRequest(slots, PUBLISH, key, cmd, redis_map_publish_callback, baton);
	}
    }
}

static void
redisMapRequest(redisMapBaton *baton)
{
    sds			cmd, key;
    redisScript		*script = redisGetScript(HASH_MAP_ID);

    key = sdscatfmt(sdsempty(), "pcp:map:%s", redisMapName(baton->mapping));
    cmd = redis_command(5);
    cmd = redis_param_str(cmd, EVALSHA, EVALSHA_LEN);
    cmd = redis_param_sds(cmd, script->hash);
    cmd = redis_param_str(cmd, "1", sizeof("1")-1);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, baton->mapKey);

    redisSlotsRequest(baton->slots, EVALSHA, key, cmd, redis_map_evalsha_callback, baton);
}

/* TODO: move this & helpers (above) into maps.c */
void
redisGetMap(redisSlots *slots, redisMap *mapping, sds mapKey, long long *mapID,
		redisDoneCallBack on_done, redisInfoCallBack on_info,
		void *userdata, void *arg)
{
    redisMapBaton	*baton;
    redisMapEntry	*entry = redisMapLookup(mapping, mapKey);

    if (entry != NULL) {
	*mapID = redisMapValue(entry);
	on_done(arg);
    } else if ((baton = calloc(1, sizeof(redisMapBaton))) == NULL) {
	*mapID = -1;
	on_done(arg);
    } else {
	initRedisMapBaton(baton, slots, mapping, mapKey, mapID,
			on_done, on_info, userdata, arg);
	redisMapRequest(baton);
    }
}

static void
redis_source_context_name(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", SADD, "mapping context to source or host name");
    doneSeriesLoadBaton(baton);
}

static void
redis_source_location(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", GEOADD, "mapping source location");
    doneSeriesLoadBaton(baton);
}

static void
redis_context_name_source(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", SADD, "mapping source names to context");
    doneSeriesLoadBaton(baton);
}

void
redis_series_source(redisSlots *slots, context_t *context, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    const char			*hash = pmwebapi_hash_str(context->hash);
    sds				cmd, key, val, val2;

    /* Async recipe:
     * . SADD pcp:source:context.name:<mapid>
     * . SADD pcp:context.name:source:<hash>
     * . SADD pcp:source:context.name:<hostid>
     * . GEOADD pcp:source:location <lat> <long> <hash>
     */
    setSeriesLoadBatonRef(baton, 4);

    key = sdscatfmt(sdsempty(), "pcp:source:context.name:%I", context->mapid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, context->hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_source_context_name, arg);

    key = sdscatfmt(sdsempty(), "pcp:source:context.name:%I", context->hostid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, context->hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_source_context_name, arg);

    val = sdscatfmt(sdsempty(), "%I", context->mapid);
    val2 = sdscatfmt(sdsempty(), "%I", context->hostid);
    key = sdscatfmt(sdsempty(), "pcp:context.name:source:%s", hash);
    cmd = redis_command(4);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, val);
    cmd = redis_param_sds(cmd, val2);
    sdsfree(val2);
    sdsfree(val);
    redisSlotsRequest(slots, SADD, key, cmd, redis_context_name_source, arg);

    key = sdsnew("pcp:source:location");
    val = sdscatprintf(sdsempty(), "%13.8f", context->location[0]);
    val2 = sdscatprintf(sdsempty(), "%13.8f", context->location[1]);
    cmd = redis_command(4);
    cmd = redis_param_str(cmd, GEOADD, GEOADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, val2);
    cmd = redis_param_sds(cmd, val);
    cmd = redis_param_sha(cmd, context->hash);
    sdsfree(val2);
    sdsfree(val);
    redisSlotsRequest(slots, GEOADD, key, cmd, redis_source_location, arg);
}

static void
redis_series_inst_name_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", SADD, "mapping series to inst name");
    decSeriesLoadBatonRef(baton);
}

static void
redis_instances_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", SADD, "mapping instance to series");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_inst_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkStatusReplyOK(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", HMSET, "setting metric inst");
    decSeriesLoadBatonRef(baton);
}

void
redis_series_inst(redisSlots *slots, metric_t *metric, value_t *value, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    const char			*hash;
    sds				cmd, key, val, id;

    assert(value->name);
    assert(value->mapid > 0);

    incSeriesLoadBatonRef(baton);
    incSeriesLoadBatonRef(baton);
    incSeriesLoadBatonRef(baton);

    key = sdscatfmt(sdsempty(), "pcp:series:inst.name:%I", value->mapid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, metric->hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_series_inst_name_callback, arg);

    hash = pmwebapi_hash_str(metric->hash);
    key = sdscatfmt(sdsempty(), "pcp:instances:series:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, value->hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_instances_series_callback, arg);

    hash = pmwebapi_hash_str(value->hash);
    id = sdscatfmt(sdsempty(), "%I", value->mapid);
    val = sdscatfmt(sdsempty(), "%i", value->inst);
    key = sdscatfmt(sdsempty(), "pcp:inst:series:%s", hash);
    cmd = redis_command(8);
    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
    cmd = redis_param_sds(cmd, val);
    cmd = redis_param_str(cmd, "name", sizeof("name")-1);
    cmd = redis_param_sds(cmd, id);
    cmd = redis_param_str(cmd, "series", sizeof("series")-1);
    cmd = redis_param_sha(cmd, metric->hash);
    sdsfree(val);
    sdsfree(id);
    redisSlotsRequest(slots, HMSET, key, cmd, redis_series_inst_callback, arg);
}

typedef struct seriesGetValueBaton {
    unsigned int	magic;		/* MAGIC_VALUE */
    sds			timestamp;
    metric_t		*metric;
    value_t		*value;
    unsigned int	meta : 1;	/* writing metadata */
    unsigned int	data : 1;	/* writing atomvalues */
    unsigned int	mark : 1;	/* writing a mark record */
    unsigned int	refcount;
    redisDoneCallBack	done;
    void		*arg;
    void		*baton;
} seriesGetValueBaton;

static void
initSeriesGetValueBaton(seriesGetValueBaton *baton,
		metric_t *metric, value_t *value, void *arg)
{
    baton->magic = MAGIC_VALUE;
    baton->metric = metric;
    baton->value = value;
    baton->baton = arg;
}

static void
freeSeriesGetValueBaton(seriesGetValueBaton *baton)
{
    assert(baton->magic == MAGIC_VALUE);
    assert(baton->refcount == 0);

    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static void
doneSeriesGetValueBaton(seriesGetValueBaton *baton)
{
    assert(baton->magic == MAGIC_VALUE);

    if (baton->refcount)
	baton->refcount--;
    if (baton->refcount == 0) {
	if (baton->done)
	    baton->done(baton->baton);
	freeSeriesGetValueBaton(baton);
    }
}

static void
incSeriesGetValueBatonRef(seriesGetValueBaton *baton)
{
    assert(baton->magic == MAGIC_VALUE);
    baton->refcount++;
}

static void
label_value_mapping_callback(void *arg)
{
    seriesGetValueBaton		*baton = (seriesGetValueBaton *)arg;
    labellist_t			*list;

    assert(baton->magic == MAGIC_VALUE);
    list = baton->value ? baton->value->labellist : baton->metric->labellist;
    redisMapRelease(list->valuemap);
    doneSeriesGetValueBaton(baton);
}

static void
label_name_mapping_callback(void *arg)
{
    seriesGetValueBaton		*baton = (seriesGetValueBaton *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    labellist_t			*list;
    sds				key;

    assert(baton->magic == MAGIC_VALUE);

    list = baton->value ? baton->value->labellist : baton->metric->labellist;
    key = sdscatfmt(sdsempty(), "label.%I.value", list->nameid);
    list->valuemap = redisMapCreate(key);

    redisGetMap(seriesLoadBatonSlots(load),
		list->valuemap, list->value, &list->valueid,
		label_value_mapping_callback,
		seriesLoadBatonInfo(load), seriesLoadBatonUser(load),
		(void *)baton);
}

static int
annotate_metric(const pmLabel *label, const char *json, void *arg)
{
    seriesGetValueBaton		*baton = (seriesGetValueBaton *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    labellist_t			*list, *head;

    assert(baton->magic == MAGIC_VALUE);

    if ((list = (labellist_t *)calloc(1, sizeof(labellist_t))) == NULL)
	return -ENOMEM;

    /*
     * TODO: decode complex values ('{...}' and '[...]'),
     * using a dot-separated name for these maps, and names
     * with explicit array index suffix for array entries.
     * This is safe as JSONB names cannot present that way.
     */

    list->arg = arg;
    list->name = sdsnewlen(json + label->name, label->namelen);
    list->value = sdsnewlen(json + label->value, label->valuelen);
    list->flags = label->flags;

    /* append map onto the list for this value or metric */
    head = baton->value ? baton->value->labellist : baton->metric->labellist;
    while (head && head->next)
	head = head->next;
    if (head == NULL)
	head = list;
    else
	head->next = list;

    incSeriesGetValueBatonRef(baton);
    redisGetMap(seriesLoadBatonSlots(load),
		labelsmap, list->name, &list->nameid,
		label_name_mapping_callback,
		seriesLoadBatonInfo(load), seriesLoadBatonUser(load),
		(void *)list);
    return 0;
}

static void
redis_series_labelvalue_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkStatusReplyOK(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", HMSET, "setting series label value");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_labelflags_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkStatusReplyOK(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", HMSET, "setting series label flags");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_label_set_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s %s", SADD, "pcp:series:label.X.value:Y");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_label(redisSlots *slots, metric_t *metric, value_t *value, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    labellist_t			*list;
    char			*hash;
    sds				cmd, key, val, name;

    if (value != NULL) {
	hash = pmwebapi_hash_str(value->hash);
	list = value->labellist;
    } else {
	hash = pmwebapi_hash_str(metric->hash);
	list = metric->labellist;
    }

    do {
	incSeriesLoadBatonRef(baton);
	incSeriesLoadBatonRef(baton);

	if (list->flags != PM_LABEL_CONTEXT) {
	    incSeriesLoadBatonRef(baton);

	    name = sdscatfmt(sdsempty(), "%I", list->nameid);
	    val = sdscatfmt(sdsempty(), "%I", list->flags);
	    key = sdscatfmt(sdsempty(), "pcp:labelflags:series:%s", hash);
	    cmd = redis_command(4);
	    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
	    cmd = redis_param_sds(cmd, key);
	    cmd = redis_param_sds(cmd, name);
	    cmd = redis_param_sds(cmd, val);
	    sdsfree(name);
	    sdsfree(val);
	    redisSlotsRequest(slots, HMSET, key, cmd,
				redis_series_labelflags_callback, arg);
	}

	name = sdscatfmt(sdsempty(), "%I", list->nameid);
	val = sdscatfmt(sdsempty(), "%I", list->valueid);
	key = sdscatfmt(sdsempty(), "pcp:labelvalue:series:%s", hash);
	cmd = redis_command(4);
	cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, name);
	cmd = redis_param_sds(cmd, val);
	sdsfree(val);
	redisSlotsRequest(slots, HMSET, key, cmd,
				redis_series_labelvalue_callback, arg);

	key = sdscatfmt(sdsempty(), "pcp:series:label.%I.value:%I",
			list->nameid, list->valueid);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sha(cmd, metric->hash);
	redisSlotsRequest(slots, SADD, key, cmd,
				redis_series_label_set_callback, arg);

    } while ((list = list->next) != NULL);
}

static int
filter(const pmLabel *label, const char *json, void *arg)
{
    if (pmDebugOptions.series)
	fprintf(stderr, "Caching label %.*s=%.*s (optional=%s)\n",
		label->namelen, json + label->name,
		label->valuelen, json + label->value,
		(label->flags & PM_LABEL_OPTIONAL) ? "yes" : "no");

    return annotate_metric(label, json, (seriesGetValueBaton *)arg);
}

void
series_label_mapping(seriesGetValueBaton *baton)
{
    struct seriesLoadBaton	*load;
    char			buf[PM_MAXLABELJSONLEN];
    char			pmmsg[PM_MAXERRMSGLEN];
    metric_t			*metric = baton->metric;
    value_t			*value = baton->value;
    sds				msg;
    int				sts;

    sts = merge_labelsets(metric, value, buf, sizeof(buf), filter, baton);
    if (sts < 0) {
	load = (struct seriesLoadBaton *)baton->baton;
	reloadfmt(msg, "Cannot merge series %s label set: %s",
			pmwebapi_hash_str(metric->hash),
			pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	(seriesLoadBatonInfo(load))(PMLOG_ERROR, msg, seriesLoadBatonUser(load));
	sdsfree(msg);
	doneSeriesGetValueBaton(baton);
    }
}

static void
series_name_mapping_callback(void *arg)
{
    seriesGetValueBaton		*baton = (seriesGetValueBaton *)arg;

    assert(baton->magic == MAGIC_VALUE);
    doneSeriesGetValueBaton(baton);
}

void
series_value_mapping(seriesGetValueBaton *baton)
{
    struct seriesLoadBaton	*load;
    struct redisSlots		*slots;
    value_t			*value;

    assert(baton->magic == MAGIC_VALUE);
    value = baton->value;

    /* map instance (value) name string to identifier if not already done */
    if (value->mapid <= 0) {
	assert(value->name != NULL);
	incSeriesGetValueBatonRef(baton);
	load = (struct seriesLoadBaton *)baton->baton;
	slots = (struct redisSlots *)seriesLoadBatonSlots(load);
	redisGetMap(slots, instmap, value->name, &value->mapid,
			series_name_mapping_callback, seriesLoadBatonInfo(load),
			seriesLoadBatonUser(load), baton);
    } else {
	doneSeriesGetValueBaton(baton);
    }
}

static void
series_value_mapped(void *arg)
{
    seriesGetValueBaton		*metric, *baton = (seriesGetValueBaton *)arg;

    assert(baton->magic == MAGIC_VALUE);
    if (baton->refcount)
	baton->refcount--;
    if (baton->refcount == 0) {
	metric = baton->arg;
	freeSeriesGetValueBaton(baton);
	doneSeriesGetValueBaton(metric);
    }
}

static void redis_series_metadata(redisSlots *, context_t *, metric_t *, void *);
static void redis_series_stream(redisSlots *, sds, metric_t *, void *); /*TODO*/

static void
series_mapped_metric(void *arg)
{
    seriesGetValueBaton		*baton = (seriesGetValueBaton *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    redisSlots			*slots = seriesLoadBatonSlots(load);
    context_t			*context = seriesLoadBatonContext(load);
    sds				timestamp = baton->timestamp;

    assert(baton->magic == MAGIC_VALUE);

    /* push the metric, instances and any label metadata into the cache */
    if (baton->meta)
	redis_series_metadata(slots, context, baton->metric, load);

    /* push values for all instances, no-value or errors into the cache */
    if (baton->data)
	redis_series_stream(slots, timestamp, baton->metric, load);

    doneSeriesGetValueBaton(baton);
}

void
redis_series_metric(redisSlots *redis, metric_t *metric,
		sds timestamp, int meta, int data, void *arg)
{
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)arg;
    seriesGetValueBaton		*baton, *field;
    value_t			*value = NULL;
    int				i;

    /*
     * First satisfy any/all mappings for metric name, instance
     * names, label names and values.  Then issue the metadata
     * and data simultaneously.
     */
    if ((baton = calloc(1, sizeof(seriesGetValueBaton))) == 0) {
	return;		/* TODO: error handling */
    }
    initSeriesGetValueBaton(baton, metric, value, load);
    incSeriesGetValueBatonRef(baton);
    baton->done = series_mapped_metric;
    baton->timestamp = timestamp;
    baton->meta = meta;
    baton->data = data;

    for (i = 0; i < metric->numnames; i++) {
	assert(metric->names[i]);
	if (metric->mapids[i] <= 0) {
	    incSeriesGetValueBatonRef(baton);
	    redisGetMap(seriesLoadBatonSlots(load),
			namesmap, metric->names[i], &metric->mapids[i],
			series_name_mapping_callback,
			seriesLoadBatonInfo(load), seriesLoadBatonUser(load),
			(void *)baton);
	}
    }

    if (metric->desc.indom == PM_INDOM_NULL) {
	incSeriesGetValueBatonRef(baton);
	series_label_mapping(baton);
    } else {
	for (i = 0; i < metric->u.inst->listcount; i++) {
	    if ((field = calloc(1, sizeof(seriesGetValueBaton))) == NULL) {
		continue;	/* TODO: error handling */
	    }

	    incSeriesGetValueBatonRef(baton);
	    value = &metric->u.inst->value[i];

	    initSeriesGetValueBaton(field, metric, value, load);
	    incSeriesGetValueBatonRef(field);
	    field->done = series_value_mapped;
	    field->arg = baton;
    
	    series_value_mapping(field);
	    series_label_mapping(field);

	    doneSeriesGetValueBaton(field);
	}
    }
    doneSeriesGetValueBaton(baton);
}

static void
redis_metric_name_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
			reply, "%s %s", SADD, "map metric name to series");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_metric_name_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
			reply, "%s: %s", SADD, "map series to metric name");
    decSeriesLoadBatonRef(baton);
}

static void
redis_desc_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkStatusReplyOK(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
			reply, "%s: %s", HMSET, "setting metric desc");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_source_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
			reply, "%s: %s", SADD, "mapping series to context");
    decSeriesLoadBatonRef(baton);
}

static void
redis_series_metadata(redisSlots *slots, context_t *context, metric_t *metric, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    value_t			*value;
    const char			*hash = pmwebapi_hash_str(metric->hash);
    const char			*units, *indom, *pmid, *sem, *type;
    long long			map;
    sds				val, cmd, key;
    int				i;

    for (i = 0; i < metric->numnames; i++) {
	assert(metric->names[i]);
	map = metric->mapids[i];
	assert(map > 0);

	incSeriesLoadBatonRef(baton);
	incSeriesLoadBatonRef(baton);

	key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%s", hash);
	val = sdscatfmt(sdsempty(), "%I", map);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, val);
	sdsfree(val);
	redisSlotsRequest(slots, SADD, key, cmd,
			redis_metric_name_series_callback, arg);

	key = sdscatfmt(sdsempty(), "pcp:series:metric.name:%I", map);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sha(cmd, metric->hash);
	redisSlotsRequest(slots, SADD, key, cmd,
			redis_series_metric_name_callback, arg);
    }

    incSeriesLoadBatonRef(baton);
    incSeriesLoadBatonRef(baton);

    indom = indom_str(metric);
    pmid = pmid_str(metric);
    sem = semantics_str(metric);
    type = type_str(metric);
    units = units_str(metric);

    key = sdscatfmt(sdsempty(), "pcp:desc:series:%s", hash);
    cmd = redis_command(14);
    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "indom", sizeof("indom")-1);
    cmd = redis_param_str(cmd, indom, strlen(indom));
    cmd = redis_param_str(cmd, "pmid", sizeof("pmid")-1);
    cmd = redis_param_str(cmd, pmid, strlen(pmid));
    cmd = redis_param_str(cmd, "semantics", sizeof("semantics")-1);
    cmd = redis_param_str(cmd, sem, strlen(sem));
    cmd = redis_param_str(cmd, "source", sizeof("source")-1);
    cmd = redis_param_sha(cmd, context->hash);
    cmd = redis_param_str(cmd, "type", sizeof("type")-1);
    cmd = redis_param_str(cmd, type, strlen(type));
    cmd = redis_param_str(cmd, "units", sizeof("units")-1);
    cmd = redis_param_str(cmd, units, strlen(units));
    redisSlotsRequest(slots, HMSET, key, cmd, redis_desc_series_callback, arg);

    hash = pmwebapi_hash_str(context->hash);
    key = sdscatfmt(sdsempty(), "pcp:series:source:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, metric->hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_series_source_callback, arg);

    if (metric->desc.indom == PM_INDOM_NULL) {
	redis_series_label(slots, metric, NULL, baton);
    } else {
	for (i = 0; i < metric->u.inst->listcount; i++) {
	    value = &metric->u.inst->value[i];
	    redis_series_inst(slots, metric, value, baton);
	    redis_series_label(slots, metric, value, baton);
	}
    }
}

typedef struct redisStreamBaton {
    int			magic;
    redisSlots		*slots;
    sds			stamp;
    char		hash[40+1];
    redisInfoCallBack   info;
    void		*userdata;
    void		*arg;
} redisStreamBaton;

void
initRedisStreamBaton(redisStreamBaton *baton,
		redisSlots *slots, sds stamp, const char *hash,
		redisInfoCallBack info, void *userdata, void *arg)
{
    baton->magic = MAGIC_STREAM;
    baton->slots = slots;
    baton->stamp = sdsdup(stamp);
    memcpy(baton->hash, hash, sizeof(baton->hash));
    baton->info = info;
    baton->userdata = userdata;
    baton->arg = arg;
}

void
doneRedisStreamBaton(redisStreamBaton *baton)
{
    assert(baton->magic == MAGIC_STREAM);
    decSeriesLoadBatonRef(baton->arg);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static sds
series_stream_append(sds cmd, sds name, sds value)
{
    unsigned int	nlen = sdslen(name);
    unsigned int	vlen = sdslen(value);

    cmd = sdscatfmt(cmd, "$%u\r\n%S\r\n$%u\r\n%S\r\n", nlen, name, vlen, value);
    sdsfree(value);
    return cmd;
}

static sds
series_stream_value(sds cmd, sds name, int type, pmAtomValue *avp)
{
    unsigned int	bytes;
    const char		*string;
    sds			value;

    if (!avp)
	return series_stream_append(cmd, name, sdsnewlen("0", 1));

    switch (type) {
    case PM_TYPE_32:
	value = sdscatfmt(sdsempty(), "%i", avp->l);
	break;
    case PM_TYPE_U32:
	value = sdscatfmt(sdsempty(), "%u", avp->ul);
	break;
    case PM_TYPE_64:
	value = sdscatfmt(sdsempty(), "%I", avp->ll);
	break;
    case PM_TYPE_U64:
	value = sdscatfmt(sdsempty(), "%U", avp->ull);
	break;

    case PM_TYPE_FLOAT:
	value = sdscatprintf(sdsempty(), "%e", (double)avp->f);
	break;
    case PM_TYPE_DOUBLE:
	value = sdscatprintf(sdsempty(), "%e", (double)avp->d);
	break;

    case PM_TYPE_STRING:
	if ((string = avp->cp) == NULL)
	    string = "<null>";
	value = sdsnew(string);
	break;

    case PM_TYPE_AGGREGATE:
    case PM_TYPE_AGGREGATE_STATIC:
	if (avp->vbp != NULL) {
	    string = avp->vbp->vbuf;
	    bytes = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	} else {
	    string = "<null>";
	    bytes = strlen(string);
	}
	value = sdsnewlen(string, bytes);
	break;

    default:
	value = sdscatfmt(sdsempty(), "%i", PM_ERR_NYI);
	break;
    }

    return series_stream_append(cmd, name, value);
}

void
redis_series_stream_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    redisStreamBaton	*baton = (redisStreamBaton *)arg;
    sds			msg;

    assert(baton->magic == MAGIC_STREAM);
    if (testReplyError(reply, REDIS_ESTREAMXADD)) {
	reloadfmt(msg, "duplicate or early stream %s insert at time %s",
		baton->hash, baton->stamp);
	reloadmsg(baton, PMLOG_WARNING, msg);
    }
    else {
	checkStatusReplyString(baton->info, baton->userdata, reply,
		baton->stamp, "stream %s status mismatch at time %s",
		baton->hash, baton->stamp);
    }
    doneRedisStreamBaton(baton);
}

void
redis_series_stream(redisSlots *slots, sds stamp, metric_t *metric, void *arg)
{
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)arg;
    redisInfoCallBack		info = seriesLoadBatonInfo(load);
    redisStreamBaton		*baton;
    unsigned int		count;
    const char			*hash = pmwebapi_hash_str(metric->hash);
    int				i, sts, type;
    sds				cmd, key, name, stream = sdsempty();

    if ((baton = malloc(sizeof(redisStreamBaton))) == NULL) {
	stream = sdscatfmt(stream, "OOM creating stream baton");
	info(PMLOG_ERROR, stream, seriesLoadBatonUser(load));
	sdsfree(stream);
	return;
    }
    initRedisStreamBaton(baton, slots, stamp, hash, info,
			seriesLoadBatonUser(load), load);
    incSeriesLoadBatonRef(arg);	/* submitting a single I/O request, below */

    count = 3;	/* XADD key stamp */
    key = sdscatfmt(sdsempty(), "pcp:values:series:%s", hash);

    if ((sts = metric->error) < 0) {
	stream = series_stream_append(stream,
			sdsnewlen("-1", 2), sdscatfmt(sdsempty(), "%i", sts));
	count += 2;
    } else {
	name = sdsempty();
	type = metric->desc.type;
	if (metric->desc.indom == PM_INDOM_NULL) {
	    stream = series_stream_value(stream, name, type, &metric->u.atom);
	    count += 2;
	} else if (metric->u.inst->listcount <= 0) {
	    stream = series_stream_append(stream, sdsnew("0"), sdsnew("0"));
	    count += 2;
	} else {
	    for (i = 0; i < metric->u.inst->listcount; i++) {
		value_t	*v = &metric->u.inst->value[i];
		name = sdscpylen(name, (const char *)&v->hash[0], sizeof(v->hash));
		stream = series_stream_value(stream, name, type, &v->atom);
		count += 2;
	    }
	}
	sdsfree(name);
    }

    cmd = redis_command(count);
    cmd = redis_param_str(cmd, XADD, XADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, stamp);
    cmd = redis_param_raw(cmd, stream);
    sdsfree(stream);

    redisSlotsRequest(slots, XADD, key, cmd, redis_series_stream_callback, baton);
}

void
redis_series_mark(redisSlots *redis, context_t *context, sds timestamp, int data, void *arg)
{
    /* TODO: inject mark records into time series */
}

static void
redis_update_version_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;

    assert(baton->magic == MAGIC_SLOTS);
    checkStatusReplyOK(baton->info, baton->userdata, reply,
			"%s setup", "pcp:version:schema");

    doneRedisSlotsBaton(baton);
}

static void
redis_update_version(redisSlotsBaton *baton)
{
    sds			cmd, key;
    const char		ver[] = TO_STRING(SCHEMA_VERSION);

    key = sdsnew("pcp:version:schema");
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SETS, SETS_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, ver, sizeof(ver)-1);
    redisSlotsRequest(baton->redis, SETS, key, cmd, redis_update_version_callback, baton);
}

static void
redis_load_version_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    unsigned int	version = 0;
    sds			msg;

    assert(baton->magic == MAGIC_SLOTS);

    baton->version = -1;
    if (reply->type == REDIS_REPLY_STRING) {
	version = (unsigned int)atoi(reply->str);
	if (version == 0 || version == SCHEMA_VERSION) {
	    baton->version = version;
	} else {
	    reloadfmt(msg, "unsupported schema (got v%u, expected v%u)",
			version, SCHEMA_VERSION);
	    reloadmsg(baton, PMLOG_ERROR, msg);
	}
    } else if (reply->type == REDIS_REPLY_ERROR) {
	reloadfmt(msg, "version check error: %s", reply->str);
	reloadmsg(baton, PMLOG_REQUEST, msg);
    } else if (reply->type != REDIS_REPLY_NIL) {
	reloadfmt(msg, "unexpected schema version reply type (%s)",
		redis_reply(reply->type));
	reloadmsg(baton, PMLOG_ERROR, msg);
    } else {
	baton->version = 0;	/* NIL - no version key yet */
    }

    /* set the version when none found (first time through) */
    if (version != SCHEMA_VERSION && baton->version != -1)
	redis_update_version(arg);
    else
	doneRedisSlotsBaton(baton);
}

static void
redis_load_version(redisSlotsBaton *baton)
{
    sds			cmd, key;

    key = sdsnew("pcp:version:schema");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, GETS, GETS_LEN);
    cmd = redis_param_sds(cmd, key);
    redisSlotsRequest(baton->redis, GETS, key, cmd, redis_load_version_callback, baton);
}

static int
decodeRedisNode(redisSlotsBaton *baton, redisReply *reply, redisSlotServer *server)
{
    redisReply		*value;
    unsigned int	port;
    sds			msg;

    /* expecting IP address and port (integer), ignore optional node ID */
    if (reply->elements < 2) {
	reloadfmt(msg, "insufficient elements in cluster NODE reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }

    value = reply->element[1];
    if (value->type != REDIS_REPLY_INTEGER) {
	reloadfmt(msg, "expected integer port in cluster NODE reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    port = (unsigned int)value->integer;

    value = reply->element[0];
    if (value->type != REDIS_REPLY_STRING) {
	reloadfmt(msg, "expected string hostspec in cluster NODE reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }

    server->hostspec = sdscatfmt(sdsempty(), "%s:%u", value->str, port);
    return server->hostspec ? 0 : -ENOMEM;
}

static int
decodeRedisSlot(redisSlotsBaton *baton, redisReply *reply)
{
    redisSlotServer	*servers = NULL;
    redisSlotRange	slots, *sp;
    redisReply		*node;
    long long		slot;
    int			i, n;
    sds			msg;

    /* expecting start and end slot range integers, then node arrays */
    if (reply->elements < 3) {
	reloadfmt(msg, "insufficient elements in cluster SLOT reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    memset(&slots, 0, sizeof(slots));

    node = reply->element[0];
    if ((slot = checkIntegerReply(baton->info, baton->userdata,
				node, "%s start", "SLOT")) < 0) {
	reloadfmt(msg, "expected integer start in cluster SLOT reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    slots.start = (__uint32_t)slot;
    node = reply->element[1];
    if ((slot = checkIntegerReply(baton->info, baton->userdata,
				node, "%s end", "SLOT")) < 0) {
	reloadfmt(msg, "expected integer end in cluster SLOT reply");
	reloadmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    slots.end = (__uint32_t)slot;
    node = reply->element[2];
    if ((decodeRedisNode(baton, node, &slots.master)) < 0)
	return -EINVAL;

    if ((sp = calloc(1, sizeof(redisSlotRange))) == NULL)
	return -ENOMEM;
    *sp = slots;    /* struct copy */

    if ((n = reply->elements - 3) > 0)
	if ((servers = calloc(n, sizeof(redisSlotServer))) == NULL)
	    n = 0;
    sp->nslaves = n;
    sp->slaves = servers;

    for (i = 0; i < n; i++) {
	node = reply->element[i + 3];
	if (checkArrayReply(baton->info, baton->userdata,
				node, "%s range %u-%u slave %d",
				"SLOTS", sp->start, sp->end, i) == 0)
	    decodeRedisNode(baton, node, &sp->slaves[i]);
    }

    return redisSlotRangeInsert(baton->redis, sp);
}

static void
decodeRedisSlots(redisSlotsBaton *baton, redisReply *reply)
{
    redisReply		*slot;
    int			i;

    for (i = 0; i < reply->elements; i++) {
	slot = reply->element[i];
	if (checkArrayReply(baton->info, baton->userdata,
			slot, "%s %s entry %d", CLUSTER, "SLOTS", i) == 0)
	    decodeRedisSlot(baton, slot);
    }
}

static void
redis_load_slots_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    redisSlotServer	*servers;
    redisSlotRange	*slots;
    sds			msg;

    assert(baton->magic == MAGIC_SLOTS);

    /* Case of a single Redis instance or loosely cooperating instances */
    if (testReplyError(reply, REDIS_ENOCLUSTER)) {
	/* TODO: allow setup of multiple servers via configuration file */
	if ((servers = calloc(1, sizeof(redisSlotServer))) != NULL) {
	    if ((slots = calloc(1, sizeof(redisSlotRange))) == NULL) {
		reloadfmt(msg, "failed to allocate Redis slots memory");
		reloadmsg(baton, PMLOG_ERROR, msg);
		baton->version = -1;
	    } else {
		servers->hostspec = sdscatfmt(sdsempty(), "%s", baton->redis->hostspec);
		slots->nslaves = 0;
		slots->start = 0;
		slots->end = MAXSLOTS;
		slots->master = *servers;
		redisSlotRangeInsert(baton->redis, slots);
	    }
	}
    }
    /* Case of a cluster of Redis instances, following the cluster spec */
    else {
	if (checkArrayReply(baton->info, baton->userdata,
				reply, "%s %s", CLUSTER, "SLOTS") == 0)
	    decodeRedisSlots(baton, reply);
    }

    /* Verify pmseries schema version if previously requested */
    if (baton->version == 1)
	redis_load_version(baton);
    else
	doneRedisSlotsBaton(baton);
}

static void
redis_load_slots(redisSlotsBaton *baton)
{
    sds			cmd;

    cmd = redis_command(2);
    cmd = redis_param_str(cmd, CLUSTER, CLUSTER_LEN);
    cmd = redis_param_str(cmd, "SLOTS", sizeof("SLOTS")-1);
    redisSlotsRequest(baton->redis, CLUSTER, NULL, cmd, redis_load_slots_callback, baton);
}

void
initRedisSlotsBaton(redisSlotsBaton *baton, int version,
		redisInfoCallBack info, redisDoneCallBack done,
		void *userdata, void *events, void *arg)
{
    baton->magic = MAGIC_SLOTS;
    baton->info = info;
    baton->done = done;
    baton->version = version;
    baton->userdata = userdata;
    baton->arg = arg;
}

void
doneRedisSlotsBaton(redisSlotsBaton *baton)
{
    assert(baton->magic == MAGIC_SLOTS);
    baton->done(baton->arg);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static redisSlots		*slots;

#if 0 /* TODO */
void
pmSeriesDone(void)
{
    redisFreeSlots(slots);
}
#endif

void
redis_init(redisSlots **slotsp, sds server, int version_check,
		redisInfoCallBack info, redisDoneCallBack done,
		void *userdata, void *events, void *arg)
{
    redisSlotsBaton		*baton;
    sds				msg;

    /* fast path for when Redis has been setup already */
    if (slots) {
	*slotsp = slots;
	done(arg);
	return;
    }

    /* create global EVAL hashes and string map caches */
    redisScriptsInit();
    redisMapsInit();

    if ((baton = (redisSlotsBaton *)calloc(1, sizeof(redisSlotsBaton))) != NULL) {
	if ((slots = redisSlotsInit(server, info, events, userdata)) != NULL) {
	    initRedisSlotsBaton(baton, version_check, info, done,
				    userdata, events, arg);
	    baton->redis = *slotsp = slots;
	    redis_load_slots(baton);
	    return;
	}
	baton->version = -1;
	doneRedisSlotsBaton(baton);
    } else {
	done(arg);
    }
    *slotsp = NULL;
    reloadfmt(msg, "Failed to allocate memory for Redis slots");
    info(PMLOG_ERROR, msg, arg);
    sdsfree(msg);
}
