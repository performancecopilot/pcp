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
#include <pcp/pmda.h>

#define STRINGIFY(s)	#s
#define TO_STRING(s)	STRINGIFY(s)
#define SCHEMA_VERSION	2
#define SHA1SZ		20

enum { INSTMAP, NAMESMAP, LABELSMAP };	/* name:mapid caches */

typedef struct redis_script {
    const char		*text;
    sds			hash;
} redisScript;

static int duplicates;	/* TODO: add to individual contexts */

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
/* Script DUMP_SHA_ID
   TODO: dump internal (20-byte) SHA as external (40-byte)
 */
};

enum {
    HASH_MAP_ID = 0,
    NSCRIPTS
};

static void
redis_load_scripts(redisContext *redis)
{
    redisReply		*reply;
    redisScript		*script;
    int			i, sts;
    sds			cmd;

    for (i = 0; i < NSCRIPTS; i++) {
	script = &scripts[i];
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, "SCRIPT", sizeof("SCRIPT")-1);
	cmd = redis_param_str(cmd, "LOAD", sizeof("LOAD")-1);
	cmd = redis_param_str(cmd, script->text, strlen(script->text));

	/* Note: needs to be executed on all Redis instances */
	if (redisAppendFormattedCommand(redis, cmd, sdslen(cmd)) != REDIS_OK) {
	    fprintf(stderr, "Failed to LOAD Redis LUA script[%d] setup\n%s\n",
			    i, script->text);
	    exit(1);	/* TODO: propogate error */
	}
	sdsfree(cmd);
    }

    for (i = 0; i < NSCRIPTS; i++) {
	script = &scripts[i];
	sts = redisGetReply(redis, (void **)&reply);
	if (sts != REDIS_OK || reply->type != REDIS_REPLY_STRING) {
	    fprintf(stderr, "Failed to LOAD Redis LUA script[%d]: %s\n%s\n",
			    i, reply->str, script->text);
	    exit(1);	/* TODO: propogate error */
	}
	if ((scripts[i].hash = sdsnew(reply->str)) == NULL) {
	    fprintf(stderr, "Failed to save LUA script SHA1 hash\n");
	    exit(1);	/* TODO: propogate error */
	}
	freeReplyObject(reply);

	if (pmDebugOptions.series)
	    fprintf(stderr, "Registered script[%d] as %s\n", i, script->hash);
    }
}

static void
checkStatusOK(redisReply *reply, const char *format, ...)
{
    va_list	arg;

    if (reply->type == REDIS_REPLY_STATUS &&
	(strcmp("OK", reply->str) == 0 || strcmp("QUEUED", reply->str) == 0)) {
	return;
    }

    fputs("Error: ", stderr);
    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    if (reply->type == REDIS_REPLY_ERROR)
	fprintf(stderr, "\nRedis: %s\n", reply->str);
    else
	fputc('\n', stderr);
}

static int
checkStreamDup(redisReply *reply, sds stamp, const char *hash)
{
    const char dupmsg[] = \
"ERR The ID specified in XADD is smaller than the target stream top item";

    if (reply->type == REDIS_REPLY_ERROR && strcmp(dupmsg, reply->str) == 0) {
	fprintf(stderr, "Warning: dup stream %s insert at %s\n", hash, stamp);
	return 1;
    }
    return 0;
}

static void
checkStatusString(redisReply *reply, sds s, const char *format, ...)
{
    va_list	arg;

    if (reply->type == REDIS_REPLY_STATUS && strcmp(s, reply->str) == 0)
	return;

    fprintf(stderr, "Error: ");
    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    if (reply->type == REDIS_REPLY_ERROR)
	fprintf(stderr, "\nRedis: %s\n", reply->str);
    else
	fputc('\n', stderr);
}

static long long
checkMapScript(redisReply *reply, long long *add, sds s, const char *format, ...)
{
    va_list	arg;

    /* on success, map script script returns two integer values via array */
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements == 2 &&
	reply->element[0]->type == REDIS_REPLY_INTEGER &&
	reply->element[1]->type == REDIS_REPLY_INTEGER) {
	*add = reply->element[1]->integer;	/* is this newly allocated */
	return reply->element[0]->integer;	/* the actual string mapid */
    }

    fprintf(stderr, "Error: ");
    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    if (reply->type == REDIS_REPLY_ERROR)
	fprintf(stderr, "\nRedis: %s\n", reply->str);
    else
	fputc('\n', stderr);
    return -1;
}

static long long
checkInteger(redisReply *reply, const char *format, ...)
{
    va_list	arg;

    if (reply && reply->type == REDIS_REPLY_INTEGER)
	return reply->integer;

    fputs("Error: ", stderr);
    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    if (reply && reply->type == REDIS_REPLY_ERROR)
	fprintf(stderr, "\nRedis: %s\n", reply->str);
    else
	fputc('\n', stderr);
    return -1;
}

static long long
redis_strmap(redisContext *redis, char *name, const char *value)
{
    redisReply		*reply;
    long long		map, add = 0;
    sds			cmd, msg, key;

    key = sdscatfmt(sdsempty(), "pcp:map:%s", name);
    cmd = redis_command(5);
    cmd = redis_param_str(cmd, EVALSHA, EVALSHA_LEN);
    cmd = redis_param_sds(cmd, scripts[HASH_MAP_ID].hash);
    cmd = redis_param_str(cmd, "1", sizeof("1")-1);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, value, strlen(value));
    redis_submit(redis, EVALSHA, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    map = checkMapScript(reply, &add, "%s: %s (%s)" EVALSHA,
			"string mapping script", name);
    freeReplyObject(reply);

    /* publish any newly created name mapping */
    if (add) {
	msg = sdscatfmt(sdsempty(), "%I:%s", map, value);
	key = sdscatfmt(sdsempty(), "pcp:channel:%s", name);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, PUBLISH, PUBLISH_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, msg);
	sdsfree(msg);
	redis_submit(redis, PUBLISH, key, cmd);

	/* TODO: async callback function */
	redisGetReply(redis, (void **)&reply);
	checkInteger(reply, "%s: %s", PUBLISH, "new %s mapping", name);
	freeReplyObject(reply);
    }

    return map;
}

void
redis_series_source(redisContext *redis, context_t *context)
{
    redisReply		*reply;
    const char		*hash = pmwebapi_hash_str(context->hash);
    long long		map;
    sds			cmd, key, val;

    if ((map = context->mapid) <= 0) {
	map = redis_strmap(redis, "context.name", context->name);
	context->mapid = map;
    }

    key = sdscatfmt(sdsempty(), "pcp:source:context.name:%I", map);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, context->hash);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s: %s", SADD, "mapping context to source name");
    freeReplyObject(reply);

    val = sdscatfmt(sdsempty(), "%I", map);
    key = sdscatfmt(sdsempty(), "pcp:context.name:source:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, val);
    sdsfree(val);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s: %s", SADD, "mapping source name to context");
    freeReplyObject(reply);
}

void
redis_series_inst(redisContext *redis, metric_t *metric, value_t *value)
{
    redisReply		*reply;
    const char		*ihash = pmwebapi_hash_str(value->hash);
    const char		*hash = pmwebapi_hash_str(metric->hash);
    long long		map;
    sds			cmd, key, val, id;

    if (!value->name)
	return;

    /* TODO: need instance name, label name, & label value map hashes */
    if ((map = value->mapid) <= 0) {
	map = redis_strmap(redis, "inst.name", value->name);
	value->mapid = map;
    }

    key = sdscatfmt(sdsempty(), "pcp:series:inst.name:%I", map);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, metric->hash);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s: %s", SADD, "mapping series to inst name");
    freeReplyObject(reply);

    key = sdscatfmt(sdsempty(), "pcp:instances:series:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, value->hash);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s: %s", SADD, "mapping instance to series");
    freeReplyObject(reply);

    id = sdscatfmt(sdsempty(), "%I", value->mapid);
    val = sdscatfmt(sdsempty(), "%i", value->inst);
    key = sdscatfmt(sdsempty(), "pcp:inst:series:%s", ihash);
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
    redis_submit(redis, HMSET, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkStatusOK(reply, "%s: %s", HMSET, "setting metric inst");
    freeReplyObject(reply);
}

typedef struct {
    redisContext	*redis;
    metric_t		*metric;
    value_t		*value;
} annotate_t;

static int
annotate_metric(const pmLabel *label, const char *json, annotate_t *my)
{
    redisContext	*redis = my->redis;
    redisReply		*reply;
    const char		*offset;
    size_t		length;
    const char		*hash = pmwebapi_hash_str(my->metric->hash);
    long long		value_mapid, name_mapid;
    sds			cmd, key, val;

    offset = json + label->name;
    val = sdsnewlen(offset, label->namelen);
    name_mapid = redis_strmap(redis, "label.name", val);
    sdsfree(val);

    offset = json + label->value;
    length = label->valuelen;

    if (*offset == '\"') {	/* remove string quotes */
	offset++;
	length -= 2;	/* remove quotes from both ends */
    }
    /* TODO: if (*offset == '{') ... decode map recursively */
    /* TODO: if (*offset == '[') ... split up an array also */

    /* TODO: need link between instance labels and instance names? */
    /* or instance name maps?  for finding matching inst in series */

    val = sdsnewlen(offset, length);
    key = sdscatfmt(sdsempty(), "label.%I.value", name_mapid);
    value_mapid = redis_strmap(redis, key, val);
    sdsfree(key);
    sdsfree(val);

    val = sdscatfmt(sdsempty(), "%I", name_mapid);
    key = sdscatfmt(sdsempty(), "pcp:label.name:series:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, val);
    sdsfree(val);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s %s", SADD, key, hash);
    freeReplyObject(reply);

    key = sdscatfmt(sdsempty(), "pcp:series:label.%I.value:%I",
		    name_mapid, value_mapid);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, my->metric->hash);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s %s", "SADD", key);
    freeReplyObject(reply);

    if (my->value != NULL) {
	value_mapid = my->value->mapid;

	val = sdscatfmt(sdsempty(), "%I", value_mapid);
	key = sdscatfmt(sdsempty(), "pcp:label.name:inst.name.%I:series:%s",
			value_mapid, hash);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, val);
	sdsfree(val);
	redis_submit(redis, SADD, key, cmd);

	/* TODO: async callback function */
	redisGetReply(my->redis, (void **)&reply);
	checkInteger(reply, "%s %s", SADD, "mapping inst labels to series");
	freeReplyObject(reply);
    }

    return 0;
}

static int
filter(const pmLabel *label, const char *json, void *arg)
{
    if (pmDebugOptions.series)
	fprintf(stderr, "Caching label %.*s=%.*s (optional=%s)\n",
		label->namelen, json + label->name,
		label->valuelen, json + label->value,
		(label->flags & PM_LABEL_OPTIONAL) ? "yes" : "no");

    return annotate_metric(label, json, (annotate_t *)arg);
}

void
redis_series_label(redisContext *redis, metric_t *metric, value_t *value)
{
    annotate_t	annotate;
    char	buf[PM_MAXLABELJSONLEN];
    int		sts;

    annotate.redis = redis;
    annotate.metric = metric;
    annotate.value = value;

    sts = merge_labelsets(metric, value, buf, sizeof(buf), filter, &annotate);
    if (sts < 0) {
	fprintf(stderr, "%s: failed to merge series %s labelsets: %s\n",
		pmGetProgname(), pmwebapi_hash_str(metric->hash),
		pmErrStr(sts));
	exit(1);
    }
}

void
redis_series_metric(redisContext *redis, context_t *context, metric_t *metric)
{
    redisReply	*reply;
    value_t	*value;
    const char	*hash = pmwebapi_hash_str(metric->hash);
    const char	*units, *indom, *pmid, *sem, *type;
    char	*name;
    int		i, map;
    sds		val, cmd, key;

    for (i = 0; i < metric->numnames; i++) {
	if ((name = metric->names[i]) == NULL)
	    continue;
	if ((map = metric->mapids[i]) <= 0) {
	    map = redis_strmap(redis, "metric.name", name);
	    metric->mapids[i] = map;
	}

	key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%s", hash);
	val = sdscatfmt(sdsempty(), "%I", map);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, val);
	sdsfree(val);
	redis_submit(redis, SADD, key, cmd);

	/* TODO: async callback function */
	redisGetReply(redis, (void **)&reply);
	checkInteger(reply, "%s %s", SADD, "map metric name to series");
	freeReplyObject(reply);

	key = sdscatfmt(sdsempty(), "pcp:series:metric.name:%I", map);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SADD, SADD_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sha(cmd, metric->hash);
	redis_submit(redis, SADD, key, cmd);

	/* TODO: async callback function */
	redisGetReply(redis, (void **)&reply);
	checkInteger(reply, "%s: %s", SADD, "map series to metric name");
	freeReplyObject(reply);
    }

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
    redis_submit(redis, HMSET, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkStatusOK(reply, "%s: %s", HMSET, "setting metric desc");
    freeReplyObject(reply);

    if (metric->desc.indom == PM_INDOM_NULL) {
	redis_series_label(redis, metric, NULL);
    } else {
	for (i = 0; i < metric->u.inst->listcount; i++) {
	    value = &metric->u.inst->value[i];
	    redis_series_inst(redis, metric, value);
	    redis_series_label(redis, metric, value);
	}
    }

    hash = pmwebapi_hash_str(context->hash);
    key = sdscatfmt(sdsempty(), "pcp:series:source:%s", hash);
    cmd = redis_command(3);
    cmd = redis_param_str(cmd, SADD, SADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sha(cmd, metric->hash);
    redis_submit(redis, SADD, key, cmd);

    /* TODO: async callback function */
    redisGetReply(redis, (void **)&reply);
    checkInteger(reply, "%s: %s", SADD, "mapping series to context");
    freeReplyObject(reply);
}

static sds
series_stream_append(sds cmd, sds name, sds value)
{
    unsigned int	nlen = sdslen(name);
    unsigned int	vlen = sdslen(value);

//fprintf(stderr, "series_stream_append enter: cmd=%s name=%s value=%s\n", cmd, name, value);
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

//fprintf(stderr, "series_stream_value enter: cmd=%s\n", cmd);
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
redis_series_stream(redisContext *redis, sds stamp, metric_t *metric)
{
    unsigned int	count;
    redisReply		*reply;
    const char		*hash = pmwebapi_hash_str(metric->hash);
    int			i, sts, type;
    sds			cmd, key, name = sdsempty(), stream = sdsempty();

    count = 3;	/* XADD key stamp */
    key = sdscatfmt(sdsempty(), "pcp:values:series:%s", hash);

    if ((sts = metric->error) < 0) {
	stream = series_stream_append(stream,
			sdsnewlen("-1", 2), sdscatfmt(sdsempty(), "%i", sts));
	count += 2;
    } else {
	type = metric->desc.type;
	if (metric->desc.indom == PM_INDOM_NULL) {
	    sdsclear(name);
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
    }
    sdsfree(name);

    cmd = redis_command(count);
    cmd = redis_param_str(cmd, XADD, XADD_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, stamp);
    cmd = redis_param_raw(cmd, stream);
    sdsfree(stream);
    redis_submit(redis, XADD, key, cmd);

    /* TODO: check return codes, use async callbacks */
    redisGetReply(redis, (void**)&reply);
    if (checkStreamDup(reply, stamp, hash))
	duplicates++;
    else
	checkStatusString(reply, stamp, "status mismatch (%s)", stamp);
    freeReplyObject(reply);
}

void
redis_series_mark(redisContext *redis, context_t *context, sds timestamp)
{
    /* TODO: inject mark records into time series */
}

static void
redis_check_schema(redisContext *redis)
{
    redisReply		*reply;
    unsigned int	version;
    const char		ver[] = TO_STRING(SCHEMA_VERSION);
    sds			cmd, key;

    key = sdsnew("pcp:version:schema");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, GETS, GETS_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, GETS, key, cmd);

    /* TODO: check-version-string function (callback) */
    redisGetReply(redis, (void**)&reply);
    if (reply->type == REDIS_REPLY_STRING) {
	version = (unsigned int)atoi(reply->str);
	if (!version || version != SCHEMA_VERSION) {
	    fprintf(stderr, "%s: unsupported schema (got v%u, not v%u)\n",
		    pmGetProgname(), version, SCHEMA_VERSION);
	    exit(1);
	}
    } else if (reply->type != REDIS_REPLY_NIL) {
	fprintf(stderr, "%s: unknown schema version type (%u)\n",
		pmGetProgname(), reply->type);
	exit(1);
    } else {
	freeReplyObject(reply);

	/* TODO: setup-version-string function (callback) */
	key = sdsnew("pcp:version:schema");
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, SETS, SETS_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, ver, sizeof(ver)-1);
	redis_submit(redis, SETS, key, cmd);

	/* TODO: check-version-setup function (callback) */
	redisGetReply(redis, (void **)&reply);
	checkStatusOK(reply, "%s setup", "pcp:version:schema");
	freeReplyObject(reply);
    }
}

redisContext *
redis_init(void)
{
    redisContext	*context;   /* TODO: redisSlots */
    static int		setup;

    if (!setup) { /* global string map caches */
	pmdaCacheOp(INSTMAP, PMDA_CACHE_STRINGS);
	pmdaCacheOp(NAMESMAP, PMDA_CACHE_STRINGS);
	pmdaCacheOp(LABELSMAP, PMDA_CACHE_STRINGS);
	setup = 1;
    }

    if ((context = redis_connect(NULL, NULL)) == NULL)
	exit(1);	/* TODO: improve error handling */
    redis_check_schema(context);
    redis_load_scripts(context);

    return context;
}

void
redis_stop(redisContext *redis)
{
    redisFree(redis);
}
