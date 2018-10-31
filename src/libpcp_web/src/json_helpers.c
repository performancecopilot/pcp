/*
 * Copyright (c) 2016-2017 Red Hat.
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
#include "pmjson.h"
#include "pmda.h"
#include "jsmn.h"

/* Number of spaces to use in each level of pretty-printed output */
#define PRETTY_SPACES	4

/*
 * JSMN helper interfaces for efficiently extracting from JSON strings
 */
#define TRUE_TOK	"true"
#define FALSE_TOK	"false"
#define TRUE_LEN	(sizeof(TRUE_TOK)-1)
#define FALSE_LEN	(sizeof(FALSE_TOK)-1)

static int
jsmneq(const char *js, jsmntok_t *tok, const char *s)
{
    size_t	length;

    if (tok->type != JSMN_STRING)
	return -1;
    length = tok->end - tok->start;
    if (strlen(s) == length && strncmp(js + tok->start, s, length) == 0)
	return 0;
    return -1;
}

static int
jsmnflag(const char *js, jsmntok_t *tok, int *bits, int flag)
{
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    if (strncmp(js + tok->start, TRUE_TOK, length) == 0)
	*bits |= flag;
    else
	*bits &= ~flag;
    return 0;
}

static int
jsmnbool(const char *js, jsmntok_t *tok, unsigned int *value)
{
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    if (length == TRUE_LEN && !strncmp(js + tok->start, TRUE_TOK, TRUE_LEN)) {
	*value = 1;
	return 0;
    }
    if (length == FALSE_LEN && !strncmp(js + tok->start, FALSE_TOK, FALSE_LEN)) {
	*value = 0;
	return 0;
    }
    return -1;
}

static int
jsmnint(const char *js, jsmntok_t *tok, int *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = (int)strtol(buffer, &endptr, 0);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmnuint(const char *js, jsmntok_t *tok, unsigned int *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = (unsigned int)strtoul(buffer, &endptr, 0);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmnlong(const char *js, jsmntok_t *tok, __int64_t *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = strtoll(buffer, &endptr, 0);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmnulong(const char *js, jsmntok_t *tok, __uint64_t *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = strtoull(buffer, &endptr, 0);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmnfloat(const char *js, jsmntok_t *tok, float *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = strtof(buffer, &endptr);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmndouble(const char *js, jsmntok_t *tok, double *value)
{
    char	buffer[64];
    char	*endptr;
    size_t	length;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    length = tok->end - tok->start;
    strncpy(buffer, js + tok->start, length);
    buffer[length] = '\0';
    *value = strtod(buffer, &endptr);
    if (*endptr == '\0')
	return 0;
    return -1;
}

static int
jsmnflagornumber(const char *js, jsmntok_t *tok, json_metric_desc *json_metrics, int flag)
{
    size_t	len;

    if (tok->type != JSMN_PRIMITIVE)
	return -1;

    len = tok->end - tok->start;
    if ((flag & pmjson_flag_bitfield) ||
	(len == TRUE_LEN && !strncmp(js + tok->start, TRUE_TOK, TRUE_LEN)) ||
	(len == FALSE_LEN && !strncmp(js + tok->start, FALSE_TOK, FALSE_LEN))) {
	if (jsmnflag(js, tok, &json_metrics->values.l, flag) < 0)
	    return -1;
    }
    else {
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "pmjson_flag: %d\n", flag);
	switch (flag) {
	case pmjson_flag_boolean:
	    if (jsmnbool(js, tok, &json_metrics->values.ul) < 0)
		return -1;
	    break;
	    
	case pmjson_flag_s32:
	    if (jsmnint(js, tok, &json_metrics->values.l) < 0)
		return -1;
	    break;

	case pmjson_flag_u32:
	    if (jsmnuint(js, tok, &json_metrics->values.ul) < 0)
		return -1;
	    break;

	case pmjson_flag_s64:
	    if (jsmnlong(js, tok, &json_metrics->values.ll) < 0)
		return -1;
	    break;
	    
	case pmjson_flag_u64:
	    if (jsmnulong(js, tok, &json_metrics->values.ull) < 0)
		return -1;
	    break;
	    
	case pmjson_flag_float:
	    if (jsmnfloat(js, tok, &json_metrics->values.f) < 0)
		return -1;
	    break;
	    
	case pmjson_flag_double:
	    if (jsmndouble(js, tok, &json_metrics->values.d) < 0)
		return -1;
	    break;
	default:  /* assume old interface? not set and default to int */
	    if (jsmnint(js, tok, &json_metrics->values.l) < 0)
		return -1;
	    break;
	}
    }
    return 0;
}

static int
jsmnstrdup(const char *js, jsmntok_t *tok, char **name)
{
    char	*s = *name;

    if (tok->type != JSMN_STRING)
	return -1;
    if (s)
	free(s);
    s = *name = strndup(js + tok->start, tok->end - tok->start);
    return (s == NULL) ? -1 : 0;
}

static int
json_extract_values(const char *json, jsmntok_t *json_tokens, size_t count,
		    json_metric_desc *json_metrics, char* pointer_part[], int key, int total)
{
    /*
     * i is used to index json_tokens
     * j, k are used to index char's into the json string
     */

    int i, j, k = 0;
    
    /* we've reached the end of the json doc */
    if (count == 0)
	return 0;

    switch (json_tokens->type) {
    case JSMN_PRIMITIVE:
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "jsmn primitive\n");
	return 1;
    case JSMN_STRING:
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "string: %.*s parent: %d\n",
			json_tokens->end - json_tokens->start,
			json + json_tokens->start, json_tokens->parent);
	if (jsmneq(json, json_tokens, pointer_part[key]) == 0) {
	    if (key == 0 && json_tokens->parent != 0)
		return 1;

	    jsmntok_t *value = json_tokens + 1;
	    if (value->type == JSMN_PRIMITIVE && (total - key == 1)) {
		jsmnflagornumber(json, value, json_metrics, json_metrics->flags);
		return count;
	    }
	    else if (value->type == JSMN_STRING && total - key == 1) {
		jsmnstrdup(json, value, &json_metrics->values.cp);
		return count;
	    }
	    if (total - key == 1)
		key = 0;
	    return -1;
	}
	return 1;
    case JSMN_OBJECT:
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "jsmn object\n");
	for (i = j = k = 0; i < json_tokens->size; i++) {
	    k = 0;
	    if (pmDebugOptions.libweb)
		pmNotifyErr(LOG_DEBUG, "object key\n");
	    k = json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	    /* returned a valid section, continue */
	    if (k > 1) {
		key = 0;
		j += k;
	    }
	    /* went a level deeper but no match */
	    if (k < 0) {
		key++;
		j++;
	    }
	    /* returned a one, nothing hit so far */
	    if (k == 1) {
		j++;
	    }

	    if (pmDebugOptions.libweb)
		pmNotifyErr(LOG_DEBUG, "object value %d\n", (json_tokens+1+j)->size);
	    if (count > j)
		j += json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	    else
		return j + 1;
	}
	return j + 1;
    case JSMN_ARRAY:
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "jsmn_array");
	for (i = j = 0; i < json_tokens->size; i++)
	    j += json_extract_values(json, json_tokens+1+j, count-j, json_metrics, pointer_part, key, total);
	return j + 1;
    default:
	return 0;
    }
    return 0;
}

static int
json_pointer_to_index(const char *json, jsmntok_t *json_tokens, size_t count, json_metric_desc *json_metrics, int nmetrics)
{
    char	*json_pointer;
    char	*pointer_part;
    char	*pointer_final[MAXPATHLEN];
    int		i, j;

    /* tokenize the json_metrics.json_pointers fields */
    for (i = 0; i < nmetrics; i++) {
	memset(&pointer_final[0], 0, sizeof(pointer_final));
	json_pointer = strdup(json_metrics[i].json_pointer);
	if (pmDebugOptions.libweb)
	    pmNotifyErr(LOG_DEBUG, "json_pointer: %s\n", json_pointer);
	j = 0;
	pointer_part = strtok(json_pointer, "/");
	if (!pointer_part) {
	    pointer_final[j++] = strdup(json_pointer);
	} else {
	    pointer_final[j++] = strdup(pointer_part);
	    while (pointer_part &&
		   j < sizeof(pointer_final)/sizeof(*pointer_final)) {
		if ((pointer_part = strtok(NULL, "/")) != NULL)
		    pointer_final[j++] = strdup(pointer_part);
	    }
	}
	json_extract_values(json, json_tokens, count, &json_metrics[i], pointer_final, 0, j);
	while (--j >= 0)
	    free(pointer_final[j]);
	if (json_pointer != pointer_final[0])
	    free(json_pointer);
    }
    return 0;
}

static int
json_read(char *buffer, int buffer_size, void *userdata)
{
    int	fd = *(int *)userdata;
    return read(fd, buffer, buffer_size);
}

int
pmjsonInit(int fd, json_metric_desc *json_metrics, int nmetrics)
{
    return pmjsonGet(json_metrics, nmetrics, PM_INDOM_NULL, json_read, (void *)&fd);
}

int
pmjsonInitIndom(int fd, json_metric_desc *json_metrics, int nmetrics, pmInDom indom)
{
    return pmjsonGet(json_metrics, nmetrics, indom, json_read, (void *)&fd);
}

int
pmjsonGet(json_metric_desc *json_metrics, int nmetrics, pmInDom indom,
		   json_get get_json, void *userdata)
{
    int		sts = 0;
    jsmn_parser	parser;
    jsmntok_t	*json_tokens, *tp;
    int		token_count = 256;
    int		json_length = 0;
    char	*json = NULL, *jp;
    char	buffer[BUFSIZ];
    int		bytes;

    jsmn_init(&parser);
    if (!(json_tokens = calloc(token_count, sizeof(*json_tokens))))
	return -ENOMEM;

    for (;;) {
	bytes = (*get_json)(buffer, sizeof(buffer), userdata);
	if (bytes == 0)
	    goto indexing;
	if (bytes < 0) {
	    if (pmDebugOptions.libweb)
		fprintf(stderr, "%s: failed to get JSON: %s\n",
			pmGetProgname(), osstrerror());
	    sts = -oserror();
	    goto finished;
	}

	/* Successfully read in more data, extend sizeof json array */
	if ((jp = realloc(json, json_length + bytes + 1)) == NULL) {
	    sts = -ENOMEM;
	    goto finished;
	}
	json = jp;
	strncpy(json + json_length, buffer, bytes);
	json_length = json_length + bytes;

parsing:
	sts = jsmn_parse(&parser, json, json_length, json_tokens, token_count);
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "jsmn_parse() -> %d\n", sts);
	if (sts < 0) {
	    if (sts == JSMN_ERROR_PART)		/* keep consuming JSON */
		continue;
	    if (sts == JSMN_ERROR_NOMEM) {	/* ran out of token space */
		token_count *= 2;
		bytes = sizeof(*json_tokens) * token_count;
		if ((tp = realloc(json_tokens, bytes)) == NULL) {
		    free(json);
		    free(json_tokens);
		    return -ENOMEM;
		}
		json_tokens = tp;
		/* set the newly allocated memory to zero */
		memset(&json_tokens[token_count / 2], 0, bytes / 2);
		goto parsing;
	    }
	    sts = -EINVAL;	/* anything else indicates invalid JSON */
	    break;
	}

indexing:
	json_pointer_to_index(json, json_tokens,
			parser.toknext, json_metrics, nmetrics);
	sts = (indom == PM_INDOM_NULL) ? 0 : pmdaCacheStore(indom,
			PMDA_CACHE_ADD, json_metrics[0].dom, json_metrics);
	break;
    }

finished:
    free(json_tokens);
    free(json);
    return sts;
}

static int
printyaml(FILE *fp, const char *js, jsmntok_t *t, size_t count, int indent)
{
    int		i, j;

    if (!count)
	return 0;

    if (t->type == JSMN_PRIMITIVE) {
	fprintf(fp, "%.*s", t->end - t->start, js + t->start);
	return 1;
    } else if (t->type == JSMN_STRING) {
	fprintf(fp, "%.*s", t->end - t->start, js + t->start);
	return 1;
    } else if (t->type == JSMN_OBJECT) {
	if (indent > 0)
	    fprintf(fp, "\n");
	for (i = j = 0; i < t->size; i++) {
	    fprintf(fp, "%*s", PRETTY_SPACES * indent, "");
	    j += printyaml(fp, js, t + 1 + j, count - j, indent + 1);
	    fprintf(fp, ": ");
	    j += printyaml(fp, js, t + 1 + j, count - j, indent + 1);
	    if (i != t->size - 1)
		fprintf(fp, "\n");
	}
	return j+1;
    } else if (t->type == JSMN_ARRAY) {
	if (indent > 0)
	    fprintf(fp, "\n");
	for (i = j = 0; i < t->size; i++) {
	    fprintf(fp, "%*s", PRETTY_SPACES * indent, "");
	    fprintf(fp, "  - ");
	    j += printyaml(fp, js, t+1+j, count-j, indent+1);
	    if (i != t->size - 1)
		fprintf(fp, "\n");
	}
	return j+1;
    }
    return 0;
}

static int
printjson(FILE *fp, const char *js, jsmntok_t *t, size_t count, int indent, json_flags flags)
{
    int		i, j;

    if (!count)
	return 0;

    if (t->type == JSMN_PRIMITIVE) {
	fprintf(fp, "%.*s", t->end - t->start, js + t->start);
	return 1;
    } else if (t->type == JSMN_STRING) {
	fprintf(fp, "\"%.*s\"", t->end - t->start, js + t->start);
	return 1;
    } else if (t->type == JSMN_OBJECT) {
	fprintf(fp, "{");
	if (!(flags & pmjson_flag_minimal) && t->size > 0)
	    fprintf(fp, "\n");
	for (i = j = 0; i < t->size; i++) {
	    if (!(flags & pmjson_flag_minimal))
		fprintf(fp, "%*s", PRETTY_SPACES * (1 + indent), "");
	    j += printjson(fp, js, t + 1 + j, count - j, indent + 1, flags);
	    fprintf(fp, ":");
	    if (!(flags & pmjson_flag_minimal))
		fprintf(fp, " ");
	    j += printjson(fp, js, t + 1 + j, count - j, indent + 1, flags);
	    if (i != t->size - 1)
		fprintf(fp, ",");
	    if (!(flags & pmjson_flag_minimal))
		fprintf(fp, "\n");
	}
	if (!(flags & pmjson_flag_minimal))
	    fprintf(fp, "%*s", PRETTY_SPACES * indent, "");
	fprintf(fp, "}");
	return j+1;
    } else if (t->type == JSMN_ARRAY) {
	fprintf(fp, "[");
	if (!(flags & pmjson_flag_minimal) && t->size > 0)
	    fprintf(fp, "\n");
	for (i = j = 0; i < t->size; i++) {
	    if (!(flags & pmjson_flag_minimal))
		fprintf(fp, "%*s", PRETTY_SPACES * (1 + indent), "");
	    j += printjson(fp, js, t+1+j, count-j, indent+1, flags);
	    if (i != t->size - 1)
		fprintf(fp, ",");
	    if (!(flags & pmjson_flag_minimal))
		fprintf(fp, "\n");
	}
	if (!(flags & pmjson_flag_minimal))
	    fprintf(fp, "%*s", PRETTY_SPACES * indent, "");
	fprintf(fp, "]");
	return j+1;
    }
    return 0;
}

int
pmjsonPrint(FILE *fp, json_flags flags, const char *pointer,
		json_get get_json, void *userdata)
{
    int		sts = 0;
    int		eof_expected = 0;
    jsmn_parser	parser;
    jsmntok_t	*json_tokens, *tp;
    int		token_count = 256;
    int		json_length = 0;
    char	*json = NULL;
    char	buffer[BUFSIZ];
    int		bytes;

    if (pointer)
	return PM_ERR_NYI;	/* API placeholder for jsonpointers */

    jsmn_init(&parser);
    if (!(json_tokens = calloc(token_count, sizeof(*json_tokens))))
	return -ENOMEM;

    for (;;) {
	bytes = (*get_json)(buffer, sizeof(buffer), userdata);
	if (bytes == 0) {
	    sts = eof_expected ? 0 : -EINVAL;
	    goto finished;
	}
	if (bytes < 0) {
	    if (pmDebugOptions.libweb)
		fprintf(stderr, "%s: failed to get JSON: %s\n",
			pmGetProgname(), osstrerror());
	    sts = -oserror();
	    goto finished;
	}

	/* Successfully read in more data, extend sizeof json array */
	if ((json = realloc(json, json_length + bytes + 1)) == NULL) {
	    sts = -ENOMEM;
	    goto finished;
	}
	strncpy(json + json_length, buffer, bytes);
	json_length = json_length + bytes;

parsing:
	sts = jsmn_parse(&parser, json, json_length, json_tokens, token_count);
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "jsmn_parse() -> %d\n", sts);
	if (sts < 0) {
	    if (sts == JSMN_ERROR_PART)		/* keep consuming JSON */
		continue;
	    if (sts == JSMN_ERROR_NOMEM) {	/* ran out of token space */
		token_count *= 2;
		bytes = sizeof(*json_tokens) * token_count;
		if ((tp = realloc(json_tokens, bytes)) == NULL) {
		    free(json);
		    free(json_tokens);
		    return -ENOMEM;
		}
		json_tokens = tp;
		/* set the newly allocated memory to zero */
		memset(&json_tokens[token_count / 2], 0, bytes / 2);
		goto parsing;
	    }
	    sts = -EINVAL;	/* anything else indicates invalid JSON */
	    break;
	}

/*printing:*/
	if (flags & pmjson_flag_quiet)
	    ; /* just checking syntax */
	else if (flags & pmjson_flag_yaml) {
	    printyaml(fp, json, json_tokens, parser.toknext, 0);
	    fprintf(fp, "\n");
	} else {
	    printjson(fp, json, json_tokens, parser.toknext, 0, flags);
	    fprintf(fp, "\n");
	}
	eof_expected = 1;
    }

finished:
    free(json_tokens);
    free(json);
    return sts;
}
