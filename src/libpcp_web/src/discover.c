/*
 * Copyright (c) 2018-2020 Red Hat.
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
#include "slots.h"
#include "util.h"

/* Decode various archive metafile records (desc, indom, labels, helptext) */
static int pmDiscoverDecodeMetaDesc(uint32_t *, int, pmDesc *, int *, char ***);
static int pmDiscoverDecodeMetaInDom(uint32_t *, int, pmTimespec *, pmInResult *);
static int pmDiscoverDecodeMetaHelpText(uint32_t *, int, int *, int *, char **);
static int pmDiscoverDecodeMetaLabelSet(uint32_t *, int, pmTimespec *, int *, int *, int *, pmLabelSet **);

/* array of registered callbacks, see pmDiscoverSetup() */
static int discoverCallBackTableSize;
static pmDiscoverCallBacks **discoverCallBackTable;
static char *pmDiscoverFlagsStr(pmDiscover *);

/* internal hash table of discovered paths */
#define PM_DISCOVER_HASHTAB_SIZE 16
static pmDiscover *discover_hashtable[PM_DISCOVER_HASHTAB_SIZE];

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

/*
 * Lookup or Add a discovered file path (directory or PCP archive file).
 * Note: the fullpath suffix (.meta, .[0-9]+) should already be stripped.
 * Return path table entry (new or existing).
 */
static pmDiscover *
pmDiscoverLookupAdd(const char *fullpath, pmDiscoverModule *module, void *arg)
{
    pmDiscover		*p, *h;
    unsigned int	k;
    sds			name;

    name = sdsnew(fullpath);
    k = strhash(name, PM_DISCOVER_HASHTAB_SIZE);

    if (pmDebugOptions.discovery)
	fprintf(stderr, "pmDiscoverLookupAdd: name=%s\n", name);

    for (p = NULL, h = discover_hashtable[k]; h != NULL; p = h, h = h->next) {
    	if (sdscmp(h->context.name, name) == 0)
	    break;
    }

    if (h == NULL && module != NULL) {	/* hash table insert mode */
	if ((h = (pmDiscover *)calloc(1, sizeof(pmDiscover))) == NULL)
	    return NULL;
	h->fd = -1; /* no meta descriptor initially */
	h->ctx = -1; /* no PMAPI context initially */
	h->flags = PM_DISCOVER_FLAGS_NEW;
	h->context.type = PM_CONTEXT_ARCHIVE;
	h->context.name = name;
	h->module = module;
	h->data = arg;
	if (p == NULL)
	    discover_hashtable[k] = h;
	else
	    p->next = h;
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "pmDiscoverLookupAdd: --> new entry %s\n", name);

    }
    else {
	/* already in hash table, so free the buffer */
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "pmDiscoverLookupAdd: --> existing entry %s\n", name);
    	sdsfree(name);
    }

    return h;
}

static pmDiscover *
pmDiscoverLookup(const char *path)
{
    return pmDiscoverLookupAdd(path, NULL, NULL);
}

static void
pmDiscoverFree(pmDiscover *p)
{
    if (p->ctx >= 0)
	pmDestroyContext(p->ctx);
    if (p->fd >= 0)
	close(p->fd);
    if (p->context.name)
	sdsfree(p->context.name);
    if (p->context.source)
	sdsfree(p->context.source);
    if (p->context.labelset)
	pmFreeLabelSets(p->context.labelset, 1);
    if (p->event_handle) {
	uv_fs_event_stop(p->event_handle);
	free(p->event_handle);
	p->event_handle = NULL;
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

    for (i = 0; i < PM_DISCOVER_HASHTAB_SIZE; i++) {
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

/*
 * Traverse and purge deleted entries
 * Return count of purged entries.
 */
static int
pmDiscoverPurgeDeleted(void)
{
    int			count = 0, i;
    pmDiscover		*p, *prev, *next;

    for (i = 0; i < PM_DISCOVER_HASHTAB_SIZE; i++) {
	p = discover_hashtable[i];
	prev = NULL;
    	while (p) {
	    next = p->next;

	    if (!(p->flags & PM_DISCOVER_FLAGS_DELETED)) {
		prev = p;
	    } else {
		if (prev)
		    prev->next = next;
		else
		    discover_hashtable[i] = next;
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "pmDiscoverPurgeDeleted: deleted %s %s\n",
		    	p->context.name, pmDiscoverFlagsStr(p));
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
    int slen, suflen;
    char *ret = NULL;

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
    uv_fs_t		sreq, req;
    uv_dirent_t		dent;
    uv_stat_t		*s;
    pmDiscover		*a;
    char		*suffix;
    char		path[MAXNAMELEN];
    int			sep = pmPathSeparator();
    int			vol;
    int			sts;

    /*
     * note: pmDiscoverLookupAdd sets PM_DISCOVER_FLAGS_NEW
     * if this is a newly discovered archive or directory
     */
    a = pmDiscoverLookupAdd(dir, module, arg);
    a->flags |= PM_DISCOVER_FLAGS_DIRECTORY;

    if ((sts = uv_fs_scandir(NULL, &req, dir, 0, NULL)) < 0) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "pmDiscoverArchives: scandir %s failed %s: err %d\n", dir, path, sts);
	return -ESRCH;
    }

    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
	pmsprintf(path, sizeof(path), "%s%c%s", dir, sep, dent.name);

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "pmDiscoverArchives: scandir found %s\n", path);

	if (uv_fs_stat(NULL, &sreq, path, NULL) < 0) {
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "pmDiscoverArchives: stat failed %s\n", path);
	    continue;
	}

	s = &sreq.statbuf;
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
		 * note: pmDiscoverLookupAdd sets PM_DISCOVER_FLAGS_NEW
		 * if this is a newly discovered archive, otherwise we're
		 * already tracking this archive.
		 */
		a->flags |= PM_DISCOVER_FLAGS_META;
	    }
	    else if ((suffix = __pmLogBaseNameVol(path, &vol)) != NULL && vol >= 0) {
		/*
		 * An archive logvol. This logvol may have been created since
		 * the context was first opened. Update the context maxvol
		 * to be sure pmFetchArchive can switch to it in due course.
		 */
		if ((a = pmDiscoverLookup(path)) != NULL) {
		    a->flags |= PM_DISCOVER_FLAGS_DATAVOL;
		    /* ensure archive context knows about this volume */
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "pmDiscoverArchives: found logvol %s %s vol=%d\n",
			    a->context.name, pmDiscoverFlagsStr(a), vol);
		    if (a->ctx >= 0 && vol >= 0) {
			__pmContext *ctxp = __pmHandleToPtr(a->ctx);
			__pmArchCtl *acp = ctxp->c_archctl;

		    	__pmLogAddVolume(acp, vol);
			PM_UNLOCK(ctxp->c_lock);
			if (pmDebugOptions.discovery)
			    fprintf(stderr, "pmDiscoverArchives: added logvol %s %s vol=%d\n",
				a->context.name, pmDiscoverFlagsStr(a), vol);
		    }
		}
	    } else if (pmDebugOptions.discovery) {
		fprintf(stderr, "pmDiscoverArchives: ignored regular file %s\n", path);
	    }
	}
	else if (S_ISDIR(s->st_mode)) {
	    /*
	     * Recurse into subdir
	     */
	    pmDiscoverArchives(path, module, arg);
	}
	uv_fs_req_cleanup(&sreq);
    }
    uv_fs_req_cleanup(&req);

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
is_deleted(pmDiscover *p, uv_fs_t *sreq)
{
    int			ret = 0;

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	if (uv_fs_stat(NULL, sreq, p->context.name, NULL) < 0)
	    ret = 1; /* directory has been deleted */
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
    	sds meta = sdsnew(p->context.name);
	meta = sdscat(meta, ".meta");
	if (uv_fs_stat(NULL, sreq, meta, NULL) < 0) {
	    /*
	     * Archive metadata file has been deleted (or compressed)
	     * hence consider the archive to be deleted because there
	     * is no more data to logtail.
	     */
	    ret = 1;
	}
	sdsfree(meta);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "is_deleted: checking %s (%s) -> %s\n",
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
    uv_fs_t		sreq;
    char		*s;
    sds			path;
    int			path_changed = 0;

    uv_fs_event_getpath(handle, buffer, &bytes);
    path = sdsnewlen(buffer, bytes);

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "fs_change_callBack: event on %s -", path);
	if (events & UV_RENAME)
	    fprintf(stderr, " renamed");
	if (events & UV_CHANGE)
	    fprintf(stderr, " changed");
	fputc('\n', stderr);
    }

    /*
     * Strip suffix and lookup the path. stat and update it's flags accordingly.
     * If the path has been deleted, stop it's event monitor and free the req
     * buffer, else call the pmDiscovery callback.
     */
    if ((s = strsuffix(path, ".meta")) != NULL)
	*s = '\0';

    p = pmDiscoverLookup(path);
    if (p && pmDebugOptions.discovery) {
	fprintf(stderr, "fs_change_callBack: ---> found entry %s (%s)\n",
		p->context.name, pmDiscoverFlagsStr(p));
    }

    if (p == NULL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "fs_change_callBack: %s lookup failed\n", filename);
    }
    else if (is_deleted(p, &sreq)) {
    	p->flags |= PM_DISCOVER_FLAGS_DELETED;
	/* path has been deleted. statbuf is invalid */
	memset(&p->statbuf, 0, sizeof(p->statbuf));
	path_changed = 1;
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "fs_change_callBack: %s (%s) has been deleted",
	    	p->context.name, pmDiscoverFlagsStr(p));
	uv_fs_req_cleanup(&sreq);
    }
    else {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "fs_change_callBack: %s has changed\n", p->context.name);
	/* avoid spurious events. only call the callBack if it really changed */
	if (p->statbuf.st_mtim.tv_sec != sreq.statbuf.st_mtim.tv_sec ||
	    p->statbuf.st_mtim.tv_nsec != sreq.statbuf.st_mtim.tv_nsec)
	    path_changed = 1;
	p->statbuf = sreq.statbuf; /* struct copy */
	uv_fs_req_cleanup(&sreq);
    }

    if (p && p->changed && path_changed)
	p->changed(p); /* returns immediately if PM_DISCOVER_FLAGS_DELETED */

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
	if (pmDebugOptions.discovery) {
	    fprintf(stderr, "pmDiscoverMonitor: lookup failed for %s\n", path);
	}
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

	if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
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

	if (pmDebugOptions.discovery) {
	    fprintf(stderr, "pmDiscoverMonitor: added event for %s (%s)\n",
	    	eventfilename, pmDiscoverFlagsStr(p));
	}
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
#define __ntohpmLabel(a)        (a)
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

static void
__ntohpmLabel(pmLabel * const label)
{
    label->name = ntohs(label->name);
    /* label->namelen is one byte */
    /* label->flags is one byte */
    label->value = ntohs(label->value);
    label->valuelen = ntohs(label->valuelen);
}
#endif

static struct {
    unsigned int	flag;
    const char		 *name;
} flags_str[] = {
    { PM_DISCOVER_FLAGS_NEW, "new|" },
    { PM_DISCOVER_FLAGS_DELETED, "deleted|" },
    { PM_DISCOVER_FLAGS_DIRECTORY, "directory|" },
    { PM_DISCOVER_FLAGS_DATAVOL, "datavol|" },
    { PM_DISCOVER_FLAGS_INDEX, "indexvol|" },
    { PM_DISCOVER_FLAGS_META, "metavol|" },
    { PM_DISCOVER_FLAGS_COMPRESSED, "compressed|" },
    { PM_DISCOVER_FLAGS_MONITORED, "monitored|" },
    { PM_DISCOVER_FLAGS_DATAVOL_READY, "datavol-ready|" },
    { PM_DISCOVER_FLAGS_META_IN_PROGRESS, "metavol-in-progress|" },
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
	    strncat(buf, flags_str[i].name, sizeof(buf)-1);
    }
    return buf;
}


static void changed_callback(pmDiscover *); /* fwd decl */

static void
created_callback(pmDiscover *p)
{
    p->flags &= ~PM_DISCOVER_FLAGS_NEW;

    if (p->flags & (PM_DISCOVER_FLAGS_COMPRESSED|PM_DISCOVER_FLAGS_INDEX))
    	return; /* compressed archives don't grow and we ignore archive index files */

    if (pmDebugOptions.discovery)
	fprintf(stderr, "CREATED %s, %s\n", p->context.name, pmDiscoverFlagsStr(p));

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR directory %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
    else if (p->flags & (PM_DISCOVER_FLAGS_META|PM_DISCOVER_FLAGS_DATAVOL)) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR archive %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
}

static void
discover_event_init(pmDiscover *p, pmTimespec *timestamp, pmDiscoverEvent *event)
{
    event->timestamp = *timestamp;
    event->context = p->context;
    event->module = p->module;
    event->data = p;
}

static void
pmDiscoverInvokeSourceCallBacks(pmDiscover *p, pmTimespec *ts)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s name %s\n", "pmDiscoverInvokeSourceCallBacks",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, p->context.name);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_source != NULL)
	    callbacks->on_source(&event, p->data);
    }
}

static void
pmDiscoverInvokeValuesCallBack(pmDiscover *p, pmTimespec *ts, pmResult *r)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s numpmid %d\n", "pmDiscoverInvokeValuesCallBack",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, r->numpmid);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_values != NULL)
	    callbacks->on_values(&event, r, p->data);
    }
}

static void
pmDiscoverInvokeMetricCallBacks(pmDiscover *p, pmTimespec *ts, pmDesc *desc,
		int numnames, char **names)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    char		buf[32];
    int			i, sts;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s name%s", "pmDiscoverInvokeMetricCallBacks",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, numnames > 0 ? " " : "(none)\n");
	for (i = 0; i < numnames; i++)
	    printf("\"%s\"%s", names[i], i < numnames - 1 ? ", " : "\n");
	pmPrintDesc(stderr, desc);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	ctxp = __pmHandleToPtr(p->ctx);
	acp = ctxp->c_archctl;
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = __pmLogAddDesc(acp, desc)) < 0)
	    fprintf(stderr, "%s: failed to add metric descriptor for %s\n",
		    "pmDiscoverInvokeMetricCallBacks", pmIDStr(desc->pmid));
    }

    for (i = 0; i < numnames; i++) {
	if ((sts = __pmLogAddPMNSNode(acp, desc->pmid, names[i])) < 0)
	    fprintf(stderr, "%s: failed to add metric name %s for %s\n",
			    "pmDiscoverInvokeMetricCallBacks", names[i], pmIDStr(desc->pmid));
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_metric != NULL)
	    callbacks->on_metric(&event, desc, numnames, names, p->data);
    }

    for (i = 0; i < numnames; i++)
	free(names[i]);
    free(names);
}

static void
pmDiscoverInvokeInDomCallBacks(pmDiscover *p, pmTimespec *ts, pmInResult *in)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    char		buf[32], inbuf[32];
    int			i, sts;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s numinst %d indom %s\n",
			"pmDiscoverInvokeInDomCallBacks",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, in->numinst,
			pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	ctxp = __pmHandleToPtr(p->ctx);
	acp = ctxp->c_archctl;
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = __pmLogAddInDom(acp, ts, in, NULL, 0)) < 0) {
	    char		errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: failed to add indom for %s: %s\n",
			"pmDiscoverInvokeInDomCallBacks", pmIDStr(in->indom),
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    } else {
	sts = PMLOGPUTINDOM_DUP;
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_indom != NULL)
	    callbacks->on_indom(&event, in, p->data);
    }

    if (sts == PMLOGPUTINDOM_DUP) {
	for (i = 0; i < in->numinst; i++)
	    free(in->namelist[i]);
	free(in->namelist);
	free(in->instlist);
    }
}

static void
pmDiscoverInvokeLabelsCallBacks(pmDiscover *p, pmTimespec *ts,
		int ident, int type, pmLabelSet *sets, int nsets)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    char		buf[32], idbuf[64];
    int			i, sts;

    if (pmDebugOptions.discovery) {
	__pmLabelIdentString(ident, type, idbuf, sizeof(idbuf));
	fprintf(stderr, "%s[%s]: %s ID %s type %s\n",
			"pmDiscoverInvokeLabelsCallBacks",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, idbuf, __pmLabelTypeString(type));
	pmPrintLabelSets(stderr, ident, type, sets, nsets);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	ctxp = __pmHandleToPtr(p->ctx);
	acp = ctxp->c_archctl;
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = __pmLogAddLabelSets(acp, ts,
					type, ident, nsets, sets)) < 0) {
	    char		errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: failed to add log labelset: %s\n",
			"pmDiscoverInvokeLabelsCallBacks",
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    } else {
	sts = -EAGAIN;	/* free labelsets memory after callbacks */
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_labels != NULL)
	    callbacks->on_labels(&event, ident, type, sets, nsets, p->data);
    }

    if (sts < 0)
	pmFreeLabelSets(sets, nsets);
}

static void
pmDiscoverInvokeTextCallBacks(pmDiscover *p, pmTimespec *ts,
		int ident, int type, char *text)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    char		buf[32];
    int			i, sts;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s ", "pmDiscoverInvokeTextCallBacks",
			timespec_str(ts, buf, sizeof(buf)),
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

    if (p->ctx >= 0 && p->context.type == PM_CONTEXT_ARCHIVE) {
	ctxp = __pmHandleToPtr(p->ctx);
	acp = ctxp->c_archctl;
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = __pmLogAddText(acp, ident, type, text)) < 0) {
	    char		errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s: failed to add %u text for %u: %s\n",
	               "pmDiscoverInvokeTextCallBacks", type, ident,
			pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_text != NULL)
	    callbacks->on_text(&event, ident, type, text, p->data);
    }

    free(text);
}

static void
pmDiscoverNewSource(pmDiscover *p, int context)
{
    pmLabelSet		*labelset = NULL;
    pmTimespec		timestamp;
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

    /* use timestamp from file creation as starting time */
    timestamp.tv_sec = p->statbuf.st_birthtim.tv_sec;
    timestamp.tv_nsec = p->statbuf.st_birthtim.tv_nsec;

    /* inform utilities that a source has been discovered */
    pmDiscoverInvokeSourceCallBacks(p, &timestamp);
}

/*
 * Process metadata records until EOF. That can span multiple
 * callbacks if we get a partial record read.
 */
static void
process_metadata(pmDiscover *p)
{
    int			partial = 0;
    pmTimespec		ts;
    pmDesc		desc;
    off_t		off;
    char		*buffer;
    int			e, nb, len, nsets;
    int			type, id; /* pmID or pmInDom */
    int			nnames;
    char		**names;
    pmInResult		inresult;
    pmLabelSet		*labelset = NULL;
    unsigned char	hash[20];
    __pmLogHdr		hdr;
    sds			msg, source;
    static uint32_t	*buf = NULL;
    static int		buflen = 0;

    /*
     * Read all metadata records from current offset though to EOF
     * and call all registered callbacks. Avoid processing logvol
     * callbacks until all metadata records have been read.
     */
    p->flags |= PM_DISCOVER_FLAGS_META_IN_PROGRESS;
    if (pmDebugOptions.discovery)
	fprintf(stderr, "process_metadata: %s in progress %s\n",
		p->context.name, pmDiscoverFlagsStr(p));
    for (;;) {
	off = lseek(p->fd, 0, SEEK_CUR);
	nb = read(p->fd, &hdr, sizeof(__pmLogHdr));

	if (nb <= 0) {
	    /* we're at EOF or an error. But may still be part way through a record */
	    break;
	}

	if (nb != sizeof(__pmLogHdr)) {
	    /* rewind so we can wait for more data on the next change CallBack */
	    lseek(p->fd, off, SEEK_SET);
	    partial = 1;
	    continue;
	}

	hdr.len = ntohl(hdr.len);
	hdr.type = ntohl(hdr.type);
	if (hdr.len <= 0) {
	    /* rewind and wait for more data, as above */
	    lseek(p->fd, off, SEEK_SET);
	    partial = 1;
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
	    continue;
	}

	if (pmDebugOptions.discovery)
	    fprintf(stderr, "Log metadata read len %4d type %d: ", len, hdr.type);

	switch (hdr.type) {
	case TYPE_DESC:
	    /* decode pmDesc result from PDU buffer */
	    nnames = 0;
	    names = NULL;
	    if ((e = pmDiscoverDecodeMetaDesc(buf, len, &desc, &nnames, &names)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaDesc", e, pmErrStr(e));
		break;
	    }
	    /* use timestamp from last modification */
	    ts.tv_sec = p->statbuf.st_mtim.tv_sec;
	    ts.tv_nsec = p->statbuf.st_mtim.tv_nsec;
	    pmDiscoverInvokeMetricCallBacks(p, &ts, &desc, nnames, names);
	    break;

	case TYPE_INDOM:
	    /* decode indom result from buffer */
	    if ((e = pmDiscoverDecodeMetaInDom(buf, len, &ts, &inresult)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaInDom", e, pmErrStr(e));
		break;
	    }
	    pmDiscoverInvokeInDomCallBacks(p, &ts, &inresult);
	    break;

	case TYPE_LABEL:
	    /* decode labelset from buffer */
	    if ((e = pmDiscoverDecodeMetaLabelSet(buf, len, &ts, &id, &type, &nsets, &labelset)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaLabelSet", e, pmErrStr(e));
		break;
	    }

	    /*
	     * If this is a context labelset, we need to store it in 'p' and
	     * also update the source identifier (pmSID) - effectively making
	     * a new source.
	     */
	    if ((type & PM_LABEL_CONTEXT)) {
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
		    pmDiscoverInvokeSourceCallBacks(p, &ts);
		}
	    }
	    pmDiscoverInvokeLabelsCallBacks(p, &ts, id, type, labelset, nsets);
	    break;

	case TYPE_TEXT:
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "TEXT\n");
	    /* decode help text from buffer */
	    buffer = NULL;
	    if ((e = pmDiscoverDecodeMetaHelpText(buf, len, &type, &id, &buffer)) < 0) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "%s failed: err=%d %s\n",
				    "pmDiscoverDecodeMetaHelpText", e, pmErrStr(e));
		break;
	    }
	    /* use timestamp from last modification */
	    ts.tv_sec = p->statbuf.st_mtim.tv_sec;
	    ts.tv_nsec = p->statbuf.st_mtim.tv_nsec;
	    pmDiscoverInvokeTextCallBacks(p, &ts, id, type, buffer);
	    break;

	default:
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s, len = %d\n",
			hdr.type == (PM_LOG_MAGIC | PM_LOG_VERS02) ?
			"PM_LOG_MAGICv2" : "UNKNOWN", len);
	    break;
	}
    }

    if (partial == 0)
	/* flag that all available metadata has now been read */
	p->flags &= ~PM_DISCOVER_FLAGS_META_IN_PROGRESS;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: completed, partial=%d %s %s\n",
			"process_metadata", partial, p->context.name, pmDiscoverFlagsStr(p));
}

/*
 * Fetch metric values to EOF and call all registered callbacks.
 * Always process metadata thru to EOF before any logvol data.
 */
static void
process_logvol(pmDiscover *p)
{
    int			sts;
    pmResult		*r;
    pmTimespec		ts;

    for (;;) {
	pmUseContext(p->ctx);
	if ((sts = pmFetchArchive(&r)) < 0) {
	    if (sts == PM_ERR_EOL) {
		if (pmDebugOptions.discovery)
		    fprintf(stderr, "process_logvol: %s end of archive reached\n",
		    	p->context.name);

		/* succesfully processed to current end of log */
		break;
	    } else if (sts == PM_ERR_LOGREC) {
		/* 
		 * This log vol was probably deleted (likely compressed)
		 * under our feet. Try and skip to the next volume.
		 */
		__pmContext *ctxp = __pmHandleToPtr(p->ctx);
		__pmArchCtl *acp = ctxp->c_archctl;

		if (acp->ac_curvol < acp->ac_log->l_maxvol) {
		    sts = __pmLogChangeVol(acp, acp->ac_curvol + 1);
		    if (sts == 0) {
			PM_UNLOCK(ctxp->c_lock);
			if (pmDebugOptions.discovery)
			    fprintf(stderr, "process_logvol: %s fetch failed, suceeded in switching to next vol\n",
				p->context.name);
			break;
		    }
		}

		if (pmDebugOptions.discovery)
		    fprintf(stderr, "process_logvol: %s fetch failed and failed to switch to next vol: %s\n",
			p->context.name, pmErrStr(sts));
		/* we are done - wait for another callback */
		PM_UNLOCK(ctxp->c_lock);
		break;
	    }
	}
	else {
	    /*
	     * Fetch succeeded - call the values callback and continue
	     */
	    if (pmDebugOptions.discovery) {
		char		tbuf[64], bufs[64];

		fprintf(stderr, "process_logvol: %s FETCHED @%s [%s] %d metrics\n",
			p->context.name, timeval_str(&r->timestamp, tbuf, sizeof(tbuf)),
			timeval_stream_str(&r->timestamp, bufs, sizeof(bufs)),
			r->numpmid);
	    }
	    ts.tv_sec = r->timestamp.tv_sec;
	    ts.tv_nsec = r->timestamp.tv_usec * 1000;
	    pmDiscoverInvokeValuesCallBack(p, &ts, r);
	    pmFreeResult(r);
	}
    }
    /* datavol is now up-to-date and at EOF */
    p->flags &= ~PM_DISCOVER_FLAGS_DATAVOL_READY;
}

static void
pmDiscoverInvokeCallBacks(pmDiscover *p)
{
    int			sts;
    sds			msg;
    sds			metaname;

    if (p->ctx < 0) {
	/*
	 * once off initialization on the first event
	 */
	if (p->flags & (PM_DISCOVER_FLAGS_DATAVOL | PM_DISCOVER_FLAGS_META)) {
	    struct timeval	tvp;

	    /* create the PMAPI context (once off) */
	    if ((sts = pmNewContext(p->context.type, p->context.name)) < 0) {
		infofmt(msg, "pmNewContext failed for %s: %s\n",
			p->context.name, pmErrStr(sts));
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		return;
	    }
	    pmDiscoverNewSource(p, sts);
	    if ((sts = pmGetArchiveEnd(&tvp)) < 0) {
		infofmt(msg, "pmGetArchiveEnd failed for %s: %s\n",
				p->context.name, pmErrStr(sts));
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		pmDestroyContext(p->ctx);
		p->ctx = -1;
		return;
	    }
	    /* seek to end of archive for logvol data */
	    pmSetMode(PM_MODE_FORW, &tvp, 1);

	    /*
	     * For archive meta files, p->fd is the direct file descriptor
	     * and we pre-scan existing metadata. Note: we do NOT scan
	     * pre-existing logvol data.
	     */
	    metaname = sdsnew(p->context.name);
	    metaname = sdscat(metaname, ".meta");
	    if ((p->fd = open(metaname, O_RDONLY)) < 0) {
		infofmt(msg, "open failed for %s: %s\n", metaname, osstrerror());
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
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
    if (p->flags & (PM_DISCOVER_FLAGS_DATAVOL | PM_DISCOVER_FLAGS_DATAVOL_READY)) {
	/*
	 * datavol has data ready (either now or earlier during a metadata CB)
	 */
	p->flags |= PM_DISCOVER_FLAGS_DATAVOL_READY;

	if (p->flags & PM_DISCOVER_FLAGS_META_IN_PROGRESS) {
	    /*
	     * metadata read is in progress - delay reading logvol until it's finished.
	     */
	    if (pmDebugOptions.discovery) {
		fprintf(stderr, "pmDiscoverInvokeCallBacks: datavol ready, but metadata read in progress\n");
	    }
	}
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
	/* process new metadata, if any */
	process_metadata(p);
    }

    if ((p->flags & PM_DISCOVER_FLAGS_META_IN_PROGRESS) == 0) {
	/* no metdata read in progress, so process new datavol data, if any */
	process_logvol(p);
    }

    /* finally, purge deleted entries, if any */
    pmDiscoverPurgeDeleted();
}

static void
print_callback(pmDiscover *p)
{
    if (p->flags & (PM_DISCOVER_FLAGS_DELETED | PM_DISCOVER_FLAGS_NEW))
    	return;

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	fprintf(stderr, "    DIRECTORY %s %s\n",
	    p->context.name, pmDiscoverFlagsStr(p));
    }
    else {
	__pmContext *ctxp;
	__pmArchCtl *acp;

	if (p->ctx >= 0 && (ctxp = __pmHandleToPtr(p->ctx)) != NULL) {
	    acp = ctxp->c_archctl;
	    fprintf(stderr, "    ARCHIVE %s fd=%d ctx=%d maxvol=%d ac_curvol=%d ac_offset=%ld ac_vol=%d %s\n",
		p->context.name, p->fd, p->ctx, acp->ac_log->l_maxvol, acp->ac_curvol,
		acp->ac_offset, acp->ac_vol, pmDiscoverFlagsStr(p));
	    PM_UNLOCK(ctxp->c_lock);
	}
    }
}

static void
changed_callback(pmDiscover *p)
{
    static time_t last_dir_scan = 0;
    time_t now;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "CHANGED %s (%s)\n", p->context.name,
			pmDiscoverFlagsStr(p));

    if (p->flags & PM_DISCOVER_FLAGS_DELETED) {
	/*
	 * Path has been deleted. Do nothing for now. Will be purged
	 * in due course by pmDiscoverPurgeDeleted.
	 */
	return;
	
    }

    if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED) {
    	/* we do not monitor compressed files - do nothing */
	return;
    }

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	/*
	 * A changed directory path means a new archive or subdirectory may have
	 * been created or deleted - traverse and update the hash table.
	 *
	 * This is throttled because we get callbacks whenever the directory
	 * or ANYTHING in that directory changes.
	 */
	now = time(NULL);
	if (pmDebugOptions.discovery) {
	    fprintf(stderr, "DIRECTORY CHANGED now=%ld last_dir_scan=%ld %s (%s)\n",
	    	now, last_dir_scan, p->context.name, pmDiscoverFlagsStr(p));
	}

	/* only throttle if nothing has been deleted and we've had a recent dirscan */
	if (pmDiscoverTraverse(PM_DISCOVER_FLAGS_DELETED, NULL) == 0 && (now - last_dir_scan < 10)) {
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "--- THROTTLED ---\n");
	}
	else {
	    pmDiscoverArchives(p->context.name, p->module, p->data);
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_NEW, created_callback);
	    last_dir_scan = now;
	}
    }
    else if (p->flags & (PM_DISCOVER_FLAGS_DATAVOL|PM_DISCOVER_FLAGS_META)) {
    	/*
	 * Fetch new archive data (both metadata or logvol) and
	 * call the registered callbacks.
	 */
	pmDiscoverInvokeCallBacks(p);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "-- tracking:\n");
	pmDiscoverTraverse(PM_DISCOVER_FLAGS_ALL, print_callback);
	fprintf(stderr, "--\n");
    }
}

static void
dir_callback(pmDiscover *p)
{
    pmDiscoverMonitor(p->context.name, changed_callback);
}

static void
archive_callback(pmDiscover *p)
{
    if (p->flags & PM_DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
}

int
pmDiscoverRegister(const char *dir, pmDiscoverModule *module,
		pmDiscoverCallBacks *callbacks, void *arg)
{
    int			handle = -1;
    int			avail_handle = -1;
    pmDiscoverCallBacks	**cbp;

    if (callbacks != NULL) {
	for (handle = 0; handle < discoverCallBackTableSize; handle++) {
	    if (callbacks == discoverCallBackTable[handle])
		break; /* we're adding new dirs using an existing handle */
	    if (discoverCallBackTable[handle] == NULL)
		avail_handle = handle;
	}
	if (handle == discoverCallBackTableSize && avail_handle < 0) {
	    avail_handle = discoverCallBackTableSize++;
	    cbp = (pmDiscoverCallBacks **)realloc(discoverCallBackTable,
			discoverCallBackTableSize * sizeof(*cbp));
	    if (cbp == NULL)
		return -ENOMEM;
	    discoverCallBackTable = cbp;
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "%s: new handle [%d] for callbacks %p\n",
		    "pmDiscoverRegister", avail_handle, callbacks);
	}
	handle = avail_handle;
	discoverCallBackTable[handle] = callbacks;
    }
    /* else we are just adding dirs for all existing registered callbacks */

    if (dir) {
	/* NULL dir means add callbacks for existing directories and archives */
	pmDiscoverArchives(dir, module, arg);
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "Now tracking %d directories and %d archives\n",
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DIRECTORY, NULL),
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DATAVOL|PM_DISCOVER_FLAGS_META, NULL));
    }

    /* monitor the directories */
    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DIRECTORY, dir_callback);

    /* monitor archive data and metadata volumes, uncompressed only */
    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DATAVOL|PM_DISCOVER_FLAGS_META, archive_callback);

    return handle;
}

void
pmDiscoverUnregister(int handle)
{
    if (discoverCallBackTable != NULL &&
	handle >= 0 && handle < discoverCallBackTableSize)
    	discoverCallBackTable[handle] = NULL; /* unregister these callbacks */
}

/*
 * Decode a metadata desc record in buf of length len
 * Return 0 on success.
 */
static int
pmDiscoverDecodeMetaDesc(uint32_t *buf, int buflen, pmDesc *p_desc, int *p_numnames, char ***p_names)
{
    int			i;
    int			len;
    int			numnames;
    char		**names;
    char		*cp;
    pmDesc		*dp;

    dp = (pmDesc *)buf;
    dp->type = ntohl(dp->type);

    dp->sem = ntohl(dp->sem);
    dp->indom = __ntohpmInDom(dp->indom);
    dp->units = __ntohpmUnits(dp->units);
    dp->pmid = __ntohpmID(dp->pmid);
    if (pmDebugOptions.discovery)
	fprintf(stderr, "DECODE DESC metric %s (", pmIDStr(dp->pmid));
    numnames = ntohl(buf[sizeof(pmDesc)/sizeof(int)]);
    names = (char **)malloc(numnames * sizeof(char *));
    if (names == NULL)
	return -ENOMEM;
    for (i = 0; i < numnames; i++) {
	names[i] = NULL;
    }
    cp = (char *)&buf[sizeof(pmDesc)/sizeof(int) + 1];
    for (i = 0; i < numnames; i++) {
	memmove(&len, cp, sizeof(len));
	len = htonl(len);
	cp += sizeof(len);
	names[i] = (char *)malloc(len + 1);
	if (names[i] == NULL) {
	    while (i)
		free(names[--i]);
	    free(names);
	    return -ENOMEM;
	}
	strncpy(names[i], cp, len);
	names[i][len] = '\0';
	cp += len;
    }
    if (pmDebugOptions.discovery) {
	__pmPrintMetricNames(stderr, numnames, names, " or ");
	fprintf(stderr, ")\n");
	pmPrintDesc(stderr, dp);
    }

    *p_numnames = numnames;
    *p_names = names;
    *p_desc = *dp;

    return 0;
}

/*
 * Decode a metadata indom record in buf of length len.
 * Return 0 on success.
 */
static int
pmDiscoverDecodeMetaInDom(uint32_t *buf, int len, pmTimespec *ts, pmInResult *inresult)
{
    pmTimeval		*tvp;
    uint32_t		*namesbuf;
    uint32_t		*index;
    char		*str;
    int			j;
    pmInResult		ir;

    tvp = (pmTimeval *)&buf[0];
    ts->tv_sec = ntohl(tvp->tv_sec);
    ts->tv_nsec = ntohl(tvp->tv_usec) * 1000;
    ir.indom = __ntohpmInDom(buf[2]);
    ir.numinst = ntohl(buf[3]);
    ir.instlist = NULL;
    ir.namelist = NULL;

    if (ir.numinst > 0) {
	if ((ir.instlist = (int *)calloc(ir.numinst, sizeof(int))) == NULL)
	    return -ENOMEM;
	ir.namelist = (char **)calloc(ir.numinst, sizeof(char *));
	if (ir.namelist == NULL) {
	    free(ir.instlist);
	    return -ENOMEM;
	}
	namesbuf = &buf[4];
	index = &namesbuf[ir.numinst];
	str = (char *)&namesbuf[ir.numinst + ir.numinst];
	for (j=0; j < ir.numinst; j++) {
	    ir.instlist[j] = ntohl(namesbuf[j]);
	    ir.namelist[j] = strdup(&str[ntohl(index[j])]);
	}
    }

    *inresult = ir;
    return 0;
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
pmDiscoverDecodeMetaLabelSet(uint32_t *buf, int buflen, pmTimespec *ts, int *identp, int *typep, int *nsetsp, pmLabelSet **setsp)
{
    char		*json, *tbuf = (char *)buf;
    pmLabelSet		*labelsets = NULL;
    pmTimeval		*tvp;
    int			type;
    int			ident;
    int			nsets;
    int			inst;
    int			jsonlen;
    int			nlabels;
    int			i, j, k;

    k = 0;
    tvp = (pmTimeval *)&tbuf[k];
    ts->tv_sec = ntohl(tvp->tv_sec);
    ts->tv_nsec = ntohl(tvp->tv_usec) * 1000;
    k += sizeof(*tvp);

    type = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(type);

    ident = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(ident);

    nsets = *((unsigned int *)&tbuf[k]);
    nsets = ntohl(nsets);
    k += sizeof(nsets);

    if (pmDebugOptions.discovery)
	fprintf(stderr, "DECODE LABELSET type=%d (%s) ident=%d nsets=%d\n",
	    type, __pmLabelTypeString(type), ident, nsets);

    if (nsets < 0)
    	return PM_ERR_IPC;
    if (nsets == 0)
	goto success;

    if ((labelsets = (pmLabelSet *)calloc(nsets, sizeof(pmLabelSet))) == NULL)
	return -ENOMEM;

    for (i = 0; i < nsets; i++) {
	inst = *((unsigned int*)&tbuf[k]);
	inst = ntohl(inst);
	k += sizeof(inst);
	labelsets[i].inst = inst;

	jsonlen = ntohl(*((unsigned int*)&tbuf[k]));
	k += sizeof(jsonlen);
	labelsets[i].jsonlen = jsonlen;

	if (jsonlen < 0 || jsonlen > PM_MAXLABELJSONLEN)
	    goto corrupt;
	json = (char *)&tbuf[k];
	k += jsonlen;
	if (jsonlen > 0) {
	    if ((labelsets[i].json = strndup(json, jsonlen)) == NULL) {
		pmFreeLabelSets(labelsets, nsets);
		return -ENOMEM;
	    }
	}

	/* setup the label nlabels count */
	nlabels = ntohl(*((unsigned int *)&tbuf[k]));
	k += sizeof(nlabels);
	labelsets[i].nlabels = nlabels;

	if (nlabels >= PM_MAXLABELS)
	    goto corrupt;

	if (nlabels > 0) { /* less than zero is an err code, skip it */
	    if (nlabels > PM_MAXLABELS || k + nlabels * sizeof(pmLabel) > buflen)
	    	goto corrupt;
	    labelsets[i].labels = (pmLabel *)calloc(nlabels, sizeof(pmLabel));
	    if (labelsets[i].labels == NULL) {
		pmFreeLabelSets(labelsets, nsets);
		return -ENOMEM;
	    }
	    /* setup the label pmLabel structures */
	    for (j = 0; j < nlabels; j++) {
		labelsets[i].labels[j] = *((pmLabel *)&tbuf[k]);
		__ntohpmLabel(&labelsets[i].labels[j]);
		k += sizeof(pmLabel);
	    }
	}
    }

success:
    *identp = ident;
    *typep = type;
    *nsetsp = nsets;
    *setsp = labelsets;
    return 0;

corrupt:
    if (labelsets)
	pmFreeLabelSets(labelsets, nsets);
    return PM_ERR_IPC;
}
