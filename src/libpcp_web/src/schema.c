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
#include "batons.h"
#include "maps.h"
#include "util.h"
#include "sha1.h"
#include <pcp/pmda.h>

#define STRINGIFY(s)	#s
#define TO_STRING(s)	STRINGIFY(s)
#define SCHEMA_VERSION	2
#define SHA1SZ		20

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
    initSeriesBatonMagic(baton, MAGIC_MAPPING);
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
    seriesBatonCheckMagic(baton, MAGIC_MAPPING, "doneRedisMapBaton");
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

    seriesBatonCheckMagic(baton, MAGIC_MAPPING, "redis_map_loadscript_callback");
    script = redisGetScript(baton->scriptID);
    if (reply->type != REDIS_REPLY_STRING) {
	seriesfmt(msg, "Failed to LOAD Redis LUA script[%d]: %s\n[%s]\n",
			baton->scriptID, reply->str, script->text);
	seriesmsg(baton, PMLOG_WARNING, msg);
	*baton->mapID = -1;
	doneRedisMapBaton(baton);
    } else {
	if (strcmp(script->hash, reply->str) != 0) {
	    seriesfmt(msg, "Hash mismatch on loaded script[%d]: %s/%s\n",
			    baton->scriptID, script->hash, reply->str);
	    seriesmsg(baton, PMLOG_WARNING, msg);
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

    seriesBatonCheckMagic(baton, MAGIC_MAPPING, "redis_map_publish_callback");
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

    seriesBatonCheckMagic(baton, MAGIC_MAPPING, "redis_map_evalsha_callback");

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
    const char			*hash = pmwebapi_hash_str(context->name.hash);
    sds				cmd, key, val, val2;

    /* Async recipe:
     * . SADD pcp:source:context.name:<mapid>
     * . SADD pcp:context.name:source:<hash>
     * . SADD pcp:source:context.name:<hostid>
     * . GEOADD pcp:source:location <lat> <long> <hash>
     */
    seriesBatonReferences(baton, 4, "redis_series_source");

    key = sdscatfmt(sdsempty(), "pcp:source:context.name:%I", context->name.mapid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, context->name.hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_source_context_name, arg);

    key = sdscatfmt(sdsempty(), "pcp:source:context.name:%I", context->hostid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, context->name.hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_source_context_name, arg);

    val = sdscatfmt(sdsempty(), "%I", context->name.mapid);
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
    val = sdscatprintf(sdsempty(), "%.8f", context->location[0]);
    val2 = sdscatprintf(sdsempty(), "%.8f", context->location[1]);
    cmd = redis_command(5);
    cmd = redis_param_str(cmd, GEOADD, GEOADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, val2);
    cmd = redis_param_sds(cmd, val);
    cmd = redis_param_sha(cmd, context->name.hash);
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
    seriesBatonDereference(baton, "redis_series_inst_name_callback");
}

static void
redis_instances_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkIntegerReply(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", SADD, "mapping instance to series");
    seriesBatonDereference(baton, "redis_instances_series_callback");
}

static void
redis_series_inst_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;

    checkStatusReplyOK(seriesLoadBatonInfo(baton), seriesLoadBatonUser(baton),
		reply, "%s: %s", HMSET, "setting metric inst");
    seriesBatonDereference(baton, "redis_series_inst_callback");
}

void
redis_series_instance(redisSlots *slots, metric_t *metric, instance_t *instance, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    const char			*hash;
    sds				cmd, key, val, id;
    int				i;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "redis_series_inst");
    seriesBatonReferences(baton, 3, "redis_series_inst");

    assert(instance->name.sds);
    assert(instance->name.mapid > 0);

    key = sdscatfmt(sdsempty(), "pcp:series:inst.name:%I", instance->name.mapid);
    cmd = redis_command(2 + metric->numnames);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    for (i = 0; i < metric->numnames; i++)
	cmd = redis_param_sha(cmd, metric->names[i].hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_series_inst_name_callback, arg);

    for (i = 0; i < metric->numnames; i++) {
	hash = pmwebapi_hash_str(metric->names[i].hash);
	key = sdscatfmt(sdsempty(), "pcp:instances:series:%s", hash);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sha(cmd, instance->name.hash);
	redisSlotsRequest(slots, SADD, key, cmd, redis_instances_series_callback, arg);
    }

    hash = pmwebapi_hash_str(instance->name.hash);
    id = sdscatfmt(sdsempty(), "%I", instance->name.mapid);
    val = sdscatfmt(sdsempty(), "%i", instance->inst);
    key = sdscatfmt(sdsempty(), "pcp:inst:series:%s", hash);
    cmd = redis_command(8);
    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
    cmd = redis_param_sds(cmd, val);
    cmd = redis_param_str(cmd, "name", sizeof("name")-1);
    cmd = redis_param_sds(cmd, id);
    cmd = redis_param_str(cmd, "source", sizeof("series")-1);
    cmd = redis_param_sha(cmd, metric->indom->domain->context->name.hash);
    sdsfree(val);
    sdsfree(id);
    redisSlotsRequest(slots, HMSET, key, cmd, redis_series_inst_callback, arg);
}

typedef struct seriesGetNames {
    seriesBatonMagic	header;		/* MAGIC_NAMES */
    metric_t		*metric;
    instance_t		*instance;

    sds			timestamp;	/* TODO: move to context */
    unsigned int	meta : 1;	/* writing metadata */
    unsigned int	data : 1;	/* writing atomvalues */
    unsigned int	mark : 1;	/* writing a mark record */

    redisDoneCallBack	done;
    void		*arg;
    void		*baton;
} seriesGetNames;

static void
initSeriesGetNames(seriesGetNames *baton,
		metric_t *metric, instance_t *instance, void *arg)
{
    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "Baton %p initd\n", baton);
    initSeriesBatonMagic(baton, MAGIC_NAMES);
    baton->instance = instance;
    baton->metric = metric;
    baton->baton = arg;
}

static void
freeSeriesGetNames(seriesGetNames *baton, const char *called)
{
    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "Baton %p freed by %s\n", baton, called);

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "freeSeriesGetNames");
    seriesBatonCheckCount(baton, "freeSeriesGetNames");

    /* TODO: insert <valgrind> here.
    memset(baton, 0, sizeof(*baton));
    free(baton);
    */
}

static void
doneSeriesGetNames(seriesGetNames *baton, const char *caller)
{
    seriesBatonCheckMagic(baton, MAGIC_NAMES, caller);

    if (seriesBatonDereference(baton, caller)) {
	if (baton->done)
	    baton->done(baton);
	freeSeriesGetNames(baton, "doneSeriesGetNames");
    }
}

static void
label_value_mapping_callback(void *arg)
{
    labellist_t			*list = (labellist_t *)arg;
    seriesGetNames		*baton = (seriesGetNames *)list->arg;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "label_value_mapping_callback");
    redisMapRelease(list->valuemap);
    doneSeriesGetNames(baton, "label_value_mapping_callback");
}

static void
label_name_mapping_callback(void *arg)
{
    labellist_t			*list = (labellist_t *)arg;
    seriesGetNames		*baton = (seriesGetNames *)list->arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    sds				key;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "label_name_mapping_callback");

    key = sdscatfmt(sdsempty(), "label.%I.value", list->nameid);
    list->valuemap = redisMapCreate(key);

    seriesBatonReference(baton, "label_name_mapping_callback");
    redisGetMap(seriesLoadBatonSlots(load),
		list->valuemap, list->value, &list->valueid,
		label_value_mapping_callback,
		seriesLoadBatonInfo(load), seriesLoadBatonUser(load),
		(void *)list);

    doneSeriesGetNames(baton, "label_name_mapping_callback");
}

static int
annotate_metric(const pmLabel *label, const char *json, void *arg)
{
    seriesGetNames		*baton = (seriesGetNames *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    labellist_t			*list;
    instance_t			*instance = baton->instance;
    metric_t			*metric = baton->metric;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "annotate_metric");

    /*
     * TODO: check id this label is already in the list?
     * Also, decode complex values ('{...}' and '[...]'),
     * using a dot-separated name for these maps, and names
     * with explicit array index suffix for array entries.
     * This is safe as JSONB names cannot present that way.
     */

    if ((list = (labellist_t *)calloc(1, sizeof(labellist_t))) == NULL)
	return -ENOMEM;

    list->arg = arg;
    list->name = sdsnewlen(json + label->name, label->namelen);
    list->value = sdsnewlen(json + label->value, label->valuelen);
    list->flags = label->flags;

    if (pmDebugOptions.series) {
	fprintf(stderr, "Annotate metric %s", metric->names[0].sds);
	if (instance)
	    fprintf(stderr, "[%s]", instance->name.sds);
	fprintf(stderr, " label %s=%s (flags=0x%x)\n",
			list->name, list->value, list->flags);
    }

    /* prepend map onto the list for this metric or instance */
    if (instance) {
	if (instance->labellist)
	    list->next = instance->labellist;
	instance->labellist = list;
    } else {
	if (metric->labellist)
	    list->next = metric->labellist;
	metric->labellist = list;
    }

    seriesBatonReference(baton, "annotate_metric");
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
    checkStatusReplyOK(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
		reply, "%s: %s", HMSET, "setting series label value");
    seriesBatonDereference(arg, "redis_series_labelvalue_callback");
}

static void
redis_series_labelflags_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkStatusReplyOK(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
		reply, "%s: %s", HMSET, "setting series label flags");
    seriesBatonDereference(arg, "redis_series_labelflags_callback");
}

static void
redis_series_label_set_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkIntegerReply(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
		reply, "%s %s", SADD, "pcp:series:label.X.value:Y");
    seriesBatonDereference(arg, "redis_series_label_set_callback");
}

static void
redis_series_label(redisSlots *slots, metric_t *metric, char *hash,
		labellist_t *list, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    sds				cmd, key, val, name;
    int				i;

    seriesBatonReferences(baton, 2, "redis_series_label");

    if (list->flags != PM_LABEL_CONTEXT) {
	seriesBatonReference(baton, "redis_series_label");

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
    sdsfree(name);
    sdsfree(val);
    redisSlotsRequest(slots, HMSET, key, cmd,
			redis_series_labelvalue_callback, arg);

    key = sdscatfmt(sdsempty(), "pcp:series:label.%I.value:%I",
			list->nameid, list->valueid);
    cmd = redis_command(2 + metric->numnames);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    for (i = 0; i < metric->numnames; i++)
	cmd = redis_param_sha(cmd, metric->names[i].hash);
    redisSlotsRequest(slots, SADD, key, cmd,
				redis_series_label_set_callback, arg);
}

static void
redis_series_labelset(redisSlots *slots, metric_t *metric, instance_t *instance, void *arg)
{
    labellist_t			*list;
    char			*hash;
    int				i;

    if (instance != NULL) {
	hash = pmwebapi_hash_str(instance->name.hash);
	list = instance->labellist;
	do {
	    redis_series_label(slots, metric, hash, list, arg);
	} while ((list = list->next) != NULL);
    } else {
	for (i = 0; i < metric->numnames; i++) {
	    hash = pmwebapi_hash_str(metric->names[0].hash);
	    list = metric->labellist;
	    do {
		redis_series_label(slots, metric, hash, list, arg);
	    } while ((list = list->next) != NULL);
	}
    }
}

void
series_metric_label_mapping(seriesGetNames *baton)
{
    struct seriesLoadBaton	*load;
    metric_t			*metric = baton->metric;
    char			buf[PM_MAXLABELJSONLEN];
    char			pmmsg[PM_MAXERRMSGLEN];
    sds				msg;
    int				sts;

    sts = metric_labelsets(metric, buf, sizeof(buf), annotate_metric, baton);
    if (sts < 0) {
	load = (struct seriesLoadBaton *)baton->baton;
	seriesfmt(msg, "Cannot merge metric %s [%s] label set: %s",
		pmwebapi_hash_str(metric->names[0].hash),
		metric->names[0].sds, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	(seriesLoadBatonInfo(load))(PMLOG_ERROR, msg, seriesLoadBatonUser(load));
	sdsfree(msg);
    }
    doneSeriesGetNames(baton, "series_label_mapping");
}

static void
series_name_mapping_callback(void *arg)
{
    seriesGetNames		*baton = (seriesGetNames *)arg;

    seriesBatonCheckMagic(arg, MAGIC_NAMES, "series_name_mapping_callback");
    if (UNLIKELY(pmDebugOptions.series)) {
	if (baton->instance)
	    fprintf(stderr, "Baton [names/%p] series_name_mapping_callback mapped instance %s to %lld\n", baton, baton->instance->name.sds, baton->instance->name.mapid);
	else
	    fprintf(stderr, "Baton [names/%p] series_name_mapping_callback mapped metric %s to %lld\n", baton, baton->metric->names[0].sds, baton->metric->names[0].mapid);
    }
    doneSeriesGetNames(arg, "series_name_mapping_callback");
}

void
series_instance_mapping(seriesGetNames *baton)
{
    struct seriesLoadBaton	*load;
    struct redisSlots		*slots;
    instance_t			*instance;
    metric_t			*metric;
    char			buf[PM_MAXLABELJSONLEN];
    char			pmmsg[PM_MAXERRMSGLEN];
    sds				msg;
    int				sts;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "series_instance_mapping");
    instance = baton->instance;
    metric = baton->metric;

    /* map instance name string to identifier if not already done */
    if (instance->name.mapid <= 0) {

	if (UNLIKELY(pmDebugOptions.series))
	    fprintf(stderr, "MAPPING instance name: %s\n", instance->name.sds);

	assert(instance->name.sds != NULL);
	seriesBatonReference(baton, "series_instance_mapping");
	load = (struct seriesLoadBaton *)baton->baton;
	slots = (struct redisSlots *)seriesLoadBatonSlots(load);
	redisGetMap(slots, instmap, instance->name.sds, &instance->name.mapid,
			series_name_mapping_callback, seriesLoadBatonInfo(load),
			seriesLoadBatonUser(load), baton);
    }

    sts = instance_labelsets(metric->indom, instance, buf, sizeof(buf),
			     annotate_metric, baton);
    if (sts < 0) {
	load = (struct seriesLoadBaton *)baton->baton;
	seriesfmt(msg, "Cannot merge instance %s [%s] label set: %s",
		pmwebapi_hash_str(instance->name.hash),
		instance->name.sds, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	(seriesLoadBatonInfo(load))(PMLOG_ERROR, msg, seriesLoadBatonUser(load));
	sdsfree(msg);
    }
    doneSeriesGetNames(baton, "series_label_mapping");
}

static void
series_instance_mapped(void *arg)
{
    seriesGetNames		*baton = (seriesGetNames *)arg;
    seriesGetNames		*metric = baton->arg;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "series_instance_mapped");
    seriesBatonCheckCount(baton, "series_instance_mapped");
    freeSeriesGetNames(baton, "series_instance_mapped");

    doneSeriesGetNames(metric, "series_instance_mapped");
}

static void redis_series_metadata(redisSlots *, context_t *, metric_t *, void *);
static void redis_series_streamed(redisSlots *, sds, metric_t *, void *); /*TODO*/

static void
series_stored_metric(void *arg)
{
    seriesGetNames		*baton = (seriesGetNames *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;

    seriesLoadBatonFetch(load);
}

static void
series_metric_mapped(void *arg)
{
    seriesGetNames		*baton = (seriesGetNames *)arg;
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)baton->baton;
    redisSlots			*slots = seriesLoadBatonSlots(load);
    context_t			*context = seriesLoadBatonContext(load);
    sds				timestamp = baton->timestamp;

    seriesBatonCheckMagic(baton, MAGIC_NAMES, "series_metric_mapped");
    seriesBatonCheckCount(baton, "series_metric_mapped");

    seriesBatonReference(load, "series_metric_mapped");
    seriesBatonReference(baton, "series_metric_mapped");
    baton->done = series_stored_metric;

    /* push the metric, instances and any label metadata into the cache */
    if (baton->meta || baton->data)
	redis_series_metadata(slots, context, baton->metric, load);

    /* push values for all instances, no-value or errors into the cache */
    if (baton->data)
	redis_series_streamed(slots, timestamp, baton->metric, load);

    doneSeriesGetNames(baton, "series_metric_mapped");
}

void
redis_series_metric(redisSlots *slots, metric_t *metric,
		sds timestamp, int meta, int data, void *arg)
{
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)arg;
    seriesGetNames		*mname, *iname;
    instance_t			*instance;
    value_t			*value;
    sds				msg;
    int				i;

    /*
     * First satisfy any/all mappings for metric name, instance
     * names, label names and values.  Then issue the metadata
     * and data simultaneously.
     */
    if ((mname = calloc(1, sizeof(seriesGetNames))) == NULL) {
	seriesfmt(msg, "OOM creating metric name baton for %s",
			metric->names[0].sds);
	seriesmsg(slots, PMLOG_ERROR, msg);
	return;
    }
    initSeriesGetNames(mname, metric, NULL, load);
    seriesBatonReference(mname, "redis_series_metric");
    mname->done = series_metric_mapped;

    mname->timestamp = sdsdup(timestamp);	/* TODO - context */
    mname->meta = meta;				/* TODO - context */
    mname->data = data;				/* TODO - context */

    for (i = 0; i < metric->numnames; i++) {
	assert(metric->names[i].sds != NULL);
	if (metric->names[i].mapid <= 0) {
	    seriesBatonReference(mname, "redis_series_metric names");
	    redisGetMap(slots,
			namesmap, metric->names[i].sds, &metric->names[i].mapid,
			series_name_mapping_callback,
			seriesLoadBatonInfo(load), seriesLoadBatonUser(load),
			(void *)mname);
	}
    }

    if (metric->desc.indom == PM_INDOM_NULL) {
	seriesBatonReference(mname, "redis_series_metric metric");
	series_metric_label_mapping(mname);
    } else {
	if (metric->u.vlist->listcount)
	    seriesBatonReference(mname, "redis_series_metric metric");
	for (i = 0; i < metric->u.vlist->listcount; i++) {
	    value = &metric->u.vlist->value[i];
	    if ((instance = findID(metric->indom->insts, &value->inst)) == NULL) {
		seriesfmt(msg, "indom lookup failure for %s instance %u",
				pmInDomStr(metric->indom->indom), value->inst);
		seriesmsg(slots, PMLOG_ERROR, msg);
		continue;
	    }
	    if ((iname = calloc(1, sizeof(seriesGetNames))) == NULL) {
		seriesfmt(msg, "OOM creating name baton for instance %s",
				instance->name.sds);
		seriesmsg(slots, PMLOG_ERROR, msg);
		continue;
	    }

	    initSeriesGetNames(iname, metric, instance, load);
	    seriesBatonReferences(iname, 2, "redis_series_metric instance");
	    iname->done = series_instance_mapped;
	    iname->arg = mname;
    
	    series_instance_mapping(iname);

	    doneSeriesGetNames(iname, "redis_series_metric instance");
	}
    }
    doneSeriesGetNames(mname, "redis_series_metric");
}

static void
redis_metric_name_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkIntegerReply(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
			reply, "%s %s", SADD, "map metric name to series");
    seriesBatonDereference(arg, "redis_metric_name_series_callback");
}

static void
redis_series_metric_name_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkIntegerReply(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
			reply, "%s: %s", SADD, "map series to metric name");
    seriesBatonDereference(arg, "redis_series_metric_name_callback");
}

static void
redis_desc_series_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkStatusReplyOK(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
			reply, "%s: %s", HMSET, "setting metric desc");
    seriesBatonDereference(arg, "redis_desc_series_callback");
}

static void
redis_series_source_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    checkIntegerReply(seriesLoadBatonInfo(arg), seriesLoadBatonUser(arg),
			reply, "%s: %s", SADD, "mapping series to context");
    seriesBatonDereference(arg, "redis_series_source_callback");
}

static void
redis_series_metadata(redisSlots *slots, context_t *context, metric_t *metric, void *arg)
{
    struct seriesLoadBaton	*baton = (struct seriesLoadBaton *)arg;
    instance_t			*instance;
    value_t			*value;
    const char			*hash;
    const char			*units, *indom, *pmid, *sem, *type;
    long long			map;
    sds				val, cmd, key;
    int				i;

    for (i = 0; i < metric->numnames; i++) {
	assert(metric->names[i].sds != NULL);
	map = metric->names[i].mapid;
	assert(map > 0);
	hash = pmwebapi_hash_str(metric->names[i].hash);

	seriesBatonReferences(baton, 2, "redis_series_metadata names");

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
	cmd = redis_param_sha(cmd, metric->names[i].hash);
	redisSlotsRequest(slots, SADD, key, cmd,
			redis_series_metric_name_callback, arg);
    }

    seriesBatonReferences(baton, 2, "redis_series_metadata");

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
    cmd = redis_param_sha(cmd, context->name.hash);
    cmd = redis_param_str(cmd, "type", sizeof("type")-1);
    cmd = redis_param_str(cmd, type, strlen(type));
    cmd = redis_param_str(cmd, "units", sizeof("units")-1);
    cmd = redis_param_str(cmd, units, strlen(units));
    redisSlotsRequest(slots, HMSET, key, cmd, redis_desc_series_callback, arg);

    hash = pmwebapi_hash_str(context->name.hash);
    key = sdscatfmt(sdsempty(), "pcp:series:source:%s", hash);
    cmd = redis_command(2 + metric->numnames);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    for (i = 0; i < metric->numnames; i++)
        cmd = redis_param_sha(cmd, metric->names[i].hash);
    redisSlotsRequest(slots, SADD, key, cmd, redis_series_source_callback, arg);

    if (metric->desc.indom == PM_INDOM_NULL) {
	redis_series_labelset(slots, metric, NULL, baton);
    } else {
	for (i = 0; i < metric->u.vlist->listcount; i++) {
	    value = &metric->u.vlist->value[i];
	    if ((instance = findID(metric->indom->insts, &value->inst)) == NULL)
		continue;
	    redis_series_instance(slots, metric, instance, baton);
	    redis_series_labelset(slots, metric, instance, baton);
	}
    }
}

typedef struct redisStreamBaton {
    seriesBatonMagic	header;
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
    initSeriesBatonMagic(baton, MAGIC_STREAM);
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
    void		*load = baton->arg;

    seriesBatonCheckMagic(baton, MAGIC_STREAM, "doneRedisStreamBaton");
    seriesBatonCheckMagic(load, MAGIC_LOAD, "doneRedisStreamBaton");
    memset(baton, 0, sizeof(*baton));
    free(baton);

    doneSeriesLoadBaton(load);
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

    if (!avp) {
	value = sdsnewlen("0", 1);
	goto append;
    }

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

append:
    return series_stream_append(cmd, name, value);
}

static void
redis_series_stream_callback(redisAsyncContext *c, redisReply *reply, void *arg)
{
    redisStreamBaton	*baton = (redisStreamBaton *)arg;
    sds			msg;

    seriesBatonCheckMagic(baton, MAGIC_STREAM, "redis_series_stream_callback");
    if (testReplyError(reply, REDIS_ESTREAMXADD)) {
	seriesfmt(msg, "duplicate or early stream %s insert at time %s",
		baton->hash, baton->stamp);
	seriesmsg(baton, PMLOG_WARNING, msg);
    }
    else {
	checkStatusReplyString(baton->info, baton->userdata, reply,
		baton->stamp, "stream %s status mismatch at time %s",
		baton->hash, baton->stamp);
    }
    doneRedisStreamBaton(baton);
}

static void
redis_series_stream(redisSlots *slots, sds stamp, metric_t *metric,
		const char *hash, void *arg)
{
    struct seriesLoadBaton	*load = (struct seriesLoadBaton *)arg;
    redisInfoCallBack		info = seriesLoadBatonInfo(load);
    redisStreamBaton		*baton;
    unsigned int		count;
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
    seriesBatonReference(load, "redis_series_stream");

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
	} else if (metric->u.vlist->listcount <= 0) {
	    stream = series_stream_append(stream, sdsnew("0"), sdsnew("0"));
	    count += 2;
	} else {
	    for (i = 0; i < metric->u.vlist->listcount; i++) {
		instance_t	*inst;
		value_t		*v = &metric->u.vlist->value[i];

		if ((inst = findID(metric->indom->insts, &v->inst)) == NULL)
		    continue;
		name = sdscpylen(name, (const char *)inst->name.hash, sizeof(inst->name.hash));
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

static void
redis_series_streamed(redisSlots *slots, sds stamp, metric_t *metric, void *arg)
{
    const char			*hash;
    int				i;

    for (i = 0; i < metric->numnames; i++) {
	hash = pmwebapi_hash_str(metric->names[i].hash);
	redis_series_stream(slots, stamp, metric, hash, arg);
    }
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

    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "redis_update_version_callback");
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

    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "redis_load_version_callback");

    baton->version = -1;
    if (reply->type == REDIS_REPLY_STRING) {
	version = (unsigned int)atoi(reply->str);
	if (version == 0 || version == SCHEMA_VERSION) {
	    baton->version = version;
	} else {
	    seriesfmt(msg, "unsupported schema (got v%u, expected v%u)",
			version, SCHEMA_VERSION);
	    seriesmsg(baton, PMLOG_ERROR, msg);
	}
    } else if (reply->type == REDIS_REPLY_ERROR) {
	seriesfmt(msg, "version check error: %s", reply->str);
	seriesmsg(baton, PMLOG_REQUEST, msg);
    } else if (reply->type != REDIS_REPLY_NIL) {
	seriesfmt(msg, "unexpected schema version reply type (%s)",
		redis_reply(reply->type));
	seriesmsg(baton, PMLOG_ERROR, msg);
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
	seriesfmt(msg, "insufficient elements in cluster NODE reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }

    value = reply->element[1];
    if (value->type != REDIS_REPLY_INTEGER) {
	seriesfmt(msg, "expected integer port in cluster NODE reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    port = (unsigned int)value->integer;

    value = reply->element[0];
    if (value->type != REDIS_REPLY_STRING) {
	seriesfmt(msg, "expected string hostspec in cluster NODE reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
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
	seriesfmt(msg, "insufficient elements in cluster SLOT reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    memset(&slots, 0, sizeof(slots));

    node = reply->element[0];
    if ((slot = checkIntegerReply(baton->info, baton->userdata,
				node, "%s start", "SLOT")) < 0) {
	seriesfmt(msg, "expected integer start in cluster SLOT reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
	return -EINVAL;
    }
    slots.start = (__uint32_t)slot;
    node = reply->element[1];
    if ((slot = checkIntegerReply(baton->info, baton->userdata,
				node, "%s end", "SLOT")) < 0) {
	seriesfmt(msg, "expected integer end in cluster SLOT reply");
	seriesmsg(baton, PMLOG_WARNING, msg);
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

    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "redis_load_slots_callback");

    /* Case of a single Redis instance or loosely cooperating instances */
    if (testReplyError(reply, REDIS_ENOCLUSTER)) {
	/* TODO: allow setup of multiple servers via configuration file */
	if ((servers = calloc(1, sizeof(redisSlotServer))) != NULL) {
	    if ((slots = calloc(1, sizeof(redisSlotRange))) == NULL) {
		seriesfmt(msg, "failed to allocate Redis slots memory");
		seriesmsg(baton, PMLOG_ERROR, msg);
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
    initSeriesBatonMagic(baton, MAGIC_SLOTS);
    baton->info = info;
    baton->done = done;
    baton->version = version;
    baton->userdata = userdata;
    baton->arg = arg;
}

void
doneRedisSlotsBaton(redisSlotsBaton *baton)
{
    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "doneRedisSlotsBaton");
    baton->done(baton->arg);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

redisSlots *
redisSlotsConnect(sds server, int version_check,
		redisInfoCallBack info, redisDoneCallBack done,
		void *userdata, void *events, void *arg)
{
    redisSlotsBaton		*baton;
    redisSlots			*slots;
    sds				msg;

    if ((baton = (redisSlotsBaton *)calloc(1, sizeof(redisSlotsBaton))) != NULL) {
	if ((slots = redisSlotsInit(server, info, events, userdata)) != NULL) {
	    initRedisSlotsBaton(baton, version_check, info, done,
				    userdata, events, arg);
	    baton->redis = slots;
	    redis_load_slots(baton);
	    return slots;
	}
	baton->version = -1;
	doneRedisSlotsBaton(baton);
    } else {
	done(arg);
    }
    seriesfmt(msg, "Failed to allocate memory for Redis slots");
    info(PMLOG_ERROR, msg, arg);
    sdsfree(msg);
    return NULL;
}

void
pmSeriesSetup(pmSeriesCommand *command, void *arg)
{
    /* fast path for when Redis has been setup already */
    if (command->slots) {
	command->on_setup(arg);
	return;
    }

    /* create global EVAL hashes and string map caches */
    redisScriptsInit();
    redisMapsInit();

    /* establish initial connection to Redis instances */
    command->slots = redisSlotsConnect(
			command->hostspec, 1, command->on_info,
			command->on_setup, arg, command->events, arg);
}

void
pmSeriesClose(pmSeriesCommand *command)
{
    redisSlotsFree((redisSlots *)command->slots);
    command->slots = NULL;
}
