/*
 * Copyright (c) 2019-2022 Red Hat.
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
#include <ctype.h>
#include "pmapi.h"
#include "pmda.h"
#include "schema.h"
#include "util.h"
#include "load.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <fnmatch.h>

#define DEFAULT_WORK_TIMER 2000
static unsigned int default_worker;	/* BG work delta, milliseconds */

#define DEFAULT_POLL_TIMEOUT 5000
static unsigned int default_timeout;	/* timeout in milliseconds */

#define DEFAULT_BATCHSIZE 256
static unsigned int default_batchsize;	/* for groups of metrics */

/* constant string keys (initialized during setup) */
static sds PARAM_HOSTNAME, PARAM_HOSTSPEC, PARAM_CTXNUM, PARAM_CTXID,
           PARAM_POLLTIME, PARAM_PREFIX, PARAM_MNAME, PARAM_MNAMES,
           PARAM_PMIDS, PARAM_PMID, PARAM_INDOM, PARAM_INSTANCE,
           PARAM_INAME, PARAM_MVALUE, PARAM_TARGET, PARAM_EXPR, PARAM_MATCH;
static sds AUTH_USERNAME, AUTH_PASSWORD;
static sds EMPTYSTRING, LOCALHOST, WORK_TIMER, POLL_TIMEOUT, BATCHSIZE;

enum matches { MATCH_EXACT, MATCH_GLOB, MATCH_REGEX };
enum profile { PROFILE_ADD, PROFILE_DEL };

enum webgroup_metric {
    CONTEXT_MAP_SIZE,
    NAMES_MAP_SIZE,
    LABELS_MAP_SIZE,
    INST_MAP_SIZE,
    NUM_WEBGROUP_METRIC
};

typedef struct webgroups {
    struct dict		*contexts;
    struct dict		*config;

    mmv_registry_t	*registry;
    pmAtomValue		*metrics[NUM_WEBGROUP_METRIC];
    void		*map;

    uv_loop_t		*events;
    uv_timer_t		timer;
    uv_mutex_t		mutex;

    unsigned int	active;
    int			timerid;
} webgroups;

static struct webgroups *
webgroups_lookup(pmWebGroupModule *module)
{
    struct webgroups *groups = module->privdata;

    if (module->privdata == NULL) {
	module->privdata = calloc(1, sizeof(struct webgroups));
	groups = (struct webgroups *)module->privdata;
	uv_mutex_init(&groups->mutex);
	groups->timerid = -1;
    }
    return groups;
}

static int
webgroup_deref_context(struct context *cp)
{
    if (cp == NULL)
	return 1;
    if (cp->refcount == 0)
	return 0;
    return (--cp->refcount > 0);
}

static void
webgroup_release_context(uv_handle_t *handle)
{
    struct context	*context = (struct context *)handle->data;

    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "releasing context %p [refcount=%u]\n",
			context, context->refcount);
    pmwebapi_free_context(context);
}

static void
webgroup_drop_context(struct context *context, struct webgroups *groups)
{
    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "destroying context %p [refcount=%u]\n",
			context, context->refcount);

    if (webgroup_deref_context(context) == 0) {
	if (context->garbage == 0) {
	    context->garbage = 1;
	    uv_timer_stop(&context->timer);
	}
	if (groups) {
	    uv_mutex_lock(&groups->mutex);
	    dictDelete(groups->contexts, &context->randomid);
	    uv_mutex_unlock(&groups->mutex);
	}
	uv_close((uv_handle_t *)&context->timer, webgroup_release_context);
    }
}

static void
webgroup_timeout_context(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct context	*cp = (struct context *)handle->data;

    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "context %u timed out (%p)\n", cp->randomid, cp);

    /*
     * Cannot free data structures in the timeout handler, as
     * they may still be actively in use - wait until reference
     * is returned to zero by the caller, or background cleanup
     * finds this context and cleans it.
     */
    if (cp->refcount == 0 && cp->garbage == 0) {
	cp->garbage = 1;
	uv_timer_stop(&cp->timer);
    }
}

static int
webgroup_access(struct context *cp, sds hostspec, dict *params,
		int *status, sds *message, void *arg)
{
    __pmHashNode	*node;
    __pmHashCtl		attrs;
    __pmHostSpec	*hosts = NULL;
    char		*msg, buf[512];
    int			sts, bytes, numhosts = 0;
    sds			value;

    __pmHashInit(&attrs);
    if ((sts = __pmParseHostAttrsSpec(hostspec,
				&hosts, &numhosts, &attrs, &msg)) < 0) {
	/* may as well report the error here */
	*message = sdsnew(msg);
	*status = sts;
	free(msg);
	return sts;
    }
    if ((node = __pmHashSearch(PCP_ATTR_USERNAME, &attrs)) != NULL)
	cp->username = sdsnew((char *)node->data);
    else
    if ((node = __pmHashSearch(PCP_ATTR_PASSWORD, &attrs)) != NULL)
	cp->password = sdsnew((char *)node->data);

    /* add username from Basic Auth header if none given in hostspec */
    if (params && cp->username == NULL) {
	if ((value = dictFetchValue(params, AUTH_USERNAME)) != NULL) {
	    __pmHashAdd(PCP_ATTR_USERNAME, strdup(value), &attrs);
	    cp->username = sdsdup(value);
	}
	if ((value = dictFetchValue(params, AUTH_PASSWORD)) != NULL) {
	    __pmHashAdd(PCP_ATTR_PASSWORD, strdup(value), &attrs);
	    cp->password = sdsdup(value);
	}
    }

    bytes = __pmUnparseHostAttrsSpec(hosts, numhosts, &attrs, buf, sizeof(buf));
    if (bytes > 0 && strcmp(cp->name.sds, buf) != 0) {
	sdsfree(cp->name.sds);
	cp->name.sds = sdsnewlen(buf, bytes);
    }
    __pmFreeHostAttrsSpec(hosts, numhosts, &attrs);
    __pmHashClear(&attrs);

    return sts;
}

static struct context *
webgroup_new_context(pmWebGroupSettings *sp, dict *params,
		int *status, sds *message, void *arg)
{
    struct webgroups	*groups = webgroups_lookup(&sp->module);
    struct context	*cp;
    unsigned int	polltime = DEFAULT_POLL_TIMEOUT;
    uv_handle_t		*handle;
    pmWebAccess		access;
    double		seconds;
    char		*endptr;
    sds			hostspec = NULL, timeout;

    if (params) {
	if ((hostspec = dictFetchValue(params, PARAM_HOSTSPEC)) == NULL)
	    hostspec = dictFetchValue(params, PARAM_HOSTNAME);

	if ((timeout = dictFetchValue(params, PARAM_POLLTIME)) != NULL) {
	    seconds = strtod(timeout, &endptr);
	    if (*endptr != '\0') {
		infofmt(*message, "invalid timeout requested in polltime");
		*status = -EINVAL;
		return NULL;
	    }
	    polltime = (unsigned int)(seconds * 1000.0);
	}
    }

    if ((cp = (context_t *)calloc(1, sizeof(context_t))) == NULL) {
	infofmt(*message, "out-of-memory on new web context");
	*status = -ENOMEM;
	return NULL;
    }
    cp->type = PM_CONTEXT_HOST;
    cp->context = -1;
    cp->timeout = polltime;

    uv_mutex_lock(&groups->mutex);
    if ((cp->randomid = random()) < 0 ||
	dictFind(groups->contexts, &cp->randomid) != NULL) {
	infofmt(*message, "random number failure on new web context");
	pmwebapi_free_context(cp);
	*status = -ESRCH;
	uv_mutex_unlock(&groups->mutex);
	return NULL;
    }
    uv_mutex_unlock(&groups->mutex);
    cp->origin = sdscatfmt(sdsempty(), "%i", cp->randomid);
    cp->name.sds = sdsdup(hostspec ? hostspec : LOCALHOST);
    cp->realm = sdscatfmt(sdsempty(), "pmapi/%i", cp->randomid);
    if (cp->name.sds == NULL || cp->origin == NULL || cp->realm == NULL) {
	infofmt(*message, "out-of-memory on new web context");
	pmwebapi_free_context(cp);
	*status = -ENOMEM;
	return NULL;
    }

    if (webgroup_access(cp, cp->name.sds, params, status, message, arg) < 0) {
	pmwebapi_free_context(cp);
	return NULL;
    }
    access.password = cp->password;
    access.username = cp->username;
    access.realm = cp->realm;
    if (sp->callbacks.on_check &&
        sp->callbacks.on_check(cp->origin, &access, status, message, arg)) {
  	pmwebapi_free_context(cp);
  	return NULL;
    }

    if ((*message = pmwebapi_new_context(cp)) != NULL) {
	*status = -ENOTCONN;
	pmwebapi_free_context(cp);
	return NULL;
    }
    uv_mutex_lock(&groups->mutex);
    dictAdd(groups->contexts, &cp->randomid, cp);
    uv_mutex_unlock(&groups->mutex);

    /* leave until the end because uv_timer_init makes this visible in uv_run */
    handle = (uv_handle_t *)&cp->timer;
    handle->data = (void *)cp;
    uv_timer_init(groups->events, &cp->timer);

    cp->privdata = groups;
    cp->setup = 1;

    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "new context[%d] setup (%p)\n", cp->randomid, cp);

    return cp;
}

static void
webgroup_garbage_collect(struct webgroups *groups)
{
    dictIterator        *iterator;
    dictEntry           *entry;
    context_t		*cp;
    unsigned int	count = 0, drops = 0;

    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "%s: started\n", "webgroup_garbage_collect");

    /* do context GC if we get the lock (else don't block here) */
    if (uv_mutex_trylock(&groups->mutex) == 0) {
	iterator = dictGetSafeIterator(groups->contexts);
	for (entry = dictNext(iterator); entry;) {
	    cp = (context_t *)dictGetVal(entry);
	    entry = dictNext(iterator);
	    if (cp->garbage && cp->privdata == groups) {
		if (pmDebugOptions.http || pmDebugOptions.libweb)
		    fprintf(stderr, "GC context %u (%p)\n", cp->randomid, cp);
		uv_mutex_unlock(&groups->mutex);
		webgroup_drop_context(cp, groups);
		uv_mutex_lock(&groups->mutex);
		drops++;
	    }
	    count++;
	}
	dictReleaseIterator(iterator);
	uv_mutex_unlock(&groups->mutex);
    }

    if (pmDebugOptions.http || pmDebugOptions.libweb)
	fprintf(stderr, "%s: finished [%u drops from %u entries]\n",
			"webgroup_garbage_collect", drops, count);
}

static void
refresh_maps_metrics(void *data)
{
    struct webgroups	*groups = (struct webgroups *)data;
    unsigned int	value;

    value = dictSize(contextmap);
    mmv_set(groups->map, groups->metrics[CONTEXT_MAP_SIZE], &value);
    value = dictSize(namesmap);
    mmv_set(groups->map, groups->metrics[NAMES_MAP_SIZE], &value);
    value = dictSize(labelsmap);
    mmv_set(groups->map, groups->metrics[LABELS_MAP_SIZE], &value);
    value = dictSize(instmap);
    mmv_set(groups->map, groups->metrics[INST_MAP_SIZE], &value);
}

static void
webgroup_worker(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct webgroups	*groups = (struct webgroups *)handle->data;

    webgroup_garbage_collect(groups);
}

static struct context *
webgroup_use_context(struct context *cp, int *status, sds *message, void *arg)
{
    char		errbuf[PM_MAXERRMSGLEN];
    int			sts;
    struct webgroups    *gp = (struct webgroups *)cp->privdata;

    if (cp->garbage == 0) {
	if (cp->setup == 0) {
	    if ((sts = pmReconnectContext(cp->context)) < 0) {
		infofmt(*message, "cannot reconnect context: %s",
			pmErrStr_r(sts, errbuf, sizeof(errbuf)));
		*status = sts;
		return NULL;
	    }
	    cp->setup = 1;
	}
	if ((sts = pmUseContext(cp->context)) < 0) {
	    infofmt(*message, "cannot use existing context: %s",
			pmErrStr_r(sts, errbuf, sizeof(errbuf)));
	    *status = sts;
	    return NULL;
	}

	if (pmDebugOptions.http || pmDebugOptions.libweb)
	    fprintf(stderr, "context %u timer set (%p) to %u msec\n",
			cp->randomid, cp, cp->timeout);

	/* refresh current time: https://github.com/libuv/libuv/issues/1068 */
	uv_update_time(gp->events);

	/* if already started, uv_timer_start updates the existing timer */
	uv_timer_start(&cp->timer, webgroup_timeout_context, cp->timeout, 0);
    } else {
	infofmt(*message, "expired context identifier: %u", cp->randomid);
	*status = -ENOTCONN;
	return NULL;
    }

    return cp;
}

static struct context *
webgroup_lookup_context(pmWebGroupSettings *sp, sds *id, dict *params,
		int *status, sds *message, void *arg)
{
    struct webgroups	*groups = webgroups_lookup(&sp->module);
    struct context	*cp = NULL;
    unsigned int	key;
    pmWebAccess		access;
    char		*endptr = NULL;

    if (groups->active == 0) {
	groups->active = 1;
	/* install general background work timer (GC) */
	uv_timer_init(groups->events, &groups->timer);
	groups->timer.data = (void *)groups;
	uv_timer_start(&groups->timer, webgroup_worker,
			default_worker, default_worker);
	/* timer for map stats refresh */
	groups->timerid = pmWebTimerRegister(refresh_maps_metrics, groups);
    }

    if (*id == NULL) {
	if (!(cp = webgroup_new_context(sp, params, status, message, arg)))
	    return NULL;
    } else {
	key = (unsigned int)strtoul(*id, &endptr, 10);
	if (*endptr != '\0') {
	    infofmt(*message, "invalid context identifier: %s", *id);
	    *status = -EINVAL;
	    return NULL;
	}
	cp = (struct context *)dictFetchValue(groups->contexts, &key);
	if (cp == NULL) {
	    infofmt(*message, "unknown context identifier: %u", key);
	    *status = -ENOTCONN;
	    return NULL;
	}
	if (cp->garbage == 0) {
	    access.username = cp->username;
	    access.password = cp->password;
	    access.realm = cp->realm;
	    if (sp->callbacks.on_check &&
		sp->callbacks.on_check(*id, &access, status, message, arg) < 0) {
		return NULL;
	    }
	}
    }

    if ((cp = webgroup_use_context(cp, status, message, arg)) != NULL)
	cp->refcount++;
    return cp;
}

int
pmWebGroupContext(pmWebGroupSettings *sp, sds id, dict *params, void *arg)
{
    struct context	*cp;
    pmWebSource		context;
    sds			msg = NULL;
    int			sts = 0;

    if ((cp = webgroup_lookup_context(sp, &id, params, &sts, &msg, arg))) {
	id = cp->origin;
	pmwebapi_context_hash(cp);
	context.source = pmwebapi_hash_sds(NULL, cp->name.hash);
	context.hostspec = cp->host;
	context.labels = cp->labels;

	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: new context %p\n", "pmWebGroupContext", cp);

	sp->callbacks.on_context(id, &context, arg);
	sdsfree(context.source);
    } else {
	id = NULL;
    }

    sp->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
    return sts;
}

void
pmWebGroupDestroy(pmWebGroupSettings *settings, sds id, void *arg)
{
    struct context	*cp;
    struct webgroups	*gp;
    int			sts = 0;
    sds			msg = NULL;

    if (id && /* do not create a new context in this case (i.e. no NULL IDs) */
	(cp = webgroup_lookup_context(settings, &id, NULL,
				      &sts, &msg, arg)) != NULL) {
	gp = settings->module.privdata;

	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: destroy context %p gp=%p\n", "pmWebGroupDestroy", cp, gp);

	webgroup_drop_context(cp, gp);
    }
    sdsfree(msg);
}

static int
ismarker(int c)
{
    return (c == '\r' || c == '\n' || c == '&' || c == ';');
}

static int
webgroup_derived_metrics(context_t *cp, sds config, sds *errmsg)
{
    unsigned int	line = 0;
    char		*p, *end, *name = NULL, *expr, *error;

    (void)cp;

    /*
     * Pick apart the derived metrics "configuration file" in expr.
     * - skip comments and blank lines
     * - split each line on name '=' expression
     * - call pmAddDerivedMetric for each, propogating any errors
     *   (with an additional 'line number' and explanatory notes).
     */
    end = config + sdslen(config);
    for (p = config; p < end; p++) {
	if (isspace((int)(*p))) {
	    if (*p == '\n')
		line++;
	    continue;
	}
	if (*p == '#') {
	    while (!ismarker(*p) && p < end)
	        p++;
	    if (p == end)
		break;
	    line++;
	    continue;
	}

	/* find start and end points of the next metric name */
	name = p;
	while (!isspace((int)(*p)) && *p != '=' && p < end)
	    p++;
	if (p == end)
	    break;
	*p++ = '\0';
	while ((isspace((int)(*p)) && !ismarker(*p)) || *p == '=')
	    p++;

	/* metric name is prepared - move onto the expression */
	expr = p;
	while (!ismarker(*p) && p < end)
	    p++;
	if (p == end)
	    break;
	*p = '\0';

	/* add the derived metric to this context and reset the parsing */
	if (pmAddDerivedMetric(name, expr, &error) < 0) {
	    infofmt(*errmsg, "failed to create derived metric \"%s\""
			" on line %u from: %s\n%s", name, line, expr, error);
	    free(error);
	    return -EINVAL;
	}
	name = expr = NULL;
	line++;
    }

    if (name) {
	/* parsing error - incomplete specification */
	infofmt(*errmsg, "failed to parse derived metric \"%s\""
			" on line %u - incomplete expression\n", name, line);
	return -EINVAL;
    }

    return 0;
}

/*
 * Register a derived metric expression for use with webgroup contexts
 */
extern void
pmWebGroupDerive(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp;
    sds			msg = NULL, expr, metric, message;
    int			sts = 0;

    if (params) {
	metric = dictFetchValue(params, PARAM_MNAME);
	expr = dictFetchValue(params, PARAM_EXPR);
    } else {
	metric = expr = NULL;
    }
    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (expr && !metric) {	/* configuration file mode */
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: register metric group\n", "pmWebGroupDerive");
	sts = webgroup_derived_metrics(cp, expr, &msg);
    } else if (expr && metric) {
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: register metric %s\n", "pmWebGroupDerive",
			    metric);
	if ((sts = pmAddDerivedMetric(metric, expr, &message)) < 0) {
	    infofmt(msg, "%s", message);
	    free(message);
	}
    } else {
	infofmt(msg, "invalid derive parameters");
	sts = -EINVAL;
    }

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

static int
webgroup_fetch_arrays(pmWebGroupSettings *settings, int numnames,
		struct metric ***mplistp, pmID **idlistp, void *arg)
{
    struct metric       **mplist;
    pmID		*idlist;

    if ((mplist = calloc(numnames, sizeof(struct metric *))) == NULL)
	return -ENOMEM;
    if ((idlist = calloc(numnames, sizeof(pmID))) == NULL) {
	free(mplist);
	return -ENOMEM;
    }
    *mplistp = mplist;
    *idlistp = idlist;
    return 0;
}

static sds
webgroup_encode_value(sds value, int type, pmAtomValue *atom)
{
    sdsclear(value);
    switch (type) {
    case PM_TYPE_32:
	return sdscatfmt(value, "%i", atom->l);
    case PM_TYPE_U32:
	return sdscatfmt(value, "%u", atom->ul);
    case PM_TYPE_64:
	return sdscatfmt(value, "%I", atom->ll);
    case PM_TYPE_U64:
	return sdscatfmt(value, "%U", atom->ull);
    case PM_TYPE_FLOAT:
	return sdscatprintf(value, "%.8g", (double)atom->f);
    case PM_TYPE_DOUBLE:
	return sdscatprintf(value, "%.16g", atom->d);

    case PM_TYPE_STRING:
    case PM_TYPE_AGGREGATE:
	if (atom->cp == NULL)
	    return value;
	return sdscatlen(value, atom->cp, sdslen(atom->cp));

    case PM_TYPE_EVENT:
    case PM_TYPE_HIGHRES_EVENT:
    default:
	break;
    }
    return sdscatlen(value, "null", 4);
}

static int
webgroup_fetch(pmWebGroupSettings *settings, context_t *cp,
		int numpmid, struct metric **mplist, pmID *pmidlist,
		sds *message, void *arg)
{
    struct instance	*instance;
    struct metric	*metric;
    struct indom	*indom;
    struct value	*value;
    pmWebResult		webresult;
    pmWebValueSet	webvalueset;
    pmWebValue		webvalue;
    pmResult		*result;
    char		err[PM_MAXERRMSGLEN];
    sds			v = sdsempty(), series = NULL;
    sds			id = cp->origin;
    int			i, j, k, sts, inst, type, status = 0;

    if ((sts = pmFetch(numpmid, pmidlist, &result)) >= 0) {
	webresult.seconds = result->timestamp.tv_sec;
	webresult.nanoseconds = result->timestamp.tv_usec * 1000;

	settings->callbacks.on_fetch(id, &webresult, arg);

	/* extract all values from the result */
	for (i = 0; i < numpmid; i++)
	    if ((metric = mplist[i]) != NULL)
		pmwebapi_add_valueset(mplist[i], result->vset[i]);

	/* for each metric, send fresh values */
	for (i = 0; i < numpmid; i++) {
	    if ((metric = mplist[i]) == NULL)
		continue;
	    type = metric->desc.type;

	    for (j = 0; j < metric->numnames; j++) {
		series = pmwebapi_hash_sds(series, metric->names[j].hash);
		webvalueset.series = series;
		webvalueset.pmid = metric->desc.pmid;
		webvalueset.name = metric->names[j].sds;
		webvalueset.labels = metric->labels;

		settings->callbacks.on_fetch_values(id, &webvalueset, arg);

		webvalue.pmid = metric->desc.pmid;
		if (metric->desc.indom == PM_INDOM_NULL) {
		    v = webgroup_encode_value(v, type, &metric->u.atom);
		    webvalue.series = series;
		    webvalue.inst = PM_IN_NULL;
		    webvalue.value = v;

		    settings->callbacks.on_fetch_value(id, &webvalue, arg);

		    continue;
		}

		indom = metric->indom;
		if (indom->updated == 0) {
		    pmwebapi_add_indom_instances(cp, indom);
		    pmwebapi_add_instances_labels(cp, indom);
		}
		pmwebapi_add_indom_labels(indom);

		if (metric->u.vlist == NULL)
		    continue;

		for (k = 0; k < metric->u.vlist->listcount; k++) {
		    value = &metric->u.vlist->value[k];
		    if (value->updated == 0)
			continue;
		    inst = value->inst;
		    instance = dictFetchValue(indom->insts, &inst);
		    if (instance == NULL) {
			/* found an instance not in existing indom cache */
			indom->updated = 0;	/* invalidate this cache */
			if ((instance = pmwebapi_lookup_instance(indom, inst)))
			    pmwebapi_add_instances_labels(cp, indom);
			else
			    continue;
		    }
		    v = webgroup_encode_value(v, type, &value->atom);
		    series = pmwebapi_hash_sds(series, instance->name.hash);
		    webvalue.series = series;
		    webvalue.inst = inst;
		    webvalue.value = v;

		    settings->callbacks.on_fetch_value(id, &webvalue, arg);
		}
	    }
	}
	pmFreeResult(result);
    } else if (sts == PM_ERR_IPC) {
	cp->setup = 0;
    }

    sdsfree(v);
    sdsfree(series);

    if (sts < 0) {
	infofmt(*message, "%s",pmErrStr_r(sts, err, sizeof(err)));
	status = sts;
    }

    return status;
}

/*
 * Parse possible PMID forms: dotted notation or unsigned integer.
 */
static pmID
webgroup_parse_pmid(const sds name)
{
    unsigned int	cluster, domain, item;
    int			sts;

    if (sdslen(name) > 1 && name[0] == '0' &&
	(name[1] == 'x' || name[1] == 'X') &&
	(sscanf(name, "%x", &item) == 1))
	return item;
    sts = sscanf(name, "%u.%u.%u", &domain, &cluster, &item);
    if (sts == 3)
	return pmID_build(domain, cluster, item);
    if (sts == 1)
	return domain;
    return PM_ID_NULL;
}

static struct metric *
webgroup_lookup_pmid(pmWebGroupSettings *settings, context_t *cp, sds name, void *arg)
{
    struct metric	*mp;
    pmID		pmid;

    if ((pmid = webgroup_parse_pmid(name)) == PM_ID_NULL) {
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: failed to parse PMID %s\n",
			    "webgroup_lookup_pmid", name);
	return NULL;
    }
    if ((mp = (struct metric *)dictFetchValue(cp->pmids, &pmid)) != NULL)
	return mp;
    return pmwebapi_new_pmid(cp, NULL, pmid, settings->module.on_info, arg);
}

static struct metric *
webgroup_lookup_metric(pmWebGroupSettings *settings, context_t *cp, sds name, void *arg)
{
    struct metric	*mp;
    pmID		pmid;
    int			sts;

    if ((mp = dictFetchValue(cp->metrics, name)) != NULL)
	return mp;
    if ((sts = pmLookupName(1, (const char **)&name, &pmid)) < 0) {
	if (sts == PM_ERR_IPC)
	    cp->setup = 0;
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: failed to lookup name %s\n",
			    "webgroup_lookup_metric", name);
	return NULL;
    }
    return pmwebapi_new_pmid(cp, name, pmid, settings->module.on_info, arg);
}

static int
webgroup_fetch_names(pmWebGroupSettings *settings, context_t *cp, int fail,
	int numnames, sds *names, struct metric **mplist, pmID *pmidlist,
	sds *message, void *arg)
{
    struct metric	*metric;
    int			i, sts = 0;

    if (webgroup_use_context(cp, &sts, message, arg) == NULL)
	return sts;

    for (i = 0; i < numnames; i++) {
	metric = mplist[i] = webgroup_lookup_metric(settings, cp, names[i], arg);
	if (metric == NULL && fail)
	    return -EINVAL;
	pmidlist[i] = metric ? metric->desc.pmid : PM_ID_NULL;
    }
    return webgroup_fetch(settings, cp, numnames, mplist, pmidlist, message, arg);
}

static int
webgroup_fetch_pmids(pmWebGroupSettings *settings, context_t *cp, int fail,
	int numpmids, sds *names, struct metric **mplist, pmID *pmidlist,
	sds *message, void *arg)
{
    struct metric	*metric;
    int			i, sts = 0;

    if (webgroup_use_context(cp, &sts, message, arg) == NULL)
	return sts;

    for (i = 0; i < numpmids; i++) {
	metric = mplist[i] = webgroup_lookup_pmid(settings, cp, names[i], arg);
	if (metric == NULL && fail)
	    return -EINVAL;
	pmidlist[i] = metric ? metric->desc.pmid : PM_ID_NULL;
    }

    return webgroup_fetch(settings, cp, numpmids, mplist, pmidlist, message, arg);
}

void
pmWebGroupFetch(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp;
    struct metric	**mplist = NULL;
    size_t		length;
    pmID		*pmidlist = NULL;
    sds			msg = NULL, metrics, pmids = NULL, *names = NULL;
    int			sts = 0, singular = 0, numnames = 0;

    if (params) {
	if ((metrics = dictFetchValue(params, PARAM_MNAMES)) == NULL) {
	    if ((metrics = dictFetchValue(params, PARAM_MNAME)) == NULL) {
		if ((pmids = dictFetchValue(params, PARAM_PMIDS)) == NULL) {
		    if ((pmids = dictFetchValue(params, PARAM_PMID)) != NULL)
			singular = 1;
		}
	    } else {
		singular = 1;
	    }
	}
    } else {
	metrics = NULL;
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    /* handle fetch via metric name list */
    if (metrics) {
	length = sdslen(metrics);
	if ((names = sdssplitlen(metrics, length, ",", 1, &numnames)) == NULL ||
	    webgroup_fetch_arrays(settings,
				numnames, &mplist, &pmidlist, arg) < 0) {
	    infofmt(msg, "out-of-memory on allocation");
	    sts = -ENOMEM;
	} else {
	    if (pmDebugOptions.libweb)
		fprintf(stderr, "%s: fetch %d names (%s,...)\n",
				"pmWebGroupFetch", numnames, names[0]);
	    sts = webgroup_fetch_names(settings, cp, singular,
				numnames, names, mplist, pmidlist, &msg, arg);
	}
    }
    /* handle fetch via numeric/dotted-form PMIDs */
    else if (pmids) {
	length = sdslen(pmids);
	if ((names = sdssplitlen(pmids, length, ",", 1, &numnames)) == NULL ||
	    webgroup_fetch_arrays(settings,
				numnames, &mplist, &pmidlist, arg) < 0) {
	    infofmt(msg, "out-of-memory on allocation");
	    sts = -ENOMEM;
	} else {
	    if (pmDebugOptions.libweb)
		fprintf(stderr, "%s: fetch %d pmids (%s,...)\n",
				"pmWebGroupFetch", numnames, names[0]);
	    sts = webgroup_fetch_pmids(settings, cp, singular,
				numnames, names, mplist, pmidlist, &msg, arg);
	}
    }
    else {
	sts = -EINVAL;
    }

    sdsfreesplitres(names, numnames);
    if (pmidlist)
	free(pmidlist);
    if (mplist)
	free(mplist);

    if (sts < 0 && msg == NULL)
	infofmt(msg, "bad parameters passed");

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

/*
 * Parse possible InDom forms: dotted notation or unsigned integer.
 */
static pmInDom
webgroup_parse_indom(const sds name)
{
    unsigned int	domain, serial;
    int			sts;

    if (sdslen(name) > 1 && name[0] == '0' &&
	(name[1] == 'x' || name[1] == 'X') &&
	(sscanf(name, "%x", &serial) == 1))
	return serial;
    sts = sscanf(name, "%u.%u", &domain, &serial);
    if (sts == 2)
	return pmInDom_build(domain, serial);
    if (sts == 1)
	return domain;
    return PM_INDOM_NULL;
}

static struct indom *
webgroup_cache_indom(struct context *cp, pmInDom indom)
{
    struct domain	*dp;
    struct indom	*ip;

    if ((ip = (struct indom *)dictFetchValue(cp->indoms, &indom)) != NULL)
	return ip;
    dp = pmwebapi_add_domain(cp, pmInDom_domain(indom));
    return pmwebapi_new_indom(cp, dp, indom);
}

static struct indom *
webgroup_lookup_indom(pmWebGroupSettings *settings, context_t *cp, sds name, void *arg)
{
    pmInDom		indom;
    sds			msg;

    if ((indom = webgroup_parse_indom(name)) == PM_INDOM_NULL) {
	infofmt(msg, "failed to parse InDom %s", name);
	moduleinfo(&settings->module, PMLOG_WARNING, msg, arg);
	return NULL;
    }
    return webgroup_cache_indom(cp, indom);
}

static void
name_match_free(regex_t *regex, int numnames)
{
    int		i;

    if (regex) {
	for (i = 0; i < numnames; i++)
	    regfree(&regex[i]);
	free(regex);
    }
}

static regex_t *
name_match_setup(enum matches match, int numnames, sds *names)
{
    regex_t		*regex = NULL;
    int			i, sts = 0;

    if (match == MATCH_REGEX && numnames > 0) {
	if ((regex = calloc(numnames, sizeof(*regex))) == NULL)
	    return NULL;
	for (i = 0; i < numnames; i++)
	    sts |= regcomp(&regex[i], names[i], REG_EXTENDED|REG_NOSUB);
	if (sts) {
	    name_match_free(regex, numnames);
	    return NULL;
	}
    }
    return regex;
}

static int
instance_name_match(struct instance *instance, int numnames, sds *names,
	enum matches match, regex_t *regex)
{
    int		i;

    for (i = 0; i < numnames; i++) {
	if (match == MATCH_EXACT && strcmp(names[i], instance->name.sds) == 0)
	    return 1;
	if (match == MATCH_REGEX &&
	    regexec(&regex[i], instance->name.sds, 0, NULL, 0) == 0)
	    return 1;
	if (match == MATCH_GLOB &&
	    fnmatch(names[i], instance->name.sds, 0) == 0)
	    return 1;
    }
    return 0;
}

static int
instance_id_match(struct instance *instance, int numids, sds *ids)
{
    int		i;

    for (i = 0; i < numids; i++)
	if (instance->inst == atoi(ids[i]))
	    return 1;
    return 0;
}

static int
webgroup_profile(struct context *cp, struct indom *ip,
		enum profile profile, enum matches match,
		sds instnames, sds instids)
{
    struct instance	*instance;
    dictIterator	*iterator;
    dictEntry		*entry;
    pmInDom		indom;
    regex_t		*regex;
    size_t		length;
    sds			*ids = NULL, *names = NULL;
    int			*insts = NULL;
    int			sts, found, count = 0, numids = 0, numnames = 0;

    if (ip == NULL) {
	indom = PM_INDOM_NULL;
	goto profile;
    }
    indom = ip->indom;
    if (instnames) {
	pmwebapi_add_indom_instances(cp, ip);
	length = sdslen(instnames);
	names = sdssplitlen(instnames, length, ",", 1, &numnames);
	insts = calloc(numnames, sizeof(int));
    } else if (instids) {
	pmwebapi_add_indom_instances(cp, ip);
	length = sdslen(instids);
	ids = sdssplitlen(instids, length, ",", 1, &numids);
	insts = calloc(numids, sizeof(int));
    }

    regex = name_match_setup(match, numnames, names);

    iterator = dictGetIterator(ip->insts);
    while (insts && (entry = dictNext(iterator)) != NULL) {
	instance = (instance_t *)dictGetVal(entry);
	if (instance->updated == 0)
	    continue;

	found = 0;
	if (numnames == 0 && numids == 0)
	    found = 1;
	else if (numnames > 0 &&
	    instance_name_match(instance, numnames, names, match, regex))
	    found = 1;
	else if (numids > 0 && instance_id_match(instance, numids, ids))
	    found = 1;
	if (found == 0)
	    continue;

	/* add instance identifier to list */
	insts[count] = instance->inst;
	count++;
    }
    dictReleaseIterator(iterator);

    name_match_free(regex, numnames);
    sdsfreesplitres(names, numnames);
    sdsfreesplitres(ids, numids);

profile:
    sts = (profile == PROFILE_ADD) ?
	    pmAddProfile(indom, count, insts) :
	    pmDelProfile(indom, count, insts);

    if (insts)
	free(insts);
    return sts;
}

/*
 * Manipulate instance domain profile(s) for this web group context
 */
extern void
pmWebGroupProfile(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp = NULL;
    struct metric	*mp;
    struct indom	*ip;
    enum profile	profile = PROFILE_DEL;
    enum matches	matches = MATCH_EXACT;
    char		err[PM_MAXERRMSGLEN];
    sds			expr, match, metric, indomid, inames, instids;
    sds			msg = NULL;
    int			sts = 0;

    if (params) {
	metric = dictFetchValue(params, PARAM_MNAME);
	indomid = dictFetchValue(params, PARAM_INDOM);
	inames = dictFetchValue(params, PARAM_INAME);
	instids = dictFetchValue(params, PARAM_INSTANCE);
	if ((expr = dictFetchValue(params, PARAM_EXPR)) != NULL) {
	    if (strcmp(expr, "add") == 0)
		profile = PROFILE_ADD;
	    else if (strcmp(expr, "del") != 0) {
		infofmt(msg, "%s - invalid 'expr' parameter value", expr);
		sts = -EINVAL;
		goto done;
	    }
	}
	if ((match = dictFetchValue(params, PARAM_MATCH)) != NULL) {
	    if (strcmp(match, "regex") == 0)
		matches = MATCH_REGEX;
	    else if (strcmp(match, "glob") == 0)
		matches = MATCH_GLOB;
	    else if (strcmp(match, "exact") != 0) {
		infofmt(msg, "%s - invalid 'match' parameter value", match);
		sts = -EINVAL;
		goto done;
	    }
	}
    } else {
	expr = indomid = inames = instids = metric = NULL;
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (expr == NULL) {
	infofmt(msg, "invalid profile parameters");
	sts = -EINVAL;
	goto done;
    }
    if (metric) {
	if ((mp = webgroup_lookup_metric(settings, cp, metric, arg)) == NULL) {
	    infofmt(msg, "%s - failed to lookup metric", metric);
	    sts = -EINVAL;
	    goto done;
	}
	if (mp->desc.indom == PM_INDOM_NULL) {
	    infofmt(msg, "%s - metric has null indom", metric);
	    sts = -EINVAL;
	    goto done;
	}
	ip = webgroup_cache_indom(cp, mp->desc.indom);
    }
    else if (indomid == NULL)
	ip = NULL;
    else if ((ip = webgroup_lookup_indom(settings, cp, indomid, arg)) == NULL) {
	infofmt(msg, "invalid profile parameters");
	sts = -EINVAL;
	goto done;
    }

    if (pmDebugOptions.libweb)
	fprintf(stderr, "%s: indom %s profile %s\n",
			"pmWebGroupProfile", ip == NULL? "null" :
			pmInDomStr_r(ip->indom, err, sizeof(err)), expr);

    if ((sts = webgroup_profile(cp, ip, profile, matches, inames, instids)) < 0)
	infofmt(msg, "%s - %s", expr, pmErrStr_r(sts, err, sizeof(err)));

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

void
pmWebGroupChildren(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp;
    pmWebChildren	children = {0};
    char		errmsg[PM_MAXERRMSGLEN];
    char		**offspring;
    sds			msg = NULL, prefix = NULL;
    int			i, l, n, sts = 0, *status;

    if (params) {
	if ((prefix = dictFetchValue(params, PARAM_PREFIX)) == NULL)
	     prefix = dictFetchValue(params, PARAM_MNAME);
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (prefix == NULL || *prefix == '\0')
	prefix = EMPTYSTRING;

    if (pmDebugOptions.libweb)
	fprintf(stderr, "%s: children for prefix \"%s\"\n",
			"pmWebGroupChildren", prefix);

    if ((sts = pmGetChildrenStatus(prefix, &offspring, &status)) < 0) {
	if (sts == PM_ERR_IPC)
	    cp->setup = 0;
	infofmt(msg, "child traversal failed - %s",
		pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    } else {
	children.name = prefix;

	/* zero or more children must be reported from this point onward */
	for (i = 0; i < sts; i++) {
	    if (status[i] == PMNS_NONLEAF_STATUS)
	        children.numnonleaf++;
	    else /* PMNS_LEAF_STATUS */
	        children.numleaf++;
	}
	if (children.numleaf)
	    children.leaf = calloc(children.numleaf, sizeof(sds));
	if (children.numnonleaf)
	    children.nonleaf = calloc(children.numnonleaf, sizeof(sds));
	for (i = l = n = 0; i < sts; i++) {
	    if (status[i] == PMNS_NONLEAF_STATUS)
	        children.nonleaf[n++] = sdsnew(offspring[i]);
	    else /* PMNS_LEAF_STATUS */
	        children.leaf[l++] = sdsnew(offspring[i]);
	}
	if (sts > 0) {
	    free(offspring);
	    sts = 0;
	}

	settings->callbacks.on_children(id, &children, arg);

	/* free up locally allocated array space */
	for (i = 0; i < children.numnonleaf; i++)
	    sdsfree(children.nonleaf[i]);
	if (children.nonleaf)
	    free(children.nonleaf);
	for (i = 0; i < children.numleaf; i++)
	    sdsfree(children.leaf[i]);
	if (children.leaf)
	    free(children.leaf);
    }

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

static void
webgroup_instances(pmWebGroupSettings *settings,
		struct context *cp, struct indom *ip, enum matches match,
		int numnames, sds *instnames, int numids, sds *instids,
		void *arg)
{
    struct instance	*instance;
    pmWebInstance	webinst;
    dictIterator	*iterator;
    dictEntry		*entry;
    regex_t		*regex;
    int			found;

    regex = name_match_setup(match, numnames, instnames);

    iterator = dictGetIterator(ip->insts);
    while ((entry = dictNext(iterator)) != NULL) {
	instance = (instance_t *)dictGetVal(entry);
	if (instance->updated == 0)
	    continue;

	found = 0;
	if (numnames == 0 && numids == 0)
	    found = 1;
	else if (numnames > 0 &&
	    instance_name_match(instance, numnames, instnames, match, regex))
	    found = 1;
	else if (numids > 0 && instance_id_match(instance, numids, instids))
	    found = 1;
	if (found == 0)
	    continue;

	webinst.indom = ip->indom;
	webinst.inst = instance->inst;
	webinst.name = instance->name.sds;
	webinst.labels = instance->labels;

	settings->callbacks.on_instance(cp->origin, &webinst, arg);
    }
    dictReleaseIterator(iterator);

    name_match_free(regex, numnames);
}

void
pmWebGroupInDom(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp = NULL;
    struct domain	*dp;
    struct metric	*mp;
    struct indom	*ip;
    enum matches	matches = MATCH_EXACT;
    pmWebInDom		webindom;
    pmInDom		indom;
    size_t		length;
    sds			msg = NULL, match, metric, indomid, instids, instnames;
    sds			*ids = NULL, *names = NULL;
    int			sts = 0, count = 0, numids = 0, numnames = 0;

    if (params) {
	metric = dictFetchValue(params, PARAM_MNAME);
	indomid = dictFetchValue(params, PARAM_INDOM);
	instids = dictFetchValue(params, PARAM_INSTANCE);
	instnames = dictFetchValue(params, PARAM_INAME);
	if ((match = dictFetchValue(params, PARAM_MATCH)) != NULL) {
	    if (strcmp(match, "regex") == 0)
		matches = MATCH_REGEX;
	    else if (strcmp(match, "glob") == 0)
		matches = MATCH_GLOB;
	    else if (strcmp(match, "exact") != 0) {
		infofmt(msg, "%s - invalid 'match' parameter value", match);
		sts = -EINVAL;
		goto done;
	    }
	    msg = NULL;
	}
    } else {
	metric = indomid = instids = instnames = NULL;
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (pmDebugOptions.libweb)
	fprintf(stderr, "%s: %s indom\n",
			"pmWebGroupInDom", metric? metric : indomid);

    if (metric) {
	if ((mp = webgroup_lookup_metric(settings, cp, metric, arg)) == NULL) {
	    infofmt(msg, "%s - failed to lookup metric", metric);
	    sts = -EINVAL;
	    goto done;
	}
	if (mp->desc.indom == PM_INDOM_NULL) {
	    infofmt(msg, "%s - metric has null indom", metric);
	    sts = -EINVAL;
	    goto done;
	}
	indom = mp->desc.indom;
    } else if (indomid) {
	if ((ip = webgroup_lookup_indom(settings, cp, indomid, arg)) == NULL) {
	    infofmt(msg, "failed to lookup indom %s", indomid);
	    sts = -EINVAL;
	    goto done;
	}
	indom = ip->indom;
    } else {
	infofmt(msg, "bad indom parameters");
	sts = -EINVAL;
	goto done;
    }

    dp = pmwebapi_add_domain(cp, pmInDom_domain(indom));
    if ((ip = pmwebapi_add_indom(cp, dp, indom)) == NULL) {
	infofmt(msg, "failed to add indom");
	sts = -EINVAL;
	goto done;
    }
    count = pmwebapi_add_indom_instances(cp, ip);
    pmwebapi_add_instances_labels(cp, ip);
    pmwebapi_add_indom_labels(ip);
    pmwebapi_indom_help(cp, ip);

    if (instnames) {
	length = sdslen(instnames);
	names = sdssplitlen(instnames, length, ",", 1, &numnames);
    } else if (instids) {
	length = sdslen(instids);
	ids = sdssplitlen(instids, length, ",", 1, &numids);
    }

    webindom.indom = ip->indom;
    webindom.labels = ip->labels;
    webindom.oneline = ip->oneline;
    webindom.helptext = ip->helptext;
    webindom.numinsts = count;
    settings->callbacks.on_indom(id, &webindom, arg);

    webgroup_instances(settings, cp, ip, matches,
				numnames, names, numids, ids, arg);

    sdsfreesplitres(names, numnames);
    sdsfreesplitres(ids, numids);

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

seriesname_t *
webgroup_lookup_series(int numnames, seriesname_t *names, const char *name)
{
    size_t		length = strlen(name);
    int			i;

    /* find the given name in an array of names */
    for (i = 0; i < numnames; i++) {
	if (length != sdslen(names[i].sds))
	    continue;
	if (strncmp(name, names[i].sds, length) == 0)
	    return &names[i];
    }
    return NULL;
}

typedef struct weblookup {
    pmWebGroupSettings	*settings;
    struct context	*context;
    pmWebMetric		metric;	/* keep buffers to reduce memory allocations */
    int			status;
    sds			message;
    void		*arg;
} weblookup_t;

/* Metric namespace failure callback for use after pmTraversePMNS_r(3) */
static void
badname(const char *name, const char *errmsg, struct weblookup *lookup)
{
    pmWebGroupSettings	*settings = lookup->settings;
    pmWebMetric		*metric = &lookup->metric;
    context_t		*cp = lookup->context;
    void		*arg = lookup->arg;

    /* clear buffer contents from any previous call(s) */
    sdsclear(metric->series);
    sdsclear(metric->name);
    sdsclear(metric->sem);
    sdsclear(metric->type);
    sdsclear(metric->units);
    sdsclear(metric->labels);
    sdsclear(metric->oneline);
    sdsclear(metric->helptext);

    /* inform caller (callback) about failure via ID_NULL and oneline */
    metric->pmid = PM_ID_NULL;
    metric->indom = PM_INDOM_NULL;
    metric->name = sdscat(metric->name, name);
    metric->oneline = sdscat(metric->oneline, errmsg);

    settings->callbacks.on_metric(cp->origin, metric, arg);
}

/* Metric namespace traversal callback for use with pmTraversePMNS_r(3) */
static void
webmetric_lookup(const char *name, void *arg)
{
    struct weblookup	*lookup = (struct weblookup *)arg;
    pmWebGroupSettings	*settings = lookup->settings;
    pmWebMetric		*metric = &lookup->metric;
    seriesname_t	*snp = NULL;
    context_t		*cp = lookup->context;
    metric_t		*mp;
    indom_t		*ip;

    if (webgroup_use_context(cp, &lookup->status, &lookup->message, arg) == NULL)
	return;

    /* make sure we use the original caller supplied arg now */
    arg = lookup->arg;

    /* clear buffer contents from any previous call(s) */
    sdsclear(metric->name);
    sdsclear(metric->sem);
    sdsclear(metric->type);
    sdsclear(metric->units);
    sdsclear(metric->labels);
    sdsclear(metric->oneline);
    sdsclear(metric->helptext);

    metric->name = sdscat(metric->name, name);
    mp = webgroup_lookup_metric(settings, cp, metric->name, arg);
    if (mp == NULL) {
	badname(name, "failed to lookup metric name", lookup);
	return;
    }
    snp = webgroup_lookup_series(mp->numnames, mp->names, name);
    if (snp == NULL)	/* a 'redirect' - pick the first series */
	snp = &mp->names[0];

    pmwebapi_add_domain_labels(cp, mp->cluster->domain);
    pmwebapi_add_cluster_labels(cp, mp->cluster);
    if ((ip = mp->indom) != NULL) {
	if (pmwebapi_add_indom_instances(cp, ip) > 0)
	    pmwebapi_add_instances_labels(cp, ip);
	pmwebapi_add_indom_labels(ip);
    }
    pmwebapi_add_item_labels(cp, mp);
    pmwebapi_metric_hash(mp);
    pmwebapi_metric_help(cp, mp);

    metric->pmid = mp->desc.pmid;
    metric->indom = mp->desc.indom;
    pmwebapi_hash_str(snp->hash, metric->series, 42);
    sdssetlen(metric->series, 40);
    pmwebapi_semantics_str(mp, metric->sem, 20);
    pmwebapi_units_str(mp, metric->units, 64);
    pmwebapi_type_str(mp, metric->type, 20);
    sdsupdatelen(metric->units);
    sdsupdatelen(metric->type);
    sdsupdatelen(metric->sem);
    if (mp->labels)
	metric->labels = sdscatsds(metric->labels, mp->labels);
    if (mp->oneline)
	metric->oneline = sdscatsds(metric->oneline, mp->oneline);
    if (mp->helptext)
	metric->helptext = sdscatsds(metric->helptext, mp->helptext);

    settings->callbacks.on_metric(cp->origin, metric, arg);
}

void
pmWebGroupMetric(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct weblookup	lookup = {0};
    struct context	*cp;
    pmWebMetric		*metric = &lookup.metric;
    size_t		length;
    char		errmsg[PM_MAXERRMSGLEN], *error;
    sds			msg = NULL, prefix = NULL, *names = NULL;
    int			i, sts = 0, numnames = 0;

    if (params) {
	if ((prefix = dictFetchValue(params, PARAM_PREFIX)) == NULL &&
	    (prefix = dictFetchValue(params, PARAM_MNAMES)) == NULL)
	     prefix = dictFetchValue(params, PARAM_MNAME);
	if (prefix) {
	    length = sdslen(prefix);
	    names = sdssplitlen(prefix, length, ",", 1, &numnames);
	}
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (pmDebugOptions.libweb)
	fprintf(stderr, "%s: metadata for %d metrics (%s,...)\n",
			"pmWebGroupMetric", prefix ? numnames : 0,
			names? names[0] : "(all)");

    lookup.settings = settings;
    lookup.context = cp;
    lookup.arg = arg;

    /* allocate zero'd space for strings */
    metric->series = sdsnewlen(NULL, 42); sdsclear(metric->series);
    metric->name = sdsnewlen(NULL, 16); sdsclear(metric->name);
    metric->sem = sdsnewlen(NULL, 20); sdsclear(metric->sem);
    metric->type = sdsnewlen(NULL, 20); sdsclear(metric->type);
    metric->units = sdsnewlen(NULL, 64); sdsclear(metric->units);
    metric->labels = sdsnewlen(NULL, 128); sdsclear(metric->labels);
    metric->oneline = sdsnewlen(NULL, 128); sdsclear(metric->oneline);
    metric->helptext = sdsnewlen(NULL, 128); sdsclear(metric->helptext);

    if (prefix == NULL || *prefix == '\0') {
	sts = pmTraversePMNS_r("", webmetric_lookup, &lookup);
	if (sts >= 0) {
	    msg = lookup.message;
	    sts = (lookup.status < 0) ? lookup.status : 0;
	} else {
	    if (sts == PM_ERR_IPC)
		cp->setup = 0;
	    infofmt(msg, "namespace traversal failed - %s",
			    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }
    for (i = 0; i < numnames; i++) {
	sts = pmTraversePMNS_r(names[i], webmetric_lookup, &lookup);
	if (sts >= 0) {
	    if (numnames != 1) {	/* already started with response */
		sts = 0;
		continue;
	    }
	    msg = lookup.message;
	    if ((sts = (lookup.status < 0) ? lookup.status : 0) < 0)
		break;
	} else {
	    if (sts == PM_ERR_IPC)
		cp->setup = 0;
	    error = pmErrStr_r(sts, errmsg, sizeof(errmsg));
	    if (numnames != 1) {
		badname(names[i], error, &lookup);
		sts = 0;
	    } else {
		infofmt(msg, "%s traversal failed - %s", names[i], error);
		break;
	    }
	}
    }

    sdsfree(metric->series);
    sdsfree(metric->name);
    sdsfree(metric->sem);
    sdsfree(metric->type);
    sdsfree(metric->units);
    sdsfree(metric->labels);
    sdsfree(metric->oneline);
    sdsfree(metric->helptext);

done:
    sdsfreesplitres(names, numnames);
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

static void
scrape_metric_labelsets(metric_t *metric, pmWebLabelSet *labels)
{
    cluster_t	*cluster = metric->cluster;
    domain_t	*domain = cluster->domain;
    context_t	*context = domain->context;
    indom_t	*indom = metric->indom;
    int		nsets = 0;

    if (context->labelset)
	labels->sets[nsets++] = context->labelset;
    if (domain->labelset)
	labels->sets[nsets++] = domain->labelset;
    if (indom && indom->labelset)
	labels->sets[nsets++] = indom->labelset;
    if (cluster->labelset)
	labels->sets[nsets++] = cluster->labelset;
    if (metric->labelset)
	labels->sets[nsets++] = metric->labelset;
    labels->nsets = nsets;
    sdsclear(labels->buffer);
    labels->instid = PM_IN_NULL;
    labels->instname = NULL;
}

static void
scrape_instance_labelsets(metric_t *metric, indom_t *indom, instance_t *inst,
		pmWebLabelSet *labels)
{
    domain_t	*domain = indom->domain;
    context_t	*context = domain->context;
    cluster_t	*cluster = metric->cluster;
    int		nsets = 0;

    if (context->labelset)
	labels->sets[nsets++] = context->labelset;
    if (domain->labelset)
	labels->sets[nsets++] = domain->labelset;
    if (indom->labelset)
	labels->sets[nsets++] = indom->labelset;
    if (cluster->labelset)
	labels->sets[nsets++] = cluster->labelset;
    if (metric->labelset)
	labels->sets[nsets++] = metric->labelset;
    if (inst->labelset)
	labels->sets[nsets++] = inst->labelset;
    labels->nsets = nsets;
    sdsclear(labels->buffer);
    labels->instid = inst->inst;
    labels->instname = inst->name.sds;
}

static int
webgroup_scrape(pmWebGroupSettings *settings, context_t *cp,
		int numpmid, struct metric **mplist, pmID *pmidlist,
		sds *msg, void *arg)
{
    struct instance	*instance;
    struct metric	*metric;
    struct indom	*indom;
    struct value	*value;
    pmWebLabelSet	labels;
    pmWebScrape		scrape;
    pmResult		*result;
    sds			sems, types, units;
    sds			v = sdsempty(), series = NULL;
    int			i, j, k, sts, type;

    /* pre-allocate buffers for metric metadata */
    sems = sdsnewlen(NULL, 20); sdsclear(sems);
    types = sdsnewlen(NULL, 20); sdsclear(types);
    units = sdsnewlen(NULL, 64); sdsclear(units);
    labels.buffer = sdsnewlen(NULL, PM_MAXLABELJSONLEN);
    sdsclear(labels.buffer);

    if ((sts = pmFetch(numpmid, pmidlist, &result)) >= 0) {
	scrape.seconds = result->timestamp.tv_sec;
	scrape.nanoseconds = result->timestamp.tv_usec * 1000;

	/* extract all values from the result for later stages */
	for (i = 0; i < numpmid; i++)
	    if ((metric = mplist[i]) != NULL)
		pmwebapi_add_valueset(metric, result->vset[i]);

	/* for each metric, send all metadata and fresh values */
	for (i = 0; i < numpmid; i++) {
	    if ((metric = mplist[i]) == NULL)
		continue;

	    if (metric->updated == 0)
		continue;
	    if (metric->labelset == NULL)
		pmwebapi_add_item_labels(cp, metric);
	    pmwebapi_metric_help(cp, metric);

	    type = metric->desc.type;
	    indom = metric->indom;
	    if (indom && indom->updated == 0 &&
		pmwebapi_add_indom_instances(cp, indom) > 0)
		pmwebapi_add_instances_labels(cp, indom);

	    for (j = 0; j < metric->numnames; j++) {
		series = pmwebapi_hash_sds(series, metric->names[j].hash);
		scrape.metric.series = series;
		scrape.metric.name = metric->names[j].sds;
		scrape.metric.pmid = metric->desc.pmid;
		scrape.metric.indom = metric->desc.indom;

		pmwebapi_semantics_str(metric, sems, 20);
		scrape.metric.sem = sems;
		sdsupdatelen(sems);
		pmwebapi_type_str(metric, types, 20);
		scrape.metric.type = types;
		sdsupdatelen(types);
		pmwebapi_units_str(metric, units, 64);
		scrape.metric.units = units;
		sdsupdatelen(units);
		scrape.metric.labels = NULL;
		scrape.metric.oneline = metric->oneline;
		scrape.metric.helptext = metric->helptext;

		if (metric->desc.indom == PM_INDOM_NULL || metric->u.vlist == NULL) {
		    v = webgroup_encode_value(v, type, &metric->u.atom);
		    scrape.value.series = series;
		    scrape.value.inst = PM_IN_NULL;
		    scrape.value.value = v;
		    memset(&scrape.instance, 0, sizeof(scrape.instance));
		    scrape.instance.inst = PM_IN_NULL;

		    if (metric->labels == NULL)
			pmwebapi_metric_hash(metric);
		    scrape_metric_labelsets(metric, &labels);
		    if (settings->callbacks.on_scrape_labels)
			settings->callbacks.on_scrape_labels(
					cp->origin, &labels, arg);
		    scrape.metric.labels = labels.buffer;

		    settings->callbacks.on_scrape(cp->origin, &scrape, arg);
		    continue;
		}
		for (k = 0; k < metric->u.vlist->listcount; k++) {
		    value = &metric->u.vlist->value[k];
		    if (value->updated == 0 || indom == NULL)
			continue;
		    instance = dictFetchValue(indom->insts, &value->inst);
		    if (instance == NULL)
			continue;
		    v = webgroup_encode_value(v, type, &value->atom);
		    series = pmwebapi_hash_sds(series, instance->name.hash);
		    scrape.value.series = series;
		    scrape.value.inst = value->inst;
		    scrape.value.value = v;
		    scrape.instance.inst = instance->inst;
		    scrape.instance.name = instance->name.sds;

		    if (instance->labels == NULL)
			pmwebapi_instance_hash(indom, instance);
		    scrape_instance_labelsets(metric, indom, instance, &labels);
		    if (settings->callbacks.on_scrape_labels)
			settings->callbacks.on_scrape_labels(
					cp->origin, &labels, arg);
		    scrape.instance.labels = labels.buffer;

		    settings->callbacks.on_scrape(cp->origin, &scrape, arg);
		}
	    }
	}
	pmFreeResult(result);
    } else {
	char		err[PM_MAXERRMSGLEN];

	if (sts == PM_ERR_IPC)
	    cp->setup = 0;

	infofmt(*msg, "%s", pmErrStr_r(sts, err, sizeof(err)));
    }

    sdsfree(v);
    sdsfree(sems);
    sdsfree(types);
    sdsfree(units);
    sdsfree(series);
    sdsfree(labels.buffer);

    return sts < 0 ? sts : 0;
}

static int
webgroup_scrape_names(pmWebGroupSettings *settings, context_t *cp,
	int numnames, sds *names, struct metric **mplist, pmID *pmidlist,
	sds *msg, void *arg)
{
    struct metric	*metric;
    int			i, sts = 0;

    if (webgroup_use_context(cp, &sts, msg, arg) == NULL)
	return sts;

    for (i = 0; i < numnames; i++) {
	metric = mplist[i] = webgroup_lookup_metric(settings, cp, names[i], arg);
	pmidlist[i] = metric ? metric->desc.pmid : PM_ID_NULL;
    }
    return webgroup_scrape(settings, cp, numnames, mplist, pmidlist, msg, arg);
}

typedef struct webscrape {
    pmWebGroupSettings	*settings;
    struct context	*context;
    sds			*msg;
    int			status;
    unsigned int	numnames;	/* current count of metric names */
    sds			*names;		/* metric names for batched up scrape */
    struct metric	**mplist;
    pmID		*pmidlist;
    void		*arg;
} webscrape_t;

/* Metric namespace traversal callback for use with pmTraversePMNS_r(3) */
static void
webgroup_scrape_batch(const char *name, void *arg)
{
    struct webscrape	*scrape = (struct webscrape *)arg;
    int			i, sts;

    if (scrape->names == NULL) {
	scrape->names = calloc(DEFAULT_BATCHSIZE, sizeof(sds));
	scrape->mplist = calloc(DEFAULT_BATCHSIZE, sizeof(metric_t *));
	scrape->pmidlist = calloc(DEFAULT_BATCHSIZE, sizeof(pmID));
	scrape->numnames = 0;
    }

    scrape->names[scrape->numnames] = sdsnew(name);
    scrape->numnames++;

    if (scrape->numnames == DEFAULT_BATCHSIZE) {
	sts = webgroup_scrape_names(scrape->settings, scrape->context,
			scrape->numnames, scrape->names,
			scrape->mplist, scrape->pmidlist,
			scrape->msg, scrape->arg);
	for (i = 0; i < scrape->numnames; i++)
	    sdsfree(scrape->names[i]);
	scrape->numnames = 0;
	if (sts < 0)
	    scrape->status = sts;
    }
}

static int
webgroup_scrape_tree(const char *prefix, struct webscrape *scrape)
{
    int			i, sts;
    char		err[PM_MAXERRMSGLEN];

    if (pmDebugOptions.libweb)
	fprintf(stderr, "%s: scraping namespace prefix \"%s\"\n",
			"pmWebGroupScrape", prefix);

    sts = pmTraversePMNS_r(prefix, webgroup_scrape_batch, scrape);
    if (sts >= 0 && scrape->status >= 0 && scrape->numnames) {
	/* complete any remaining (sub-batchsize) leftovers */
	sts = webgroup_scrape_names(scrape->settings, scrape->context,
				    scrape->numnames, scrape->names,
				    scrape->mplist, scrape->pmidlist,
				    scrape->msg, scrape->arg);
	for (i = 0; i < scrape->numnames; i++)
	    sdsfree(scrape->names[i]);
	scrape->numnames = 0;
    } else {
	infofmt(*scrape->msg, "'%s' - %s", prefix,
		pmErrStr_r(sts, err, sizeof(err)));
    }

    if (sts >= 0)
	sts = (scrape->status < 0) ? scrape->status : 0;
    return sts;
}

void
pmWebGroupScrape(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct webscrape	scrape = {0};
    struct context	*cp;
    size_t		length;
    int			sts = 0, i, numnames = 0;
    sds			msg = NULL, *names = NULL, metrics;

    if (params) {
	if ((metrics = dictFetchValue(params, PARAM_MNAMES)) == NULL)
	     if ((metrics = dictFetchValue(params, PARAM_MNAME)) == NULL)
		if ((metrics = dictFetchValue(params, PARAM_PREFIX)) == NULL)
		    metrics = dictFetchValue(params, PARAM_TARGET);
    } else {
	metrics = NULL;
    }

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    scrape.settings = settings;
    scrape.context = cp;
    scrape.msg = &msg;
    scrape.arg = arg;

    /* handle scrape via metric name list traversal (else entire namespace) */
    if (metrics && sdslen(metrics)) {
	length = sdslen(metrics);
	if ((names = sdssplitlen(metrics, length, ",", 1, &numnames)) == NULL)
	    sts = webgroup_scrape_tree("", &scrape);
	for (i = 0; i < numnames; i++)
	    sts = webgroup_scrape_tree(names[i], &scrape);
	sdsfreesplitres(names, numnames);
    } else {
	sts = webgroup_scrape_tree("", &scrape);
    }

    if (scrape.names) {
	free(scrape.names);
	free(scrape.mplist);
	free(scrape.pmidlist);
    }

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

struct instore {
    pmAtomValue		*atom;
    pmValueSet		*vset;
    sds			*names;
    int			*insts;
    unsigned int	numnames;
    unsigned int	count;
    unsigned int	maximum;
    unsigned int	type;
    pmInDom		indom;
    int			status;
};

#define STORE_INST_MAX	100000	/* cap number of storable instances */

static int
store_add_instid(struct instore *store, int id)
{
    int		*insts, count = store->count++;

    if ((store->count >= STORE_INST_MAX) ||
	(insts = realloc(store->insts, store->count * sizeof(int))) == NULL) {
	store->status = -E2BIG;
	store->count = count;	/* reset */
	return count;
    }
    store->insts = insts;
    store->insts[count] = id;
    return count;	/* return current array index */
}

static void
store_add_profile(struct instore *store)
{
    int		sts;

    if (store->insts) {
	sts = pmDelProfile(store->indom, 0, NULL);
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "pmDelProfile: sts=%d\n", sts);
	sts = pmAddProfile(store->indom, store->count, store->insts);
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "pmAddProfile: sts=%d\n", sts);
	free(store->insts);
	store->insts = NULL;
    }
}

static void
store_found_insts(void *arg, const struct dictEntry *entry)
{
    struct instore	*store = (struct instore *)arg;
    pmValue		*value;
    int			i, id, sts;

    if (store->count < store->maximum) {
	id = *(int *)entry->key;
	i = store_add_instid(store, id);
	value = &store->vset->vlist[i];
	if ((sts = __pmStuffValue(store->atom, value, store->type)) < 0)
	    store->status = sts;
	value->inst = id;
    } else {
	store->status = -E2BIG;
    }
}

static void
store_named_insts(void *arg, const struct dictEntry *entry)
{
    struct instore	*store = (struct instore *)arg;
    struct instance	*instance = (instance_t *)dictGetVal(entry);
    int			i;

    for (i = 0; i < store->numnames; i++)
	if (sdscmp(instance->name.sds, store->names[i]) == 0)
	    break;
    if (i != store->numnames)
	store_found_insts(arg, entry);
}

static int
webgroup_store(struct context *context, struct metric *metric,
		int numnames, sds *names,
		int numids, sds *ids, sds value)
{
    struct instore	store = {0};
    struct indom	*indom;
    pmAtomValue		atom = {0};
    pmValueSet		*valueset = NULL;
    pmResult		*result = NULL;
    size_t		bytes;
    long		cursor = 0;
    int			i, id, sts, count;

    if ((sts = __pmStringValue(value, &atom, metric->desc.type)) < 0)
	return sts;

    if ((indom = metric->indom) != NULL &&
	(indom->updated == 0))
	pmwebapi_add_indom_instances(context, indom);

    if (metric->desc.indom == PM_INDOM_NULL)
	count = 1;
    else if (numids > 0)
	count = numids;
    else if (numnames > 0)
	count = numnames;
    else
	count = 1;

    bytes = sizeof(pmValueSet) + sizeof(pmValue) * (count - 1);
    if ((result = (pmResult *)calloc(1, sizeof(pmResult))) == NULL ||
	(valueset = (pmValueSet *)calloc(1, bytes)) == NULL) {
	if (atom.cp && metric->desc.type == PM_TYPE_STRING)
	    free(atom.cp);
	if (result)
	    free(result);
	return -ENOMEM;
    }
    result->vset[0] = valueset;
    result->numpmid = 1;

    if (metric->desc.indom == PM_INDOM_NULL || indom == NULL) {
	valueset->vlist[0].inst = PM_IN_NULL;
        sts = __pmStuffValue(&atom, &valueset->vlist[0], metric->desc.type);
    } else if (numids > 0) {
	store.indom = metric->desc.indom;
	for (i = 0; i < numids && sts >= 0; i++) {
	    id = atoi(ids[i]);
	    store_add_instid(&store, id);
	    valueset->vlist[i].inst = id;
	    sts = __pmStuffValue(&atom, &valueset->vlist[i], metric->desc.type);
	    if (sts < 0)
		store.status = sts;
	}
	store_add_profile(&store);
	sts = store.status;
    } else if (numnames > 0) {
	/* walk instances dictionary adding named instances */
	store.atom = &atom;
	store.vset = valueset;
	store.type = metric->desc.type;
	store.indom = metric->desc.indom;
	store.names = names;
	store.maximum = count;
	store.numnames = numnames;
	do {
	    cursor = dictScan(indom->insts, cursor,
				store_named_insts, NULL, &store);
	} while (cursor && store.status >= 0);
	store_add_profile(&store);
	sts = store.status;
    } else {
	valueset->vlist[0].inst = PM_IN_NULL;
        sts = __pmStuffValue(&atom, &valueset->vlist[0], metric->desc.type);
    }
    if (atom.cp && metric->desc.type == PM_TYPE_STRING)
	free(atom.cp);
    if (sts >= 0) {
	valueset->valfmt = sts;
	valueset->numval = count;
	valueset->pmid = metric->desc.pmid;
	sts = pmStore(result);
    }
    pmFreeResult(result);
    return sts;
}

void
pmWebGroupStore(pmWebGroupSettings *settings, sds id, dict *params, void *arg)
{
    struct context	*cp;
    struct metric	*mp;
    size_t		length;
    char		err[PM_MAXERRMSGLEN];
    sds			metric, value, pmid, instids, instnames;
    sds			msg = NULL, *names = NULL, *ids = NULL;
    int			sts = 0, numids = 0, numnames = 0;

    if (params) {
	metric = dictFetchValue(params, PARAM_MNAME);
	value = dictFetchValue(params, PARAM_MVALUE);
	pmid = dictFetchValue(params, PARAM_PMID);
	instids = dictFetchValue(params, PARAM_INSTANCE);
	instnames = dictFetchValue(params, PARAM_INAME);
    } else {
	metric = value = pmid = instids = instnames = NULL;
    }

    if (value == NULL)
	value = EMPTYSTRING;

    if (!(cp = webgroup_lookup_context(settings, &id, params, &sts, &msg, arg)))
	goto done;
    id = cp->origin;

    if (instnames) {
	length = sdslen(instnames);
	names = sdssplitlen(instnames, length, ",", 1, &numnames);
    } else if (instids) {
	length = sdslen(instids);
	ids = sdssplitlen(instids, length, ",", 1, &numids);
    }

    /* handle store via metric name list */
    if (metric) {
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: by metric %s\n", "pmWebGroupStore", metric);

	if ((mp = webgroup_lookup_metric(settings, cp, metric, arg)) == NULL) {
	    infofmt(msg, "%s - failed to lookup metric", metric);
	    sts = -EINVAL;
	}
    }
    /* handle store via numeric/dotted-form PMIDs */
    else if (pmid) {
	if (pmDebugOptions.libweb)
	    fprintf(stderr, "%s: by pmid %s\n", "pmWebGroupStore", pmid);

	if ((mp = webgroup_lookup_pmid(settings, cp, pmid, arg)) == NULL) {
	    infofmt(msg, "%s - failed to lookup PMID", pmid);
	    sts = -EINVAL;
	}
    } else {
	infofmt(msg, "bad parameters passed");
	sts = -EINVAL;
    }

    if ((sts >= 0) &&
	(sts = webgroup_store(cp, mp, numnames, names, numids, ids, value)) < 0) {
	infofmt(msg, "failed to store value to metric %s: %s",
		metric ? metric : pmid, pmErrStr_r(sts, err, sizeof(err)));
    }

    sdsfreesplitres(names, numnames);
    sdsfreesplitres(ids, numids);

done:
    settings->callbacks.on_done(id, sts, msg, arg);
    webgroup_deref_context(cp);
    sdsfree(msg);
}

int
pmWebGroupSetup(pmWebGroupModule *module)
{
    struct webgroups	*groups = webgroups_lookup(module);
    struct timeval	tv;
    unsigned int	pid;

    if (groups == NULL)
	return -ENOMEM;

    /* allocate strings for parameter dictionary key lookups */
    PARAM_HOSTNAME = sdsnew("hostname");
    PARAM_HOSTSPEC = sdsnew("hostspec");
    PARAM_CTXNUM = sdsnew("context");
    PARAM_CTXID = sdsnew("contextid");
    PARAM_POLLTIME = sdsnew("polltimeout");
    PARAM_PREFIX = sdsnew("prefix");
    PARAM_MNAMES = sdsnew("names");
    PARAM_PMIDS = sdsnew("pmids");
    PARAM_PMID = sdsnew("pmid");
    PARAM_INDOM = sdsnew("indom");
    PARAM_MNAME = sdsnew("name");
    PARAM_INSTANCE = sdsnew("instance");
    PARAM_INAME = sdsnew("iname");
    PARAM_MVALUE = sdsnew("value");
    PARAM_TARGET = sdsnew("target");
    PARAM_EXPR = sdsnew("expr");
    PARAM_MATCH = sdsnew("match");

    /* generally needed strings, error messages */
    EMPTYSTRING = sdsnew("");
    LOCALHOST = sdsnew("localhost");
    WORK_TIMER = sdsnew("pmwebapi.work");
    POLL_TIMEOUT = sdsnew("pmwebapi.timeout");
    BATCHSIZE = sdsnew("pmwebapi.batchsize");
    AUTH_USERNAME = sdsnew("auth.username");
    AUTH_PASSWORD = sdsnew("auth.password");

    /* setup the random number generator for context IDs */
    gettimeofday(&tv, NULL);
    pid = (unsigned int)getpid();
    srandom(pid ^ (unsigned int)tv.tv_sec ^ (unsigned int)tv.tv_usec);

    /* setup a dictionary mapping context number to data */
    groups->contexts = dictCreate(&intKeyDictCallBacks, NULL);

    return 0;
}

int
pmWebGroupSetEventLoop(pmWebGroupModule *module, void *events)
{
    struct webgroups	*groups = webgroups_lookup(module);

    if (groups) {
	groups->events = (uv_loop_t *)events;
	return 0;
    }
    return -ENOMEM;
}

int
pmWebGroupSetConfiguration(pmWebGroupModule *module, dict *config)
{
    struct webgroups	*groups = webgroups_lookup(module);
    char		*endnum;
    sds			value;

    if ((value = dictFetchValue(config, WORK_TIMER)) == NULL) {
	default_worker = DEFAULT_WORK_TIMER;
    } else {
	default_worker = strtoul(value, &endnum, 0);
	if (*endnum != '\0')
	    default_worker = DEFAULT_WORK_TIMER;
    }

    if ((value = dictFetchValue(config, POLL_TIMEOUT)) == NULL) {
	default_timeout = DEFAULT_POLL_TIMEOUT;
    } else {
	default_timeout = strtoul(value, &endnum, 0);
	if (*endnum != '\0')
	    default_timeout = DEFAULT_POLL_TIMEOUT;
    }

    if ((value = dictFetchValue(config, BATCHSIZE)) == NULL) {
	default_batchsize = DEFAULT_BATCHSIZE;
    } else {
	default_batchsize = strtoul(value, &endnum, 0);
	if (*endnum != '\0')
	    default_batchsize = DEFAULT_BATCHSIZE;
    }

    if (groups) {
	groups->config = config;
	return 0;
    }
    return -ENOMEM;
}

static void
pmWebGroupSetupMetrics(pmWebGroupModule *module)
{
    struct webgroups	*groups = webgroups_lookup(module);
    pmAtomValue		**ap;
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    void		*map;

    if (groups == NULL || groups->registry == NULL)
	return; /* no metric registry has been set up */

    /*
     * Reverse mapping dict metrics
     */
    mmv_stats_add_metric(groups->registry, "contextmap.size", 1,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"context map dictionary size",
	"Number of entries in the context map dictionary");

    mmv_stats_add_metric(groups->registry, "namesmap.size", 2,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"metric names map dictionary size",
	"Number of entries in the metric names map dictionary");

    mmv_stats_add_metric(groups->registry, "labelsmap.size", 3,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"labels map dictionary size",
	"Number of entries in the labels map dictionary");

    mmv_stats_add_metric(groups->registry, "instmap.size", 4,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"instance name map dictionary size",
	"Number of entries in the instance name map dictionary");

    groups->map = map = mmv_stats_start(groups->registry);

    ap = groups->metrics;
    ap[CONTEXT_MAP_SIZE] = mmv_lookup_value_desc(map, "contextmap.size", NULL);
    ap[NAMES_MAP_SIZE] = mmv_lookup_value_desc(map, "namesmap.size", NULL);
    ap[LABELS_MAP_SIZE] = mmv_lookup_value_desc(map, "labelsmap.size", NULL);
    ap[INST_MAP_SIZE] = mmv_lookup_value_desc(map, "instmap.size", NULL);
}


int
pmWebGroupSetMetricRegistry(pmWebGroupModule *module, mmv_registry_t *registry)
{
    struct webgroups	*groups = webgroups_lookup(module);

    if (groups) {
	groups->registry = registry;
	pmWebGroupSetupMetrics(module);
	return 0;
    }
    return -ENOMEM;
}

void
pmWebGroupClose(pmWebGroupModule *module)
{
    struct webgroups	*groups = (struct webgroups *)module->privdata;
    dictIterator	*iterator;
    dictEntry		*entry;

    if (groups) {
	/* walk the contexts, stop timers and free resources */
	if (groups->active) {
	    groups->active = 0;
	    uv_timer_stop(&groups->timer);
	    pmWebTimerRelease(groups->timerid);
	    groups->timerid = -1;
	}
	iterator = dictGetIterator(groups->contexts);
	while ((entry = dictNext(iterator)) != NULL)
	    webgroup_drop_context((context_t *)dictGetVal(entry), NULL);
	dictReleaseIterator(iterator);
	dictRelease(groups->contexts);
	memset(groups, 0, sizeof(struct webgroups));
	free(groups);
	module->privdata = NULL;
    }

    sdsfree(PARAM_HOSTNAME);
    sdsfree(PARAM_HOSTSPEC);
    sdsfree(PARAM_CTXNUM);
    sdsfree(PARAM_CTXID);
    sdsfree(PARAM_POLLTIME);
    sdsfree(PARAM_PREFIX);
    sdsfree(PARAM_MNAMES);
    sdsfree(PARAM_PMIDS);
    sdsfree(PARAM_PMID);
    sdsfree(PARAM_INDOM);
    sdsfree(PARAM_MNAME);
    sdsfree(PARAM_INSTANCE);
    sdsfree(PARAM_INAME);
    sdsfree(PARAM_MVALUE);
    sdsfree(PARAM_TARGET);
    sdsfree(PARAM_EXPR);
    sdsfree(PARAM_MATCH);

    /* generally needed strings, error messages */
    sdsfree(EMPTYSTRING);
    sdsfree(LOCALHOST);
    sdsfree(WORK_TIMER);
    sdsfree(POLL_TIMEOUT);
    sdsfree(BATCHSIZE);
    sdsfree(AUTH_USERNAME);
    sdsfree(AUTH_PASSWORD);
}
