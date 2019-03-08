/*
 * Copyright (c) 2018 Red Hat.
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

/* internal hash table of discovered paths */
#define PM_DISCOVER_HASHTAB_SIZE 64
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
 * Lookup or Add a discovered file path (directory or PCP archive file)
 * Return path table entry (new or existing).
 */
static pmDiscover *
pmDiscoverLookupAdd(const char *path, pmDiscoverModule *module, void *arg)
{
    pmDiscover		*p, *h;
    unsigned int	k = strhash(path, PM_DISCOVER_HASHTAB_SIZE);

    for (p = NULL, h = discover_hashtable[k]; h != NULL; p = h, h = h->next) {
    	if (strcmp(h->context.name, path) == 0)
	    break;
    }

    if (h == NULL && module != NULL) {	/* hash table insert mode */
	if ((h = (pmDiscover *)calloc(1, sizeof(pmDiscover))) == NULL)
	    return NULL;
	h->fd = -1; /* no meta descriptor initially */
	h->ctx = -1; /* no PMAPI context initially */
	h->flags = PM_DISCOVER_FLAGS_NEW;
	h->context.type = PM_CONTEXT_ARCHIVE;
	h->context.name = sdsnew(path);
	h->module = module;
	h->data = arg;
	if (p == NULL)
	    discover_hashtable[k] = h;
	else
	    p->next = h;
    }
    return h;
}

static pmDiscover *
pmDiscoverLookup(const char *path)
{
    return pmDiscoverLookupAdd(path, NULL, NULL);
}

static pmDiscover *
pmDiscoverAdd(const char *path, pmDiscoverModule *module, void *arg)
{
    return pmDiscoverLookupAdd(path, module, arg);
}

/*
 * Delete tracking of a previously discovered path. Frees resources and
 * destroy PCP context (if any).
 */
static void
pmDiscoverDelete(sds path)
{
    pmDiscover		*p, *h;
    unsigned int	k = strhash(path, PM_DISCOVER_HASHTAB_SIZE);

    for (p = NULL, h = discover_hashtable[k]; h != NULL; p = h, h = h->next) {
    	if (sdscmp(h->context.name, path) == 0) {
	    if (p == NULL)
	    	discover_hashtable[k] = NULL;
	    else
	    	p->next = h->next;

	    if (h->ctx >= 0)
		pmDestroyContext(h->ctx);
	    if (h->fd >= 0)
		close(h->fd);

	    if (h->context.name)
		sdsfree(h->context.name);
	    if (h->context.source)
		sdsfree(h->context.source);
	    if (h->context.labelset)
		pmFreeLabelSets(h->context.labelset, 1);
	    memset(h, 0, sizeof(pmDiscover));
	    free(h);
	    break;
	}
    }
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
    char		path[MAXNAMELEN];
    char		basepath[MAXNAMELEN];
    int			sep = pmPathSeparator();

    if (uv_fs_scandir(NULL, &req, dir, 0, NULL) < 0)
	return -ESRCH;

    a = pmDiscoverAdd(dir, module, arg);
    a->flags |= PM_DISCOVER_FLAGS_DIRECTORY;

    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
	snprintf(path, sizeof(path), "%s%c%s", dir, sep, dent.name);
	if (uv_fs_stat(NULL, &sreq, path, NULL) < 0)
	    continue;
	s = &sreq.statbuf;
	strncpy(basepath, path, sizeof(basepath)); /* __pmLogBaseName modifies it's argument */
	if (S_ISREG(s->st_mode) && __pmLogBaseName(basepath) != NULL) {
	    /*
	     * An archive file (index, meta or data vol). If compressed, then
	     * it is read-only and we don't have to monitor it for growth.
	     */
	    a = pmDiscoverAdd(path, module, arg);
	    a->flags &= ~PM_DISCOVER_FLAGS_DELETED;

	    if (strstr(path, ".meta"))
	    	a->flags |= PM_DISCOVER_FLAGS_META;
	    else if (strstr(path, ".index"))
	    	a->flags |= PM_DISCOVER_FLAGS_INDEX;
	    else
	    	a->flags |= PM_DISCOVER_FLAGS_DATAVOL;

	    /* compare to libpcp io.c for suffix list */
	    if (strstr(path, ".xz") || strstr(path, ".gz"))
	    	a->flags |= PM_DISCOVER_FLAGS_COMPRESSED;
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

static void
fs_change_callBack(uv_fs_event_t *handle, const char *filename, int events, int status)
{
    char		buffer[MAXNAMELEN];
    size_t		bytes = sizeof(buffer) - 1;
    pmDiscover		*p;
    uv_fs_t		sreq;
    sds			path;
    int			path_changed = 0;

    uv_fs_event_getpath(handle, buffer, &bytes);
    path = sdsnewlen(buffer, bytes);

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s: event on %s -", "fs_change_callBack", path);
	if (events & UV_RENAME)
	    fprintf(stderr, " renamed");
	if (events & UV_CHANGE)
	    fprintf(stderr, " changed");
	fputc('\n', stderr);
    }

    /*
     * Lookup the path, stat and update it's flags accordingly. If the
     * path has been deleted, stop it's event monitor and free the req buffer.
     * Then call the pmDiscovery callback.
     */
    if ((p = pmDiscoverLookup(path)) == NULL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: filename %s lookup failed\n",
		    "fs_change_callBack", filename);
    }
    else if (uv_fs_stat(NULL, &sreq, p->context.name, NULL) < 0) {
    	p->flags |= PM_DISCOVER_FLAGS_DELETED;
	if (p->event_handle) {
	    if (p->event_handle) {
		uv_fs_event_stop(p->event_handle);
		free(p->event_handle);
		p->event_handle = NULL;
	    }
	}
	/* path has been deleted. statbuf is invalid */
	memset(&p->statbuf, 0, sizeof(p->statbuf));
	path_changed = 1;
    }
    else {
	/* avoid spurious events. only call the callBack if it really changed */
	if (p->statbuf.st_mtim.tv_sec != sreq.statbuf.st_mtim.tv_sec ||
	    p->statbuf.st_mtim.tv_nsec != sreq.statbuf.st_mtim.tv_nsec)
	    path_changed = 1;
	p->statbuf = sreq.statbuf; /* struct copy */
	uv_fs_req_cleanup(&sreq);
    }

    if (p && p->changed && path_changed)
    	p->changed(p);

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

    if ((p = pmDiscoverLookup(path)) == NULL)
	return -ESRCH;
    data = getDiscoverModuleData(p->module);

    /* save the discovery callback to be invoked */
    p->changed = callback;

    /* filesystem event request buffer */
    if ((p->event_handle = malloc(sizeof(uv_fs_event_t))) != NULL) {
	/*
	 * Start monitoring, using given uv loop. Up to the caller to create
	 * a PCP PMAPI context and to fetch/logtail in the changed callback.
	 */
	uv_fs_event_init(data->events, p->event_handle);
	uv_fs_event_start(p->event_handle, fs_change_callBack, p->context.name,
			UV_FS_EVENT_WATCH_ENTRY);
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

    snprintf(buf, sizeof(buf), "flags: 0x%04x |", p->flags);
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
    if (pmDebugOptions.discovery)
	fprintf(stderr, "CREATED %s, %s\n", p->context.name, pmDiscoverFlagsStr(p));

    p->flags &= ~PM_DISCOVER_FLAGS_NEW;

    if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED)
    	return; /* compressed archives don't grow */

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR directory %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }

    if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR logvol %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR metadata %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }
}

static void
deleted_callback(pmDiscover *p)
{
    if (pmDebugOptions.discovery)
	fprintf(stderr, "DELETED %s (%s)\n", p->context.name,
			pmDiscoverFlagsStr(p));
    pmDiscoverDelete(p->context.name);
    /* p is now no longer valid */
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
	fprintf(stderr, "%s[%s]: %s name %s\n", "redisSlotsDiscoverSource",
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
	fprintf(stderr, "%s[%s]: %s numpmid %d\n", "redisSlotsDiscoverValues",
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
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s name%s", "redisSlotsDiscoverMetric",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, numnames > 0 ? " " : "(none)\n");
	for (i = 0; i < numnames; i++)
	    printf("\"%s\"%s", names[i], i < numnames - 1 ? ", " : "\n");
	pmPrintDesc(stderr, desc);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_metric != NULL)
	    callbacks->on_metric(&event, desc, numnames, names, p->data);
    }
}

static void
pmDiscoverInvokeInDomCallBacks(pmDiscover *p, pmTimespec *ts, pmInResult *in)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32], inbuf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s numinst %d indom %s\n",
			"redisSlotsDiscoverInDom",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, in->numinst,
			pmInDomStr_r(in->indom, inbuf, sizeof(inbuf)));
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_indom != NULL)
	    callbacks->on_indom(&event, in, p->data);
    }
}

static void
pmDiscoverInvokeLabelsCallBacks(pmDiscover *p, pmTimespec *ts,
		int ident, int type, pmLabelSet *sets, int nsets)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32], idbuf[64];
    int			i;

    if (pmDebugOptions.discovery) {
	__pmLabelIdentString(ident, type, idbuf, sizeof(idbuf));
	fprintf(stderr, "%s[%s]: %s ID %s type %s\n",
			"redisSlotsDiscoverLabels",
			timespec_str(ts, buf, sizeof(buf)),
			p->context.source, idbuf, __pmLabelTypeString(type));
	pmPrintLabelSets(stderr, ident, type, sets, nsets);
	if (pmDebugOptions.labels)
	    fprintf(stderr, "context labels %s\n", p->context.labelset->json);
    }

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_labels != NULL)
	    callbacks->on_labels(&event, ident, type, sets, nsets, p->data);
    }
}

static void
pmDiscoverInvokeTextCallBacks(pmDiscover *p, pmTimespec *ts,
		int ident, int type, char *text)
{
    pmDiscoverCallBacks	*callbacks;
    pmDiscoverEvent	event;
    char		buf[32];
    int			i;

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "%s[%s]: %s ", "redisSlotsDiscoverText",
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

    discover_event_init(p, ts, &event);
    for (i = 0; i < discoverCallBackTableSize; i++) {
	if ((callbacks = discoverCallBackTable[i]) &&
	    callbacks->on_text != NULL)
	    callbacks->on_text(&event, ident, type, text, p->data);
    }
}

static void
pmDiscoverNewSource(pmDiscover *p, int context)
{
    pmLabelSet		*labelset;
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

static void
pmDiscoverInvokeCallBacks(pmDiscover *p)
{
    pmResult		*r;
    pmTimespec		ts;
    pmDesc		desc;
    off_t		off;
    char		*buffer;
    int			i, nb, len, sts, nrec, nsets;
    int			type, id; /* pmID or pmInDom */
    int			nnames;
    char		**names;
    pmInResult		inresult;
    pmLabelSet		*labelset;
    unsigned char	hash[20];
    __pmLogHdr		hdr;
    sds			msg, source;
    uint32_t		*bp;
    static uint32_t	*buf = NULL;
    static int		buflen = 0;

    if (p->ctx < 0) {
	/*
	 * once off initialization on the first event
	 */
	if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
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
	    pmSetMode(PM_MODE_FORW, &tvp, 1);
	}
	else if (p->flags & PM_DISCOVER_FLAGS_META) {
	    if ((sts = pmNewContext(p->context.type, p->context.name)) < 0) {
		infofmt(msg, "pmNewContext failed for %s: %s\n",
				p->context.name, pmErrStr(sts));
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		return;
	    }
	    pmDiscoverNewSource(p, sts);

	    /* for archive meta files, p->fd is the direct file descriptor */
	    if ((p->fd = open(p->context.name, O_RDONLY)) < 0) {
		infofmt(msg, "open failed for %s: %s\n", p->context.name,
				osstrerror());
		moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
		return;
	    }

	    /*
	     * Scan raw metadata file thru to current EOF, correctly handling
	     * partial reads.
	     */
	    off = lseek(p->fd, 0, SEEK_CUR);
	    for (nrec = 0; ; nrec++) {
		if ((nb = read(p->fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		    /* EOF or partial read: rewind so we can wait for more data */
		    lseek(p->fd, off, SEEK_SET);
		    break;
		}
		hdr.len = ntohl(hdr.len);
		hdr.type = ntohl(hdr.type);
		len = hdr.len - sizeof(hdr);
		if (len > buflen) {
		    buflen = len + 4096;
		    if ((bp = (uint32_t *)realloc(buf, buflen)) != NULL) {
			buf = bp;
		    } else {
			infofmt(msg, "realloc %d bytes failed for %s\n",
					buflen, p->context.name);
			moduleinfo(p->module, PMLOG_ERROR, msg, p->data);
			break;
		    }
		}

		if ((nb = read(p->fd, buf, len)) != len) {
		    /* EOF or partial read: rewind and wait for more data, as above */
		    lseek(p->fd, off, SEEK_SET);
		    break;
		}
	    }
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "METADATA opened and scanned"
				" %d metadata records to EOF\n", nrec);
	}
    }

    /*
     * Now call the registered callbacks, if any, for this path
     */
    if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	/*
	 * fetch metric values to EOF and call all registered callbacks
	 */
	pmUseContext(p->ctx);
	while (pmFetchArchive(&r) == 0) {
	    if (pmDebugOptions.discovery) {
		char		tbuf[64], bufs[64];

		fprintf(stderr, "FETCHED @%s [%s] %d metrics\n",
			timeval_str(&r->timestamp, tbuf, sizeof(tbuf)),
			timeval_stream_str(&r->timestamp, bufs, sizeof(bufs)),
			r->numpmid);
	    }
	    ts.tv_sec = r->timestamp.tv_sec;
	    ts.tv_nsec = r->timestamp.tv_usec * 1000;
	    pmDiscoverInvokeValuesCallBack(p, &ts, r);
	    pmFreeResult(r);
	}
    }
    else if (p->flags & PM_DISCOVER_FLAGS_META) {
    	/*
	 * Read all metadata records from current offset though to EOF
	 * and call all registered callbacks
	 */
	for (;;) {
	    off = lseek(p->fd, 0, SEEK_CUR);
	    nb = read(p->fd, &hdr, sizeof(__pmLogHdr));

	    if (nb != sizeof(__pmLogHdr)) {
		/* rewind so we can wait for more data on the next change CallBack */
		lseek(p->fd, off, SEEK_SET);
		break;
	    }

	    hdr.len = ntohl(hdr.len);
	    hdr.type = ntohl(hdr.type);
	    if (hdr.len <= 0) {
	    	/* rewind and wait for more data, as above */
		lseek(p->fd, off, SEEK_SET);
		break;
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
		break;
	    }

	    if (pmDebugOptions.discovery)
		fprintf(stderr, "Log metadata read len %4d type %d:", len, hdr.type);

	    switch (hdr.type) {
		case TYPE_DESC:
		    /* decode pmDesc result from PDU buffer */
		    nnames = 0;
		    names = NULL;
		    if (pmDiscoverDecodeMetaDesc(buf, len, &desc, &nnames, &names) < 0)
			break;
		    /* use timestamp from last modification */
		    ts.tv_sec = p->statbuf.st_mtim.tv_sec;
		    ts.tv_nsec = p->statbuf.st_mtim.tv_nsec;
		    pmDiscoverInvokeMetricCallBacks(p, &ts, &desc, nnames, names);
		    for (i = 0; i < nnames; i++)
			free(names[i]);
		    if (names)
			free(names);
		    break;

	    	case TYPE_INDOM:
		    /* decode indom result from buffer */
		    if (pmDiscoverDecodeMetaInDom(buf, len, &ts, &inresult) < 0)
			break;
		    pmDiscoverInvokeInDomCallBacks(p, &ts, &inresult);
		    if (inresult.numinst > 0) {
			for (i = 0; i < inresult.numinst; i++)
			    free(inresult.namelist[i]);
			free(inresult.namelist);
			free(inresult.instlist);
		    }
		    break;

		case TYPE_LABEL:
		    /* decode labelset from buffer */
		    if (pmDiscoverDecodeMetaLabelSet(buf, len, &ts, &id, &type, &nsets, &labelset) < 0)
			break;

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
			    p->context.labelset = labelset;
			    pmDiscoverInvokeSourceCallBacks(p, &ts);
			}
		    }
		    pmDiscoverInvokeLabelsCallBacks(p, &ts, id, type, labelset, nsets);
		    if (labelset != p->context.labelset)
			pmFreeLabelSets(labelset, nsets);
		    break;

		case TYPE_TEXT:
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "TEXT\n");
		    /* decode help text from buffer */
		    buffer = NULL;
		    if ((sts = pmDiscoverDecodeMetaHelpText(buf, len, &type, &id, &buffer)) < 0)
			break;
		    /* use timestamp from last modification */
		    ts.tv_sec = p->statbuf.st_mtim.tv_sec;
		    ts.tv_nsec = p->statbuf.st_mtim.tv_nsec;
		    pmDiscoverInvokeTextCallBacks(p, &ts, id, type, buffer);
		    if (buffer)
			free(buffer);
		    break;

		default:
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "%s, len = %d\n",
				hdr.type == (PM_LOG_MAGIC | PM_LOG_VERS02) ?
				"PM_LOG_MAGICv2" : "UNKNOWN", len);
		    break;
	    }
	}
    }
}

static void
changed_callback(pmDiscover *p)
{
    if (pmDebugOptions.discovery)
	fprintf(stderr, "CHANGED %s (%s)\n", p->context.name,
			pmDiscoverFlagsStr(p));

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	/*
	 * A changed directory path means a new archive or subdirectory
	 * has been created, or an existing path has been deleted.
	 */
	pmDiscoverArchives(p->context.name, p->module, p->data);
	pmDiscoverTraverse(PM_DISCOVER_FLAGS_NEW, created_callback);
	pmDiscoverTraverse(PM_DISCOVER_FLAGS_DELETED, deleted_callback);
    }
    else if (p->flags & PM_DISCOVER_FLAGS_DELETED) {
	/* path has been deleted - cleanup and remove */
	deleted_callback(p);
    }
    else if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED) {
    	/* we do not monitor any compressed files - do nothing */
	; /**/
    }
    else if (p->flags & (PM_DISCOVER_FLAGS_DATAVOL|PM_DISCOVER_FLAGS_META)) {
    	/*
	 * We only monitor uncompressed logvol and metadata paths. Fetch new data
	 * (metadata or logvol) and call the registered callbacks.
	 */
	pmDiscoverInvokeCallBacks(p);
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
    if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED)
    	return; /* compressed archives don't grow */

    if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE LOGVOL %s\n", p->context.name);
	pmDiscoverMonitor(p->context.name, changed_callback);
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE METADATA %s\n", p->context.name);
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
	fprintf(stderr, "Now managing %d directories and %d archive files\n",
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DIRECTORY, NULL),
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DATAVOL, NULL));
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

    if (nsets < 0)
    	return PM_ERR_IPC;
    if (nsets == 0) {
    	labelsets = NULL;
	goto success;
    }

    if ((labelsets = (pmLabelSet *)calloc(nsets, sizeof(pmLabelSet))) == NULL)
	return -ENOMEM;
    memset(labelsets, 0, nsets * sizeof(pmLabelSet));

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
