/*
 * Copyright (c) 2017 Red Hat.
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
#include <hiredis/hiredis.h>
#include "redis.h"
#include "util.h"

#define PCP_SCHEMA_VERSION 1

/* TODO: externalise Redis configuration */
static char default_server[] = "localhost:6379";

typedef struct redis_script {
    const char	*script;
    char	*hash;
} redisScript;

static redisScript scripts[] = {
/* Script HASH_MAP_ID pcp:map:<name> , <string> -> ID
	returns a map identifier from a given key (hash) and
	value (string).  If the string has not been observed
	previously a new identifier is assigned and the hash
	updated.  This is a cache-internal identifier only.
 */
    { .script = \
	"local ID = redis.pcall('HGET', KEYS[1], ARGV[1])\n"
	"if (ID == false) then\n"
	"    ID = redis.pcall('HLEN', KEYS[1]) + 1\n"
	"    redis.call('HSETNX', KEYS[1], ARGV[1], tostring(ID))\n"
	"end\n"
	"return tonumber(ID)\n"
    },
};

enum {
    HASH_MAP_ID = 0,
    NSCRIPTS
};

static void
redis_load_scripts(redisContext *redis)
{
    redisReply	*reply;
    char	*cmd;
    int		i, len, sts;

    for (i = 0; i < NSCRIPTS; i++) {
	len = redisFormatCommand(&cmd, "SCRIPT LOAD %s", scripts[i].script);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed LOAD redis LUA script[%d] setup\n%s\n",
			    i, scripts[i].script);
	    exit(1);
	}
	free(cmd);
    }

    for (i = 0; i < NSCRIPTS; i++) {
	sts = redisGetReply(redis, (void **)&reply);
	if (sts != REDIS_OK || reply->type != REDIS_REPLY_STRING) {
	    fprintf(stderr, "Failed to LOAD redis LUA script[%d]: %s\n%s\n",
			    i, reply->str, scripts[i].script);
	    exit(1);
	}
	if ((scripts[i].hash = strdup(reply->str)) == NULL) {
	    fprintf(stderr, "Failed to save LUA script SHA1 hash\n");
	    exit(1);
	}
	freeReplyObject(reply);

	if (pmDebugOptions.series)
	    fprintf(stderr, "Registered script[%d] as %s\n", i, scripts[i].hash);
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

    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    exit(1);
}

static int
checkInteger(redisReply *reply, const char *format, ...)
{
    va_list	arg;

    if (reply->type == REDIS_REPLY_INTEGER)
	return reply->integer;

    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    exit(1);
}

static int
redis_strmap(redisContext *redis, char *map, char *value)
{
    redisReply	*reply;
    int		mapID;

    reply = redisCommand(redis,
			"EVALSHA %s 1 %s %s",
			scripts[HASH_MAP_ID].hash, map, value);
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
	mapID = reply->integer;
	freeReplyObject(reply);
	return mapID;
    }

    fprintf(stderr, "Failed to EVALSHA %s, string map for %s[%s]\n",
		    scripts[HASH_MAP_ID].hash, map, value);
    exit(1);
}

void
redis_series_desc(redisContext *redis, metric_t *metric, value_t *value)
{
    redisReply	*reply;

    if (metric->desc.indom != PM_INDOM_NULL) {
	reply = redisCommand(redis,
		"HMSET pcp:desc:series:%s"
		" cluster %u"
		" domain %u"
		" item %u"
		" semantics %u"
		" serial %d"
		" type %u"
		" units %s",
	    value->hash,
	    pmID_cluster(metric->desc.pmid),
	    pmID_domain(metric->desc.pmid),
	    pmID_item(metric->desc.pmid),
	    metric->desc.sem,
	    pmInDom_serial(metric->desc.indom),
	    metric->desc.type,
	    pmUnitsStr(&metric->desc.units));
    } else {
	reply = redisCommand(redis,
		"HMSET pcp:desc:series:%s"
		" cluster %u"
		" domain %u"
		" item %u"
		" semantics %u"
		" type %u"
		" units %s",
	    value->hash,
	    pmID_cluster(metric->desc.pmid),
	    pmID_domain(metric->desc.pmid),
	    pmID_item(metric->desc.pmid),
	    metric->desc.sem,
	    metric->desc.type,
	    pmUnitsStr(&metric->desc.units));
    }
    checkStatusOK(reply, "pcp:desc:series:%s setup\n", value->hash);
    freeReplyObject(reply);
}

void
redis_series_inst(redisContext *redis, metric_t *metric, value_t *value)
{
    redisReply	*reply;
    int		mapID;

    if (!value->name)
	return;
    mapID = redis_strmap(redis, "pcp:map:inst.name", value->name);

    reply = redisCommand(redis,
		"HMSET pcp:inst:series:%s id %u name %u",
		value->hash, value_instid(value), mapID);
    checkStatusOK(reply, "pcp:inst:series:%s setup\n", value->hash);
    freeReplyObject(reply);

    reply = redisCommand(redis,
		"SADD pcp:series:inst.name:%u %s", mapID, value->hash);
    checkInteger(reply, "pcp:series:inst.name:%u (sadd %s)\n", mapID, value->hash);
    freeReplyObject(reply);
}

static int
redis_series_name(redisContext *redis, metric_t *mp, int index, value_t *value)
{
    redisReply	*reply;
    char	*name = mp->names[index];
    int		mapID = mp->mapids[index];

    if (!name)
	return -EINVAL;
    if (!mapID) {
	mapID = redis_strmap(redis, "pcp:map:metric.name", name);
	mp->mapids[index] = mapID;
    }

    reply = redisCommand(redis,
		"SADD pcp:metric.name:series:%s %u", value->hash, mapID);
    checkInteger(reply, "pcp:series:metric.name:%s (sadd %u)\n", value->hash, mapID);
    freeReplyObject(reply);

    reply = redisCommand(redis,
		"SADD pcp:series:metric.name:%u %s", mapID, value->hash);
    checkInteger(reply, "pcp:series:metric.name:%u (sadd %s)\n", mapID, value->hash);
    freeReplyObject(reply);
    return 0;
}

static void
redis_series_pmns(redisContext *redis, metric_t *metric, value_t *value)
{
    int		i;

    for (i = 0; i < metric->numnames; i++)
	redis_series_name(redis, metric, i, value);
}

typedef struct {
    redisContext	*redis;
    metric_t		*metric;
    value_t		*value;
    const char		*type;
} annotate_t;

static int
cache_annotation(const pmLabel *label, const char *json, annotate_t *my)
{
    redisReply	*reply;
    const char	*offset;
    size_t	length;
    char	key[256];
    char	val[PM_MAXLABELJSONLEN];
    int		name_mapID;
    int		value_mapID;

    offset = json + label->name;
    snprintf(val, sizeof(val), "%.*s", label->namelen, offset);
    snprintf(key, sizeof(key), "pcp:map:%s.name", my->type);
    name_mapID = redis_strmap(my->redis, key, val);

    offset = json + label->value;
    length = label->valuelen;

    if (*offset == '\"') {	/* remove string quotes */
	offset++;
	length -= 2;	/* remove quotes from both ends */
    }
    // TODO: if (*offset == '{') ... decode map recursively
    // TODO: if (*offset == '[') ... divvy up the array too

    snprintf(val, sizeof(val), "%.*s", (int)length, offset);
    snprintf(key, sizeof(key), "pcp:map:%s.%d.value", my->type, name_mapID);
    value_mapID = redis_strmap(my->redis, key, val);

    reply = redisCommand(my->redis,
		"SADD pcp:%s.name:series:%s %d",
		my->type, my->value->hash, name_mapID);
    checkInteger(reply, "pcp:%s.name:series:%s (sadd %d)\n",
		my->type, my->value->hash, name_mapID);
    freeReplyObject(reply);

    reply = redisCommand(my->redis,
		"SADD pcp:series:%s.%d.value:%d %s",
		my->type, name_mapID, value_mapID, my->value->hash);
    checkInteger(reply, "pcp:series:%s.%d.value:%d (sadd %s)\n",
		my->type, name_mapID, value_mapID, my->value->hash);
    freeReplyObject(reply);
    return 0;
}

static int
cache_label(const pmLabel *label, const char *json, void *arg)
{
    if ((label->flags & PM_LABEL_OPTIONAL) != 0)
	return 0;
    return cache_annotation(label, json, (annotate_t *)arg);
}

static int
cache_note(const pmLabel *label, const char *json, void *arg)
{
    if ((label->flags & PM_LABEL_OPTIONAL) == 0)
	return 0;
    return cache_annotation(label, json, (annotate_t *)arg);
}

void
redis_series_annotate(redisContext *redis,
	metric_t *metric, value_t *value, const char *type,
	int (*filter)(const pmLabel *, const char *, void *))
{
    annotate_t	annotate;
    char	buf[PM_MAXLABELJSONLEN];
    int		sts;

    annotate.redis = redis;
    annotate.metric = metric;
    annotate.value = value;
    annotate.type = type;

    sts = merge_labelsets(metric, value, buf, sizeof(buf), filter, &annotate);
    if (sts < 0) {
	fprintf(stderr, "%s: failed to merge series %s labelsets: %s\n",
		pmGetProgname(), value->hash, pmErrStr(sts));
	exit(1);
    }
}

void
redis_series_metadata(redisContext *redis, metric_t *metric, value_t *value)
{
    redis_series_pmns(redis, metric, value);
    redis_series_inst(redis, metric, value);
    redis_series_desc(redis, metric, value);

    redis_series_annotate(redis, metric, value, "label", cache_label);
    redis_series_annotate(redis, metric, value, "note", cache_note);
}

void
redis_series_addvalue(redisContext *redis, metric_t *metric, value_t *value)
{
    redisReply	*reply;
    double	timestamp = pmtimevalToReal(&value->lasttime);

    reply = redisCommand(redis, "ZADD pcp:values:series:%s %.64g %s",
		value->hash, timestamp, value_atomstr(metric, value));
    checkInteger(reply, "pcp:values:series:%s sorted set update", value->hash);
    freeReplyObject(reply);
}

static redisContext *
redis_connect(char *server, struct timeval *timeout)
{
    redisContext *redis;

    if (server == NULL)
	server = strdup("localhost:6379");

    if (strncmp(server, "unix:", 5) == 0) {
        redis = redisConnectUnixWithTimeout(server + 5, *timeout);
    } else {
        unsigned int    port;
        char            *endnum, *p;

        if ((p = rindex(server, ':')) == NULL) {
            port = 6379;  /* default redis port */
        } else {
            port = (unsigned int) strtoul(p + 1, &endnum, 10);
            if (*endnum != '\0')
                port = 6379;
            else
                *p = '\0';
        }
        redis = redisConnectWithTimeout(server, port, *timeout);
    }

    if (!redis || redis->err) {
        if (redis) {
            fprintf(stderr, "Redis connection error: %s\n", redis->errstr);
            redisFree(redis);
        } else {
            fprintf(stderr, "Redis connection error: can't allocate context\n");
        }
        return NULL;
    }

    redisSetTimeout(redis, *timeout);
    redisEnableKeepAlive(redis);
    return redis;
}

static void
redis_check_schema(redisContext *redis)
{
    redisReply	*reply = redisCommand(redis, "GET pcp:version:schema");

    if (reply->type == REDIS_REPLY_STRING) {
	unsigned int	version = (unsigned int) atoi(reply->str);

	if (!version || version > PCP_SCHEMA_VERSION) {
	    fprintf(stderr, "%s: unknown schema version in use (%u)\n",
		    pmGetProgname(), version);
	    exit(1);
	}
    } else if (reply->type != REDIS_REPLY_NIL) {
	fprintf(stderr, "%s: unknown schema version type (%u)\n",
		pmGetProgname(), reply->type);
	exit(1);
    } else {
	freeReplyObject(reply);

	reply = redisCommand(redis,
		"SET pcp:version:schema %u", PCP_SCHEMA_VERSION);
	checkStatusOK(reply, "pcp:schema:version setup");
	freeReplyObject(reply);
    }
}

redisContext *
redis_init(void)
{
    struct timeval	timeout = { 1, 500000 }; // 1.5 seconds
    char		*server = &default_server[0];
    redisContext	*context;

    if ((context = redis_connect(server, &timeout)) == NULL)
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
