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
#if defined(HAVE_LIBUV)
#include "discover.h"

/* array of registered callbacks, see pmDiscoverRegister() */
static int n_discoverCallBacksList = 0;
static pmDiscoverCallBacks **discoverCallBacksList = NULL;

/* internal hash table of discovered paths */
#define PM_DISCOVER_HASHTAB_SIZE 64
static pmDiscover *discover_hashtable[PM_DISCOVER_HASHTAB_SIZE] = {NULL};

/* FNV string hash algorithm. Return unsigned in range 0 .. limit-1 */
static unsigned int
strhash(const char *s, unsigned int limit)
{
    unsigned int h = 2166136261; /* FNV offset_basis */
    unsigned char *us = (unsigned char *)s;

    for (; *us != '\0'; us++) {
    	h ^= *us;
	h *= 16777619; /* fnv_prime */
    }
    return h % limit;
}

static pmDiscover *
pmDiscoverLookupAdd(char *path, int add)
{
    pmDiscover *p, *h;
    unsigned int k = strhash(path, PM_DISCOVER_HASHTAB_SIZE);

    for (p=NULL, h=discover_hashtable[k]; h; p=h, h=h->next) {
    	if (strcmp(h->path, path) == 0)
	    break;
    }

    if (add && h == NULL) {
	h = (pmDiscover *)malloc(sizeof(pmDiscover));
	memset(h, 0, sizeof(pmDiscover));
	h->ctx = -1; /* no PMAPI context initially */
	h->flags = PM_DISCOVER_FLAGS_NEW;
	h->path = strdup(path);
	h->next = NULL;
	if (p == NULL)
	    discover_hashtable[k] = h;
	else
	    p->next = h;
    }
    return h;
}

pmDiscover *
pmDiscoverLookup(char *path)
{
    return pmDiscoverLookupAdd(path, 0);
}

pmDiscover *
pmDiscoverAdd(char *path)
{
    return pmDiscoverLookupAdd(path, 1);
}

void
pmDiscoverDelete(char *path)
{
    pmDiscover *p, *h;
    unsigned int k = strhash(path, PM_DISCOVER_HASHTAB_SIZE);

    for (p=NULL, h=discover_hashtable[k]; h; p=h, h=h->next) {
    	if (strcmp(h->path, path) == 0) {
	    if (p == NULL)
	    	discover_hashtable[k] = NULL;
	    else
	    	p->next = h->next;

	    if (h->ctx >= 0) {
		if (h->flags & PM_DISCOVER_FLAGS_META)
		    close(h->ctx);
		else
		    pmDestroyContext(h->ctx);

		if (h->archiveLabel)
		    free(h->archiveLabel);
	    }

	    free(h->path);
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
int
pmDiscoverTraverse(unsigned int flags, void (*callBack)(pmDiscover *))
{
    int c = 0, i;
    pmDiscover *p;

    for (i=0; i < PM_DISCOVER_HASHTAB_SIZE; i++) {
    	for (p = discover_hashtable[i]; p; p = p->next) {
	    if (p->flags & flags) {
		if (callBack)
		    callBack(p);
		c++;
	    }
	}
    }
    return c;
}

/*
 * Discover/refresh directories and archive paths below dir.
 */
int
pmDiscoverArchives(char *dir)
{
    int sts;
    uv_fs_t sreq, req;
    uv_dirent_t dent;
    uv_stat_t *s;
    pmDiscover *a;
    int sep = pmPathSeparator();
    char path[MAXNAMLEN];
    char basepath[MAXNAMLEN];

    if ((sts = uv_fs_scandir(NULL, &req, dir, 0, NULL)) < 0)
    	return sts; /* err */

    a = pmDiscoverAdd(dir);
    a->flags |= PM_DISCOVER_FLAGS_DIRECTORY;

    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
	snprintf(path, sizeof(path), "%s%c%s", dir, sep, dent.name);
	if ((sts = uv_fs_stat(NULL, &sreq, path, NULL)) < 0)
	    continue;
	s = &sreq.statbuf;
	strncpy(basepath, path, sizeof(basepath)); /* __pmLogBaseName modifies it's argument */
	if (S_ISREG(s->st_mode) && __pmLogBaseName(basepath) != NULL) {
	    /*
	     * An archive file (index, meta or data vol). If compressed, then
	     * it is read-only and we don't have to monitor it for growth.
	     */
	    a = pmDiscoverAdd(path);
	    a->flags &= ~PM_DISCOVER_FLAGS_DELETED;

	    if (strstr(path, ".meta"))
	    	a->flags |= PM_DISCOVER_FLAGS_META;
	    else if (strstr(path, ".index"))
	    	a->flags |= PM_DISCOVER_FLAGS_INDEX;
	    else
	    	a->flags |= PM_DISCOVER_FLAGS_DATAVOL;

	    if (strstr(path, ".xz") || strstr(path, ".gz")) /* any other suffixes? */
	    	a->flags |= PM_DISCOVER_FLAGS_COMPRESSED;
	}
	else if (S_ISDIR(s->st_mode)) {
	    /*
	     * Recurse into subdir
	     */
	    pmDiscoverArchives(path);
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
    int sts;
    uv_fs_t sreq;
    char path[MAXNAMLEN];
    size_t size = sizeof(path)-1;
    pmDiscover *p;
    int path_changed = 0;

    uv_fs_event_getpath(handle, path, &size);
    path[size] = '\0';

#if 0
    printf("Change detected in %s: ", path);
    if (events & UV_RENAME)
        printf("renamed\n");
    if (events & UV_CHANGE)
        fprintf(stderr, "changed\n");
#endif

    /*
     * Lookup the path, stat and update it's flags accordingly. If the
     * path has been deleted, stop it's event monitor and free the req buffer.
     * Then call the pmDiscovery callback.
     */
    p = pmDiscoverLookup(path);
    if (p == NULL) {
	/* TODO wrap this in pmDebug */
    	// fprintf(stderr, "fs_change_callBack filename=%s failed to lookup\n", filename);
	return;
    }
    if ((sts = uv_fs_stat(NULL, &sreq, p->path, NULL)) < 0) {
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

    if (p->changeCallBack && path_changed)
    	p->changeCallBack(p);
}

/*
 * Monitor path (directory or archive) and invoke given callback when it changes
 * return 0 or -1 on error
 */
int
pmDiscoverMonitor(char *path, void (*callBack)(pmDiscover *))
{
    pmDiscover *p;

    if ((p = pmDiscoverLookup(path)) == NULL)
    	return -1; /* err */

    /* save the discovery callback to be invoked */
    p->changeCallBack = callBack;

    /* filesystem event request buffer */
    if ((p->event_handle = malloc(sizeof(uv_fs_event_t))) != NULL) {
	/*
	 * Start monitoring, using default uv loop. Up to the caller to create
	 * a PCP PMAPI context and to fetch/logtail in the changed callback.
	 */
	uv_fs_event_init(uv_default_loop(), p->event_handle);
	uv_fs_event_start(p->event_handle, fs_change_callBack, p->path, UV_FS_EVENT_WATCH_ENTRY);
    }

    return 0;
}

static struct {
    unsigned int flag;
    char *name;
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

char *
pmDiscoverFlagsStr(pmDiscover *p)
{
    unsigned int i;
    static char buf[MAXPATHLEN];

    snprintf(buf, sizeof(buf), "flags: 0x%04x |", p->flags);
    for (i=0; flags_str[i].name; i++) {
    	if (p->flags & flags_str[i].flag)
	    strncat(buf, flags_str[i].name, sizeof(buf)-1);
    }
    return buf;
}


static void changed_CallBack(pmDiscover *); /* fwd decl */

static void
created_CallBack(pmDiscover *p)
{
    if (pmDebugOptions.discovery)
	fprintf(stderr, "CREATED %s, %s\n", p->path, pmDiscoverFlagsStr(p));
    p->flags &= ~PM_DISCOVER_FLAGS_NEW;

    if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED)
    	return; /* compressed archives don't grow */

    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR directory %s\n", p->path);
	pmDiscoverMonitor(p->path, changed_CallBack);
    }

    if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR logvol %s\n", p->path);
	pmDiscoverMonitor(p->path, changed_CallBack);
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "MONITOR metadata %s\n", p->path);
	pmDiscoverMonitor(p->path, changed_CallBack);
    }
}

static void
deleted_CallBack(pmDiscover *p)
{
    if (pmDebugOptions.discovery)
	fprintf(stderr, "DELETED %s, %s\n", p->path, pmDiscoverFlagsStr(p));
    pmDiscoverDelete(p->path);
    /* p is now no longer valid */
}

static void
pmDiscoverInvokeCallBacks(pmDiscover *p)
{
    struct timeval tvp;
    pmResult *r;
    struct tm tmp;
    time_t now;
    double stamp;
    char *tp, tmbuf[32];
    off_t off;
    int nb;
    int len;
    __pmLogHdr hdr;
    uint32_t *buf = NULL;
    int buflen = 0;
    int sts;
    int nrec;
    struct timeval tv;
    pmTimeval ptv;
    int i;
    char *buffer = NULL;
    int type;
    int id; /* pmID or pmInDom */
    pmDesc desc;
    int numnames = 0;
    char **names = NULL;
    pmInResult inresult;
    int ident;
    int nsets;
    pmLabelSet *labelset;


    if (p->ctx < 0) {
    	/*
	 * once off initialization on the first event
	 */
	if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	    /* create the PMAPI context (once off) */
	    if ((p->ctx = pmNewContext(PM_CONTEXT_ARCHIVE, p->path)) < 0) {
		fprintf(stderr, "ERROR pmNewContext \"%s\" -> %s\n", p->path, pmErrStr(p->ctx));
		return;
	    }
	    if ((p->archiveLabel = (pmLogLabel *)malloc(sizeof(pmLogLabel))) == NULL) {
		fprintf(stderr, "ERROR malloc pmGetArchiveLabel \"%s\" failed\n", p->path);
		return;
	    }
	    if ((sts = pmGetArchiveLabel(p->archiveLabel)) < 0) {
		fprintf(stderr, "ERROR pmGetArchiveLabel \"%s\" failed: %s\n", p->path, pmErrStr(sts));
		return;
	    }
	    if ((sts = pmGetArchiveEnd(&tvp)) < 0) {
		fprintf(stderr, "ERROR pmGetArchiveEnd \"%s\" failed: %s\n", p->path, pmErrStr(sts));
		return;
	    }
	    pmSetMode(PM_MODE_FORW, &tvp, 1);
	}
	else if (p->flags & PM_DISCOVER_FLAGS_META) {
	    /* temp context to get the archive label */
	    if ((p->ctx = pmNewContext(PM_CONTEXT_ARCHIVE, p->path)) < 0) {
		fprintf(stderr, "ERROR pmNewContext \"%s\" -> %s\n", p->path, pmErrStr(p->ctx));
		return;
	    }
	    if ((p->archiveLabel = (pmLogLabel *)malloc(sizeof(pmLogLabel))) == NULL) {
		fprintf(stderr, "ERROR malloc pmGetArchiveLabel \"%s\" failed\n", p->path);
		pmDestroyContext(p->ctx);
		return;
	    }
	    if ((sts = pmGetArchiveLabel(p->archiveLabel)) < 0) {
		fprintf(stderr, "ERROR pmGetArchiveLabel \"%s\" failed: %s\n", p->path, pmErrStr(sts));
		pmDestroyContext(p->ctx);
		return;
	    }
	    pmDestroyContext(p->ctx);

	    /* note: for archive meta files, p->ctx is a regular file descriptor */
	    if ((p->ctx = open(p->path, O_RDONLY)) < 0) {
		fprintf(stderr, "ERROR open \"%s\" -> error %d\n", p->path, errno);
		return;
	    }
	    off = lseek(p->ctx, 0, SEEK_CUR);

	    /*
	     * Scan raw metadata file thru to current EOF, correctly handle partial reads
	     * TODO: should all this initial metadata be sent via callbacks?
	     * (code below currently skips thru to EOF)
	     */
	    for (nrec=0;;nrec++) {
		if ((nb = read(p->ctx, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		    /* EOF or partial read: rewind so we can wait for more data */
		    lseek(p->ctx, off, SEEK_SET);
		    break;
		}
		hdr.len = ntohl(hdr.len);
		hdr.type = ntohl(hdr.type);
		len = hdr.len - sizeof(hdr);
		if (len > buflen) {
		    buflen = len + 1048576;
		    buf = (uint32_t *)realloc(buf, buflen);
		}

		if ((nb = read(p->ctx, buf, len)) != len) {
		    /* EOF or partial read: rewind and wait for more data, as above */
		    lseek(p->ctx, off, SEEK_SET);
		    break;
		}
	    }
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "METADATA opened and scanned past %d metadata records to EOF\n", nrec);
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
	    /* no need to decode result. pmFetchArchive does it for us */
	    now = (time_t)r->timestamp.tv_sec;
	    pmLocaltime(&now, &tmp);
	    pmCtime(&now, tmbuf);
	    if ((tp = strrchr(tmbuf, '\n')) != NULL)
		*tp = '\0';
	    stamp = now + ((double)r->timestamp.tv_usec) / 1000000.0;
	    if (stamp < p->stamp) {
		fprintf(stderr, "WARNING fetched out of order result! (%.06lf < %.06lf)\n",
			stamp, p->stamp);
	    }
	    p->stamp = stamp;
	    if (pmDebugOptions.discovery)
		fprintf(stderr, "FETCHED @%.06lf [%s.%06lu] %d metrics\n",
			p->stamp, tmbuf, r->timestamp.tv_usec, r->numpmid);
	    ptv.tv_sec = r->timestamp.tv_sec;
	    ptv.tv_usec = r->timestamp.tv_usec;
	    for (i=0; i < n_discoverCallBacksList; i++) {
		if (discoverCallBacksList[i] && discoverCallBacksList[i]->resultCallBack)
		    discoverCallBacksList[i]->resultCallBack(p->archiveLabel->ll_hostname, &ptv, r);
	    }
	    pmFreeResult(r);
	}
    }
    else if (p->flags & PM_DISCOVER_FLAGS_META) {
    	/*
	 * Read all metadata records from current offset though to EOF
	 * and call all registered callbacks
	 */
	for (;;) {
	    off = lseek(p->ctx, 0, SEEK_CUR);
	    if ((nb = read(p->ctx, &hdr, sizeof(__pmLogHdr))) != sizeof(__pmLogHdr)) {
		/* rewind so we can wait for more data on the next change CallBack */
		lseek(p->ctx, off, SEEK_SET);
		break;
	    }
	    hdr.len = ntohl(hdr.len);
	    hdr.type = ntohl(hdr.type);
	    len = hdr.len - sizeof(hdr);
	    if (len > buflen) {
		buflen = len + 1048576;
	    	buf = (uint32_t *)realloc(buf, buflen);
	    }

	    /* read the body, len bytes */
	    if ((nb = read(p->ctx, buf, len)) != len) {
	    	/* rewind and wait for more data, as above */
		lseek(p->ctx, off, SEEK_SET);
		break;
	    }

	    if (pmDebugOptions.discovery)
		fprintf(stderr, "METADATA read len %4d type %d:", len, hdr.type);

	    /* TODO - use m_time from p->statbuf */
	    gettimeofday(&tv, NULL);
	    ptv.tv_sec = tv.tv_sec;
	    ptv.tv_usec = tv.tv_usec;

	    switch (hdr.type) {
		case TYPE_DESC:
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "DESC\n");
		    /* decode pmDesc result from pdu buf */
		    if ((sts = pmDiscoverDecodeMetaDesc(buf, len, &desc, &numnames, &names)) != 0)
			fprintf(stderr, "Error pmDiscoverDecodeMetaDesc: %s\n", pmErrStr(sts));
		    else {
			for (i=0; i < n_discoverCallBacksList; i++) {
			    if (discoverCallBacksList[i] && discoverCallBacksList[i]->descCallBack)
				discoverCallBacksList[i]->descCallBack(p->archiveLabel->ll_hostname, &ptv, &desc, numnames, names);
			}
			for (i=0; i < numnames; i++)
			    free(names[i]);
			free(names);
		    }
		    break;

	    	case TYPE_INDOM:
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "INDOM\n");
		    /* decode indom result from buf */
		    if ((sts = pmDiscoverDecodeMetaIndom(buf, len, &inresult)) < 0)
			fprintf(stderr, "Error pmDiscoverDecodeMetaIndom: %s\n", pmErrStr(sts));
		    else {
			for (i=0; i < n_discoverCallBacksList; i++) {
			    if (discoverCallBacksList[i] && discoverCallBacksList[i]->indomCallBack)
				discoverCallBacksList[i]->indomCallBack(p->archiveLabel->ll_hostname, &ptv, &inresult);
			}
			if (inresult.numinst > 0) {
			    for (i=0; i < inresult.numinst; i++)
				free(inresult.namelist[i]);
			    free(inresult.namelist);
			    free(inresult.instlist);
			}
		    }
		    break;

		case TYPE_LABEL:
		    if (pmDebugOptions.discovery)
			fprintf(stderr, "LABEL\n");
		    /* decode labelset from buf */
		    if ((sts = pmDiscoverDecodeMetaLabelset(buf, len, &ident, &type, &nsets, &labelset)) < 0)
			fprintf(stderr, "Error pmDiscoverDecodeMetaLabelset: %s\n", pmErrStr(sts));
		    else {
			for (i=0; i < n_discoverCallBacksList; i++) {
			    if (discoverCallBacksList[i] && discoverCallBacksList[i]->labelCallBack)
				discoverCallBacksList[i]->labelCallBack(p->archiveLabel->ll_hostname, &ptv, ident, type, nsets, labelset);
			}
			/* TODO free labelset */
		    }
		    break;

		case TYPE_TEXT:
		    fprintf(stderr, "TEXT\n");
		    /* decode help text from buffer */
		    if ((sts = pmDiscoverDecodeMetaHelptext(buf, len, &type, &id, &buffer)) < 0)
			fprintf(stderr, "Error pmDiscoverDecodeMetaHelptext: %s\n", pmErrStr(sts));
		    else {
			for (i=0; i < n_discoverCallBacksList; i++) {
			    if (discoverCallBacksList[i] && discoverCallBacksList[i]->textCallBack)
				discoverCallBacksList[i]->textCallBack(p->archiveLabel->ll_hostname, &ptv, type, id, buffer);
			}
			if (buffer)
			    free(buffer);
		    }

		default:
		    if (pmDebugOptions.discovery)
                        fprintf(stderr, "%s\n", hdr.type == (PM_LOG_MAGIC | PM_LOG_VERS02) ? "PM_LOG_MAGICv2" : "UNKNOWN");
		    break;
	    }
	}
    }
    if (buf)
	free(buf);
}

static void
changed_CallBack(pmDiscover *p)
{
    if (pmDebugOptions.discovery)
	fprintf(stderr, "CHANGED %s, %s\n", p->path, pmDiscoverFlagsStr(p));
    if (p->flags & PM_DISCOVER_FLAGS_DIRECTORY) {
	/*
	 * A changed directory path means a new archive or subdirectory
	 * has been created, or an existing path has been deleted.
	 */
	pmDiscoverArchives(p->path);
	pmDiscoverTraverse(PM_DISCOVER_FLAGS_NEW, created_CallBack);
	pmDiscoverTraverse(PM_DISCOVER_FLAGS_DELETED, deleted_CallBack);
    }
    else if (p->flags & PM_DISCOVER_FLAGS_DELETED) {
	/* path has been deleted - cleanup and remove */
	deleted_CallBack(p);
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
dir_CallBack(pmDiscover *p)
{
    pmDiscoverMonitor(p->path, changed_CallBack);
}

static void
archive_CallBack(pmDiscover *p)
{
    if (p->flags & PM_DISCOVER_FLAGS_COMPRESSED)
    	return; /* compressed archives don't grow */

    if (p->flags & PM_DISCOVER_FLAGS_DATAVOL) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE LOGVOL %s\n", p->path);
	pmDiscoverMonitor(p->path, changed_CallBack);
    }

    if (p->flags & PM_DISCOVER_FLAGS_META) {
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "DISCOVERED ARCHIVE METADATA %s\n", p->path);
	pmDiscoverMonitor(p->path, changed_CallBack);
    }
}


int
pmDiscoverRegister(char *dir, pmTimeval *origin, pmDiscoverCallBacks *callBacks)
{
    int handle = -1;
    int avail_handle = -1;

    if (callBacks != NULL) {
	for (handle=0; handle < n_discoverCallBacksList; handle++) {
	    if (discoverCallBacksList[handle] == callBacks)
		break; /* we're adding new dirs using an existing handle */
	    if (discoverCallBacksList[handle] == NULL)
		avail_handle = handle;
	}
	if (handle == n_discoverCallBacksList && avail_handle < 0) {
	    avail_handle = n_discoverCallBacksList++;
	    discoverCallBacksList = (pmDiscoverCallBacks **)realloc(discoverCallBacksList,
		n_discoverCallBacksList * sizeof(pmDiscoverCallBacks *));
	    if (pmDebugOptions.discovery) {
	    	fprintf(stderr, "pmDiscoverRegister: new handle [%d] for callbacks %p\n",
		    avail_handle, callBacks);
	    }
	}
	handle = avail_handle;
	discoverCallBacksList[handle] = callBacks;
    }
    /* else we are just adding dirs for all existing registered callbacks */

    if (dir) {
	/* NULL dir means add callbacks for existing directories and archives */
	pmDiscoverArchives(dir); /* TODO use the origin (currently ignored) */
    }

    if (pmDebugOptions.discovery) {
	fprintf(stderr, "Now managing %d directories and %d archive files\n",
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DIRECTORY, NULL),
	    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DATAVOL, NULL));
    }

    /* monitor the directories */
    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DIRECTORY, dir_CallBack);

    /* monitor archive data and metadata volumes, uncompressed only */
    pmDiscoverTraverse(PM_DISCOVER_FLAGS_DATAVOL|PM_DISCOVER_FLAGS_META, archive_CallBack);

    return handle;
}

void
pmDiscoverUnregister(int handle)
{
    if (discoverCallBacksList && handle >= 0 && handle < n_discoverCallBacksList)
    	discoverCallBacksList[handle] = NULL; /* unregister these callbacks */
}

/* from internal.h ... */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohpmInDom(a)        (a)
#define __ntohpmUnits(a)        (a)
#define __ntohpmID(a)           (a)
#else
#define __ntohpmInDom(a)        ntohl(a)
#define __ntohpmID(a)           ntohl(a)
#endif

/* from endian.c ... */
#ifndef __ntohpmUnits
static pmUnits
__ntohpmUnits(pmUnits units)
{
    unsigned int        x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

/*
 * Decode a metadata desc record in buf of length len
 * Return 0 on success.
 */
int
pmDiscoverDecodeMetaDesc(uint32_t *buf, int buflen, pmDesc *p_desc, int *p_numnames, char ***p_names)
{
    int		i;
    int		len;
    int		numnames;
    char	**names;
    char	*cp;
    pmDesc	*dp;

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
    if (names == NULL) {
	fprintf(stderr, "Arrgh: names x %d malloc failed\n", numnames);
	return -ENOMEM;
    }
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
	    fprintf(stderr, "Arrgh: names[%d] malloc failed\n", i);
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
int
pmDiscoverDecodeMetaIndom(uint32_t *buf, int len, pmInResult *inresult)
{
    // pmTimeval		*tvp;
    uint32_t		*namesbuf;
    uint32_t		*index;
    char		*str;
    int			j;
    pmInResult		ir;

    // tvp = (pmTimeval *)buf;
    // stamp.tv_sec = ntohl(tvp->tv_sec);
    // stamp.tv_usec = ntohl(tvp->tv_usec);
    ir.indom = __ntohpmInDom(buf[2]);
    ir.numinst = ntohl(buf[3]);
    ir.instlist = NULL;
    ir.namelist = NULL;

    if (ir.numinst > 0) {
	ir.instlist = (int *)malloc(ir.numinst * sizeof(int));
	ir.namelist = (char **)malloc(ir.numinst * sizeof(char *));
	if (ir.instlist == NULL || ir.namelist == NULL)
	    return -ENOMEM;
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

int
pmDiscoverDecodeMetaHelptext(uint32_t *buf, int len, int *type, int *id, char **buffer)
{
    *type = ntohl(buf[0]);

    if ((*type) & PM_TEXT_INDOM)
    	*id = __ntohpmInDom(buf[1]);
    else
    	*id = __ntohpmID(buf[1]);
    if ((*buffer = strdup((char *)&buf[2])) == NULL)
    	return -ENOMEM;

    return 0;
}

int
pmDiscoverDecodeMetaLabelset(uint32_t *buf, int len, int *ident, int *type, int *nsets, pmLabelSet **labelset)
{
    // pmTimeval tv;

    // tv.tv_sec = ntohl(buf[0]);
    // tv.tv_usec = ntohl(buf[1]);
    *type = ntohl(buf[2]);
    *ident = ntohl(buf[3]);
    *nsets = ntohl(buf[4]);
    if (*nsets == 0) {
    	*labelset = NULL;
	return 0;
    }

#if 1
    *labelset = NULL;
#else /* TODO decode the label sets, maybe use but the PDU varient is slightly different */
__pmDecodeLabel(__pmPDU *pdubuf, int *ident, int *type, pmLabelSet **setsp, int *nsetp)
#endif /* TODO */

    return 0;
}

#endif /* HAVE_LIBUV */
