/*
 * Copyright (c) 2021 Red Hat.
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
#include <ctype.h>
#include "podman.h"
#include <jsonsl/jsonsl.h>
#include "pmhttp.h"
#include "http_client.h"
#include "http_parser.h"

typedef struct {
    uint32_t		id;
    container_stats_e	field;
    container_stats_t	values;
} container_stats_parser_t;

typedef struct {
    uint32_t		id;
    container_info_e	field;
    container_info_t	values;
} container_info_parser_t;

typedef struct {
    uint32_t		id;
    pod_info_e		field;
    pod_info_t		values;
} pod_info_parser_t;

static container_stats_parser_t	container_stats_parser;
static jsonsl_t			container_stats_json;
static container_info_parser_t	container_info_parser;
static jsonsl_t			container_info_json;
static pod_info_parser_t	pod_info_parser;
static jsonsl_t			pod_info_json;

static int
log_error(jsonsl_t json, jsonsl_error_t error,
	struct jsonsl_state_st *state, jsonsl_char_t *at)
{
    pmNotifyErr(LOG_ERR, "Error %s at position %zd. Remaining: %s\n",
		jsonsl_strerror(error), json->pos, at);
    return 0;
}

/*
 * Parse and refresh metric values relating to container stats cluster
 */

static void
container_info_reset(container_info_t *info)
{
    info->name = -1;
    info->command = -1;
    info->status = -1;
    info->labelmap = -1;
    info->image = -1;
    info->podid = -1;
}

static void
container_stats_update(container_stats_parser_t *parser, const char *position)
{
    if (container_stats_json->level == 2) {	/* complete */
	container_t *cp = NULL;
	pmInDom indom = INDOM(CONTAINER_INDOM);
	char *name = podman_strings_lookup(parser->id);
	int sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cp);
	if (sts < 0 || cp == NULL) {
	    if ((cp = calloc(1, sizeof(*cp))) == NULL)
		return;
	    if (pmDebugOptions.attr)
		fprintf(stderr, "adding container %s (%u)\n", name, parser->id);
	    container_info_reset(&cp->info);
	}
	/* store the completed info values into the cached structure */
	cp->flags |= STATE_STATS;
	memcpy(&cp->stats, &parser->values, sizeof(cp->stats));
	pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)cp);
    }
}

static void
container_stats_field(container_stats_parser_t *cp,
			const char *key, size_t len)
{
    cp->field = -1;

    if (len == 11 && strncmp("ContainerID", key, len) == 0)
	cp->field = STATS_ID;
    else if (len == 10 && strncmp("BlockInput", key, len) == 0)
	cp->field = STATS_BLOCK_INPUT;
    else if (len == 11 && strncmp("BlockOutput", key, len) == 0)
	cp->field = STATS_BLOCK_OUTPUT;
    else if (len == 8 && strncmp("NetInput", key, len) == 0)
	cp->field = STATS_NET_INPUT;
    else if (len == 9 && strncmp("NetOutput", key, len) == 0)
	cp->field = STATS_NET_OUTPUT;
    else if (len == 3 && strncmp("CPU", key, len) == 0)
	cp->field = STATS_CPU;
    else if (len == 7 && strncmp("CPUNano", key, len) == 0)
	cp->field = STATS_CPU_NANO;
    else if (len == 13 && strncmp("CPUSystemNano", key, len) == 0)
	cp->field = STATS_SYSTEM_NANO;
    else if (len == 8 && strncmp("MemUsage", key, len) == 0)
	cp->field = STATS_MEM_USAGE;
    else if (len == 8 && strncmp("MemLimit", key, len) == 0)
	cp->field = STATS_MEM_LIMIT;
    else if (len == 7 && strncmp("MemPerc", key, len) == 0)
	cp->field = STATS_MEM_PERC;
    else if (len == 4 && strncmp("PIDs", key, len) == 0)
	cp->field = STATS_PIDS;
}

static void
container_stats_value(container_stats_parser_t *cp,
			const char *value, size_t len)
{
    char	buffer[BUFSIZ];
    char	*end;

    switch (cp->field) {
    case STATS_NET_INPUT:
	cp->values.net_input = strtoull(value, &end, 0);
	if (end - value != len)
	    cp->values.net_input = 0;
	break;
    case STATS_NET_OUTPUT:
	cp->values.net_output = strtoull(value, &end, 0);
	if (end - value != len)
	    cp->values.net_output = 0;
	break;
    case STATS_BLOCK_INPUT:
	cp->values.block_input = strtoull(value, &end, 0);
	if (end - value != len)
	    cp->values.block_input = 0;
	break;
    case STATS_BLOCK_OUTPUT:
	cp->values.block_output = strtoull(value, &end, 0);
	if (end - value != len)
	    cp->values.block_output = 0;
	break;
    case STATS_CPU:
	cp->values.cpu = strtod(value, &end);
	if (end - value != len)
	    cp->values.cpu = 0;
	break;
    case STATS_CPU_NANO:
	cp->values.cpu_nano = strtoll(value, &end, 0);
	if (end - value != len)
	    cp->values.cpu_nano = 0;
	break;
    case STATS_SYSTEM_NANO:
	cp->values.system_nano = strtoll(value, &end, 0);
	if (end - value != len)
	    cp->values.system_nano = 0;
	break;
    case STATS_MEM_USAGE:
	cp->values.mem_usage = strtoll(value, &end, 0);
	if (end - value != len)
	    cp->values.mem_usage = 0;
	break;
    case STATS_MEM_LIMIT:
	cp->values.mem_limit = strtoll(value, &end, 0);
	if (end - value != len)
	    cp->values.mem_limit = 0;
	break;
    case STATS_MEM_PERC:
	cp->values.mem_perc = strtod(value, &end);
	if (end - value != len)
	    cp->values.mem_perc = 0;
	break;
    case STATS_PIDS:
	cp->values.nprocesses = strtoul(value, &end, 0);
	if (end - value != len)
	    cp->values.nprocesses = 0;
	break;
    case STATS_ID:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	cp->id = podman_strings_insert(buffer);
	break;
    default:
	break;
    }
}

/*
 * Sample output:
 * {"Error":null,"Stats":[{"ContainerID":"aff836906c5bc3b4c932ba546984752784617645639632dcf0cd348d5c5f0d82","Name":"bold_nash","PerCPU":null,"CPU":1.5856378300710223e-9,"CPUNano":25700000,"CPUSystemNano":17224,"SystemNano":1620798867976609176,"MemUsage":7237632,"MemLimit":16391106560,"MemPerc":0.04415584740118973,"NetInput":1468,"NetOutput":167816,"BlockInput":12435456,"BlockOutput":0,"PIDs":1}]}
 */

static void
container_stats_create(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    container_stats_parser_t *parser;

    if (state->level == 3 && state->type == JSONSL_T_OBJECT) {
	/* new container, any previous one is stashed in indom cache */
	parser = (container_stats_parser_t *)json->data;
	memset(&parser->values, 0, sizeof(container_stats_t));
	parser->id = -1;
    }
}

static void
container_stats_complete(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    container_stats_parser_t	*parser;

    parser = (container_stats_parser_t *)json->data;
    if ((state->level == 4) &&
        (state->type == JSONSL_T_STRING || state->type == JSONSL_T_SPECIAL)) {
	const char *value = at - (json->pos - state->pos_begin);
	size_t bytes = json->pos - state->pos_begin;
	if (state->type == JSONSL_T_STRING)	/* skip enclosing quotes */
	    value++, bytes--;
	container_stats_value(parser, value, bytes);
    }
    else if (state->level == 4 && state->type == JSONSL_T_HKEY) {
	const char *key = (at - (json->pos - state->pos_begin)) + 1;
	size_t bytes = (json->pos - state->pos_begin) - 1;
	container_stats_field(parser, key, bytes);
    }
    else if (state->level == 3 && state->type == JSONSL_T_OBJECT) {
	container_stats_update(parser, at);
    }
}

/*
 * Parse and refresh metric values relating to container info cluster
 */

static void
container_info_add_labels(container_info_parser_t *ip, const char *value)
{
    if (ip->values.labels == NULL) {
	ip->values.labels = value - 2;
	ip->values.nlabels = 1;
    } else {
	ip->values.nlabels++;
    }
}

static void
container_info_end_labels(container_info_parser_t *ip, int bytes)
{
    char labels[PM_MAXLABELJSONLEN];

    if (ip->field == INFO_LABELS && ip->values.labels != NULL) {
	pmsprintf(labels, sizeof(labels),
		"{\"podman\":%.*s}", bytes, ip->values.labels);
	ip->values.labelmap = podman_strings_insert(labels);
	ip->values.labels = NULL;
    }
    ip->field = -1;	/* end labels */
}

static void
container_info_update(container_info_parser_t *ip, int level, const char *position)
{
    if (level > 2) {	/* labels complete */
	container_info_end_labels(ip, position - ip->values.labels + 1);
    } else {		/* container is complete */
	container_t *cp = NULL;
	pmInDom indom = INDOM(CONTAINER_INDOM);
	char *name = podman_strings_lookup(ip->id);
	int sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cp);
	if (sts < 0 || cp == NULL) {
	    if ((cp = calloc(1, sizeof(*cp))) == NULL)
		return;
	    if (pmDebugOptions.attr)
		fprintf(stderr, "adding container %s (%u)\n", name, ip->id);
	    container_info_reset(&cp->info);
	}
	cp->flags |= STATE_INFO;
	/* store the completed values into the cached structure */
	memcpy(&cp->info, &ip->values, sizeof(cp->info));
	pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)cp);
    }
}

static void
container_info_field(container_info_parser_t *ip, int level,
			const char *key, size_t len)
{
    if (level >= 3 && ip->field == INFO_LABELS) {
	container_info_add_labels(ip, key);
	return;
    }
    if (level != 3)
	return;

    ip->field = -1;
    if (len == 5 && strncmp("Names", key, len) == 0)
	ip->field = INFO_NAME;
    else if (len == 5 && strncmp("Image", key, len) == 0)
	ip->field = INFO_IMAGE;
    else if (len == 6 && strncmp("Status", key, len) == 0)
	ip->field = INFO_STATUS;
    else if (len == 7 && strncmp("Command", key, len) == 0)
	ip->field = INFO_COMMAND;
    else if (len == 3 && strncmp("Pod", key, len) == 0)
	ip->field = INFO_POD;
    else if (len == 2 && strncmp("Id", key, len) == 0)
	ip->field = INFO_ID;
    else if (len == 6 && strncmp("Labels", key, len) == 0)
	ip->field = INFO_LABELS;
}

static void
container_info_value(container_info_parser_t *ip, int level,
			const char *value, size_t len)
{
    char	buffer[BUFSIZ];
    char	*acc;	/* accumulated string */
    size_t	off;	/* string byte offset */

    if (level != 3 && level != 4)
	return;

    switch (ip->field) {
    case INFO_NAME:
	acc = podman_strings_lookup(ip->values.name);
	off = strlen(acc) ? pmsprintf(buffer, sizeof(buffer), "%s ", acc) : 0;
	pmsprintf(buffer + off, sizeof(buffer) - off, "%.*s", (int)len, value);
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	ip->values.name = podman_strings_insert(buffer);
	break;
    case INFO_COMMAND:
	acc = podman_strings_lookup(ip->values.command);
	off = strlen(acc) ? pmsprintf(buffer, sizeof(buffer), "%s ", acc) : 0;
	pmsprintf(buffer + off, sizeof(buffer) - off, "%.*s", (int)len, value);
	ip->values.command = podman_strings_insert(buffer);
	break;
    case INFO_STATUS:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	ip->values.status = podman_strings_insert(buffer);
	ip->values.running = (strncmp("Running", value, len) == 0);
	break;
    case INFO_IMAGE:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	ip->values.image = podman_strings_insert(buffer);
	break;
    case INFO_POD:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	ip->values.podid = podman_strings_insert(buffer);
	break;
    case INFO_ID:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	ip->id = podman_strings_insert(buffer);
	break;
    default:
	break;
    }
}

/*
 * Sample output:
 * [{"AutoRemove":false,"Command":["bash"],"Created":"2021-05-12T10:29:51.438438116+10:00","CreatedAt":"","Exited":false,"ExitedAt":-62135596800,"ExitCode":0,"Id":"aff836906c5bc3b4c932ba546984752784617645639632dcf0cd348d5c5f0d82","Image":"registry.fedoraproject.org/fedora:latest","ImageID":"eb7134a03cd6bd8a3de99c16cf174d66ad2d93724bac3307795efcd8aaf914c5","IsInfra":false,"Labels":{"license":"MIT","name":"fedora","vendor":"Fedora Project","version":"32"},"Mounts":[],"Names":["bold_nash"],"Namespaces":{},"Networks":["podman"],"Pid":51085,"Pod":"","PodName":"","Ports":null,"Size":null,"StartedAt":1620779391,"State":"running","Status":""}]
 */

static void
container_info_create(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    container_info_parser_t *parser;

    if (state->level == 2 && state->type == JSONSL_T_OBJECT) {
	/* new container, any previous one is stashed in indom cache */
	parser = (container_info_parser_t *)json->data;
	memset(&parser->values, 0, sizeof(container_info_t));
	container_info_reset(&parser->values);
	parser->id = -1;
    }
}

static void
container_info_complete(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    container_info_parser_t	*parser;

    parser = (container_info_parser_t *)json->data;
    if (state->type == JSONSL_T_STRING || state->type == JSONSL_T_SPECIAL) {
	const char *value = at - (json->pos - state->pos_begin);
	size_t bytes = json->pos - state->pos_begin;
	if (state->type == JSONSL_T_STRING)	/* skip enclosing quotes */
	    value++, bytes--;
	container_info_value(parser, state->level, value, bytes);
    }
    else if (state->type == JSONSL_T_HKEY) {
	const char *key = (at - (json->pos - state->pos_begin)) + 1;
	size_t bytes = (json->pos - state->pos_begin) - 1;
	container_info_field(parser, state->level, key, bytes);
    }
    else if (state->type == JSONSL_T_OBJECT) {
	container_info_update(parser, state->level, at);
    }
}


/*
 * Parse and refresh metric values relating to pod info cluster
 */

static void
pod_info_reset(pod_info_t *info)
{
    info->name = -1;
    info->cgroup = -1;
    info->labelmap = -1;
    info->status = -1;
}

static void
pod_info_add_labels(pod_info_parser_t *pp, const char *value)
{
    if (pp->values.labels == NULL) {
	pp->values.labels = value - 2;
	pp->values.nlabels = 1;
    } else {
	pp->values.nlabels++;
    }
}

static void
pod_info_end_labels(pod_info_parser_t *pp, int bytes)
{
    char labels[PM_MAXLABELJSONLEN];

    if (pp->field == POD_LABELS && pp->values.labels != NULL) {
	pmsprintf(labels, sizeof(labels),
		"{\"podman\":%.*s}", bytes, pp->values.labels);
	pp->values.labelmap = podman_strings_insert(labels);
	pp->values.labels = NULL;
    }
    pp->field = -1;	/* end labels */
}

static void
pod_info_update(pod_info_parser_t *ip, int level, const char *position)
{
    if (level > 2) {	/* labels are now complete */
	pod_info_end_labels(ip, position - ip->values.labels + 1);
    } else {		/* pod is complete */
	pod_t *pp = NULL;
	pmInDom indom = INDOM(POD_INDOM);
	char *name = podman_strings_lookup(ip->id);
	int sts = pmdaCacheLookupName(indom, name, NULL, (void **)&pp);
	if (sts < 0 || pp == NULL) {
	    if ((pp = calloc(1, sizeof(*pp))) == NULL)
		return;
	    if (pmDebugOptions.attr)
		fprintf(stderr, "adding pod %s (%u)\n", name, ip->id);
	    pod_info_reset(&pp->info);
	}
	pp->flags |= STATE_POD;
	/* store the completed pod values into the cached structure */
	memcpy(&pp->info, &ip->values, sizeof(pp->info));
	pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)pp);
    }
}

static void
pod_info_field(pod_info_parser_t *pp, int level, const char *key, size_t len)
{
    if (level >= 3 && pp->field == POD_LABELS) {
	pod_info_add_labels(pp, key);
	return;
    }
    if (level != 3)
	return;

    pp->field = -1;
    if (len == 2 && strncmp("Id", key, len) == 0) {
	pod_info_e lastfield = pp->field;
	if (pod_info_json->level >= 2) {
	    if (lastfield == POD_CONTAINERS)	/* containers */
		pp->values.ncontainers++;
	} else if (pod_info_json->level < 2) {		/* pods */
	    pp->field = POD_ID;
	}
    }
    else if (len == 4 && strncmp("Name", key, len) == 0)
	pp->field = POD_NAME;
    else if (len == 6 && strncmp("Cgroup", key, len) == 0)
	pp->field = POD_CGROUP;
    else if (len == 6 && strncmp("Labels", key, len) == 0)
	pp->field = POD_LABELS;
    else if (len == 6 && strncmp("Status", key, len) == 0)
	pp->field = POD_STATUS;
    else if (len == 10 && strncmp("Containers", key, len) == 0) {
	pp->field = POD_CONTAINERS;
	pp->values.ncontainers = 0;
    }
}

static void
pod_info_value(pod_info_parser_t *pp, int level, const char *value, size_t len)
{
    char	buffer[BUFSIZ];

    if (level != 3)
	return;

    switch (pp->field) {
    case POD_NAME:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	pp->values.name = podman_strings_insert(buffer);
	break;
    case POD_CGROUP:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	pp->values.cgroup = podman_strings_insert(buffer);
	break;
    case POD_STATUS:
	pmsprintf(buffer, sizeof(buffer), "%.*s", (int)len, value);
	pp->values.status = podman_strings_insert(buffer);
	pp->values.running = (strcmp(buffer, "Running") == 0);
	break;
    case POD_CONTAINERS:
	pp->values.ncontainers = 0;
	/* count Id fields in pod_info_containers */
	break;
    default:
	break;
    }
}

/*
 * Sample output:
 * [{"Cgroup":"user.slice","Containers":[{"Id":"527c6d322dd68e261630a3f46b3a7db3b1b79ceb0c09d8aeb36012fa8efa28b2","Names":"b9ab3ec058fc-infra","Status":"configured"}],"Created":"2021-05-11T15:26:25.325078296-07:00","Id":"b9ab3ec058fc50417bd68e79a40b0ff75d67c074ca37cba1c73a012c7bb40953","InfraId":"527c6d322dd68e261630a3f46b3a7db3b1b79ceb0c09d8aeb36012fa8efa28b2","Name":"pod001","Namespace":"","Networks":null,"Status":"Created","Labels":{}}]
 */

static void
pod_info_create(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    pod_info_parser_t *parser;

    if (state->level == 2 && state->type == JSONSL_T_OBJECT) {
	/* new pod, any previous one is stashed in indom cache */
	parser = (pod_info_parser_t *)json->data;
	memset(&parser->values, 0, sizeof(pod_info_t));
	pod_info_reset(&parser->values);
	parser->id = -1;
    }
}

static void
pod_info_complete(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const jsonsl_char_t *at)
{
    pod_info_parser_t	*parser = (pod_info_parser_t *)json->data;

    if (state->type == JSONSL_T_STRING || state->type == JSONSL_T_SPECIAL) {
	const char *value = at - (json->pos - state->pos_begin);
	size_t bytes = json->pos - state->pos_begin;
	if (state->type == JSONSL_T_STRING)	/* skip quotes */
	    value++, bytes--;
	pod_info_value(parser, state->level, value, bytes);
    }
    else if (state->type == JSONSL_T_HKEY) {
	const char *key = (at - (json->pos - state->pos_begin)) + 1;
	size_t bytes = (json->pos - state->pos_begin) - 1;
	pod_info_field(parser, state->level, key, bytes);
    }
    else if (state->type == JSONSL_T_OBJECT) {
	pod_info_update(parser, state->level, at);
    }
}

/*
 * Global setup and teardown routines
 */

int
podman_parse_init(void)
{
    if ((container_stats_json = jsonsl_new(16)) == 0)
	return -ENOMEM;

    if ((container_info_json = jsonsl_new(16)) == 0) {
	jsonsl_destroy(container_stats_json);
	return -ENOMEM;
    }

    if ((pod_info_json = jsonsl_new(16)) == 0) {
	jsonsl_destroy(container_stats_json);
	jsonsl_destroy(container_info_json);
	return -ENOMEM;
    }

    container_stats_json->data = &container_stats_parser;
    container_stats_json->error_callback = log_error;
    container_stats_json->action_callback_PUSH = container_stats_create;
    container_stats_json->action_callback_POP = container_stats_complete;
    jsonsl_enable_all_callbacks(container_stats_json);

    container_info_json->data = &container_info_parser;
    container_info_json->error_callback = log_error;
    container_info_json->action_callback_PUSH = container_info_create;
    container_info_json->action_callback_POP = container_info_complete;
    jsonsl_enable_all_callbacks(container_info_json);

    pod_info_json->data = &pod_info_parser;
    pod_info_json->error_callback = log_error;
    pod_info_json->action_callback_PUSH = pod_info_create;
    pod_info_json->action_callback_POP = pod_info_complete;
    jsonsl_enable_all_callbacks(pod_info_json);

    return 0;
}

void
podman_parse_end(void)
{
    jsonsl_destroy(container_stats_json);
    jsonsl_destroy(container_info_json);
    jsonsl_destroy(pod_info_json);
}

static void
podman_http_parse(struct http_client *cp,
	jsonsl_t json, const char *buffer, size_t length)
{
    if (cp->parser.status_code == 200) {
	jsonsl_reset(json);
	jsonsl_feed(json, buffer, length);
    }
}

static char *
podman_buffer(char *buffer, size_t *buflen)
{
    size_t		length = *buflen;
    char		*p;

    if (length == 0)
	length = 256;
    if (length >= UINT_MAX / 256)
	return NULL;

    length *= 2;
    if ((p = realloc(buffer, length)) != NULL) {
	*buflen = length;
    } else {
	free(buffer);
	*buflen = 0;
    }
    return p;
}

static int
podman_validate_socket(const char *path)
{
    struct stat		sbuf;

    if (stat(path, &sbuf) < 0)
	return -ENOENT;
    if (S_ISSOCK(sbuf.st_mode))
	return 0;
    return -EINVAL;
}

static void
podman_http_fetch(const char *url, const char *query, jsonsl_t parser)
{
    struct http_client	*client;
    static size_t	jsonlen;
    static char		*json;
    char		type[64];
    int			sts, len;

    if ((json == NULL) &&
	(json = podman_buffer(json, &jsonlen)) == NULL)
	return;

    if ((client = pmhttpNewClient()) == NULL)
	return;

retry:
    len = pmsprintf(type, sizeof(type), "/v3.0.0/libpod/%s", query);
    if ((sts = pmhttpClientFetch(client, url, json, jsonlen, type, len)) > 0) {
	if (pmDebugOptions.attr)
	    fprintf(stderr, "podman_http_fetch: %.*s\n", sts, json);
	podman_http_parse(client, parser, json, sts);
    } else if (sts == -E2BIG) {
	json = podman_buffer(json, &jsonlen);
	goto retry;
    }
    pmhttpFreeClient(client);
}

#define PODQUERY "pods/json"
#define INFOQUERY "containers/json"
#define STATSQUERY "containers/stats?stream=false"

static void
podman_refresh_socket(const char *path, unsigned int need_refresh[])
{
    char		url[MAXPATHLEN + 8];

    if (podman_validate_socket(path) < 0)
	return;
    if (pmDebugOptions.attr)
	fprintf(stderr, "refreshing on socket %s\n", path);

    /* Unix domain socket protocol and path (used for all requests) */
    pmsprintf(url, sizeof(url), "unix:/%s", path);

    if (need_refresh[CLUSTER_POD])
	podman_http_fetch(url, PODQUERY, pod_info_json);

    if (need_refresh[CLUSTER_INFO])
	podman_http_fetch(url, INFOQUERY, container_info_json);

    if (need_refresh[CLUSTER_STATS])
	podman_http_fetch(url, STATSQUERY, container_stats_json);
}

/*
 * Refresh instance domain and all values for given clusters.
 * Algorith used is designed to produce a union of containers
 * across all podman invocations (using root or rootless); so
 * we make requests on /run/podman socket first, then iterate
 * over all /run/user/.../podman sockets.
 */
void
podman_refresh(unsigned int need_refresh[])
{
    static const char	sockpath[] = "podman/podman.sock";
    pmInDom		containers = INDOM(CONTAINER_INDOM);
    pmInDom		pods = INDOM(POD_INDOM);
    char		dirpath[MAXPATHLEN];
    char		path[MAXPATHLEN];
    DIR			*rundir;
    struct dirent	*entry;

    if (need_refresh[CLUSTER_STATS] || need_refresh[CLUSTER_INFO])
	pmdaCacheOp(containers, PMDA_CACHE_INACTIVE);
    if (need_refresh[CLUSTER_POD])
	pmdaCacheOp(pods, PMDA_CACHE_INACTIVE);

    /* first the root socket */
    pmsprintf(path, MAXPATHLEN, "%s/%s", podman_rundir, sockpath);
    podman_refresh_socket(path, need_refresh);

    /* then any user sockets (rootless) */
    pmsprintf(dirpath, MAXPATHLEN, "%s/user", podman_rundir);
    if ((rundir = opendir(dirpath)) == NULL)
	return;
    while ((entry = readdir(rundir)) != NULL) {
	char *user = entry->d_name;
	if (!isdigit(user[0]) || strcmp(user, "0") == 0)
	    continue;
	pmsprintf(path, MAXPATHLEN, "%s/%s/%s", dirpath, user, sockpath);
	podman_refresh_socket(path, need_refresh);
    }
    closedir(rundir);
}
