/*
 * Copyright (c) 2018-2022,2024-2025 Red Hat.
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
#include "discover.h"
#include "schema.h"
#include "slots.h"
#include "util.h"
#include <assert.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>

/* Decode various archive metafile records (desc, indom, labels, helptext) */
static ssize_t pmDiscoverDecodeMetaDesc(uint32_t *, size_t, pmDesc *, int *, char ***);
static int pmDiscoverDecodeMetaInDom(__int32_t *, int, int, __pmTimestamp *, pmInResult *);
static int pmDiscoverDecodeMetaHelpText(uint32_t *, int, int *, int *, char **);
static int pmDiscoverDecodeMetaLabelSet(uint32_t *, int, int, __pmTimestamp *, int *, int *, int *, pmLabelSet **);

/* array of registered callbacks, see pmDiscoverSetup() */
static int discoverCallBackTableSize;
static pmDiscoverCallBacks **discoverCallBackTable;
static char *pmDiscoverFlagsStr(pmDiscover *);
static void pmDiscoverInvokeClosedCallBacks(pmDiscover *);

/* internal hash table of discovered paths */
#define DISCOVER_HASHTAB_SIZE 32
static pmDiscover *discover_hashtable[DISCOVER_HASHTAB_SIZE];

/* number of archives or directories currently being monitored */
static uint64_t monitored;
static const int MAX_INFLIGHT_MON = 40;
static const int MAX_INFLIGHT_REQ = 1000000;

static const int MAX_INPUT_SIZE = 16 * 1024 * 1024;	/* 16Mb */


/* FNV string hash algorithm. Return unsigned in range 0 .. limit-1 */
static unsigned int
strhash(const char *s, unsigned int limit)
{
    unsigned int	h = 2166136261LL; /* FNV offset_basis */
    unsigned char	*us = (unsigned char *)s;

    for (; *us != '\0'; us++) {
    	h ^= *us;
	h *= 16777619; /* fnv_prime */
    }
    return h % limit;
}

/* ctime string from latest time sample */
static char *
strtimestamp(time_t *now, char *buffer)
{
    char		*p, *c;

    c = ctime_r(now, buffer);
    if ((p = strrchr(c, '\n')) != NULL)
    	*p = '\0';
    return c;
}

static void
discover_modified_timestamp(pmDiscover *p, __pmTimestamp *stamp)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    stamp->sec = p->statbuf.st_mtime.tv_sec;
    stampn->sec = p->statbuf.st_mtime.tv_nsec;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    stamp->sec = p->statbuf.st_mtimespec.tv_sec;
    stamp->nsec = p->statbuf.st_mtimespec.tv_nsec;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
    stamp->sec = p->statbuf.st_mtim.tv_sec;
    stamp->nsec = p->statbuf.st_mtim.tv_nsec;
#else
!bozo!
#endif
}

/*
 * Lookup or Add a discovered file path (directory or PCP archive file).
 * Note: the fullpath suffix (.meta, .[0-9]+) should already be stripped.
 * Return path table entry (new or existing).
 */
pmDiscover *
pmDiscoverLookupAdd(const char *fullpath, pmDiscoverModule *module, void *arg)
{
    discoverModuleData	*data;
    pmDiscover		*p, *h;
    unsigned int	k;
    sds			name;

    name = sdsnew(fullpath);
    k = strhash(name, DISCOVER_HASHTAB_SIZE);

    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: name=%s\n", __FUNCTION__, name);

    for (p = NULL, h = discover_hashtable[k]; h != NULL; p = h, h = h->next) {
    	if (sdscmp(h->context.name, name) == 0)
	    break;
    }

    if (h == NULL && module != NULL) {	/* hash table insert mode */
	if ((h = (pmDiscover *)calloc(1, sizeof(pmDiscover))) == NULL) {
	    sdsfree(name);
	    return NULL;
	}
	h->fd = -1; /* no meta descriptor initially */
	h->ctx = -1; /* no PMAPI context initially */
	h->flags = DISCOVER_FLAGS_NEW;
	h->context.type = PM_CONTEXT_ARCHIVE;
	h->context.name = name;
	h->metavol = sdsempty();
	h->datavol = sdsempty();
	h->module = module;
	h->data = arg;
	if (p == NULL)
	    discover_hashtable[k] = h;
	else
	    p->next = h;
	++monitored;
	data = getDiscoverModuleData(module);
	mmv_set(data->map, data->metrics[DISCOVER_MONITORED], &monitored);
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: --> new entry %s\n", __FUNCTION__, name);
    }
    else {
	/* already in hash table, so free the buffer */
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: --> existing entry %s\n", __FUNCTION__, name);
    	sdsfree(name);
    }

    return h;
}

static pmDiscover *
pmDiscoverLookup(const char *path)
{
    return pmDiscoverLookupAdd(path, NULL, NULL);
}

void
pmDiscoverFree(pmDiscover *p)
{
    if (p->ctx >= 0)
	pmDestroyContext(p->ctx);
    if (p->fd >= 0)
	close(p->fd);
    sdsfree(p->metavol);
    sdsfree(p->datavol);
    sdsfree(p->context.name);
    sdsfree(p->context.hostname);
    sdsfree(p->context.source);
    if (p->context.labelset)
	pmFreeLabelSets(p->context.labelset, 1);
    if (p->event_handle) {
	uv_fs_event_stop(p->event_handle);
	free(p->event_handle);
    }

    memset(p, 0, sizeof(*p));
    free(p);
}

/*
 * Traverse and invoke callback for all paths matching any bit
 * in the flags bitmap. Callback can be NULL to just get a count.
 * Return count of matching paths, may be 0.
 */
static int
pmDiscoverTraverse(unsigned int flags, void (*callback)(pmDiscover *))
{
    int			count = 0, i;
    pmDiscover		*p;

    for (i = 0; i < DISCOVER_HASHTAB_SIZE; i++) {
    	for (p = discover_hashtable[i]; p; p = p->next) {
	    if (p->flags & flags) {
		if (callback)
		    callback(p);
		count++;
	    }
	}
    }
    return count;
}

/* as above, but with an extra (void *)arg passed to the cb */
static int
pmDiscoverTraverseArg(unsigned int flags, void (*callback)(pmDiscover *, void *), void *arg)
{
    int			count = 0, i;
    pmDiscover		*p;

    for (i = 0; i < DISCOVER_HASHTAB_SIZE; i++) {
    	for (p = discover_hashtable[i]; p; p = p->next) {
	    if (p->flags & flags) {
		if (callback)
		    callback(p, arg);
		count++;
	    }
	}
    }
    return count;
}


/*
 * Traverse and purge deleted entries
 * Return count of purged entries.
 */
static int
pmDiscoverPurgeDeleted(void)
{
    int			count = 0, i;
    pmDiscover		*p, *prev, *next;

    for (i = 0; i < DISCOVER_HASHTAB_SIZE; i++) {
	p = discover_hashtable[i];
	prev = NULL;
    	while (p) {
	    next = p->next;

	    if (!(p->flags & DISCOVER_FLAGS_DELETED)) {
		prev = p;
	    } else {
		if (prev)
		    prev->next = next;
		else
		    discover_hashtable[i] = next;
		pmDiscoverInvokeClosedCallBacks(p);
		pmDiscoverFree(p);
		count++;
	    }
	    p = next;
	}
    }

    return count;
}

/*
 * if string ends with given suffix then return pointer
 * to start of suffix in string, else NULL
 */
static char *
strsuffix(char *s, const char *suffix)
{
    int			slen, suflen;
    char		*ret = NULL;

    if (s && suffix) {
    	slen = strlen(s);
	suflen = strlen(suffix);
	if (slen >= suflen) {
	    ret = s + (slen - suflen);
	    if (strncmp(ret, suffix, suflen) != 0)
	    	ret = NULL;
	}
    }
    return ret;
}

/*
 * Discover dirs and archives - add new entries or refresh existing.
 * Call this for each top-level directory. Discovered paths are not
 * automatically monitored. After discovery, need to traverse and
 * activate monitoring using pmDiscoverMonitor.
 */
static int
pmDiscoverArchives(const char *dir, pmDiscoverModule *module, void *arg)
{
    DIR			*dirp;
    struct dirent	*dent;
    struct stat		*s;
    struct stat		statbuf;
    pmDiscover		*a;
    char		*suffix;
    char		path[MAXNAMELEN];
    int			sep = pmPathSeparator();
    int			vol;

    /*
     * note: pmDiscoverLookupAdd sets DISCOVER_FLAGS_NEW
     * if this is a newly discovered archive or directory
     */
    a = pmDiscoverLookupAdd(dir, module, arg);
    a->flags |= DISCOVER_FLAGS_DIRECTORY;

    if ((dirp = opendir(dir)) == NULL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: opendir %s failed: err %d\n",
			    __FUNCTION__, dir, errno);
	return -ESRCH;
    }

    while ((dent = readdir(dirp)) != NULL) {
	if (dent->d_name[0] == '.')
	    continue;
	pmsprintf(path, sizeof(path), "%s%c%s", dir, sep, dent->d_name);

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: readdir found %s\n", __FUNCTION__, path);

	if (stat(path, &statbuf) < 0) {
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: stat failed %s, err %d\n",
				__FUNCTION__, path, errno);
	    continue;
	}

	s = &statbuf;
	if (S_ISREG(s->st_mode)) {
	    if ((suffix = strsuffix(path, ".meta")) != NULL) {
		/*
		 * An uncompressed PCP archive meta file. Track the meta
		 * file - the matching logvol filename varies because logvols
		 * are periodically rolled by pmlogger. Importantly, process all
		 * available metadata to EOF before processing any logvol data.
		 */
		*suffix = '\0'; /* strip suffix from path giving archive name */
		a = pmDiscoverLookupAdd(path, module, arg);

		/*
		 * note: pmDiscoverLookupAdd sets DISCOVER_FLAGS_NEW
		 * if this is a newly discovered archive, otherwise we're
		 * already tracking this archive.
		 */
		a->flags |= DISCOVER_FLAGS_META;
	    }
	    else if ((suffix = __pmLogBaseNameVol(path, &vol)) != NULL && vol >= 0) {
		/*
		 * An archive logvol. This logvol may have been created since
		 * the context was first opened. Update the context maxvol
		 * to be sure pmFetchArchive can switch to it in due course.
		 */
		if ((a = pmDiscoverLookup(path)) != NULL) {
		    a->flags |= DISCOVER_FLAGS_DATAVOL;
		    /* ensure archive context knows about this volume */
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "%s: found logvol %s %s vol=%d\n",
					__FUNCTION__, a->context.name,
					pmDiscoverFlagsStr(a), vol);
		    if (a->ctx >= 0 && vol >= 0) {
			__pmContext *ctxp = __pmHandleToPtr(a->ctx);
			__pmArchCtl *acp = ctxp->c_archctl;

		    	__pmLogAddVolume(acp, vol);
			PM_UNLOCK(ctxp->c_lock);
		    }
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "%s: added logvol %s %s vol=%d\n",
					__FUNCTION__, a->context.name,
					pmDiscoverFlagsStr(a), vol);
		}
	    } else if (pmDebugOptions.discovery) {
		fprintf(stderr, "%s: ignored regular file %s\n",
				__FUNCTION__, path);
	    }
	}
	else if (S_ISDIR(s->st_mode)) {
	    /*
	     * Recurse into subdir
	     */
	    pmDiscoverArchives(path, module, arg);
	}
    }
    if (dirp)
	closedir(dirp);

    /* success */
    return 0;
}

/*
 * Return 1 if monitored path has been deleted.
 * For archives, we only check the meta file because
 * a logvol can be deleted (e.g. via compression when
 * the logvol is rolled to a new volume) without
 * actually deleting the archive.
 */
static int
is_deleted(pmDiscover *p, struct stat *sbuf)
{
    int			ret = 0;

    if (p->flags & DISCOVER_FLAGS_DIRECTORY) {
	if (stat(p->context.name, sbuf) < 0)
	    ret = 1; /* directory has been deleted */
    }

    if (p->flags & (DISCOVER_FLAGS_META|DISCOVER_FLAGS_DATAVOL)) {
    	sds meta = sdsnew(p->context.name);
	meta = sdscat(meta, ".meta");
	if (stat(meta, sbuf) < 0) {
	    /*
	     * Archive metadata file has been deleted (or compressed)
	     * hence consider the archive to be deleted because there
	     * is no more data to logtail.
	     */
	    ret = 1;
	} else {
	    memcpy(&p->statbuf, sbuf, sizeof(struct stat));
	}
	sdsfree(meta);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s: checking %s (%s) -> %s\n", __FUNCTION__,
		p->context.name, pmDiscoverFlagsStr(p), ret ? "DELETED" : "no");
    }

    return ret;
}

static void
fs_change_callBack(uv_fs_event_t *handle, const char *filename, int events, int status)
{
    char		buffer[MAXNAMELEN];
    size_t		bytes = sizeof(buffer) - 1;
    pmDiscover		*p;
    char		*s;
    sds			path;
    struct stat		statbuf;

    uv_fs_event_getpath(handle, buffer, &bytes);
    path = sdsnewlen(buffer, bytes);

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s: event on %s -", __FUNCTION__, path);
	if (events & UV_RENAME)
	    fprintf(stderr, " renamed");
	if (events & UV_CHANGE)
	    fprintf(stderr, " changed");
	fputc('\n', stderr);
    }

    	
    /*
     * Strip ".meta" suffix (if any) and lookup the path. stat and update its
     * flags accordingly. If the path has been deleted, stop its event monitor
     * and free the req buffer, else call the pmDiscovery callback.
     */
    if ((s = strsuffix(path, ".meta")) != NULL)
	*s = '\0';

    p = pmDiscoverLookup(path);
    if (p && pmDebugOptions.discovery) {
	fprintf(stderr, "%s: ---> found entry %s (%s)\n",
			__FUNCTION__, p->context.name, pmDiscoverFlagsStr(p));
    }

    if (p == NULL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: %s lookup failed\n", __FUNCTION__, filename);
    }
    else if (is_deleted(p, &statbuf)) {
	/* path has been deleted. statbuf is invalid */
    	p->flags |= DISCOVER_FLAGS_DELETED;
	memset(&p->statbuf, 0, sizeof(p->statbuf));
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: %s (%s) has been deleted", __FUNCTION__,
			    p->context.name, pmDiscoverFlagsStr(p));
    }

    /*
     * Something in the directory changed - new or deleted archive, or
     * a tracked archive meta data file or logvolume grew
     */
    if (p)
	p->changed(p); /* returns immediately if DISCOVER_FLAGS_DELETED */

    sdsfree(path);
}

/*
 * Monitor path (directory or archive) and invoke given callback when it changes
 * Monitored archive data volumes will create a PCP context for log tailing.
 */
static int
pmDiscoverMonitor(sds path, void (*callback)(pmDiscover *))
{
    discoverModuleData	*data;
    pmDiscover		*p;
    sds			eventfilename;

    if ((p = pmDiscoverLookup(path)) == NULL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: lookup failed for %s\n", __FUNCTION__, path);
	return -ESRCH;
    }
    data = getDiscoverModuleData(p->module);

    /* save the discovery callback to be invoked */
    p->changed = callback;

    /* filesystem event request buffer */
    if ((p->event_handle = malloc(sizeof(uv_fs_event_t))) != NULL) {
	/*
	 * Start monitoring, using given uv loop. Up to the caller to create
	 * a PCP PMAPI context and to fetch/logtail in the changed callback.
	 */
	eventfilename = sdsnew(p->context.name);
	uv_fs_event_init(data->events, p->event_handle);

	if (p->flags & DISCOVER_FLAGS_DIRECTORY) {
	    uv_fs_event_start(p->event_handle, fs_change_callBack, eventfilename,
			    UV_FS_EVENT_WATCH_ENTRY);
	}
	else {
	    /*
	     * Monitor an archive file. This tracks the archive meta file
	     * but the change callback processes both meta and logvol on
	     * every callback (meta before logvol).
	     */
	    eventfilename = sdscat(eventfilename, ".meta");
	    uv_fs_event_start(p->event_handle, fs_change_callBack, eventfilename,
			UV_FS_EVENT_WATCH_ENTRY);
	}

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: added event for %s (%s)\n",
			    __FUNCTION__, eventfilename, pmDiscoverFlagsStr(p));
	sdsfree(eventfilename);
    }

    return 0;
}

/*
 * Byte swabbing code borrowed from libpcp (internal.h and endian.c)
 */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohpmInDom(a)        (a)
#define __ntohpmUnits(a)        (a)
#define __ntohpmID(a)           (a)
#else
#define __ntohpmInDom(a)        ntohl(a)
#define __ntohpmID(a)           ntohl(a)

static pmUnits
__ntohpmUnits(pmUnits units)
{
    unsigned int        x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;
    return units;
}
#endif

static struct {
    unsigned int	flag;
    const char		 *name;
} flags_str[] = {
    { DISCOVER_FLAGS_NEW, "new|" },
    { DISCOVER_FLAGS_DELETED, "deleted|" },
    { DISCOVER_FLAGS_DIRECTORY, "directory|" },
    { DISCOVER_FLAGS_DATAVOL, "datavol|" },
    { DISCOVER_FLAGS_INDEX, "indexvol|" },
    { DISCOVER_FLAGS_META, "metavol|" },
    { DISCOVER_FLAGS_COMPRESSED, "compressed|" },
    { DISCOVER_FLAGS_MONITORED, "monitored|" },
    { DISCOVER_FLAGS_DATAVOL_READY, "datavol-ready|" },
    { DISCOVER_FLAGS_META_IN_PROGRESS, "metavol-in-progress|" },
    { 0, NULL }
};

/*
 * return flags bitmap as a static string
 */
static char *
pmDiscoverFlagsStr(pmDiscover *p)
{
    unsigned int	i;
    static char		buf[128];

    pmsprintf(buf, sizeof(buf), "flags: 0x%04x |", p->flags);
    for (i=0; flags_str[i].name; i++) {
    	if (p->flags & flags_str[i].flag)
	    pmstrncat(buf, sizeof(buf), flags_str[i].name);
    }
    return buf;
}


static void changed_callback(pmDiscover *); /* fwd decl */

static void
created_callback(pmDiscover *p)
{
    if (p->flags &
	(DISCOVER_FLAGS_DELETED| /* fsevents race: creating and deleting */
	 DISCOVER_FLAGS_COMPRESSED| /* compressed archives do not grow */
	 DISCOVER_FLAGS_INDEX))        /* ignore archive index files */
	return;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "CREATED %s, %s\n", p->context.name, pmDiscoverFlagsStr(p));

    if (p->flags & DISCOVER_FLAGS_DIRECTORY) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR directory %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
    else if (p->flags & (DISCOVER_FLAGS_META|DISCOVER_FLAGS_DATAVOL)) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR archive %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
    p->flags &= ~DISCOVER_FLAGS_NEW;
}

static void
discover_event_init(pmDiscover *p, __pmTimestamp *tsp, pmDiscoverEvent *event)
{
    event->timestamp.tv_sec = tsp->sec;
    event->timestamp.tv_nsec = tsp->nsec;
    event->context = p->context;
    event->module = p->module;
    event->data = p;
}

static void
pmDiscoverInvokeClosedCallBacks(pmDiscover *p)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    __pmTimestamp	stamp;
    char		buf[32];
    int			i;

    /* use timestamp from last file modification as closed time */
    discover_modified_timestamp(p, &stamp);

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s closed name %s %s\n", __FUNCTION__,
			timestamp_str(&stamp, buf, sizeof(buf)),
			p->context.source, p->context.name, pmDiscoverFlagsStr(p));
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, &stamp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_closed != NULL)
	    callbacks->on_closed(&event, p->data);
    }
}

static void
pmDiscoverInvokeSourceCallBacks(pmDiscover *p, __pmTimestamp *tsp)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s name %s\n", __FUNCTION__,
			timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source, p->context.name);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_source != NULL)
	    callbacks->on_source(&event, p->data);
    }
}

static void
pmDiscoverInvokeValuesCallBack(pmDiscover *p, __pmTimestamp *tsp, pmResult *r)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s numpmid %d\n", __FUNCTION__,
			timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source, r->numpmid);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_values != NULL)
	    callbacks->on_values(&event, r, p->data);
    }
}

static void
pmDiscoverInvokeMetricCallBacks(pmDiscover *p, __pmTimestamp *tsp, pmDesc *desc,
		int numnames, char **names)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    char		*found = NULL;
    int			i, j, sts;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s name%s", __FUNCTION__,
			timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source, numnames > 0 ? " " : "(none)\n");
	for (i = 0; i < numnames; i++)
	    fprintf(stderr, "[%u/%u] \"%s\"%s", i+1, numnames, names[i],
			    i < numnames - 1 ? ", " : "\n");
	pmPrintDesc(stderr, desc);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if (data->pmids) {
	if (dictFind(data->pmids, &desc->pmid) != NULL)
	    goto out;	/* metric contains an already excluded PMID */
	for (i = 0; i < numnames && !found; i++) {
	    for (j = 0; j < data->exclude_names && !found; j++) {
		if (fnmatch(data->patterns[j], names[i], 0) == 0)
		    found = names[i];
	    }
	}
	if (found) {
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: excluding metric %s\n",
				__FUNCTION__, found);
	    /* add this pmid to the exclusion list and return early */
	    dictAdd(data->pmids, &desc->pmid, NULL);
	    goto out;
	}
    }
    if (data->indoms) {
	if (dictFind(data->indoms, &desc->indom) != NULL)
	    goto out;	/* metric contains an already excluded InDom */
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	__pmContext	*ctxp = __pmHandleToPtr(p->ctx);
	__pmArchCtl	*acp = ctxp->c_archctl;
	char		idstr[32];
	char		errmsg[PM_MAXERRMSGLEN];

	if ((sts = __pmLogAddDesc(acp, desc)) < 0)
	    fprintf(stderr, "%s: failed to add metric descriptor for %s: %s\n",
			    __FUNCTION__,
			    pmIDStr_r(desc->pmid, idstr, sizeof(idstr)),
			    pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	for (i = 0; i < numnames; i++) {
	    if ((sts = __pmLogAddPMNSNode(acp, desc->pmid, names[i])) < 0)
		fprintf(stderr, "%s: failed to add metric name %s for %s: %s\n",
				__FUNCTION__, names[i],
				pmIDStr_r(desc->pmid, idstr, sizeof(idstr)),
				pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_metric != NULL)
	    callbacks->on_metric(&event, desc, numnames, names, p->data);
    }

out:
    for (i = 0; i < numnames; i++)
	free(names[i]);
    free(names);
}

static void
pmDiscoverInvokeInDomCallBacks(pmDiscover *p, int type, __pmTimestamp *tsp, pmInResult *in)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32], inbuf[32];
    int			i, sts = 0;
    pmInResult		full;			/* undelta'd indom */

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s numinst %d indom %s\n",
			__FUNCTION__,
			timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source, in->numinst,
			pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if (data->indoms) {
	if (dictFind(data->indoms, &in->indom) != NULL)
	    goto out;	/* excluded InDom */
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	__pmContext	*ctxp = __pmHandleToPtr(p->ctx);
	__pmArchCtl	*acp = ctxp->c_archctl;
	char		errmsg[PM_MAXERRMSGLEN];
	__pmLogInDom	lid;
	__pmLogInDom	*duplid;

	lid.stamp = *tsp;	/* struct assignment */
	lid.indom = in->indom;
	lid.numinst = in->numinst;
	lid.instlist = in->instlist;
	lid.namelist = in->namelist;
	lid.alloc = 0;

	/*
	 * The indom at this point has been loaded via __pmLogLoadInDom()
	 * in pmDiscoverDecodeMetaInDom() and points into the I/O buffer
	 * that is transient
	 * ... duplicate the indom to make all the storage safe.
	 */
	duplid = __pmDupLogInDom(&lid);

	sts = __pmLogAddInDom(acp, type, duplid, NULL);
	if (sts == PMLOGPUTINDOM_DUP) {
	    if (pmDebugOptions.dev0) {
		fprintf(stderr, "%s: indom %s numinst %d type %s stamp ",
				__FUNCTION__,
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
				in->numinst, __pmLogMetaTypeStr(type));
		__pmPrintTimestamp(stderr, tsp);
		fprintf(stderr, ": duplicate!\n");
	    }
	    __pmFreeLogInDom(duplid);
	    duplid = NULL;
	}
	if (sts < 0)
	    fprintf(stderr, "%s: failed to add indom for %s: %s\n",
			__FUNCTION__,
			pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));

	if (type == TYPE_INDOM_DELTA) {
	    __pmHashNode	*hp;
	    __pmLogInDom	*idp;
	    int			bad = 0;
	    char		**dupnamelist = NULL;	/* DEBUG */
	    int			*dupinstlist = NULL;	/* DEBUG */

	    if ((hp = __pmHashSearch((unsigned int)in->indom, &acp->ac_log->hashindom)) == NULL) {
		fprintf(stderr, "%s: Botch:  indom %s search failed\n",
				__FUNCTION__,
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
		bad = 1;
	    }
	    else {
		int	j;

		idp = (__pmLogInDom *)hp->data;
		/*
		 * idp => would be the delta indom added above via
		 * __pmLogAddInDom(), unless that function returns
		 * PMLOGPUTINDOM_DUP ... so we need to walk the linked
		 * list until we find the one for this delta indom
		 */
		if (pmDebugOptions.dev0) {
		    fprintf(stderr, "walk %s chain: want idp @",
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
		    __pmPrintTimestamp(stderr, tsp);
		    fprintf(stderr, " ...");
		}
		for ( ; idp != NULL; idp = idp->next) {
		    if (pmDebugOptions.dev0) {
			fputc(' ', stderr);
			__pmPrintTimestamp(stderr, &idp->stamp);
			fputc('?', stderr);
		    }
		    if (__pmTimestampCmp(&idp->stamp, tsp) == 0) {
			if (pmDebugOptions.dev0)
			    fprintf(stderr, " bingo isdelta=%d\n", idp->isdelta);
			break;
		    }
		}
		if (idp == NULL) {
		    if (pmDebugOptions.dev0)
			fprintf(stderr, " fail!\n");
		    fprintf(stderr, "%s: Botch: indom %s @ ", __FUNCTION__,
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
		    fprintf(stderr, " failed to find __pmLogInDom @");
		    __pmPrintTimestamp(stderr, tsp);
		    fputc('\n', stderr);
		    /*
		     * No choice ... if we can't undelta the indom there
		     * is nothing we can do ... choose to skip this one
		     * rather than exit(1)!
		     */
		    if (duplid != NULL) {
			__pmFreeLogInDom(duplid);
			duplid = NULL;
		    }
		    PM_UNLOCK(ctxp->c_lock);
		    goto out;
		}
		if (pmDebugOptions.dev0) {
		    __pmLogInDom	*ldp;		/* previous indom */
		    /*
		     * ldp is the one _before_ this (in time) ... must exist
		     * if this one is a delta indom
		     */
		    ldp = idp->next;
		    dupnamelist = (char **)calloc(in->numinst, sizeof(char *));
		    dupinstlist = (int *)calloc(in->numinst, sizeof(int));
		    for (j = 0; j < ldp->numinst; j++) {
			fprintf(stderr, "%d ", ldp->instlist[j]);
		    }
		    fputc('\n', stderr);
/* DEBUG */
		    for (i = 0; i < in->numinst; i++) {
			if (in->namelist[i] == NULL)
			    fprintf(stderr, "-%d ", in->instlist[i]);
			else
			    fprintf(stderr, "+%d \"%s\" ", in->instlist[i], in->namelist[i]);
			dupnamelist[i] = in->namelist[i];
			dupinstlist[i] = in->instlist[i];
		    }
		    fputc('\n', stderr);
		    fprintf(stderr, "%s: delta indom %s before numinst %d "
				    "isdelta %d", __FUNCTION__,
			    pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
			    idp->numinst, idp->isdelta);
		    fflush(stderr);
		}
		if (pmDebugOptions.dev1) {
		    pmDebugOptions.logmeta = pmDebugOptions.desperate = 1;
		}
		__pmLogUndeltaInDom(in->indom, idp);
		if (pmDebugOptions.dev1) {
		    pmDebugOptions.logmeta = pmDebugOptions.desperate = 0;
		}
		if (pmDebugOptions.dev0) {
		    fprintf(stderr, " after numinst %d isdelta %d\n", idp->numinst, idp->isdelta);
		    for (j = 0; j < idp->numinst; j++)
			fprintf(stderr, "%d ", idp->instlist[j]);
		    fputc('\n', stderr);
		    for (i = 0; i < in->numinst; i++) {
			if (dupnamelist[i] == NULL) {
			    /* delete */
			    for (j = 0; j < idp->numinst; j++) {
				if (dupinstlist[i] == idp->instlist[j]) {
				    /* botch, instance still in indom */
				    fprintf(stderr, "%s: Botch: indom %s delete inst %d but \"%s\" still present\n",
						__FUNCTION__,
						pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
						dupinstlist[i], idp->namelist[j]);
				    break;
				    bad = 1;
				}
			    }
			}
			else {
			    /* add */
			    for (j = 0; j < idp->numinst; j++) {
				if (dupinstlist[i] == idp->instlist[j])
				    break;
			    }
			    if (j == idp->numinst) {
				/* botch, instance not in indom */
				fprintf(stderr, "%s: Botch: indom %s add inst %d \"%s\" not present\n",
						__FUNCTION__,
						pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
						dupinstlist[i], dupnamelist[i]);
				bad = 1;
			    }
			}
		    }
		    free(dupnamelist);
		    free(dupinstlist);
		}
	    }
	    if (bad) {
		/*
		 * real snarfoo ... better to add nothing that to add bogus indom info
		 */
		full.indom = in->indom;
		full.numinst = 0;
	    }
	    else {
		full.indom = in->indom;
		full.numinst = idp->numinst;
		full.instlist = idp->instlist;
		full.namelist = idp->namelist;
/* DEBUG */
		for (i = 0; i < idp->numinst; i++) {
		    if (idp->namelist[i] == NULL) {
			fprintf(stderr, "%s: Botch: indom %s inst %d namelist[%d] NULL\n",
				__FUNCTION__,
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
				idp->instlist[i], i);
		    }
		    else if (idp->namelist[i][0] == '\0') {
			fprintf(stderr, "%s: Botch: indom %s inst %d namelist[%d] empty\n",
				__FUNCTION__,
				pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)),
				idp->instlist[i], i);
		    }
		}
	    }
	}
	/*
	 * components of duplid are stashed away in libpcp archive hashed
	 * indom structures, so just duplid to be free'd here
	 */
	if (duplid != NULL) {
	    free(duplid);
	    duplid = NULL;
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_indom != NULL) {
	    if (type == TYPE_INDOM_DELTA)
		callbacks->on_indom(&event, &full, p->data);
	    else
		callbacks->on_indom(&event, in, p->data);
	}
    }

out:
    /* do not free the pmInResult - buffer is managed by caller */
    return;
}

static void
pmDiscoverInvokeLabelsCallBacks(pmDiscover *p, __pmTimestamp *tsp,
		int ident, int type, pmLabelSet *sets, int nsets)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32], idbuf[64];
    int			i, sts = -EAGAIN; /* free labelsets after callbacks */

    if (pmDebugOptions.discovery) {
	__pmLabelIdentString(ident, type, idbuf, sizeof(idbuf));
	fprintf(stderr, "%s[%s]: %s ID %s type %s\n",
			__FUNCTION__, timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source, idbuf, __pmLabelTypeString(type));
	pmPrintLabelSets(stderr, ident, type, sets, nsets);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if ((type & PM_LABEL_ITEM) && data->pmids) {
	if (dictFind(data->pmids, &ident) != NULL)
	    goto out;	/* text from an already excluded InDom */
    }
    if ((type & (PM_LABEL_INDOM|PM_LABEL_INSTANCES)) && data->indoms) {
	if (dictFind(data->indoms, &ident) != NULL)
	    goto out;	/* text from an already excluded InDom */
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	__pmContext	*ctxp = __pmHandleToPtr(p->ctx);
	__pmArchCtl	*acp = ctxp->c_archctl;
	char		errmsg[PM_MAXERRMSGLEN];

	if ((sts = __pmLogAddLabelSets(acp, tsp, type, ident, nsets, sets)) < 0)
	    fprintf(stderr, "%s: failed to add log labelset: %s\n",
			__FUNCTION__, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	PM_UNLOCK(ctxp->c_lock);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_labels != NULL)
	    callbacks->on_labels(&event, ident, type, sets, nsets, p->data);
    }

out:
    if (sts < 0)
	pmFreeLabelSets(sets, nsets);
}

static void
pmDiscoverInvokeTextCallBacks(pmDiscover *p, __pmTimestamp *tsp,
		int ident, int type, char *text)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i, sts;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s ", __FUNCTION__,
			timestamp_str(tsp, buf, sizeof(buf)),
			p->context.source);
	if (type & PM_TEXT_INDOM)
	    fprintf(stderr, "type indom %s ", pmInDomStr((pmInDom)ident));
	else
	    fprintf(stderr, "type pmid %s ", pmIDStr((pmID)ident));
	if (type & PM_TEXT_ONELINE)
	    fprintf(stderr, "oneline: \"%s\"\n", text);
	else
	    fprintf(stderr, "text:\n%s\n", text);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if ((type & PM_TEXT_PMID) && data->pmids) {
	if (dictFind(data->pmids, &ident) != NULL)
	    goto out;	/* text from an already excluded InDom */
    }
    if ((type & PM_TEXT_INDOM) && data->indoms) {
	if (dictFind(data->indoms, &ident) != NULL)
	    goto out;	/* text from an already excluded InDom */
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	__pmContext	*ctxp = __pmHandleToPtr(p->ctx);
	__pmArchCtl	*acp = ctxp->c_archctl;
	char		errmsg[PM_MAXERRMSGLEN];

	if ((sts = __pmLogAddText(acp, ident, type, text)) < 0)
	    fprintf(stderr, "%s: failed to add %u text for %u: %s\n",
	                __FUNCTION__, type, ident,
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	PM_UNLOCK(ctxp->c_lock);
    }

    discover_event_init(p, tsp, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_text != NULL)
	    callbacks->on_text(&event, ident, type, text, p->data);
    }

out:
    free(text);
}

static void
pmDiscoverNewSource(pmDiscover *p, int context)
{
    pmLabelSet		*labelset = NULL;
    pmLogLabel		loglabel = {0};
    __pmTimestamp	stamp;
    unsigned char	hash[20];
    char		buf[PM_MAXLABELJSONLEN];
    char		*host, hostname[MAXHOSTNAMELEN];
    int			len, nsets;

    p->ctx = context;
    host = pmGetContextHostName_r(context, hostname, sizeof(hostname));
    if ((nsets = pmGetContextLabels(&labelset)) > 0) {
	pmwebapi_source_hash(hash, labelset->json, labelset->jsonlen);
    } else {
	/* fallback for older archives without any labels at all */
	len = pmsprintf(buf, sizeof(buf), "{\"hostname\":\"%s\"}", host);
	nsets = __pmAddLabels(&labelset, buf, 0);
	pmwebapi_source_hash(hash, buf, len);
    }
    p->context.source = pmwebapi_hash_sds(NULL, hash);
    p->context.hostname = sdsnew(host);
    p->context.labelset = labelset;

    /* use timestamp from archive start time */
    (void)pmGetArchiveLabel(&loglabel);
    stamp.sec = loglabel.start.tv_sec;
    stamp.nsec = loglabel.start.tv_nsec;

    /* inform utilities that a source has been discovered */
    pmDiscoverInvokeSourceCallBacks(p, &stamp);
}

static char *
archive_dir_lock_path(pmDiscover *p)
{
    char	path[MAXNAMELEN], lockpath[MAXNAMELEN];
    int		sep = pmPathSeparator();

    pmstrncpy(path, sizeof(path), p->context.name);
    pmsprintf(lockpath, sizeof(lockpath), "%s%c%s", dirname(path), sep, "lock");
    return strndup(lockpath, sizeof(lockpath));
}

/*
 * When we get a context labelset, we need to store it in 'p' and
 * also update the source identifier (pmSID) - effectively making
 * a new source.
 */
static void
pmDiscoverResetSource(pmDiscover *p, __pmTimestamp *stamp, pmLabelSet *labelset)
{
    unsigned char	hash[20];
    sds			source;

    pmwebapi_source_hash(hash, labelset->json, labelset->jsonlen);
    source = pmwebapi_hash_sds(NULL, hash);
    if (sdscmp(source, p->context.source) == 0) {
       sdsfree(source);
    } else {
       sdsfree(p->context.source);
       p->context.source = source;
       if (p->context.labelset)
           pmFreeLabelSets(p->context.labelset, 1);
       p->context.labelset = __pmDupLabelSets(labelset, 1);
       pmDiscoverInvokeSourceCallBacks(p, stamp);
    }
}

void
pmDiscoverStreamEnd(const char *path)
{
    pmDiscover		*p, *next, *prev = NULL;
    unsigned int	i;

    i = strhash(path, DISCOVER_HASHTAB_SIZE);

    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: name=%s\n", __FUNCTION__, path);

    p = discover_hashtable[i];
    while (p) {
	next = p->next;
	if (path == p->context.name || strcmp(p->context.name, path) == 0) {
	    if (prev)
		prev->next = next;
	    else
		discover_hashtable[i] = next;
	    pmDiscoverInvokeClosedCallBacks(p);
	    pmDiscoverFree(p);
	    break;
	}
	prev = p;
    }
}

static void
copy_loglabel(__pmLogLabel *l1, __pmLogLabel *l2)
{
    *l2 = *l1;	/* struct copy */
    if (l1->hostname)
	l2->hostname = strdup(l1->hostname);
    if (l1->timezone)
	l2->timezone = strdup(l1->timezone);
    if (l1->zoneinfo)
	l2->zoneinfo = strdup(l1->zoneinfo);
}

pmDiscover *
pmDiscoverStreamLabel(const char *path, __pmLogLabel *label,
                pmDiscoverModule *module, void *arg)
{
    discoverModuleData	*data = getDiscoverModuleData(module);
    __pmContext		*ctxp;
    __pmLogCtl		*lcp;
    pmDiscover 		*p;
    sds			msg;
    int			sts, type;

    mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_NEW_CONTEXTS]);

    /* delete hash table entry if one already exists */
    pmDiscoverStreamEnd(path);

    /* insert hash table entry for this new archive */
    if ((p = pmDiscoverLookupAdd(path, module, arg)) == NULL)
        return p;

    /* one-time context initialization on the first metadata buffer */
    type = p->context.type | PM_CTXFLAG_STREAMING_WRITER;
    if ((sts = pmNewContext(type, p->context.name)) < 0) {
	infofmt(msg, "%s: %s failed for %s: %s\n", __FUNCTION__,
			 "pmNewContext", p->context.name, pmErrStr(sts));
	moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	/* no further processing for this archive */
	return NULL;
    }
    p->ctx = sts;

    ctxp = __pmHandleToPtr(p->ctx);
    PM_UNLOCK(ctxp->c_lock);
    lcp = ctxp->c_archctl->ac_log;
    copy_loglabel(label, &lcp->label);

    pmDiscoverNewSource(p, p->ctx);
    return p;
}

/*
 * Process metadata records from a streamed input buffer.
 */
static int
stream_metadata(pmDiscover *p, int mtype, uint32_t *buf, size_t len)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    __pmTimestamp	stamp;
    pmDesc		desc;
    int			nsets;
    int			type, id; /* pmID or pmInDom */
    int			nnames;
    char		**names;
    char		*buffer;
    pmInResult		inresult;
    pmLabelSet		*labelset = NULL;
    int			sts;

    switch (mtype) {
    case TYPE_DESC:
	nnames = 0;
	names = NULL;
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_DESC]);
	sts = pmDiscoverDecodeMetaDesc(buf, len, &desc, &nnames, &names);
	if (sts < 0) {
	    if (pmDebugOptions.discovery || pmDebugOptions.logmeta)
		fprintf(stderr, "%s failed: error=%d %s\n",
			"pmDiscoverDecodeMetaDesc", sts, pmErrStr(sts));
	    return sts;
	}
	__pmGetTimestamp(&stamp);
	pmDiscoverInvokeMetricCallBacks(p, &stamp, &desc, nnames, names);
	break;

    case TYPE_INDOM:
    case TYPE_INDOM_V2:
    case TYPE_INDOM_DELTA:
	/* decode indom, indom_v2 or indom_delta result from buffer */
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_INDOM]);
	sts = pmDiscoverDecodeMetaInDom((__int32_t *)buf, len, mtype, &stamp, &inresult);
	if (sts < 0) {
	    if (pmDebugOptions.discovery || pmDebugOptions.logmeta)
		fprintf(stderr, "%s failed: err=%d %s\n",
			"pmDiscoverDecodeMetaInDom", sts, pmErrStr(sts));
	    return sts;
	}
	pmDiscoverInvokeInDomCallBacks(p, mtype, &stamp, &inresult);
	free(inresult.namelist);
	break;

    case TYPE_LABEL:
    case TYPE_LABEL_V2:
	/* decode labelset from buffer */
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_LABEL]);
	sts = pmDiscoverDecodeMetaLabelSet(buf, len, mtype, &stamp, &type, &id, &nsets, &labelset);
	if (sts < 0) {
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s failed: err=%d %s\n",
			"pmDiscoverDecodeMetaLabelSet", sts, pmErrStr(sts));
	    return sts;
	}
	if ((type & PM_LABEL_CONTEXT))
	    pmDiscoverResetSource(p, &stamp, labelset);
	pmDiscoverInvokeLabelsCallBacks(p, &stamp, id, type, labelset, nsets);
	break;

    case TYPE_TEXT:
	/* decode help text from buffer */
	buffer = NULL;
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_HELPTEXT]);
	sts = pmDiscoverDecodeMetaHelpText(buf, len, &type, &id, &buffer);
	if (sts < 0) {
	    if (pmDebugOptions.discovery || pmDebugOptions.logmeta)
		fprintf(stderr, "%s failed: err=%d %s\n",
			"pmDiscoverDecodeMetaHelpText", sts, pmErrStr(sts));
	    return sts;
	}
	__pmGetTimestamp(&stamp);
	pmDiscoverInvokeTextCallBacks(p, &stamp, id, type, buffer);
	break;

    default:
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: type %s, len = %zu\n", __FUNCTION__,
		mtype == (PM_LOG_MAGIC | PM_LOG_VERS02) ? "PM_LOG_MAGICv2"
		: (type == (PM_LOG_MAGIC | PM_LOG_VERS03) ? "PM_LOG_MAGICv3"
		: "UNKNOWN"), len);
	return PM_ERR_LOGREC;
    }
    return 0;
}

/*
 * Process metadata records from a streamed input buffer,
 * taking care to preserve remaining unprocessed input at
 * the end (yet-to-arrive content should complete this).
 */
int
pmDiscoverStreamMeta(pmDiscover *p, const char *content, size_t length)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    ssize_t		sts;
    size_t		rlen, bytes;
    char		*buf;
    __pmLogHdr		hdr;
    sds			msg;

    mmv_inc(data->map, data->metrics[DISCOVER_META_STREAMING]);

    if (p->ctx < 0) {
	/* PMAPI context initialization is a pre-requisite here */
	infofmt(msg, "%s: no context exists for %s data\n",
			__FUNCTION__, p->context.name);
	moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	/* no further processing for this archive */
	return PM_ERR_NOCONTEXT;
    }

    p->metavol = sdscatlen(p->metavol, content, length);
    bytes = sdslen(p->metavol);
    buf = p->metavol;

    while (bytes > 0) {
	if (bytes < sizeof(__pmLogHdr) + sizeof(uint32_t))
	    break;     /* insufficient data to continue */

	memcpy(&hdr, buf, sizeof(__pmLogHdr));
	hdr.len = ntohl(hdr.len);
	hdr.type = ntohl(hdr.type);
	/* record length: see __pmLogLoadMeta() */
	if ((size_t)hdr.len <= sizeof(__pmLogHdr) + sizeof(uint32_t)) {
	    infofmt(msg, "Unknown metadata record type %d (0x%02x), len=%d\n",
			hdr.type, hdr.type, hdr.len);
	    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	    return -EINVAL;
	}
	rlen = (size_t)hdr.len - sizeof(__pmLogHdr) - sizeof(uint32_t);
	if (bytes < (size_t)hdr.len) {
	    mmv_inc(data->map, data->metrics[DISCOVER_META_PARTIAL_READS]);
	    break;     /* insufficient data to continue */
	}

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "Streaming metadata record %d(%zu) bytes, type %s\n",
			hdr.len, rlen, __pmLogMetaTypeStr(hdr.type));

	buf += sizeof(__pmLogHdr); /* skip over header length and type */

	if ((sts = stream_metadata(p, hdr.type, (uint32_t *)buf, rlen)) < 0) {
	    infofmt(msg, "Failed metadata record type %d (0x%02x), len=%d\n",
			hdr.type, hdr.type, hdr.len);
	    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	    return sts;
	}

	assert(bytes >= (size_t)hdr.len);
	bytes -= (size_t)hdr.len;
	buf += (rlen + sizeof(uint32_t));
    }

    if (bytes > 0) {
	/* keep just the remaining part for subsequent processing */
	msg = p->metavol;
	p->metavol = sdsnewlen(buf, bytes);
	sdsfree(msg);
    } else {
	/* buffer was completely processed, clear for later reuse */
	sdsclear(p->metavol);
    }

    return 0;
}

static void
bump_logvol_decode_stats(discoverModuleData *data, pmResult *r)
{
    if (r->numpmid == 0)
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_MARK_RECORD]);
    else if (r->numpmid < 0)
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_RESULT_ERRORS]);
    else {
	uint64_t pmids = (uint64_t)r->numpmid;
	mmv_add(data->map, data->metrics[DISCOVER_DECODE_RESULT_PMIDS], &pmids);
	mmv_inc(data->map, data->metrics[DISCOVER_DECODE_RESULT]);
    }
}

/*
 * Process data volume result records from a streamed input buffer,
 * taking care to preserve remaining unprocessed input at the end
 * (yet-to-arrive content expected to complete this in due course).
 */
int
pmDiscoverStreamData(pmDiscover *p, const char *content, size_t length)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    __pmTimestamp	stamp;
    __pmContext		*ctxp;
    __pmResult		*rp;
    __pmPDU		*pb;
    pmResult		*result;
    uint32_t		buflen;
    size_t		bytes;
    char		*buf;
    sds			msg;
    int			sts;

    mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_STREAMING]);

    if (p->ctx < 0) {
	/* PMAPI context initialization is a pre-requisite here */
	infofmt(msg, "%s: no context exists for %s data\n",
			__FUNCTION__, p->context.name);
	moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	/* no further processing for this archive */
	return PM_ERR_NOCONTEXT;
    }
    pmUseContext(p->ctx);
    ctxp = __pmHandleToPtr(p->ctx);
    PM_UNLOCK(ctxp->c_lock);

    p->datavol = sdscatlen(p->datavol, content, length);
    bytes = sdslen(p->datavol);
    buf = p->datavol;

    while (bytes > 0) {
	if (bytes <= sizeof(uint32_t) + sizeof(uint32_t))
	    break;     /* insufficient data to continue */

	buflen = ntohl(*(uint32_t *)buf); /* includes header and trailer */
	if (buflen <= sizeof(uint32_t) + sizeof(uint32_t)) {
	    infofmt(msg, "Invalid %u byte result buffer\n", buflen);
	    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	    return -EINVAL;
	}

	if (bytes < buflen) {
	    mmv_inc(data->map, data->metrics[DISCOVER_META_PARTIAL_READS]);
	    break;     /* insufficient data to continue */
	}

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "Streaming result %u bytes\n", buflen);

	if ((pb = __pmFindPDUBuf(buflen + sizeof(uint32_t))) == NULL) {
	    sts = -oserror();
	    infofmt(msg, "Failed to find %zu(%u) byte buffer: %s\n",
			    sizeof(uint32_t) + buflen, buflen, pmErrStr(sts));
	    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	    return sts;
	}
	pb[0] = buflen + sizeof(uint32_t);
	pb[1] = pb[2] = 0;	/* type, from */
	memcpy(&pb[3], buf + sizeof(uint32_t), buflen - (sizeof(uint32_t) * 2));
	sts = __pmDecodeResult_ctx(ctxp, pb, &rp);
	__pmUnpinPDUBuf(pb);
	if (sts < 0) {
	    infofmt(msg, "Failed decoding %u bytes result buffer: %s\n",
			    buflen, pmErrStr(sts));
	    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
	    return sts;
	}

	result = __pmOffsetResult(rp);
	stamp.sec = result->timestamp.tv_sec;
	stamp.nsec = result->timestamp.tv_nsec;
	bump_logvol_decode_stats(data, result);
	pmDiscoverInvokeValuesCallBack(p, &stamp, result);
	pmFreeResult(result);

	assert(bytes >= (size_t)buflen);
	bytes -= buflen;
	buf += buflen;
    }

    if (bytes > 0) {
	/* keep just the remaining part for subsequent processing */
	msg = p->datavol;
	p->datavol = sdsnewlen(buf, bytes);
	sdsfree(msg);
    } else {
	/* buffer was completely processed, clear for later reuse */
	sdsclear(p->datavol);
    }

    return 0;
}

/*
 * Process metadata records until EOF. That can span multiple
 * callbacks if we get a partial record read.
 */
static void
process_metadata(pmDiscover *p)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    int			partial = 0;
    __pmTimestamp	stamp;
    pmDesc		desc;
    off_t		off;
    char		*buffer;
    int			e, nb, len, nsets;
    int			type, id; /* pmID or pmInDom */
    int			nnames;
    char		**names;
    pmInResult		inresult;
    pmLabelSet		*labelset = NULL;
    __pmLogHdr		hdr;
    sds			msg;
    char		*lock_path;
    int			deleted;
    struct stat		sbuf;
    static uint32_t	*buf = NULL;
    static int		buflen = 0;

    /*
     * Read all metadata records from current offset though to EOF
     * and call all registered callbacks. Avoid processing logvol
     * callbacks until all metadata records have been read.
     */
    p->flags |= DISCOVER_FLAGS_META_IN_PROGRESS;
    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: %s in progress %s\n", __FUNCTION__,
			p->context.name, pmDiscoverFlagsStr(p));
    mmv_inc(data->map, data->metrics[DISCOVER_META_CALLBACKS]);
    lock_path = archive_dir_lock_path(p);
    for (;;) {
	if (lock_path && access(lock_path, F_OK) == 0)
	    break;
	mmv_inc(data->map, data->metrics[DISCOVER_META_LOOPS]);
	off = lseek(p->fd, 0, SEEK_CUR);
	nb = read(p->fd, &hdr, sizeof(__pmLogHdr));

	deleted = is_deleted(p, &sbuf);
	if (nb <= 0 || deleted) {
	    /*
	     * We've encountered EOF, an error or file deletion,
	     * however may still be part way through a record.
	     */
	    if (deleted)
	    	p->flags |= DISCOVER_FLAGS_DELETED;
	    break;
	}

	if (nb != sizeof(__pmLogHdr)) {
	    /* rewind so we can wait for more data on the next change CallBack */
	    lseek(p->fd, off, SEEK_SET);
	    partial = 1;
	    mmv_inc(data->map, data->metrics[DISCOVER_META_PARTIAL_READS]);
	    continue;
	}

	hdr.len = ntohl(hdr.len);
	hdr.type = ntohl(hdr.type);
	if (hdr.len <= 0) {
	    /* rewind and wait for more data, as above */
	    lseek(p->fd, off, SEEK_SET);
	    partial = 1;
	    mmv_inc(data->map, data->metrics[DISCOVER_META_PARTIAL_READS]);
	    continue;
	}

	/* record length: see __pmLogLoadMeta() */
	len = hdr.len - (int)sizeof(__pmLogHdr); /* includes trailer */
	if (len <= 0) {
	    infofmt(msg, "Unknown metadata record type %d (0x%02x), len=%d\n",
		    hdr.type, hdr.type, len);
	    moduleinfo(p->module, PMLOG_WARNING, msg, p->data);
	    continue; /* skip this one */
	}

	if (len > buflen) {
	    buflen = len + 4096;
	    buf = (uint32_t *)realloc(buf, buflen);
	}

	/* read the body + trailer */
	if ((nb = read(p->fd, buf, len)) != len) {
	    /* rewind and wait for more data, as above */
	    lseek(p->fd, off, SEEK_SET);
	    partial = 1;
	    mmv_inc(data->map, data->metrics[DISCOVER_META_PARTIAL_READS]);
	    continue;
	}

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "Log metadata read len %4d type %s\n", len, __pmLogMetaTypeStr(hdr.type));

	switch (hdr.type) {
	case TYPE_DESC:
	    /* decode pmDesc result from PDU buffer */
	    nnames = 0;
	    names = NULL;
	    mmv_inc(data->map, data->metrics[DISCOVER_DECODE_DESC]);
	    if ((e = pmDiscoverDecodeMetaDesc(buf, len, &desc, &nnames, &names)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaDesc", e, pmErrStr(e));
		break;
	    }
	    /* use timestamp from last modification */
	    discover_modified_timestamp(p, &stamp);
	    pmDiscoverInvokeMetricCallBacks(p, &stamp, &desc, nnames, names);
	    break;

	case TYPE_INDOM:
	case TYPE_INDOM_V2:
	case TYPE_INDOM_DELTA:
	    /* decode indom, indom_v2 or indom_delta result from buffer */
	    mmv_inc(data->map, data->metrics[DISCOVER_DECODE_INDOM]);
	    if ((e = pmDiscoverDecodeMetaInDom((__int32_t *)buf, len, hdr.type, &stamp, &inresult)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaInDom", e, pmErrStr(e));
		break;
	    }
	    pmDiscoverInvokeInDomCallBacks(p, hdr.type, &stamp, &inresult);
	    free(inresult.namelist);
	    break;

	case TYPE_LABEL:
	case TYPE_LABEL_V2:
	    /* decode labelset from buffer */
	    mmv_inc(data->map, data->metrics[DISCOVER_DECODE_LABEL]);
	    if ((e = pmDiscoverDecodeMetaLabelSet(buf, len, hdr.type, &stamp, &type, &id, &nsets, &labelset)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaLabelSet", e, pmErrStr(e));
		break;
	    }
	    if ((type & PM_LABEL_CONTEXT))
		pmDiscoverResetSource(p, &stamp, labelset);
	    pmDiscoverInvokeLabelsCallBacks(p, &stamp, id, type, labelset, nsets);
	    break;

	case TYPE_TEXT:
	    /* decode help text from buffer */
	    buffer = NULL;
	    mmv_inc(data->map, data->metrics[DISCOVER_DECODE_HELPTEXT]);
	    if ((e = pmDiscoverDecodeMetaHelpText(buf, len, &type, &id, &buffer)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaHelpText", e, pmErrStr(e));
		break;
	    }
	    /* use timestamp from last modification */
	    discover_modified_timestamp(p, &stamp);
	    pmDiscoverInvokeTextCallBacks(p, &stamp, id, type, buffer);
	    break;

	default:
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: type %s, len = %d\n", __FUNCTION__,
			hdr.type == (PM_LOG_MAGIC | PM_LOG_VERS02) ? "PM_LOG_MAGICv2"
			: (hdr.type == (PM_LOG_MAGIC | PM_LOG_VERS03) ? "PM_LOG_MAGICv3"
			: "UNKNOWN"), len);
	    break;
	}
    }

    if (partial == 0)
	/* flag that all available metadata has now been read */
	p->flags &= ~DISCOVER_FLAGS_META_IN_PROGRESS;

    if (lock_path)
    	free(lock_path);

    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: completed, partial=%d %s %s\n",
			__FUNCTION__, partial, p->context.name, pmDiscoverFlagsStr(p));
}

/*
 * Fetch metric values to EOF and call all registered callbacks.
 * Always process metadata thru to EOF before any logvol data.
 */
static void
process_logvol(pmDiscover *p)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    __pmTimestamp	stamp;
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    pmResult		*r = NULL;
    char		*lock_path;
    int			oldcurvol;
    int			sts;

    mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_CALLBACKS]);
    lock_path = archive_dir_lock_path(p);
    for (;;) {
	if (lock_path && access(lock_path, F_OK) == 0)
	    break;
	mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_LOOPS]);
	pmUseContext(p->ctx);
	ctxp = __pmHandleToPtr(p->ctx);
	acp = ctxp->c_archctl;
	oldcurvol = acp->ac_curvol;
	PM_UNLOCK(ctxp->c_lock);

	r = NULL; /* so we know if pmFetchArchive() assigned it */
	if ((sts = pmFetchArchive(&r)) < 0) {
	    /* err handling to skip to the next vol */
	    ctxp = __pmHandleToPtr(p->ctx);
	    acp = ctxp->c_archctl;
	    if (oldcurvol < acp->ac_curvol) {
	    	__pmLogChangeVol(acp, acp->ac_curvol);
		acp->ac_offset = 0; /* __pmLogFetch will fix it up */
		mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_CHANGE_VOL]);
	    }
	    PM_UNLOCK(ctxp->c_lock);

	    if (sts == PM_ERR_EOL) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s: %s end of archive reached\n",
				    __FUNCTION__, p->context.name);

		/* succesfully processed to current end of log */
		break;
	    } else {
		/* 
		 * This log vol was probably deleted (likely compressed)
		 * under our feet. Try and skip to the next volume.
		 * We hold the context lock during error recovery here.
		 */
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s: %s fetch failed:%s\n",
				    __FUNCTION__, p->context.name,
				    pmErrStr(sts));
	    }

	    /* we are done - return and wait for another callback */
	    r = NULL;
	    break;
	}

	/*
	 * Fetch succeeded - call the values callback and continue
	 */
	if (pmDebugOptions.discovery) {
	    char		tbuf[64], bufs[64];

	    fprintf(stderr, "%s: %s FETCHED @%s [%s] %d metrics\n",
		    __FUNCTION__, p->context.name,
		    timespec_str(&r->timestamp, tbuf, sizeof(tbuf)),
		    timespec_stream_str(&r->timestamp, bufs, sizeof(bufs)),
		    r->numpmid);
	}

	/*
	 * Consider persistently saving current timestamp so that after a
	 * restart pmproxy can resume where it left off for each archive.
	 */
	stamp.sec = r->timestamp.tv_sec;
	stamp.nsec = r->timestamp.tv_nsec;
	bump_logvol_decode_stats(data, r);
	pmDiscoverInvokeValuesCallBack(p, &stamp, r);
	pmFreeResult(r);
	r = NULL;
    }

    if (r) {
	stamp.sec = r->timestamp.tv_sec;
	stamp.nsec = r->timestamp.tv_nsec;
	bump_logvol_decode_stats(data, r);
	pmDiscoverInvokeValuesCallBack(p, &stamp, r);
	pmFreeResult(r);
    }

    /* datavol is now up-to-date and at EOF */
    p->flags &= ~DISCOVER_FLAGS_DATAVOL_READY;

    if (lock_path)
    	free(lock_path);
}

static void
pmDiscoverInvokeCallBacks(pmDiscover *p)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    struct stat		sbuf;
    int			sts, type;
    sds			msg;
    sds			metaname;

    if (!(p->flags & DISCOVER_FLAGS_DELETED) && is_deleted(p, &sbuf))
    	p->flags |= DISCOVER_FLAGS_DELETED;
    if (p->flags & DISCOVER_FLAGS_DELETED)
    	return; /* ignore deleted archive */

    if (p->ctx < 0) {
	/*
	 * once off initialization on the first event
	 */
	if (p->flags & (DISCOVER_FLAGS_DATAVOL | DISCOVER_FLAGS_META)) {
	    struct timespec	after = {0, 1};
	    struct timespec	tp;

	    /*
	     * create the PMAPI context (once off) ...
	     * position at last volume (a) to reduce the need for
	     * possible decompression in the case of an active
	     * archive being concurrently written while earlier volumes
	     * have been compressed, and (b) we're interested in the
	     * last timestamp => the last volume
	     */
	    type = p->context.type | PM_CTXFLAG_LAST_VOLUME;
	    if ((sts = pmNewContext(type, p->context.name)) < 0) {
		if (sts == -ENOENT) {
		    /* newly deleted archive */
		    p->flags |= DISCOVER_FLAGS_DELETED;
		}
		else {
		    /*
		     * Likely an early callback on a new (still empty) archive.
		     * If so, just ignore the callback and don't log any scary
		     * looking messages. We'll get another CB soon.
		     */
		    if (sts != PM_ERR_NODATA || pmDebugOptions.desperate) {
			infofmt(msg, "%s failed for %s: %s\n",
				"pmNewContext", p->context.name, pmErrStr(sts));
			moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		    }
		}
		/* no further processing for this archive */
		return;
	    }
	    mmv_inc(data->map, data->metrics[DISCOVER_LOGVOL_NEW_CONTEXTS]);
	    p->ctx = sts;

	    if ((sts = pmGetArchiveEnd(&tp)) < 0) {
		mmv_inc(data->map, data->metrics[DISCOVER_ARCHIVE_END_FAILED]);
		/* Less likely, but could still be too early (as above) */
		infofmt(msg, "%s failed for %s: %s\n",
			"pmGetArchiveEnd", p->context.name, pmErrStr(sts));
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		pmDestroyContext(p->ctx);
		p->ctx = -1;

		return;
	    }

	    /*
	     * We have a valid pmapi context. Initialize context state
	     * and invoke registered source callbacks.
	     */
	    pmDiscoverNewSource(p, p->ctx);

	    /*
	     * Seek to end of archive for logvol data (see notes in
	     * process_logvol routine also).
	     */
	    pmSetMode(PM_MODE_FORW, &tp, &after);

	    /*
	     * For archive meta files, p->fd is the direct file descriptor
	     * and we pre-scan all existing metadata. Note: we do NOT scan
	     * pre-existing logvol data (see pmSetMode above)
	     */
	    metaname = sdsnew(p->context.name);
	    metaname = sdscat(metaname, ".meta");
	    if ((p->fd = open(metaname, O_RDONLY)) < 0) {
		if (p->fd == -ENOENT)
		    p->flags |= DISCOVER_FLAGS_DELETED;
		else {
		    infofmt(msg, "%s failed for %s: %s\n",
				 "open", metaname, osstrerror());
		    moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		}
		sdsfree(metaname);
		return;
	    }
	    /* pre-process all existing metadata */
	    process_metadata(p);
	    sdsfree(metaname);
	}
    }

    /*
     * Now call the registered callbacks, if any, for this path
     */
    if (p->flags & (DISCOVER_FLAGS_DATAVOL | DISCOVER_FLAGS_DATAVOL_READY)) {
	/*
	 * datavol has data ready (either now or earlier during a metadata CB)
	 */
	p->flags |= DISCOVER_FLAGS_DATAVOL_READY;

	if (p->flags & DISCOVER_FLAGS_META_IN_PROGRESS) {
	    /*
	     * metadata read in progress - delay reading logvol until finished.
	     */
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: datavol ready, but metadata read in progress\n", __FUNCTION__);
	}
    }

    if (p->flags & DISCOVER_FLAGS_META) {
	/* process new metadata, if any */
	process_metadata(p);
    }

    if ((p->flags & DISCOVER_FLAGS_META_IN_PROGRESS) == 0) {
	/* no metadata read in progress, so process new datavol data, if any */
	process_logvol(p);
    }
}

static void
print_callback(pmDiscover *p)
{
    if (p->flags & DISCOVER_FLAGS_DIRECTORY) {
	fprintf(stderr, "    DIRECTORY %s %s\n",
			p->context.name, pmDiscoverFlagsStr(p));
    }
    else {
	__pmContext *ctxp;
	__pmArchCtl *acp;

	if (p->ctx >= 0 && (ctxp = __pmHandleToPtr(p->ctx)) != NULL) {
	    acp = ctxp->c_archctl;
	    fprintf(stderr, "    ARCHIVE %s fd=%d ctx=%d maxvol=%d ac_curvol=%d ac_offset=%ld %s\n",
		p->context.name, p->fd, p->ctx, acp->ac_log->maxvol, acp->ac_curvol,
		acp->ac_offset, pmDiscoverFlagsStr(p));
	    PM_UNLOCK(ctxp->c_lock);
	} else {
	    /* no context yet - probably DISCOVER_FLAGS_NEW */
	    fprintf(stderr, "    ARCHIVE %s fd=%d ctx=%d %s\n",
		p->context.name, p->fd, p->ctx, pmDiscoverFlagsStr(p));
	}
    }
}

/*
 * p is a tracked archive and arg is a directory path.
 * If p is in the directory, call its callbacks to
 * process metadata and logvol data. This allows better
 * scalability because we only process archives in the
 * directories that have changed.
 */
static void
directory_changed_cb(pmDiscover *p, void *arg)
{
    char		*dirpath = (char *)arg;
    int			dlen = strlen(dirpath);

    if (strncmp(p->context.name, dirpath, dlen) == 0) {
    	/* archive is in this directory - process its metadata and logvols */
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: archive %s is in dir %s\n",
			__FUNCTION__, p->context.name, dirpath);
	pmDiscoverInvokeCallBacks(p);
    }
}

static void
changed_callback(pmDiscover *p)
{
    discoverModuleData	*data = getDiscoverModuleData(p->module);
    uint64_t		throttle, purged;
    time_t		now;
    char		buf[32];

    /* dynamic callback throttle - to improve scaling */
    if ((throttle = monitored / MAX_INFLIGHT_MON) < 1)
	throttle = 1;
    mmv_set(data->map, data->metrics[DISCOVER_THROTTLE], &throttle);

    time(&now);
    if ((p->flags & DISCOVER_FLAGS_NEW) == 0) {
	if (now - p->lastcb < throttle ||
	    keySlotsInflightRequests(data->slots) > MAX_INFLIGHT_REQ) {
	    mmv_inc(data->map, data->metrics[DISCOVER_THROTTLE_CALLBACKS]);
	    return; /* throttled */
	}
    }
    p->lastcb = now;

    mmv_inc(data->map, data->metrics[DISCOVER_CHANGED_CALLBACKS]);
    if (pmDebugOptions.discovery)
	fprintf(stderr, "CHANGED %s (%s)\n", p->context.name,
			pmDiscoverFlagsStr(p));

    if (p->flags & DISCOVER_FLAGS_DELETED) {
	/*
	 * Path has been deleted. Do nothing for now. Will be purged
	 * in due course by pmDiscoverPurgeDeleted.
	 */
	return;
    }

    if (p->flags & DISCOVER_FLAGS_COMPRESSED) {
    	/* we do not monitor compressed files - do nothing */
	return;
    }

    if (p->flags & DISCOVER_FLAGS_DIRECTORY) {
	/*
	 * A changed directory path means a new archive or subdirectory may have
	 * been created or deleted - traverse and update the hash table.
	 */
	if (pmDebugOptions.discovery) {
	    fprintf(stderr, "%s DIRECTORY CHANGED %s (%s)\n",
			strtimestamp(&now, buf),
			p->context.name, pmDiscoverFlagsStr(p));
	}

	pmDiscoverArchives(p->context.name, p->module, p->data);
	pmDiscoverTraverse(DISCOVER_FLAGS_NEW, created_callback);

	/*
	 * Walk directory and invoke callbacks for tracked archives in this
	 * directory that have changed
	 */
	pmDiscoverTraverseArg(DISCOVER_FLAGS_DATAVOL|DISCOVER_FLAGS_META,
	    directory_changed_cb, (void *)p->context.name);

	/* finally, purge deleted entries (globally), if any */
	purged = pmDiscoverPurgeDeleted();
	mmv_add(data->map, data->metrics[DISCOVER_PURGED], &purged);
	monitored = monitored < purged ? 0 : monitored - purged;
	mmv_set(data->map, data->metrics[DISCOVER_MONITORED], &monitored);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s -- tracking status\n", strtimestamp(&now, buf));
	pmDiscoverTraverse(DISCOVER_FLAGS_ALL, print_callback);
	fprintf(stderr, "--\n");
    }
}

static void
dir_callback(pmDiscover *p)
{
    pmDiscoverMonitor(p->context.name, changed_callback);
    p->flags &= ~DISCOVER_FLAGS_NEW;
}

static void
archive_callback(pmDiscover *p)
{
    if (p->flags & DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
	p->flags &= ~DISCOVER_FLAGS_NEW;
    }
}

/*
 * Decode a metadata desc record in a given buffer buf of buflen bytes.
 * Careful not to modify the input buffer because it may be incomplete,
 * in which case we will pass through here at least once again later.
 * Return bytes processes on success, zero indicates insufficient data,
 * or a negative error code to end processing this stream (bad data).
 */
static ssize_t
pmDiscoverDecodeMetaDesc(uint32_t *buf, size_t buflen, pmDesc *p_desc, int *p_numnames, char ***p_names)
{
    int			i;
    int			len, sts;
    int			numnames;
    char		**names;
    char		*cp;
    pmDesc		desc;
    size_t		bytes;

    bytes = sizeof(pmDesc) + sizeof(int);
    if (buflen < bytes)
	return 0;
    memcpy(&desc, buf, sizeof(pmDesc));
    desc.type = ntohl(desc.type);
    desc.sem = ntohl(desc.sem);
    desc.indom = __ntohpmInDom(desc.indom);
    desc.units = __ntohpmUnits(desc.units);
    desc.pmid = __ntohpmID(desc.pmid);
    if (pmDebugOptions.discovery)
	fprintf(stderr, "DECODE DESC metric %s (", pmIDStr(desc.pmid));
    numnames = ntohl(buf[sizeof(pmDesc)/sizeof(int)]);
    if (numnames <= 0 ||
	numnames > MAX_INPUT_SIZE / sizeof(int) ||
	buflen > MAX_INPUT_SIZE - (numnames * sizeof(int))) {
	return -EINVAL;
    }
    names = (char **)calloc(numnames, sizeof(char *));
    if (names == NULL)
	return -ENOMEM;
    cp = (char *)&buf[sizeof(pmDesc)/sizeof(int) + 1];
    for (i = sts = 0; i < numnames; i++) {
	memmove(&len, cp, sizeof(len));
	len = ntohl(len);
	if (len <= 0 || len > MAX_INPUT_SIZE || buflen < bytes + len) {
	    sts = -EINVAL;
	    goto fail;
	}
	bytes += len;
	if (buflen < bytes) {
	    return 0;
	    goto fail;
	}
	cp += sizeof(len);
	if ((names[i] = (char *)malloc(len + 1)) == NULL) {
	    sts = -ENOMEM;
	    goto fail;
	}
	memcpy(names[i], cp, len);
	names[i][len] = '\0';
	cp += len;
    }
    if (pmDebugOptions.discovery) {
	__pmPrintMetricNames(stderr, numnames, names, " or ");
	fprintf(stderr, ")\n");
	pmPrintDesc(stderr, &desc);
    }

    *p_numnames = numnames;
    *p_names = names;
    *p_desc = desc;
    return bytes;

fail:
    while (i)
	free(names[--i]);
    free(names);
    return sts;
}

/*
 * Decode a metadata indom record (of given type) in buf of length len.
 * Return 0 on success.
 */
static int
pmDiscoverDecodeMetaInDom(__int32_t *buf, int len, int type, __pmTimestamp *tsp, pmInResult *inresult)
{
    int			sts = 0;
    __pmLogInDom	lid;

    if ((sts = __pmLogLoadInDom(NULL, len, type, &lid, &buf)) >= 0) {
	inresult->indom = lid.indom;
	inresult->numinst = lid.numinst;
	inresult->instlist = lid.instlist;
	if ((lid.alloc & PMLID_NAMELIST) == 0) {
	    /*
	     * 32-bit architecture and namelist[] is really part
	     * of buf[], so need to alloc a new namelist[]
	     */
	    char	**namelist;
	    int		i;
	    namelist = (char **)calloc(lid.numinst, sizeof(char *));
	    if (namelist == NULL) {
		pmNoMem(__FUNCTION__, lid.numinst * sizeof(char *), PM_RECOV_ERR);
		__pmFreeLogInDom(&lid);
		return -ENOMEM;
	    }
	    for (i = 0; i < lid.numinst; i++) {
		namelist[i] = lid.namelist[i];
	    }
	    inresult->namelist = namelist;
	}
	else
	    inresult->namelist = lid.namelist;
	*tsp = lid.stamp;
	sts = 0;
    }

    return sts;
}

static int
pmDiscoverDecodeMetaHelpText(uint32_t *buf, int len, int *type, int *id, char **buffer)
{
    char		*bp;

    *type = ntohl(buf[0]);

    if ((*type) & PM_TEXT_INDOM)
    	*id = __ntohpmInDom(buf[1]);
    else
    	*id = __ntohpmID(buf[1]);
    if ((bp = strdup((char *)&buf[2])) == NULL)
    	return -ENOMEM;
    *buffer = bp;
    return 0;
}

static int
pmDiscoverDecodeMetaLabelSet(uint32_t *buf, int buflen, int type, __pmTimestamp *tsp, int *typep, int *idp, int *nsetsp, pmLabelSet **setsp)
{
    return __pmLogLoadLabelSet((char *)buf, buflen, type, tsp, typep, idp, nsetsp, setsp);
}

int
pmDiscoverRegister(const char *dir, pmDiscoverModule *module,
		pmDiscoverCallBacks *callbacks, void *arg)
{
    int			handle = -1;
    int			avail_handle;
    pmDiscoverCallBacks	**cbp;

    while (callbacks != NULL) {
	avail_handle = -1;
	for (handle = 0; handle < discoverCallBackTableSize; handle++) {
	    if (callbacks == discoverCallBackTable[handle])
		break; /* we're adding new dirs using an existing handle */
	    if (discoverCallBackTable[handle] == NULL) {
		avail_handle = handle;
		break;
	    }
	}
	if (handle == discoverCallBackTableSize || avail_handle < 0) {
	    avail_handle = discoverCallBackTableSize++;
	    cbp = (pmDiscoverCallBacks **)realloc(discoverCallBackTable,
			discoverCallBackTableSize * sizeof(*cbp));
	    if (cbp == NULL)
		return -ENOMEM;
	    discoverCallBackTable = cbp;
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: new handle [%d] for callbacks %p\n",
			__FUNCTION__, avail_handle, callbacks);
	}
	handle = avail_handle;
	discoverCallBackTable[handle] = callbacks;
	callbacks = callbacks->next;
    }
    /* else we are just adding dirs for all existing registered callbacks */

    if (dir) {
	/* NULL dir means add callbacks for existing directories and archives */
	pmDiscoverArchives(dir, module, arg);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "Now tracking %d directories and %d archives\n",
	    pmDiscoverTraverse(DISCOVER_FLAGS_DIRECTORY, NULL),
	    pmDiscoverTraverse(DISCOVER_FLAGS_DATAVOL|DISCOVER_FLAGS_META, NULL));
    }

    /* monitor the directories */
    pmDiscoverTraverse(DISCOVER_FLAGS_DIRECTORY, dir_callback);

    /* monitor archive data and metadata volumes, uncompressed only */
    pmDiscoverTraverse(DISCOVER_FLAGS_DATAVOL|DISCOVER_FLAGS_META, archive_callback);

    return handle;
}

void
pmDiscoverUnregister(int handle)
{
    if (discoverCallBackTable != NULL &&
	handle >= 0 && handle < discoverCallBackTableSize)
    	discoverCallBackTable[handle] = NULL; /* unregister these callbacks */
}

discoverModuleData *
getDiscoverModuleData(pmDiscoverModule *module)
{
    if (module->privdata == NULL)
	module->privdata = calloc(1, sizeof(discoverModuleData));
    return module->privdata;
}

int
pmDiscoverSetSlots(pmDiscoverModule *module, void *slots)
{
    discoverModuleData	*data = getDiscoverModuleData(module);

    if (data) {
	data->slots = (keySlots *)slots;
	data->shareslots = 1;
	return 0;
    }
    return -ENOMEM;
}

int
pmDiscoverSetHostSpec(pmDiscoverModule *module, sds hostspec)
{
    (void)module;
    (void)hostspec;
    return -ENOTSUP;	/* deprecated, use pmDiscoverSetConfiguration */
}

int
pmDiscoverSetConfiguration(pmDiscoverModule *module, dict *config)
{
    discoverModuleData	*data = getDiscoverModuleData(module);

    if (data) {
	data->config = config;
	return 0;
    }
    return -ENOMEM;
}

int
pmDiscoverSetEventLoop(pmDiscoverModule *module, void *events)
{
    discoverModuleData	*data = getDiscoverModuleData(module);

    if (data) {
	data->events = (uv_loop_t *)events;
	return 0;
    }
    return -ENOMEM;
}

void
pmDiscoverSetupMetrics(pmDiscoverModule *module)
{
    discoverModuleData	*data = getDiscoverModuleData(module);
    pmAtomValue		**metrics;
    pmUnits		nounits = MMV_UNITS(0,0,0,0,0,0);
    pmUnits		countunits = MMV_UNITS(0,0,1,0,0,0);
    pmUnits		secondsunits = MMV_UNITS(0,1,0,0,PM_TIME_SEC,0);
    void		*map;

    if (data == NULL || data->registry == NULL)
	return;	/* no metric registry has been set up */

    mmv_stats_add_metric(data->registry, "monitored", 1,
	MMV_TYPE_U64, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"directories and archives currently being monitored",
	"Number of directories and archives currently being monitored");

    mmv_stats_add_metric(data->registry, "purged", 2,
	MMV_TYPE_U64, MMV_SEM_INSTANT, nounits, MMV_INDOM_NULL,
	"directories and archives purged",
	"Number of directories and archives no longer being monitored");

    mmv_stats_add_metric(data->registry, "metadata.callbacks", 3,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"process metadata for monitored archives",
	"total calls to process metadata for monitored archives");

    mmv_stats_add_metric(data->registry, "metadata.loops", 4,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"processing metadata for monitored archives",
	"Total loops processing metadata for monitored archives");

    mmv_stats_add_metric(data->registry, "metadata.decode.desc", 5,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"desc records decoded for monitored archives",
	"Total metric descriptor records decoded processing metadata for monitored\n"
	"archives");

    mmv_stats_add_metric(data->registry, "metadata.decode.indom", 6,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"indom records decoded for all monitored archives",
	"Total indom records decoded processing metadata for monitored archives");

    mmv_stats_add_metric(data->registry, "metadata.decode.label", 7,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"label records decoded for monitored archives",
	"Total label records decoded processing metadata for monitored archives");

    mmv_stats_add_metric(data->registry, "metadata.decode.helptext", 8,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"help text records decoded for all monitored archives",
	"Total help text records decoded processing metadata for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.callbacks", 9,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"calls to process logvol data for monitored archives",
	"Total calls to process log volume data for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.loops", 10,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"loops processing logvol data for monitored archives",
	"Total loops processing logvol data for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.change_vol", 11,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"log vol values callbacks made for monitored archives",
	"Total log volume values callbacks made for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.decode.result", 12,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"result records decoded for monitored archives",
	"Total result records decoded for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.decode.result_pmids", 13,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"metric IDs in decoded result records for monitored archives",
	"Total metric identifers in decoded result records for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.decode.mark_record", 14,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"mark records decoded for monitored archives",
	"Total mark records in result records decoded for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.new_contexts", 15,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"successful new context calls made for monitored archives",
	"Total successful new context calls made for monitored archives");

    mmv_stats_add_metric(data->registry, "logvol.get_archive_end_failed", 16,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"Failed pmGetArchiveEnd calls for all monitored archives",
	"Total failed pmGetArchiveEnd calls after successfully creating a new context\n"
        "for all monitored archives");

    mmv_stats_add_metric(data->registry, "changed_callbacks", 17,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"filesystem changed callbacks",
	"Number of observed filesystem changes to PCP archives");

    mmv_stats_add_metric(data->registry, "throttled_changed_callbacks", 18,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"filesystem changed callbacks ignored due to throttling",
	"Number of filesystem change callbacks that were ignored due to throttling");

    mmv_stats_add_metric(data->registry, "throttle", 19,
	MMV_TYPE_U64, MMV_SEM_INSTANT, secondsunits, MMV_INDOM_NULL,
	"minimum filesystem changed callback throttle time",
	"Minimum time between filesystem changed callbacks for each monitored archive");

    mmv_stats_add_metric(data->registry, "metadata.partial_reads", 20,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"metadata read returned less data than expected",
	"Number of times a metadata record read returned less than expected length");

    mmv_stats_add_metric(data->registry, "logvol.decode.result_errors", 21,
	MMV_TYPE_U64, MMV_SEM_COUNTER, countunits, MMV_INDOM_NULL,
	"error result records decoded for monitored archives",
	"Total errors in result records decoded for monitored archives");

    data->map = map = mmv_stats_start(data->registry);
    metrics = data->metrics;

    metrics[DISCOVER_MONITORED] = mmv_lookup_value_desc(
				    map, "monitored", NULL);
    metrics[DISCOVER_PURGED] = mmv_lookup_value_desc(
				    map, "purged", NULL);
    metrics[DISCOVER_META_CALLBACKS] = mmv_lookup_value_desc(
				    map, "metadata.callbacks", NULL);
    metrics[DISCOVER_META_LOOPS] = mmv_lookup_value_desc(
				    map, "metadata.loops", NULL);
    metrics[DISCOVER_DECODE_DESC] = mmv_lookup_value_desc(
				    map, "metadata.decode.desc", NULL);
    metrics[DISCOVER_DECODE_INDOM] = mmv_lookup_value_desc(
				    map, "metadata.decode.indom", NULL);
    metrics[DISCOVER_DECODE_LABEL] = mmv_lookup_value_desc(
				    map, "metadata.decode.label", NULL);
    metrics[DISCOVER_DECODE_HELPTEXT] = mmv_lookup_value_desc(
				    map, "metadata.decode.helptext", NULL);
    metrics[DISCOVER_LOGVOL_CALLBACKS] = mmv_lookup_value_desc(
				    map, "logvol.callbacks", NULL);
    metrics[DISCOVER_LOGVOL_LOOPS] = mmv_lookup_value_desc(
				    map, "logvol.loops", NULL);
    metrics[DISCOVER_LOGVOL_CHANGE_VOL] = mmv_lookup_value_desc(
				    map, "logvol.change_vol", NULL);
    metrics[DISCOVER_DECODE_RESULT] = mmv_lookup_value_desc(
				    map, "logvol.decode.result", NULL);
    metrics[DISCOVER_DECODE_RESULT_PMIDS] = mmv_lookup_value_desc(
				    map, "logvol.decode.result_pmids", NULL);
    metrics[DISCOVER_DECODE_MARK_RECORD] = mmv_lookup_value_desc(
				    map, "logvol.decode.mark_record", NULL);
    metrics[DISCOVER_LOGVOL_NEW_CONTEXTS] = mmv_lookup_value_desc(
				    map, "logvol.new_contexts", NULL);
    metrics[DISCOVER_ARCHIVE_END_FAILED] = mmv_lookup_value_desc(
				    map, "logvol.get_archive_end_failed", NULL);
    metrics[DISCOVER_CHANGED_CALLBACKS] = mmv_lookup_value_desc(
				    map, "changed_callbacks", NULL);
    metrics[DISCOVER_THROTTLE_CALLBACKS] = mmv_lookup_value_desc(
				    map, "throttled_changed_callbacks", NULL);
    metrics[DISCOVER_THROTTLE] = mmv_lookup_value_desc(
				    map, "throttle", NULL);
    metrics[DISCOVER_META_PARTIAL_READS] = mmv_lookup_value_desc(
				    map, "metadata.partial_reads", NULL);
    metrics[DISCOVER_DECODE_RESULT_ERRORS] = mmv_lookup_value_desc(
				    map, "logvol.decode.result_errors", NULL);
}

int
pmDiscoverSetMetricRegistry(pmDiscoverModule *module, mmv_registry_t *registry)
{
    discoverModuleData	*data = getDiscoverModuleData(module);

    if (data) {
	data->registry = registry;
	return 0;
    }
    return -ENOMEM;
}

int
pmDiscoverSetup(pmDiscoverModule *module, pmDiscoverCallBacks *cbs, void *arg)
{
    discoverModuleData	*data = getDiscoverModuleData(module);
    const char		fallback[] = "/var/log/pcp/pmlogger";
    const char		*logdir = pmGetOptionalConfig("PCP_ARCHIVE_DIR");
    struct dict		*config;
    unsigned int	domain, serial;
    pmInDom		indom;
    sds			option, *ids;
    int			i, sts, nids;

    if (data == NULL)
	return -ENOMEM;
    config = data->config;

    /* double-check that we are supposed to be in here */
    if ((option = pmIniFileLookup(config, "discover", "enabled"))) {
	if (strcmp(option, "false") == 0)
	    return 0;
    }

    /* see if an alternate archive directory is sought */
    if ((option = pmIniFileLookup(config, "discover", "path")))
	logdir = option;

    /* prepare for optional metric and indom exclusion */
    if ((option = pmIniFileLookup(config, "discover", "exclude.metrics"))) {
	if ((data->pmids = dictCreate(&intKeyDictCallBacks, NULL)) == NULL)
	    return -ENOMEM;
	/* parse comma-separated metric name glob patterns, in 'option' */
	if ((ids = sdssplitlen(option, sdslen(option), ",", 1, &nids))) {
	    data->exclude_names = nids;
	    for (i = 0; i < nids; i++)
		ids[i] = sdstrim(ids[i], " ");
	    data->patterns = ids;
	}
    }
    if ((option = pmIniFileLookup(config, "discover", "exclude.indoms"))) {
	if ((data->indoms = dictCreate(&intKeyDictCallBacks, NULL)) == NULL)
	    return -ENOMEM;
	/* parse comma-separated indoms in 'option', convert to pmInDom */
	if ((ids = sdssplitlen(option, sdslen(option), ",", 1, &nids))) {
	    data->exclude_indoms = nids;
	    for (i = 0; i < nids; i++) {
		if (sscanf(ids[i], "%u.%u", &domain, &serial) == 2) {
		    indom = pmInDom_build(domain, serial);
		    dictAdd(data->indoms, &indom, NULL);
		}
		sdsfree(ids[i]);
	    }
	    free(ids);
	}
    }

    /* create global string map caches */
    keysGlobalsInit(data->config);

    if (!logdir)
	logdir = fallback;

    pmDiscoverSetupMetrics(module);

    if (access(logdir, F_OK) == 0) {
	sts = pmDiscoverRegister(logdir, module, cbs, arg);
	if (sts >= 0) {
	    data->handle = sts;
	    return 0;
	}
    }
    return -ESRCH;
}

void
pmDiscoverClose(pmDiscoverModule *module)
{
    discoverModuleData	*discover = (discoverModuleData *)module->privdata;
    unsigned int	i;

    if (discover) {
	pmDiscoverUnregister(discover->handle);
	if (discover->slots && !discover->shareslots)
	    keySlotsFree(discover->slots);
	for (i = 0; i < discover->exclude_names; i++)
	    sdsfree(discover->patterns[i]);
	if (discover->patterns)
	    free(discover->patterns);
	if (discover->pmids)
	    dictRelease(discover->pmids);
	if (discover->indoms)
	    dictRelease(discover->indoms);
	memset(discover, 0, sizeof(*discover));
	free(discover);
    }

    keysGlobalsClose();
}
