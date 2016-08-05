/*
 * Copyright (c) 2016 Red Hat.
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


#include "pmapi.h"
#include "impl.h"
#include "private.h"
#include "pmjson.h"
#include "pmda.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
/*
 * JSMN helper interfaces for efficiently extracting JSON configs
 */

int
jsmneq(const char *js, jsmntok_t *tok, const char *s)
{
    if (tok->type != JSMN_STRING)
	return -1;
    if (strlen(s) == tok->end - tok->start &&
	strncasecmp(js + tok->start, s, tok->end - tok->start) == 0)
	return 0;
    return -1;
}

int
jsmnflag(const char *js, jsmntok_t *tok, int *bits, int flag)
{
    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    if (strncmp(js + tok->start, "true", sizeof("true")-1) == 0)
	*bits |= flag;
    else
	*bits &= ~flag;
    return 0;
}

int
jsmnint(const char *js, jsmntok_t *tok, int *value)
{
    char	buffer[64];

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    strncpy(buffer, js + tok->start, tok->end - tok->start);
    buffer[tok->end - tok->start] = '\0';
    *value = (int)strtol(buffer, NULL, 0);
    return 0;
}

int
jsmnflagorint(const char *js, jsmntok_t *tok, int *bits, int flag)
{
    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    
    if (strncmp(js + tok->start, "true", sizeof("true")-1) != 0 &&
	strncmp(js + tok->start, "false", sizeof("false")-1) != 0) {
	if(jsmnint(js, tok, bits)<0)
	    return -1;
	else
	    return 0;
    }
    else {
	if(jsmnflag(js, tok, bits, flag)<0)
	    return -1;
    }
    return 0;
}

int
jsmnstrdup(const char *js, jsmntok_t *tok, char **name)
{
    char	*s = *name;

    if (tok->type != JSMN_STRING)
	return -1;
    if (s)
	free(s);
    s = strndup(js + tok->start, tok->end - tok->start);
    return ((*name = s) == NULL) ? -1 : 0;
}

int
json_extract_values(const char *json, jsmntok_t *json_tokens, size_t count,
		    json_metric_desc *json_metrics, char* pointer_part[], int key, int total)
{
    int i, j, k = 0;
    
    /* we've reached the end of the json doc */
    if (count == 0)
	return 0;

    switch (json_tokens->type) {
    case JSMN_PRIMITIVE:
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_DEBUG, "jsmn primitive\n");
	return 1;
    case JSMN_STRING:
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_DEBUG, "string: %.*s parent: %d\n", json_tokens->end - json_tokens->start, json+json_tokens->start, json_tokens->parent);
	if (jsmneq(json, json_tokens, pointer_part[key]) == 0){
	    //XXX this needs to have a better method for detecting
	    if (key == 0 && json_tokens->parent != 0)
		return 1;
	    jsmntok_t *value = json_tokens + 1;
	    if(value->type == JSMN_PRIMITIVE && (total - key == 1)){
		jsmnflagorint(json, value, &json_metrics->values.l, json_metrics->flags);
		return count;
	    }
	    else if(value->type == JSMN_STRING && total - key == 1){
		jsmnstrdup(json, value, &json_metrics->values.cp);
		return count;
	    }
	    if (total - key == 1)
		key = 0;
	    return -1;
	}
	return 1;
    case JSMN_OBJECT:
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_DEBUG, "jsmn object\n");
	for (i = j = 0; i < json_tokens->size; i++){
	    if (pmDebug & DBG_TRACE_APPL2)
		__pmNotifyErr(LOG_DEBUG, "object key\n");
	    k = json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	    /* returned a valid section, continue */
	    if (k > 1){
		key = 0;
		j += k;
	    }
	    /* went a level deeper but no match */
	    if (k < 0){
		key++;
		j++;
	    }
	    /* returned a one, nothing hit so far */
	    if (k == 1){
		j++;
	    }

	    if (pmDebug & DBG_TRACE_APPL2)
		__pmNotifyErr(LOG_DEBUG, "object value %d\n", (json_tokens+1+j)->size);
	    j += json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	}
	return j + 1;
    case JSMN_ARRAY:
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_DEBUG, "jsmn_array");
	for (i = j = 0; i < json_tokens->size; i++)
	    j += json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	return j + 1;
    default:
	return 0;
    }
    return 0;
}

int
json_pointer_to_index(const char *json, jsmntok_t *json_tokens, size_t count, json_metric_desc *json_metrics, int nummetrics)
{
    const char *delim = "/";
    char *json_pointer = "";
    char *pointer_part = "";
    char *pointer_final[MAXPATHLEN] = { "" };

    int i, j = 0;

    /* tokenize the json_metrics.json_pointers fields */
    for(i = 0; i < nummetrics; i++) {
 	j = 0;
	memset(&pointer_final[0], 0, sizeof(pointer_final));
	json_pointer = strdup(json_metrics[i].json_pointer);
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_DEBUG, "json_pointer: %s\n", json_pointer);
	pointer_part = strtok(json_pointer, delim);
	if (pointer_part){
	    pointer_final[j] = strdup(pointer_part);
	    j++;
	    while(pointer_part){
		pointer_part = strtok(NULL, delim);
		if (pointer_part){
		    pointer_final[j] = strdup(pointer_part);
		    j++;
		}
	    }
	}
	json_extract_values(json, json_tokens, count, &json_metrics[i], pointer_final, 0, j);
    }
    return 0;
}

int
pmjsonInit(int fd, json_metric_desc *json_metrics, int nummetrics)
{
    return pmjsonInitIndom(fd, json_metrics, nummetrics, PM_INDOM_NULL);
}

int
pmjsonInitIndom(int fd, json_metric_desc *json_metrics, int nummetrics, pmInDom indom)
{
    int               number_of_bytes_read;
    int               number_of_tokens_read;
    char              buffer[BUFSIZ] = {""};
    int               sts = 0;
    int               eof_expected = 0;
    jsmn_parser       parser;
    jsmntok_t         *json_tokens = NULL;
    int               json_token_count = 0;
    char              *json = NULL;
    int               json_length = 0;

    if (!json_tokens) {
	json_token_count = 2;
	if ((json_tokens = calloc(json_token_count, sizeof(*json_tokens))) == NULL)
	    sts = -ENOMEM;
    }
    jsmn_init(&parser);

    for (;;) {
	number_of_bytes_read = read(fd, buffer, sizeof(buffer));
	if (number_of_bytes_read < 0) {
	    if (pmDebug & DBG_TRACE_ATTR) {
		fprintf(stderr, "%s: failed read on json file: %s\n",
			pmProgname, osstrerror());
		sts = -oserror();
		return sts;
	    }
	}
	/* Ok, we've read in the json file */
	if (number_of_bytes_read == 0) {
	    /* eof_expected when our parsing looks done, not before */
	    if (!eof_expected) {
		if (pmDebug & DBG_TRACE_ATTR)
		    fprintf(stderr, "%s: unexpected EOF on json file: %s\n",
			    pmProgname, osstrerror());
		sts = -EINVAL;
		break;
	    }
	    /* We haven't read in any more information, and it appears parsing is done, return */
	    if (indom != PM_INDOM_NULL)
		sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, json_metrics[0].dom, json_metrics);
	    return sts;
	}


	/* We've read in more json data, adjust our json size */
	if ((json = realloc(json, json_length + number_of_bytes_read + 1)) == NULL) {
	    sts = -ENOMEM;
	    break;
	}

	strncpy(json + json_length, buffer, number_of_bytes_read);
	json_length = json_length + number_of_bytes_read;
again:
	number_of_tokens_read = jsmn_parse(&parser, json, json_length, json_tokens, json_token_count);
	if (number_of_tokens_read < 0) {
	    if (number_of_tokens_read == JSMN_ERROR_NOMEM) {
		json_token_count = json_token_count * 2;
		if ((json_tokens = realloc(json_tokens, sizeof(*json_tokens) * json_token_count)) == NULL) {
		    sts = -ENOMEM;
		    break;
		}
		goto again;
	    }
	} else {
	    json_pointer_to_index(json, json_tokens, parser.toknext, json_metrics, nummetrics);
	    eof_expected = 1;
	}
    }
    return sts;
}
