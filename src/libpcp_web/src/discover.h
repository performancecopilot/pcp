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
#ifndef PCP_DISCOVER_H
#define PCP_DISCOVER_H
#if defined(HAVE_LIBUV)

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

/*
 * Top level registered callbacks for clients using pmDiscoverRegister().
 * Arguments are metric source (e.g. hostname), timestamp and then type
 * specific data values.
 */
typedef void (*pmDiscoverResultCallBack)(char *, pmTimeval *, pmResult *);
typedef void (*pmDiscoverDescCallBack)(char *, pmTimeval *, pmDesc *, int, char **);
typedef void (*pmDiscoverIndomCallBack)(char *, pmTimeval *, pmInResult *);
typedef void (*pmDiscoverLabelCallBack)(char *, pmTimeval *, int, int, int, pmLabelSet *);
typedef void (*pmDiscoverTextCallBack)(char *, pmTimeval *, int, int, char *);

typedef struct {
    pmDiscoverResultCallBack	resultCallBack;	/* source, timestamp, pmResult */
    pmDiscoverDescCallBack	descCallBack;	/* source, timestamp, pmDesc, n, names */
    pmDiscoverIndomCallBack	indomCallBack;	/* source, timestamp, indom data */
    pmDiscoverLabelCallBack	labelCallBack;	/* source, timestamp, label data */
    pmDiscoverTextCallBack	textCallBack;	/* source, timestamp, help text data */
} pmDiscoverCallBacks;

/*
 * Register a directory to be monitored, along with corresponding callback pointers
 * to user functions. If NULL, defaults to $PCP_LOG_DIR/pmlogger. Can be called
 * multiple times to register additional top level dirs and/or callbacks. Use
 * pmDiscoverUnregister(handle) to de-register specific callbacks. All registered
 * callbacks are invoked on change events.
 *
 * Directories are recursively traversed to discover all subdirs and archives, and
 * dynamically managed if new archives are discovered or existing archives deleted.
 * Only directories and *uncompressed* PCP archive log volumes and meta data are monitored
 * for changes. Monitoring is efficient - uses libuv/fs_notify mechanisms - no polling
 * and callbacks are issued with low latency when changes are detected.
 *
 * The pmTimeval argument sets the time origin - how much history to initially replay
 * (if available in discovered archives) before on-going "near live log tailing".
 * If the time is NULL, "now" is assumed, i.e. no history will be replayed.
 * [Note: history is not implemented - currently always starts "now"]
 *
 * Returns an integer handle, that can be used with pmDiscoverUnregister()
 * to unregister a set of previously registered callbacks.
 */
extern int pmDiscoverRegister(char *, pmTimeval *, pmDiscoverCallBacks *);
extern void pmDiscoverUnregister(int);

/* --- below here is currently all internal --- */

/* pmDiscover.flags bitmap */
enum {
    PM_DISCOVER_FLAGS_NONE	= (0),
    PM_DISCOVER_FLAGS_NEW	= (1 << 0), /* new path (stays set until cleared) */
    PM_DISCOVER_FLAGS_DELETED	= (1 << 1), /* deleted (may have been compressed) */
    PM_DISCOVER_FLAGS_COMPRESSED= (1 << 2), /* file is compressed */
    PM_DISCOVER_FLAGS_MONITORED	= (1 << 3), /* path is monitored */
    PM_DISCOVER_FLAGS_DIRECTORY	= (1 << 4), /* directory path */
    PM_DISCOVER_FLAGS_DATAVOL	= (1 << 5), /* archive data volume */
    PM_DISCOVER_FLAGS_INDEX	= (1 << 6), /* archive index file */
    PM_DISCOVER_FLAGS_META	= (1 << 7), /* archive metadata */

    PM_DISCOVER_FLAGS_ALL	= ((unsigned int)~PM_DISCOVER_FLAGS_NONE)
};

/*
 * Path to file or directory, possibly monitored
 */
typedef struct pmDiscover {
    char			*path;		/* dir path or archive file name */
    struct pmDiscover		*next;		/* hash chain */
    pmLogLabel			*archiveLabel;	/* archive label (DATAVOL and META paths only) */
    int				ctx;		/* PMAPI context (for result), fd (metadata) */
    unsigned int		flags;		/* PM_DISCOVER_FLAGS_* bitmap */
    double			stamp;		/* timestamp of last result */
    void			(*changeCallBack)(struct pmDiscover *); /* low level 'change' callback */
    uv_fs_event_t		*event_handle;	/* uv fs_notify event handle */ 
    uv_stat_t			statbuf;	/* stat buffer, filled in by event CB */
} pmDiscover;

/*
 * Lookup or Add a discovered file path (directory or PCP archive file)
 * Return path table entry (new or existing).
 */
extern pmDiscover *pmDiscoverLookup(char *);
extern pmDiscover *pmDiscoverAdd(char *);

/*
 * Delete tracking of a previously discovered path. Frees resources and
 * destroy PCP context (if any).
 */
extern void pmDiscoverDelete(char *);

/*
 * Traverse known paths, invoke callback for those matching any bit in the
 * given flags bitmap. Callback may be NULL, in which case just return count.
 * Return count of matching paths, which could be 0.
 */
extern int pmDiscoverTraverse(unsigned int, void (*)(pmDiscover *));

/*
 * Discover dirs and archives - add new entries or refresh existing.
 * Call this for each top-level directory. Discovered paths are not
 * automatically monitored. After discovery, need to traverse and
 * activate monitoring using pmDiscoverMonitor.
 */
extern int pmDiscoverArchives(char *);

/*
 * Monitor path (dir or archive) and call callback when it changes.
 * Monitored archive data volumes will create a PCP context for log tailing.
 */
extern int pmDiscoverMonitor(char *, void (*)(pmDiscover *));

/*
 * return flags bitmap as a static string
 */
extern char *pmDiscoverFlagsStr(pmDiscover *);

/*
 * Decode various archive metafile records (desc, indom, labels, helptext)
 */
extern int pmDiscoverDecodeMetaDesc(uint32_t *, int, pmDesc *, int *, char ***);
extern int pmDiscoverDecodeMetaIndom(uint32_t *, int, pmInResult *);
extern int pmDiscoverDecodeMetaHelptext(uint32_t *, int, int *, int *, char **);
extern int pmDiscoverDecodeMetaHelptext(uint32_t *, int, int *, int *, char **);
extern int pmDiscoverDecodeMetaLabelset(uint32_t *, int, int *, int *, int *, pmLabelSet **);

#endif /* HAVE_LIBUV */
#endif /* PCP_DISCOVER_H */
