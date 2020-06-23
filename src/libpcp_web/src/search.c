/*
 * Copyright (c) 2020 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <assert.h>
#include "pmapi.h"
#include "pmda.h"
#include "schema.h"
#include "search.h"
#include "util.h"
#include "sha1.h"

static sds		resultcount;

static void
initRedisSearchBaton(redisSearchBaton *baton,
		pmSearchSettings *settings, void *userdata)
{
    seriesModuleData	*data = getSeriesModuleData(&settings->module);

    initSeriesBatonMagic(baton, MAGIC_SEARCH);
    baton->callbacks = &settings->callbacks;
    baton->info = settings->module.on_info;
    baton->slots = data->slots;
    baton->module = &settings->module;
    baton->userdata = userdata;
    pmtimevalNow(&baton->started);
}

static void
doneRedisSearchBaton(redisSearchBaton *baton)
{
    seriesBatonCheckMagic(baton, MAGIC_SEARCH, "doneRedisSearchBaton");
    memset(baton, 0, sizeof(redisSearchBaton));
    free(baton);
}

const char *
pmSearchTextTypeStr(pmSearchTextType type)
{
    switch (type) {
    case PM_SEARCH_TYPE_METRIC:
	return "metric";
    case PM_SEARCH_TYPE_INDOM:
	return "indom";
    case PM_SEARCH_TYPE_INST:
	return "instance";
    }
    return "unknown";
}

static int
pmwebapi_search_hash(unsigned char *hash, const char *string, int length)
{
    SHA1_CTX		shactx;
    const char		prefix[] = "{\"series\":\"search\",";
    const char		suffix[] = "}";

    /* Calculate unique string identifier 20-byte SHA1 hash */
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)string, length);
    SHA1Update(&shactx, (unsigned char *)suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);
    return 0;
}

static sds
redis_search_docid(const char *key, const char *type, const char *name)
{
    unsigned char	hash[20];
    sds			docid = sdsempty();

    docid = sdscatfmt(docid, "\"key\":\"%s\",\"type\":\"%s\",name:\"%s\"",
			key, type, name);
    pmwebapi_search_hash(hash, docid, sdslen(docid));
    return pmwebapi_hash_sds(docid, hash);
}

static void
redis_search_text_add_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;
    int			sts;

    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_search_text_add_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */
    if (sts == 0)
	checkStatusReplyOK(baton->info, baton->userdata, reply,
		"%s: %s", FT_ADD, "search text add");

    doneSeriesGetContext(context, "redis_search_text_add_callback");
}

void
redis_search_text_add(redisSlots *slots, pmSearchTextType type,
		const char *name, const char *indom,
		const char *oneline, const char *helptext, void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;
    unsigned int	length;
    const char		*typestr = pmSearchTextTypeStr(type);
    char		buffer[8];
    sds			cmd, key, docid;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "redis_search_text_add");

    if (pmDebugOptions.search)
	fprintf(stderr, "%s: %s %s\n", "redis_search_text_add", typestr, name);

    seriesBatonReference(context, "redis_search_text_add");

    /*
     * FT.ADD pcp:text <docid> 1.0
     *		REPLACE PARTIAL
     *		PAYLOAD <type>
     *		FIELDS NAME <name> TYPE <type>
     *		[INDOM <indom>] [ONELINE <oneline>] [HELPTEXT <helptext>]
     */
    key = sdsnewlen(FT_TEXT_KEY, FT_TEXT_KEY_LEN);
    length = 4 + 2 + 2 + 5;
    if (indom && *indom != '\0')
	length += 2;
    if (oneline && *oneline != '\0')
	length += 2;
    if (helptext && *helptext != '\0')
	length += 2;
    cmd = redis_command(length);

    cmd = redis_param_str(cmd, FT_ADD, FT_ADD_LEN);
    cmd = redis_param_str(cmd, FT_TEXT_KEY, FT_TEXT_KEY_LEN);
    docid = redis_search_docid(FT_TEXT_KEY, typestr, name);
    cmd = redis_param_sds(cmd, docid);
    sdsfree(docid);
    cmd = redis_param_str(cmd, "1", 1);

    cmd = redis_param_str(cmd, FT_REPLACE, FT_REPLACE_LEN);
    cmd = redis_param_str(cmd, FT_PARTIAL, FT_PARTIAL_LEN);

    length = pmsprintf(buffer, sizeof(buffer), "%u", type);
    cmd = redis_param_str(cmd, FT_PAYLOAD, FT_PAYLOAD_LEN);
    cmd = redis_param_str(cmd, buffer, length);

    cmd = redis_param_str(cmd, FT_FIELDS, FT_FIELDS_LEN);
    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
    cmd = redis_param_str(cmd, name, strlen(name));
    cmd = redis_param_str(cmd, FT_TYPE, FT_TYPE_LEN);
    cmd = redis_param_str(cmd, typestr, strlen(typestr));
    if (indom && *indom != '\0') {
	cmd = redis_param_str(cmd, FT_INDOM, FT_INDOM_LEN);
	cmd = redis_param_str(cmd, indom, strlen(indom));
    }
    if (oneline && *oneline != '\0') {
	cmd = redis_param_str(cmd, FT_ONELINE, FT_ONELINE_LEN);
	cmd = redis_param_str(cmd, oneline, strlen(oneline));
    }
    if (helptext && *helptext != '\0') {
	cmd = redis_param_str(cmd, FT_HELPTEXT, FT_HELPTEXT_LEN);
	cmd = redis_param_str(cmd, helptext, strlen(helptext));
    }

    redisSlotsRequest(slots, FT_ADD, key, cmd, redis_search_text_add_callback, arg);
}

void
pmSearchDiscoverMetric(pmDiscoverEvent *event,
		pmDesc *desc, int numnames, char **names, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = (seriesLoadBaton *)p->baton;
    context_t		*context = &baton->pmapi.context;
    char		*oneline = NULL, *helptext = NULL;
    char		buffer[64] = {0};
    pmID		id = desc->pmid;
    int			i;

    if (pmDebugOptions.discovery || pmDebugOptions.search) {
	for (i = 0; i < numnames; i++)
	    fprintf(stderr, "%s: [%d/%d] %s - %s\n", "pmSearchDiscoverMetric",
			i + 1, numnames, pmIDStr_r(id, buffer, sizeof(buffer)),
			names[i]);
    }

    if (baton == NULL || baton->slots == NULL || baton->slots->search <= 0)
	return;

    /* we have the metric name(s) and desc, has text been discovered yet? */
    pmUseContext(context->context);
    pmLookupText(id, PM_TEXT_PMID | PM_TEXT_ONELINE, &oneline);
    pmLookupText(id, PM_TEXT_PMID | PM_TEXT_HELP | PM_TEXT_DIRECT, &helptext);

    if (desc->indom != PM_INDOM_NULL)
	pmInDomStr_r(desc->indom, buffer, sizeof(buffer));

    for (i = 0; i < numnames; i++)
	redis_search_text_add(baton->slots, PM_SEARCH_TYPE_METRIC,
			names[i], buffer, oneline, helptext, baton);

    if (oneline)
	free(oneline);
    if (helptext)
	free(helptext);
}

void
pmSearchDiscoverInDom(pmDiscoverEvent *event, pmInResult *in, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    pmInDom		id = in->indom;
    char		*oneline = NULL, *helptext = NULL;
    char		buffer[64];
    int			i;

    pmInDomStr_r(id, buffer, sizeof(buffer));

    if (pmDebugOptions.discovery || pmDebugOptions.search)
	fprintf(stderr, "%s: %s\n", "pmSearchDiscoverInDom", buffer);

    if (baton == NULL || baton->slots == NULL || baton->slots->search <= 0)
	return;

    /*
     * We have the indom and instances, has text been discovered yet?
     * Not a problem if not as we use PARTIAL FT.ADD and subsequently
     * we will find the text via pmSearchDiscoverText.
     */
    pmUseContext(p->ctx);
    pmLookupText(id, PM_TEXT_INDOM | PM_TEXT_ONELINE, &oneline);
    pmLookupText(id, PM_TEXT_INDOM | PM_TEXT_HELP | PM_TEXT_DIRECT, &helptext);

    redis_search_text_add(baton->slots, PM_SEARCH_TYPE_INDOM,
			"", buffer, oneline, helptext, baton);
    for (i = 0; i < in->numinst; i++)
	redis_search_text_add(baton->slots, PM_SEARCH_TYPE_INST,
			in->namelist[i], buffer, NULL, NULL, baton);
    if (oneline)
	free(oneline);
    if (helptext)
	free(helptext);
}

void
pmSearchDiscoverText(pmDiscoverEvent *event,
		int ident, int type, char *text, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    char		indom[64] = {0}, **metrics, *oneline, *helptext;
    int			i, count;

    if (pmDebugOptions.discovery || pmDebugOptions.search)
	fprintf(stderr, "%s: ident=%u type=%u arg=%p\n",
			"pmSearchDiscoverText", ident, type, arg);

    if (baton == NULL || baton->slots == NULL || baton->slots->search <= 0)
	return;

    oneline = (type & PM_TEXT_ONELINE) ? text : NULL;
    helptext = (type & PM_TEXT_HELP) ? text : NULL;

    if (type & PM_TEXT_PMID) {
	pmUseContext(p->ctx);
	if ((count = pmNameAll(ident, &metrics)) <= 0)
	    return;
	for (i = 0; i < count; i++)
	    redis_search_text_add(baton->slots, PM_SEARCH_TYPE_METRIC,
			metrics[i], NULL, oneline, helptext, baton);
	free(metrics);
    } else { /* PM_TEXT_INDOM */
	pmInDomStr_r(ident, indom, sizeof(indom));
	redis_search_text_add(baton->slots, PM_SEARCH_TYPE_INDOM,
			indom, NULL, oneline, helptext, baton);
    }
}

static void
redis_search_info_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    redisSearchBaton	*baton = (redisSearchBaton *)arg;
    redisReply		*child, *value;
    pmSearchMetrics	metrics = {0};
    int			i, sts;
    sds			msg;

    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_search_info_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements >= 30) {
	for (i = 0; i < reply->elements-1; i++) {
	    value = reply->element[i+1];
	    child = reply->element[i];
	    if (child->type != REDIS_REPLY_STRING &&
		value->type != REDIS_REPLY_STRING)
		continue;
	    else if (strcmp("num_docs", child->str) == 0)
		metrics.docs = strtoull(value->str, NULL, 0);
	    else if (strcmp("num_terms", child->str) == 0)
		metrics.terms = strtoull(value->str, NULL, 0);
	    else if (strcmp("num_records", child->str) == 0)
		metrics.records = strtoull(value->str, NULL, 0);
	    else if (strcmp("inverted_sz_mb", child->str) == 0)
		metrics.inverted_sz_mb = strtod(value->str, NULL);
	    else if (strcmp("inverted_cap_mb", child->str) == 0)
		metrics.inverted_cap_mb = strtod(value->str, NULL);
	    else if (strcmp("inverted_cap_ovh", child->str) == 0)
		metrics.inverted_cap_ovh = strtod(value->str, NULL);
	    else if (strcmp("offset_vectors_sz_mb", child->str) == 0)
		metrics.offset_vectors_sz_mb = strtod(value->str, NULL);
	    else if (strcmp("skip_index_size_mb", child->str) == 0)
		metrics.skip_index_size_mb = strtod(value->str, NULL);
	    else if (strcmp("score_index_size_mb", child->str) == 0)
		metrics.score_index_size_mb = strtod(value->str, NULL);
	    else if (strcmp("records_per_doc_avg", child->str) == 0)
		metrics.records_per_doc_avg = strtod(value->str, NULL);
	    else if (strcmp("bytes_per_record_avg", child->str) == 0)
		metrics.bytes_per_record_avg = strtod(value->str, NULL);
	    else if (strcmp("offsets_per_term_avg", child->str) == 0)
		metrics.offsets_per_term_avg = strtod(value->str, NULL);
	    else if (strcmp("offset_bits_per_record_avg", child->str) == 0)
		metrics.offset_bits_per_record_avg = strtod(value->str, NULL);
	}
	baton->callbacks->on_metrics(&metrics, baton->userdata);
    } else {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s (reply=%s)",
			FT_INFO, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
	baton->error = -EPROTO;
    }

    baton->callbacks->on_done(baton->error, baton->userdata);
    doneRedisSearchBaton(baton);
}

void
redis_search_info(redisSlots *slots, sds pcpkey, void *arg)
{
    redisSearchBaton	*baton = (redisSearchBaton *)arg;
    sds			cmd, key;

    seriesBatonCheckMagic(baton, MAGIC_SEARCH, "redis_search_info");
    seriesBatonCheckCount(baton, "redis_search_info");

    if (pmDebugOptions.search)
	fprintf(stderr, "%s: search key metrics\n", "redis_search_info");

    seriesBatonReference(baton, "redis_search_info");

    /*
     * FT.INFO pcp:<key>
     */
    key = sdscatfmt(sdsempty(), "pcp:%S", pcpkey);
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, FT_INFO, FT_INFO_LEN);
    cmd = redis_param_sds(cmd, key);
    redisSlotsRequest(slots, FT_INFO, key, cmd, redis_search_info_callback, arg);
}

int
pmSearchInfo(pmSearchSettings *settings, sds key, void *arg)
{
    redisSearchBaton	*baton;

    if ((baton = calloc(1, sizeof(redisSearchBaton))) == NULL)
	return -ENOMEM;
    initRedisSearchBaton(baton, settings, arg);
    redis_search_info(baton->slots, key, baton);
    return 0;
}

static void
extract_search_results(redisSearchBaton *baton,
		unsigned int total, double timer, redisReply *reply)
{
    pmSearchTextResult	result;
    redisReply		*docid, *score, *payload, *array;
    int			i, j;

    for (i = 1; i < reply->elements - 3; i += 4) {
	docid = reply->element[i];
	score = reply->element[i+1];
	payload = reply->element[i+2];
	array = reply->element[i+3];
	if (payload->type != REDIS_REPLY_STRING ||
	    score->type != REDIS_REPLY_STRING ||
	    docid->type != REDIS_REPLY_STRING ||
	    array->type != REDIS_REPLY_ARRAY) {
	    baton->error = -EPROTO;
	    break;
	}

	memset(&result, 0, sizeof(result));
	result.total = total;
	result.timer = timer;
	result.count = (i / 2) + 1;
	result.docid = sdsnewlen(docid->str, docid->len);
	result.score = strtod(score->str, NULL);
	result.type = atoi(payload->str);

	for (j = 0; j < array->elements - 1; j += 2) {
	    redisReply	*field = array->element[j];
	    redisReply	*value = array->element[j+1];

	    if (field->type != REDIS_REPLY_STRING ||
	        (value->type != REDIS_REPLY_STRING &&
		 value->type != REDIS_REPLY_NIL)) {
		baton->error = -EPROTO;
		break;
	    }

	    if (strcmp(field->str, FT_NAME) == 0)
		result.name = sdsnewlen(value->str, value->len);
	    else if (strcmp(field->str, FT_INDOM) == 0)
		result.indom = sdsnewlen(value->str, value->len);
	    else if (strcmp(field->str, FT_ONELINE) == 0)
		result.oneline = sdsnewlen(value->str, value->len);
	    else if (strcmp(field->str, FT_HELPTEXT) == 0)
		result.helptext = sdsnewlen(value->str, value->len);
	}
	if (baton->error == 0)
	    baton->callbacks->on_text_result(&result, baton->userdata);

	sdsfree(result.docid);
	sdsfree(result.name);
	sdsfree(result.indom);
	sdsfree(result.oneline);
	sdsfree(result.helptext);
    }
}

static void
redis_search_text_query_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    redisSearchBaton	*baton = (redisSearchBaton *)arg;
    redisReply		*value;
    struct timeval	finished;
    unsigned int	total;
    double		timer;
    int			sts;
    sds			msg;

    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_search_text_query_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	value = reply->element[0];
	if (reply->elements == 1)	/* no search results - done! */
	    /* do nothing */;
	else if (reply->elements < 2)	/* expect total then results */
	    baton->error = -EPROTO;
	else if (value->type != REDIS_REPLY_INTEGER)
	    baton->error = -EPROTO;
	else {
	    pmtimevalNow(&finished);
	    timer = pmtimevalSub(&finished, &baton->started);
	    total = (unsigned int)value->integer;
	    extract_search_results(baton, total, timer, reply);
	}
    } else {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s (reply=%s)",
			FT_SEARCH, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
	baton->error = -EPROTO;
    }

    baton->callbacks->on_done(baton->error, baton->userdata);
    doneRedisSearchBaton(baton);
}

static void
redis_search_text_query(redisSlots *slots, pmSearchTextRequest *request, void *arg)
{
    redisSearchBaton	*baton = (redisSearchBaton *)arg;
    const char		*typestr;
    size_t		length;
    char		buffer[64];
    sds			cmd, key, query;
    unsigned int	types = 0, infields = 0, returns = 0, highlights = 0;

    seriesBatonCheckMagic(baton, MAGIC_SEARCH, "redis_search_text_query");
    seriesBatonCheckCount(baton, "redis_search_text_query");

    if (pmDebugOptions.search)
	fprintf(stderr, "%s: %s\n", "redis_search_text_query", request->query);

    seriesBatonReference(baton, "redis_search_text_query");

    query = sdscatrepr(sdsempty(), request->query, sdslen(request->query));

    types += request->type_metric;
    types += request->type_indom;
    types += request->type_inst;

    highlights += request->highlight_name;
    highlights += request->highlight_indom;
    highlights += request->highlight_oneline;
    highlights += request->highlight_helptext;

    infields += request->infields_name;
    infields += request->infields_indom;
    infields += request->infields_oneline;
    infields += request->infields_helptext;
    if (infields == 0) {
	infields = 3;	/* defaults */
	request->infields_name = 1;
	request->infields_oneline = 1;
	request->infields_helptext = 1;
    }

    returns += request->return_name;
    returns += request->return_indom;
    returns += request->return_oneline;
    returns += request->return_helptext;
    if (returns == 0) {
	returns = 4;	/* defaults */
	request->return_name = 1;
	request->return_indom = 1;
	request->return_oneline = 1;
	request->return_helptext = 1;
    }

    /*
     * FT.SEARCH pcp:text "query" WITHSCORES WITHPAYLOADS
     *		[@type={ {?type separated by pipe} }]
     *		[INFIELDS {?field item count} {?field separated by space}]
     *		[RETURN {?return item count} {?return separated by space}]
     *		[HIGHLIGHT FIELDS {num} {field} ... ]
     *		LIMIT {?pagination offset} {?return result count}
     *		SORTBY NAME ASC
     */
    key = sdsnewlen(FT_TEXT_KEY, FT_TEXT_KEY_LEN);
    length = 5;
    if (types)
	length += 2 + (types * 2) - 1;
    if (infields)
	length += 2 + infields;
    if (returns)
	length += 2 + returns;
    if (highlights)
	length += 3 + highlights;
    length += 3 + 3;

    cmd = redis_command(length);
    cmd = redis_param_str(cmd, FT_SEARCH, FT_SEARCH_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, query);
    sdsfree(query);
    cmd = redis_param_str(cmd, FT_WITHSCORES, FT_WITHSCORES_LEN);
    cmd = redis_param_str(cmd, FT_WITHPAYLOADS, FT_WITHPAYLOADS_LEN);

    if (types) {
	cmd = redis_param_str(cmd, "@type={ ", 8);
	if (request->type_metric) {
	    typestr = pmSearchTextTypeStr(PM_SEARCH_TYPE_METRIC);
	    cmd = redis_param_str(cmd, typestr, strlen(typestr));
	}
	if (request->type_indom && (request->type_metric))
	    cmd = redis_param_str(cmd, " | ", 3);
	if (request->type_indom) {
	    typestr = pmSearchTextTypeStr(PM_SEARCH_TYPE_INDOM);
	    cmd = redis_param_str(cmd, typestr, strlen(typestr));
	}
	if (request->type_inst && (request->type_indom || request->type_metric))
	    cmd = redis_param_str(cmd, " | ", 3);
	if (request->type_inst) {
	    typestr = pmSearchTextTypeStr(PM_SEARCH_TYPE_INST);
	    cmd = redis_param_str(cmd, typestr, strlen(typestr));
	}
	cmd = redis_param_str(cmd, " }", 2);
    }

    if (infields) {
	cmd = redis_param_str(cmd, FT_INFIELDS, FT_INFIELDS_LEN);
	length = pmsprintf(buffer, sizeof(buffer), "%u", infields);
	cmd = redis_param_str(cmd, buffer, length);
	if (request->infields_name)
	    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
	if (request->infields_indom)
	    cmd = redis_param_str(cmd, FT_INDOM, FT_INDOM_LEN);
	if (request->infields_oneline)
	    cmd = redis_param_str(cmd, FT_ONELINE, FT_ONELINE_LEN);
	if (request->infields_helptext)
	    cmd = redis_param_str(cmd, FT_HELPTEXT, FT_HELPTEXT_LEN);
    }

    if (returns) {
	cmd = redis_param_str(cmd, FT_RETURN, FT_RETURN_LEN);
	length = pmsprintf(buffer, sizeof(buffer), "%u", returns);
	cmd = redis_param_str(cmd, buffer, length);
	if (request->return_name)
	    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
	if (request->return_indom)
	    cmd = redis_param_str(cmd, FT_INDOM, FT_INDOM_LEN);
	if (request->return_oneline)
	    cmd = redis_param_str(cmd, FT_ONELINE, FT_ONELINE_LEN);
	if (request->return_helptext)
	    cmd = redis_param_str(cmd, FT_HELPTEXT, FT_HELPTEXT_LEN);
    }

    if (highlights) {
	cmd = redis_param_str(cmd, FT_HIGHLIGHT, FT_HIGHLIGHT_LEN);
	cmd = redis_param_str(cmd, FT_FIELDS, FT_FIELDS_LEN);
	length = pmsprintf(buffer, sizeof(buffer), "%u", highlights);
	cmd = redis_param_str(cmd, buffer, length);
	if (request->highlight_name)
	    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
	if (request->highlight_indom)
	    cmd = redis_param_str(cmd, FT_INDOM, FT_INDOM_LEN);
	if (request->highlight_oneline)
	    cmd = redis_param_str(cmd, FT_ONELINE, FT_ONELINE_LEN);
	if (request->highlight_helptext)
	    cmd = redis_param_str(cmd, FT_HELPTEXT, FT_HELPTEXT_LEN);
    }

    cmd = redis_param_str(cmd, FT_LIMIT, FT_LIMIT_LEN);
    length = pmsprintf(buffer, sizeof(buffer), "%u", request->offset);
    cmd = redis_param_str(cmd, buffer, length);
    if (request->count == 0) {
	cmd = redis_param_sds(cmd, resultcount);
    } else {
	length = pmsprintf(buffer, sizeof(buffer), "%u", request->count);
	cmd = redis_param_str(cmd, buffer, length);
    }

    cmd = redis_param_str(cmd, FT_SORTBY, FT_SORTBY_LEN);
    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
    cmd = redis_param_str(cmd, FT_ASC, FT_ASC_LEN);

    redisSlotsRequest(slots, FT_SEARCH, key, cmd, redis_search_text_query_callback, arg);
}

int
pmSearchTextQuery(pmSearchSettings *settings, pmSearchTextRequest *request, void *arg)
{
    redisSearchBaton	*baton;

    if ((baton = calloc(1, sizeof(redisSearchBaton))) == NULL)
	return -ENOMEM;
    initRedisSearchBaton(baton, settings, arg);
    redis_search_text_query(baton->slots, request, baton);
    return 0;
}

static void
redis_search_schema_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "redis_search_schema_callback");

    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_search_schema_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */
    if (sts == 0) {
	if (checkStatusReplyOK(baton->info, baton->userdata, reply,
		"%s: %s", FT_CREATE, "creating search schema") == 0)
	    baton->slots->search = -ENOTSUP;
	else
	    baton->slots->search = 1;
    } else if (!testReplyError(reply, REDIS_EDROPINDEX)) {
	baton->slots->search = -ENOTSUP;
    } else {
	baton->slots->search = 1;
    }

    redis_slots_end_phase(baton);
}

void
redis_load_search_schema(void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    sds			cmd, key;

    seriesBatonCheckMagic(baton, MAGIC_SLOTS, "redis_load_search_schema");

    if (pmDebugOptions.search && pmDebugOptions.desperate)
	fprintf(stderr, "%s: loading schema\n", "redis_search_schema");

    seriesBatonReference(baton, "redis_load_search_schema");

    /*
     * FT.CREATE pcp:text SCHEMA
     *		type TAG
     *		name TEXT WEIGHT 9 SORTABLE
     *		indom TEXT WEIGHT 1
     *		oneline TEXT WEIGHT 4
     *		helptext TEXT WEIGHT 2
     */
    key = sdsnewlen(FT_TEXT_KEY, FT_TEXT_KEY_LEN);
    cmd = redis_command(3 + 2 + 5 + 4 + 4 + 4);

    cmd = redis_param_str(cmd, FT_CREATE, FT_CREATE_LEN);
    cmd = redis_param_str(cmd, FT_TEXT_KEY, FT_TEXT_KEY_LEN);
    cmd = redis_param_str(cmd, FT_SCHEMA, FT_SCHEMA_LEN);

    cmd = redis_param_str(cmd, FT_TYPE, FT_TYPE_LEN);
    cmd = redis_param_str(cmd, FT_TAG, FT_TAG_LEN);

    cmd = redis_param_str(cmd, FT_NAME, FT_NAME_LEN);
    cmd = redis_param_str(cmd, FT_TEXT, FT_TEXT_LEN);
    cmd = redis_param_str(cmd, FT_WEIGHT, FT_WEIGHT_LEN);
    cmd = redis_param_str(cmd, "9", sizeof("9")-1);
    cmd = redis_param_str(cmd, FT_SORTABLE, FT_SORTABLE_LEN);

    cmd = redis_param_str(cmd, FT_INDOM, FT_INDOM_LEN);
    cmd = redis_param_str(cmd, FT_TEXT, FT_TEXT_LEN);
    cmd = redis_param_str(cmd, FT_WEIGHT, FT_WEIGHT_LEN);
    cmd = redis_param_str(cmd, "1", sizeof("1")-1);

    cmd = redis_param_str(cmd, FT_ONELINE, FT_ONELINE_LEN);
    cmd = redis_param_str(cmd, FT_TEXT, FT_TEXT_LEN);
    cmd = redis_param_str(cmd, FT_WEIGHT, FT_WEIGHT_LEN);
    cmd = redis_param_str(cmd, "4", sizeof("4")-1);

    cmd = redis_param_str(cmd, FT_HELPTEXT, FT_HELPTEXT_LEN);
    cmd = redis_param_str(cmd, FT_TEXT, FT_TEXT_LEN);
    cmd = redis_param_str(cmd, FT_WEIGHT, FT_WEIGHT_LEN);
    cmd = redis_param_str(cmd, "2", sizeof("2")-1);

    redisSlotsRequest(baton->slots, FT_CREATE, key, cmd, redis_search_schema_callback, arg);
}

int
pmSearchSetSlots(pmSearchModule *module, void *slots)
{
    return pmSeriesSetSlots(module, slots);
}

int
pmSearchSetConfiguration(pmSearchModule *module, dict *config)
{
    return pmSeriesSetConfiguration(module, config);
}

int
pmSearchSetEventLoop(pmSearchModule *module, void *events)
{
    return pmSeriesSetEventLoop(module, events);
}

int
pmSearchSetMetricRegistry(pmSearchModule *module, mmv_registry_t *registry)
{
    return pmSeriesSetMetricRegistry(module, registry);
}

void
redisSearchInit(struct dict *config)
{
    sds		option;

    if (!resultcount) {
	if ((option = pmIniFileLookup(config, "pmsearch", "result.count")))
	    resultcount = option;
	else
	    resultcount = sdsnew("10");
    }
}

int
pmSearchSetup(pmSearchModule *module, void *arg)
{
    seriesModuleData	*data = getSeriesModuleData(module);

    if (data == NULL)
	return -ENOMEM;

    /* create global EVAL hashes and string map caches */
    redisGlobalsInit(data->config);

    /* fast path for when Redis has been setup already */
    if (data->slots) {
	module->on_setup(arg);
	data->shareslots = 1;
    } else {
	/* establish an initial connection to Redis instance(s) */
	data->slots = redisSlotsConnect(
			data->config, SLOTS_SEARCH, module->on_info,
			module->on_setup, arg, data->events, arg);
	data->shareslots = 0;
    }
    return 0;
}

int
pmSearchEnabled(void *arg)
{
    redisSlots	*slots = (redisSlots *)arg;

    if (slots)
	return slots->search > 0 ? 1 : 0;
    return 0;
}

void
pmSearchClose(pmSearchModule *module)
{
    seriesModuleData	*search = (seriesModuleData *)module->privdata;

    if (search) {
	if (!search->shareslots)
	    redisSlotsFree(search->slots);
	memset(search, 0, sizeof(*search));
	free(search);
    }
}
