/*
 * Copyright (c) 2025 Red Hat.
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
#include <uv.h>
#include <ctype.h>
#include <assert.h>

#include "pmapi.h"
#include "pmwebapi.h"
#include "mmv_stats.h"
#define PCP_INTERNAL	/* for log label structure and helper interfaces */
#include "libpcp.h"
#include "util.h"
#include "discover.h"

static unsigned int cached_only;	/* perform key server caching only */

#define DEFAULT_WORK_TIMER 5000
static unsigned int default_worker;	/* BG work delta, milliseconds */

#define DEFAULT_POLL_TIMEOUT 60000
static unsigned int default_timeout;	/* timeout in milliseconds */

/* constant string keys (initialized during setup) */
static sds PARAM_LOGID, PARAM_POLLTIME;
static sds CACHED_ONLY, WORK_TIMER, POLL_TIMEOUT;

/* constant global strings (read-only) */
static const char *TIME_FORMAT = "%Y%m%d.%H.%M";

/* constant global integers (read-only) */
static const size_t MAX_BUFFER_SIZE = 10 * 1024 * 1024; /* PDU limit */
static const size_t MAX_VOLUME_COUNT = 5000; /* limit of log volumes */

enum loggroup_metric {
    LOGGROUP_LOGS = 1,
    LOGGROUP_BYTES,
    LOGGROUP_WRITES,
    LOGGROUP_GC_COUNT,
    LOGGROUP_GC_DROPS,
    NUM_LOGGROUP_METRIC
};

typedef struct archive {
    sds			fullpath;	/* log path */
    int			randomid;	/* random number identifier */
    unsigned int	setup	: 1;	/* log label present */
    unsigned int	garbage	: 1;	/* log pending removal */
    unsigned int	inactive: 1;	/* log removal deferred */
    unsigned int	padding : 13;	/* zero-filled struct padding */
    unsigned int	refcount : 16;	/* currently-referenced counter */
    unsigned int	timeout;	/* log timeout in milliseconds */
    uv_timer_t		timer;
    int			datafd;
    unsigned int	datavol;
    __pmLogLabel	loglabel;	/* log label (common header) */
    pmDiscover		*discover;
    void		*privdata;
} archive_t;

typedef struct loggroups {
    struct dict		*archives;
    struct dict		*config;

    mmv_registry_t	*registry;
    pmAtomValue		*metrics[NUM_LOGGROUP_METRIC];
    void		*map;

    uv_loop_t		*events;
    uv_timer_t		timer;
    uv_mutex_t		mutex;

    unsigned int	active;
} loggroups_t;

static struct loggroups *
loggroups_create(pmLogGroupModule *module)
{
    struct loggroups *groups = module->privdata;

    if (module->privdata == NULL) {
	module->privdata = calloc(1, sizeof(struct loggroups));
	groups = (struct loggroups *)module->privdata;
	uv_mutex_init(&groups->mutex);
    }
    return groups;
}

static inline struct loggroups *
loggroups_lookup(pmLogGroupModule *module)
{
    return (struct loggroups *)module->privdata;
}

static int
loggroup_deref_archive(struct archive *ap)
{
    if (ap == NULL)
	return 1;
    if (ap->refcount == 0)
	return 0;
    return (--ap->refcount > 0);
}

void
loggroup_free_archive(struct archive *ap)
{
    __pmLogFreeLabel(&ap->loglabel);
    if (ap->datafd > 0)
	close(ap->datafd);
    if (ap->discover)
	pmDiscoverStreamEnd(ap->discover->context.name);
    sdsfree(ap->fullpath);
    memset(ap, 0, sizeof(*ap));
    free(ap);
}

static void
loggroup_release_archive(uv_handle_t *handle)
{
    struct archive	*archive = (struct archive *)handle->data;

    if (pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log)
	fprintf(stderr, "releasing archive %p [refcount=%u]\n",
			archive, archive->refcount);
    loggroup_free_archive(archive);
}

static void
loggroup_drop_archive(struct archive *archive, struct loggroups *groups)
{
    if (pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log)
	fprintf(stderr, "destroying archive %p [refcount=%u]\n",
			archive, archive->refcount);

    if (loggroup_deref_archive(archive) == 0) {
	if (archive->garbage == 0) {
	    archive->garbage = 1;
	    uv_timer_stop(&archive->timer);
	}
	if (groups) {
	    uv_mutex_lock(&groups->mutex);
	    dictDelete(groups->archives, &archive->randomid);
	    uv_mutex_unlock(&groups->mutex);
	}
	uv_close((uv_handle_t *)&archive->timer, loggroup_release_archive);
    }
}

static void
loggroup_timeout_archive(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct archive	*ap = (struct archive *)handle->data;

    if (pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log)
	fprintf(stderr, "archive %u timed out (%p)\n", ap->randomid, ap);

    /*
     * Cannot free data structures in the timeout handler, as
     * they may still be actively in use - wait until reference
     * is returned to zero by the caller, or background cleanup
     * finds this archive and cleans it.
     */
    if (ap->refcount == 0) {
	if (ap->garbage == 0) {
	    ap->garbage = 1;
	    uv_timer_stop(&ap->timer);
	}
    } else {
	/*
	 * Archive timed out but still referenced, must wait
	 * until the caller releases its reference (shortly)
	 * before beginning garbage collection process.
	 */
	ap->inactive = 1;
    }
}

static int
loggroup_new_archive(pmLogGroupSettings *sp, __pmLogLabel *label,
		char *fullpath, dict *params, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap;
    unsigned int	polltime = DEFAULT_POLL_TIMEOUT;
    uv_handle_t		*handle;
    double		seconds;
    char		*endptr;
    sds			timeout;
    int			archive;

    if (params) {
	if ((timeout = dictFetchValue(params, PARAM_POLLTIME)) != NULL) {
	    seconds = strtod(timeout, &endptr);
	    if (*endptr != '\0')
		return -EINVAL;
	    polltime = (unsigned int)(seconds * 1000.0);
	}
    }

    if ((ap = (archive_t *)calloc(1, sizeof(archive_t))) == NULL)
	return -ENOMEM;
    ap->timeout = polltime;
    ap->randomid = archive = random();

    uv_mutex_lock(&groups->mutex);
    if (dictFind(groups->archives, &archive) != NULL) {
	uv_mutex_unlock(&groups->mutex);
	loggroup_free_archive(ap);
	return -EEXIST;
    }
    uv_mutex_unlock(&groups->mutex);
    if ((ap->fullpath = sdsnew(fullpath)) == NULL) {
	loggroup_free_archive(ap);
	return -ENOMEM;
    }
    if (sp->module.discover &&
	(ap->discover = pmDiscoverStreamLabel(fullpath, label,
					sp->module.discover, ap)) == NULL) {
	loggroup_free_archive(ap);
	return -ENOMEM;
    }
    ap->loglabel = *label;	/* struct copy */
    ap->datavol = UINT_MAX;
    ap->datafd = -1;

    uv_mutex_lock(&groups->mutex);
    dictAdd(groups->archives, &archive, ap);
    uv_mutex_unlock(&groups->mutex);

    /* leave until the end because uv_timer_init makes this visible in uv_run */
    handle = (uv_handle_t *)&ap->timer;
    handle->data = (void *)ap;
    uv_timer_init(groups->events, &ap->timer);

    ap->privdata = groups;
    ap->setup = 1;

    if (pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log)
	fprintf(stderr, "new archive[%d] setup (%p)\n", archive, ap);

    return archive;
}

static void
loggroup_timers_stop(struct loggroups *groups)
{
    if (groups->active) {
	uv_timer_stop(&groups->timer);
	uv_close((uv_handle_t *)&groups->timer, NULL);
	groups->active = 0;
    }
}

static void
loggroup_garbage_collect(struct loggroups *groups)
{
    dictIterator        *iterator;
    dictEntry           *entry;
    archive_t		*ap;
    unsigned int	debug;
    unsigned int	count = 0, drops = 0, garbageset = 0, inactiveset = 0;

    debug = pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log;
    if (debug)
	fprintf(stderr, "%s: started for groups %p\n",
			"loggroup_garbage_collect", groups);

    /* do archive GC if we get the lock (else don't block here) */
    if (uv_mutex_trylock(&groups->mutex) == 0) {
	iterator = dictGetSafeIterator(groups->archives);
	for (entry = dictNext(iterator); entry;) {
	    ap = (archive_t *)dictGetVal(entry);
	    if (ap->privdata != groups)
		continue;
	    entry = dictNext(iterator);
	    if (ap->garbage)
		garbageset++;
	    if (ap->inactive && ap->refcount == 0)
		inactiveset++;
	    if (ap->garbage || (ap->inactive && ap->refcount == 0)) {
		if (debug)
		    fprintf(stderr, "GC dropping archive %u (%p)\n",
				    ap->randomid, ap);
		uv_mutex_unlock(&groups->mutex);
		loggroup_drop_archive(ap, groups);
		uv_mutex_lock(&groups->mutex);
		drops++;
	    }
	    count++;
	}
	dictReleaseIterator(iterator);

	/* if dropping the last remaining archive, do cleanup */
	if (groups->active && drops == count) {
	    if (debug)
		fprintf(stderr, "%s: freezing groups %p\n",
				"loggroup_garbage_collect", groups);
	    loggroup_timers_stop(groups);
	}
	uv_mutex_unlock(&groups->mutex);
    }

    mmv_set(groups->map, groups->metrics[LOGGROUP_GC_DROPS], &drops);
    mmv_set(groups->map, groups->metrics[LOGGROUP_GC_COUNT], &count);

    if (debug)
	fprintf(stderr, "%s: finished [%u drops from %u entries,"
			" %u garbageset, %u inactiveset]\n",
			"loggroup_garbage_collect", drops, count,
			garbageset, inactiveset);
}

static void
loggroup_worker(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct loggroups	*groups = (struct loggroups *)handle->data;

    loggroup_garbage_collect(groups);
}

static struct archive *
loggroup_use_archive(struct archive *ap)
{
    struct loggroups    *gp = (struct loggroups *)ap->privdata;

    if (ap->garbage == 0 && ap->inactive == 0) {
	if (ap->setup == 0)
	    ap->setup = 1;
	ap->refcount++;

	if (pmDebugOptions.http || pmDebugOptions.libweb || pmDebugOptions.log)
	    fprintf(stderr, "archive %u timer set (%p) to %u msec\n",
			ap->randomid, ap, ap->timeout);

	/* refresh current time: https://github.com/libuv/libuv/issues/1068 */
	uv_update_time(gp->events);

	/* if already started, uv_timer_start updates the existing timer */
	uv_timer_start(&ap->timer, loggroup_timeout_archive, ap->timeout, 0);

	return ap;
    }

    /* expired archive identifier */
    return NULL;
}

static int
loggroup_lookup_archive(pmLogGroupSettings *sp, int id, struct archive **pp, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap;

    if (groups->active == 0) {
	groups->active = 1;
	/* install general background work timer (GC) */
	uv_timer_init(groups->events, &groups->timer);
	groups->timer.data = (void *)groups;
	uv_timer_start(&groups->timer, loggroup_worker,
			default_worker, default_worker);
    }

    ap = (struct archive *)dictFetchValue(groups->archives, &id);
    if (ap == NULL)
	return -ESRCH;
    if ((ap = loggroup_use_archive(ap)) == NULL)
	return -ENOTCONN;
    *pp = ap;
    return 0;
}

void
pmLogGroupDestroy(pmLogGroupSettings *sp, int id, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap;

    if (loggroup_lookup_archive(sp, id, &ap, arg) == 0) {
	if (pmDebugOptions.libweb || pmDebugOptions.log)
	    fprintf(stderr, "%s: destroy archive %p groups=%p\n",
			    "pmLogGroupDestroy", ap, groups);

	loggroup_deref_archive(ap);
	loggroup_drop_archive(ap, groups);
    }
}

static ssize_t
logger_write_buffer(int fd, const char *content, size_t length)
{
    ssize_t	bytes, written;

    assert(cached_only == 0);
    for (written = 0; written < length; written += bytes)
	if ((bytes = write(fd, content + written, length - written)) < 0)
	    return -oserror();
    return written;
}

static int
logger_volume_label(archive_t *ap)
{
    ssize_t	bytes;
    size_t	length;
    void	*buffer;
    char	*dir, path[MAXPATHLEN];
    int		fd = 0;
    int		sts;

    assert(cached_only == 0);

    /* make any needed subdirectories, especially for this hostname */
    pmsprintf(path, sizeof(path), "%s.%u", ap->fullpath, ap->datavol);
    dir = dirname(path); /* overwrites part of on-stack path buffer */
    if ((sts = __pmMakePath(dir, 0775)) < 0)
	return sts;

    /* create the new data volume for pre-existing pmlogger archive */
    pmsprintf(path, sizeof(path), "%s.%u", ap->fullpath, ap->datavol);
    fd = open(path, O_CREAT|O_EXCL|O_APPEND|O_NOFOLLOW|O_WRONLY, 0644);
    if (fd < 0) {
	sts = -errno;
    } else {
	ap->loglabel.vol = ap->datavol;
	sts = __pmLogEncodeLabel(&ap->loglabel, &buffer, &length);
	if (sts == 0) {
	    if ((bytes = logger_write_buffer(fd, buffer, length)) < 0)
		sts = bytes;
	    else
	        sts = fd;
	    free(buffer);
	}
    }

    return sts;
}

/*
 * First pass for this archive, create .meta and .index files.
 * Defer creating any data volume until this streams in and we
 * find out what the volume number will be (typically 0).
 */
static int
logger_write_labels(const char *fullpath, __pmLogLabel *loglabel)
{
    char	*dir, path[MAXPATHLEN];
    void	*buffer;
    int		fd, sts;
    size_t	length;

    assert(cached_only == 0);

    /* make any needed subdirectories, especially for this hostname */
    pmsprintf(path, sizeof(path), "%s", fullpath);
    dir = dirname(path); /* overwrites part of on-stack path buffer */
    if ((sts = __pmMakePath(dir, 0775)) < 0)
	return sts;

    /* create the metadata file including the initial log label */
    loglabel->vol = PM_LOG_VOL_META;
    if ((sts = __pmLogEncodeLabel(loglabel, &buffer, &length)) < 0)
	return sts;
    pmsprintf(path, sizeof(path), "%s.meta", fullpath);
    fd = open(path, O_CREAT|O_EXCL|O_APPEND|O_NOFOLLOW|O_WRONLY, 0644);
    if (fd < 0) {
	free(buffer);
	return -oserror();
    }
    sts = logger_write_buffer(fd, buffer, length);
    free(buffer);
    close(fd);
    if (sts < 0)
	return sts;

    /* create the temporal index file with its initial log label */
    loglabel->vol = PM_LOG_VOL_TI;
    if ((sts = __pmLogEncodeLabel(loglabel, &buffer, &length)) < 0)
	return sts;
    pmsprintf(path, sizeof(path), "%s.index", fullpath);
    fd = open(path, O_CREAT|O_EXCL|O_APPEND|O_NOFOLLOW|O_WRONLY, 0644);
    if (fd < 0) {
	free(buffer);
	return -oserror();
    }
    sts = logger_write_buffer(fd, buffer, length);
    free(buffer);
    close(fd);

    return sts;
}

int
pmLogGroupLabel(pmLogGroupSettings *sp, const char *content, size_t length,
		dict *params, void *arg)
{
    __pmLogLabel	loglabel = {0};
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct tm		tm;
    char		*dir, timebuf[64];
    char		pathbuf[MAXPATHLEN];
    int			sts, sep;

    if (groups == NULL) {	/* disabled via config file */
	sts = -ENOTSUP;
	goto fail;
    }

    /* safely verify buffer contents/length (user-supplied) */
    if ((sts = __pmLogDecodeLabel(content, length, &loglabel)) < 0)
	goto fail;

    if (pmDebugOptions.log)
	fprintf(stderr, "New archive label for host: %s\n", loglabel.hostname);

    if (localtime_r(&loglabel.start.sec, &tm) == NULL ||
        strftime(timebuf, sizeof(timebuf), TIME_FORMAT, &tm) < 2) {
	sts = -EINVAL;
	goto fail;
    }

    sep = pmPathSeparator();
    dir = pmGetConfig("PCP_LOG_DIR");
    pmsprintf(pathbuf, sizeof(pathbuf), "%s%c%s%c%s%c%s", dir, sep,
		pmGetProgname(), sep, loglabel.hostname, sep, timebuf);

    if (cached_only)
	goto done;

    if (pmDebugOptions.log)
	fprintf(stderr, "Writing archive %s.{meta,index} labels\n", pathbuf);

    if ((sts = logger_write_labels(pathbuf, &loglabel)) < 0)
	goto fail;

    /* wrote two label headers and created one archive - update stats */
    mmv_inc(groups->map, groups->metrics[LOGGROUP_LOGS]);
    length *= 2;
    mmv_add(groups->map, groups->metrics[LOGGROUP_BYTES], &length);
    length = 2;
    mmv_add(groups->map, groups->metrics[LOGGROUP_WRITES], &length);

    if (pmDebugOptions.log)
	fprintf(stderr, "Caching details for new archive %s\n", pathbuf);

done:
    /* add to in-memory group of loggers for quick lookup */
    if ((sts = loggroup_new_archive(sp, &loglabel, pathbuf, params, arg)) < 0)
	goto fail;

    sp->callbacks.on_archive(sts, arg);
    return 0;

fail:
    __pmLogFreeLabel(&loglabel);
    sp->callbacks.on_done(sts, arg);
    return sts;
}

int
pmLogGroupMeta(pmLogGroupSettings *sp, int id,
		const char *content, size_t length, dict *params, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap = NULL;
    ssize_t		bytes;
    char		path[MAXPATHLEN];
    int			fd, sts;

    if (groups == NULL) {	/* disabled via config file */
	sts = -ENOTSUP;
	goto done;
    }
    if (length >= MAX_BUFFER_SIZE) {
	sts = -E2BIG;
	goto done;
    }

    if ((sts = loggroup_lookup_archive(sp, id, &ap, arg)) < 0)
	goto done;

    if ((ap->discover != NULL) &&
	(sts = pmDiscoverStreamMeta(ap->discover, content, length)) < 0)
	goto done;

    if (cached_only)
	goto done;

    pmsprintf(path, sizeof(path), "%s.meta", ap->fullpath);
    if ((fd = open(path, O_APPEND|O_NOFOLLOW|O_WRONLY, 0644)) < 0) {
	sts = -oserror();
	goto done;
    }
    bytes = logger_write_buffer(fd, content, length);
    close(fd);
    if (bytes < 0) {
	sts = -oserror();
	goto done;
    }

    mmv_add(groups->map, groups->metrics[LOGGROUP_BYTES], &bytes);
    mmv_inc(groups->map, groups->metrics[LOGGROUP_WRITES]);

    if (pmDebugOptions.log)
	fprintf(stderr, "Wrote %zu bytes to %s.meta\n", bytes, ap->fullpath);

done:
    if (ap)
	loggroup_deref_archive(ap);
    sp->callbacks.on_done(sts, arg);
    return sts;
}

int
pmLogGroupIndex(pmLogGroupSettings *sp, int id,
		const char *content, size_t length, dict *params, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap = NULL;
    ssize_t		bytes;
    char		path[MAXPATHLEN];
    int			fd, sts = 0;

    if (groups == NULL) {	/* disabled via config file */
	sts = -ENOTSUP;
	goto done;
    }
    if (length >= MAX_BUFFER_SIZE) {
	sts = -E2BIG;
	goto done;
    }

    if (cached_only)
	goto done;

    if ((sts = loggroup_lookup_archive(sp, id, &ap, arg)) < 0)
	goto done;

    pmsprintf(path, sizeof(path), "%s.index", ap->fullpath);
    if ((fd = open(path, O_APPEND|O_NOFOLLOW|O_WRONLY, 0644)) < 0) {
	sts = -oserror();
	goto done;
    }
    bytes = logger_write_buffer(fd, content, length);
    close(fd);
    if (bytes < 0) {
	sts = -oserror();
	goto done;
    }

    mmv_add(groups->map, groups->metrics[LOGGROUP_BYTES], &bytes);
    mmv_inc(groups->map, groups->metrics[LOGGROUP_WRITES]);

    if (pmDebugOptions.log)
	fprintf(stderr, "Wrote %zu bytes to %s.index\n", bytes, ap->fullpath);

done:
    if (ap)
	loggroup_deref_archive(ap);
    sp->callbacks.on_done(sts, arg);
    return sts;
}

int
pmLogGroupVolume(pmLogGroupSettings *sp, int id, unsigned int volume,
		const char *content, size_t length, dict *params, void *arg)
{
    struct loggroups	*groups = loggroups_lookup(&sp->module);
    struct archive	*ap = NULL;
    ssize_t		bytes;
    int			sts = 0;

    if (groups == NULL) {	/* disabled via config file */
	sts = -ENOTSUP;
	goto done;
    }
    if (length >= MAX_BUFFER_SIZE || volume >= MAX_VOLUME_COUNT) {
	sts = -E2BIG;
	goto done;
    }

    if ((sts = loggroup_lookup_archive(sp, id, &ap, arg)) < 0)
	goto done;

    if (volume != ap->datavol) {
	ap->datavol = volume;
	if (!cached_only && (ap->datafd = logger_volume_label(ap)) < 0) {
	    sts = -oserror();
	    goto done;
	}
    }

    if ((ap->discover != NULL) &&
	(sts = pmDiscoverStreamData(ap->discover, content, length)) < 0)
	goto done;

    if (cached_only)
	goto done;

    if ((bytes = logger_write_buffer(ap->datafd, content, length)) < 0) {
	sts = -oserror();
	goto done;
    }

    mmv_add(groups->map, groups->metrics[LOGGROUP_BYTES], &bytes);
    mmv_inc(groups->map, groups->metrics[LOGGROUP_WRITES]);

    if (pmDebugOptions.log)
	fprintf(stderr, "Wrote %zu data volume bytes to %s.%u\n",
			bytes, ap->fullpath, ap->datavol);

done:
    if (ap)
	loggroup_deref_archive(ap);
    sp->callbacks.on_done(sts, arg);
    return sts;
}

int
pmLogGroupSetup(pmLogGroupModule *module)
{
    struct loggroups	*groups = loggroups_create(module);
    struct timespec	ts;
    unsigned int	pid;

    if (groups == NULL)
	return -ENOMEM;

    PARAM_LOGID = sdsnew("id");
    PARAM_POLLTIME = sdsnew("polltimeout");

    /* generally needed strings, error messages */
    WORK_TIMER = sdsnew("pmlogger.work");
    CACHED_ONLY = sdsnew("pmlogger.cached");
    POLL_TIMEOUT = sdsnew("pmlogger.timeout");

    /* setup the random number generator for archive IDs */
    pmtimespecNow(&ts);
    pid = (unsigned int)getpid();
    srandom(pid ^ (unsigned int)ts.tv_sec ^ (unsigned int)ts.tv_nsec);

    /* setup a dictionary mapping archive number to data */
    groups->archives = dictCreate(&intKeyDictCallBacks, NULL);

    return 0;
}

int
pmLogGroupSetEventLoop(pmLogGroupModule *module, void *events)
{
    struct loggroups	*groups = loggroups_lookup(module);

    if (groups) {
	groups->events = (uv_loop_t *)events;
	return 0;
    }
    return -ENOMEM;
}

static void
loggroup_free(struct loggroups *groups)
{
    dictIterator	*iterator;
    dictEntry		*entry;

    /* walk the archives, stop timers and free resources */
    iterator = dictGetIterator(groups->archives);
    while ((entry = dictNext(iterator)) != NULL)
	loggroup_drop_archive((archive_t *)dictGetVal(entry), NULL);
    dictReleaseIterator(iterator);
    dictRelease(groups->archives);
    loggroup_timers_stop(groups);
    memset(groups, 0, sizeof(struct loggroups));
    free(groups);
}

int
pmLogGroupSetConfiguration(pmLogGroupModule *module, dict *config)
{
    struct loggroups	*groups = loggroups_lookup(module);
    char		*endnum;
    sds			value;

    if ((value = pmIniFileLookup(config, "pmlogger", "enabled")) &&
	(strcmp(value, "false") == 0)) {
	module->privdata = NULL;
	loggroup_free(groups);
	return -ENOTSUP;
    }

    /* allocate strings for parameter dictionary key lookups */
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

    if ((value = dictFetchValue(config, CACHED_ONLY)) != NULL)
	cached_only = (strcmp(value, "true") == 0);

    if (groups) {
	groups->config = config;
	return 0;
    }
    return -ENOMEM;
}

static void
pmLogGroupSetupMetrics(pmLogGroupModule *module)
{
    struct loggroups	*groups = loggroups_lookup(module);
    pmAtomValue		**ap;
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    pmUnits		countunits = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE);
    pmUnits		bytesunits = MMV_UNITS(1,0,0,PM_SPACE_KBYTE,0,0);
    void		*map;

    if (groups == NULL || groups->registry == NULL)
	return; /* no metric registry has been set up */

    mmv_stats_add_metric(groups->registry, "archive.count", LOGGROUP_LOGS,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"archives created by this service",
	"Count of remote archive creations by the running service");

    mmv_stats_add_metric(groups->registry, "archive.bytes", LOGGROUP_BYTES,
	MMV_TYPE_U64, MMV_SEM_COUNTER, bytesunits, MMV_INDOM_NULL,
	"total bytes written to archives by this service",
	"Total remote archive bytes written by the running service");

    mmv_stats_add_metric(groups->registry, "archive.write", LOGGROUP_WRITES,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"write system calls to archives by this service",
	"Total remote archive write system calls by the running service");

    mmv_stats_add_metric(groups->registry, "gc.archive.scans", LOGGROUP_GC_COUNT,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"archives scanned in last garbage collection",
	"Archives scanned during most recent loggroup garbage collection");

    mmv_stats_add_metric(groups->registry, "gc.archive.drops", LOGGROUP_GC_DROPS,
	MMV_TYPE_U32, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"archives dropped in last garbage collection",
	"Archives dropped during most recent loggroup garbage collection");

    groups->map = map = mmv_stats_start(groups->registry);

    ap = groups->metrics;
    ap[LOGGROUP_LOGS] = mmv_lookup_value_desc(map, "archive.count", NULL);
    ap[LOGGROUP_BYTES] = mmv_lookup_value_desc(map, "archive.bytes", NULL);
    ap[LOGGROUP_WRITES] = mmv_lookup_value_desc(map, "archive.write", NULL);
    ap[LOGGROUP_GC_DROPS] = mmv_lookup_value_desc(map, "gc.archive.scans", NULL);
    ap[LOGGROUP_GC_COUNT] = mmv_lookup_value_desc(map, "gc.archive.drops", NULL);
}

int
pmLogGroupSetMetricRegistry(pmLogGroupModule *module, mmv_registry_t *registry)
{
    struct loggroups	*groups = loggroups_lookup(module);

    if (groups) {
	groups->registry = registry;
	pmLogGroupSetupMetrics(module);
	return 0;
    }
    return -ENOMEM;
}

void
pmLogGroupClose(pmLogGroupModule *module)
{
    struct loggroups	*groups = loggroups_lookup(module);

    if (groups) {
	loggroup_free(groups);
	module->privdata = NULL;
    }

    sdsfree(PARAM_LOGID);
    sdsfree(PARAM_POLLTIME);

    /* generally needed strings, error messages */
    sdsfree(WORK_TIMER);
    sdsfree(CACHED_ONLY);
    sdsfree(POLL_TIMEOUT);
}
