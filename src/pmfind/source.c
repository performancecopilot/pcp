/*
 * Copyright (c) 2020,2022 Red Hat.
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
#include <uv.h>
#include "dict.h"
#include "pmapi.h"
#include "source.h"
#include "pmwebapi.h"

extern dictType sdsDictCallBacks;
extern dictType sdsOwnDictCallBacks;
extern dictType sdsKeyDictCallBacks;
static pmWebGroupSettings settings;

typedef struct {
    sds			source;
    sds			hostspec;
    unsigned int	refcount;
} context_t;

typedef struct {
    int			status;		/* exit status */
    int			count;		/* url count */
    char		**urls;		/* callers */
    uv_mutex_t		mutex;
    dict		*uniq;
    dict		*params;
    dict		*contexts;
} sources_t;

static void
source_release(sources_t *sp, context_t *cp, sds ctx)
{
    pmWebGroupDestroy(&settings, ctx, sp);
    sdsfree(cp->hostspec);
    sdsfree(cp->source);
    free(cp);
}

static void
sources_release(void *arg, const struct dictEntry *entry)
{
    sources_t	*sp = (sources_t *)arg;
    context_t	*cp = (context_t *)dictGetVal(entry);
    sds		ctx = (sds)entry->key;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "releasing context %s\n", ctx);

    source_release(sp, cp, ctx);
}

static void
sources_containers(sources_t *sp, context_t *cp, sds id, dictEntry *uniq)
{
    uv_mutex_lock(&sp->mutex);
    /* issuing another PMWEBAPI request */
    sp->count++;
    cp->refcount++;
    uv_mutex_unlock(&sp->mutex);

    pmWebGroupScrape(&settings, id, sp->params, sp);
}

static void
on_source_context(sds id, pmWebSource *src, void *arg)
{
    sources_t	*sp = (sources_t *)arg;
    context_t	*cp;
    dictEntry	*entry;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "context %s created for %s [%s]\n",
			id, src->hostspec, src->source);

    if ((cp = malloc(sizeof(context_t))) == NULL)
	return;

    cp->source = sdsdup(src->source);
    cp->hostspec = sdsdup(src->hostspec);
    cp->refcount = 1;

    uv_mutex_lock(&sp->mutex);
    dictAdd(sp->contexts, id, cp);
    entry = dictAddRaw(sp->uniq, src->source, NULL);
    uv_mutex_unlock(&sp->mutex);

    if (entry) {	/* source just discovered */
	printf("%s %s\n", src->source, src->hostspec);
	if (containers)
	    sources_containers(sp, cp, id, entry);
    }
}

/* value returned from container scrape (one container) */
static int
on_source_scrape(sds context, pmWebScrape *wsp, void *arg)
{
    sources_t	*sp = (sources_t *)arg;
    context_t	*cp;
    dictEntry	*he;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "context %s container %s discovered (running=%s)\n",
			context, wsp->instance.name, wsp->value.value);

    if (strcmp(wsp->value.value, "1") != 0)	/* running? */
	return 0;

    uv_mutex_lock(&sp->mutex);
    he = dictFind(sp->contexts, context);
    cp = (context_t *)dictGetVal(he);
    uv_mutex_unlock(&sp->mutex);

    printf("%s %s?container=%s\n", cp->source, cp->hostspec, wsp->instance.name);
    return 0;
}

static void
on_source_done(sds context, int status, sds message, void *arg)
{
    sources_t	*sp = (sources_t *)arg;
    context_t	*cp;
    dictEntry	*he;
    int		remove = 0, count = 0, release = 0;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "done on context %s (sts=%d)\n", context, status);

    if (status)
	sp->status = 1;

    uv_mutex_lock(&sp->mutex);
    if ((count = --sp->count) <= 0)
	release = 1;
    if ((he = dictFind(sp->contexts, context)) != NULL &&
	(cp = (context_t *)dictGetVal(he)) != NULL &&
	(--cp->refcount <= 0))
	remove = 1;
    uv_mutex_unlock(&sp->mutex);

    if (remove) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "remove context %s\n", context);
	source_release(sp, cp, context);
	dictDelete(sp->contexts, context);
    }

    if (release) {
	unsigned long	cursor = 0;
	do {
	    cursor = dictScan(sp->contexts, cursor, sources_release, NULL, sp);
	} while (cursor);
    } else if (pmDebugOptions.discovery) {
	fprintf(stderr, "not yet releasing (count=%d)\n", count);
    }
}

static void
on_source_info(pmLogLevel level, sds message, void *arg)
{
    sources_t	*sp = (sources_t *)arg;
    FILE	*fp = (level == PMLOG_INFO) ? stdout : stderr;

    if (level >= PMLOG_ERROR)
	sp->status = 1; /* exit with error */
    if (level >= PMLOG_INFO || pmDebugOptions.discovery)
	pmLogLevelPrint(fp, level, message, 0);
}

static void
sources_discovery_start(uv_timer_t *arg)
{
    uv_handle_t	*handle = (uv_handle_t *)arg;
    sources_t	*sp = (sources_t *)handle->data;
    dict	*dp = dictCreate(&sdsOwnDictCallBacks, NULL);
    sds		name, value;
    int		i, fail = 0, total = sp->count;

    if (dp == NULL)
	return;
    name = sdsnew("hostspec");

    for (i = 0; i < total; i++) {
	value = sdsnew(sp->urls[i]);
	dictReplace(dp, name, value);
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "source discovery for hostspec %s\n", value);
	if (pmWebGroupContext(&settings, NULL, dp, sp) < 0)
	    fail++;
    }

    if (fail) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "failed on %d/%d contexts\n", fail, total);
	uv_mutex_lock(&sp->mutex);
	sp->count -= fail;
	uv_mutex_unlock(&sp->mutex);
	sp->status = 1;
    } else {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "successfully setup %d contexts\n", total);
    }

    dictRelease(dp);
    pmWebTimerClose();
}

/*
 * Use pmWebGroup APIs to extract source and container identifiers from pmcd.
 * We'll be using this to drive enablement of services like pmie and pmlogger
 * so we want to ensure only one line of output for each unique connection -
 * the source identifiers are perfect for this.
 *
 * We'll use the PMWEBAPI to extract sources and discover running containers;
 * the latter is achieved using containers.state.running metric values.  Note
 * discovery can result in duplicated sources - fetch once from each.
 */
int
source_discovery(int count, char **urls)
{
    uv_handle_t	*handle;
    uv_timer_t	timing;
    uv_loop_t	*loop;
    sources_t	find;

    memset(&find, 0, sizeof(find));
    uv_mutex_init(&find.mutex);
    find.urls = urls;
    find.count = count;	/* at least one PMWEBAPI request for each url */
    find.uniq = dictCreate(&sdsKeyDictCallBacks, NULL);
    find.params = dictCreate(&sdsOwnDictCallBacks, NULL);
    dictAdd(find.params, sdsnew("name"), sdsnew("containers.state.running"));
    find.contexts = dictCreate(&sdsKeyDictCallBacks, NULL);

    /*
     * Setup an async event loop and prepare for pmWebGroup API use
     */
    loop = uv_default_loop();
    settings.callbacks.on_context = on_source_context;
    settings.callbacks.on_scrape = on_source_scrape;
    settings.callbacks.on_done = on_source_done;
    settings.module.on_info = on_source_info;

    pmWebGroupSetup(&settings.module);
    pmWebGroupSetEventLoop(&settings.module, loop);
    pmWebTimerSetEventLoop(loop);

    /*
     * Start a one-shot timer to add a start function into the loop
     */
    handle = (uv_handle_t *)&timing;
    handle->data = (void *)&find;
    uv_timer_init(loop, &timing);
    uv_timer_start(&timing, sources_discovery_start, 0, 0);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);

    /*
     * Finished, release all resources acquired so far
     */
    pmWebGroupClose(&settings.module);
    uv_mutex_destroy(&find.mutex);
    dictRelease(find.uniq);
    dictRelease(find.params);
    dictRelease(find.contexts);
    return find.status;
}
