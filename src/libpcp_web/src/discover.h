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
#ifndef SERIES_DISCOVER_H
#define SERIES_DISCOVER_H

#include "pmwebapi.h"
#include "libpcp.h"
#include "mmv_stats.h"
#include "slots.h"

#ifdef HAVE_LIBUV
#include <uv.h>
#else
typedef void uv_loop_t;
#endif

/*
 * Register a directory to be monitored, along with corresponding callback
 * pointers to user functions.  If NULL, defaults to $PCP_LOG_DIR/pmlogger.
 * Can be called multiple times to register additional top level dirs and/or
 * callbacks.  Use pmDiscoverUnregister(handle) to de-register specific
 * callbacks. All registered callbacks are invoked on change events.
 *
 * Directories are recursively traversed to discover all subdirectories and
 * archives, and are dynamically managed if new archives are discovered or
 * existing archives deleted.  Only directories and uncompressed PCP archive
 * log volumes and meta data are monitored for changes.  Monitoring is done
 * efficiently - using libuv/fs_notify mechanisms - no polling and callbacks
 * are issued with low latency when changes are detected.
 */

/*
 * Discovery state flags for a given path
 */
typedef enum pmDiscoverFlags {
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
} pmDiscoverFlags;

struct pmDiscover;
typedef void (*pmDiscoverChangeCallBack)(struct pmDiscover *);

/*
 * Path to file or directory, possibly monitored (internals)
 */
typedef struct pmDiscover {
    struct pmDiscover		*next;		/* hash chain */
    pmDiscoverChangeCallBack	changed;	/* low level changes callback */
    pmDiscoverContext		context;	/* metadata for metric source */
    pmDiscoverModule		*module;	/* global state from caller */
    pmDiscoverFlags		flags;		/* state for discovery process */
    pmTimespec			timestamp;	
    int				ctx;		/* PMAPI context handle ) */
    int				fd;		/* meta file descriptor */
#ifdef HAVE_LIBUV
    uv_fs_event_t		*event_handle;	/* uv fs_notify event handle */ 
    uv_stat_t			statbuf;	/* stat buffer from event CB */
#endif
    void			*baton;		/* private internal lib data */
    void			*data;		/* opaque user data pointer */
} pmDiscover;

extern void pmSeriesDiscoverSource(pmDiscoverEvent *, void *);
extern void pmSeriesDiscoverClosed(pmDiscoverEvent *, void *);

extern void pmSeriesDiscoverLabels(pmDiscoverEvent *,
				int, int, pmLabelSet *, int, void *);
extern void pmSeriesDiscoverMetric(pmDiscoverEvent *,
				pmDesc *, int, char **, void *);
extern void pmSeriesDiscoverValues(pmDiscoverEvent *,
				pmResult *, void *);
extern void pmSeriesDiscoverInDom(pmDiscoverEvent *,
				pmInResult *, void *);
extern void pmSeriesDiscoverText(pmDiscoverEvent *,
				int, int, char *, void *);

/*
 * Module internals data structure
 */
typedef struct discoverModuleData {
    unsigned int		handle;		/* callbacks context handle */
    sds				logname;	/* archive directory dirname */
    mmv_registry_t		*metrics;	/* registry of metrics */
    struct dict			*config;	/* configuration dict */
    uv_loop_t			*events;	/* event library loop */
    redisSlots			*slots;		/* server slots data */
    void			*data;		/* user-supplied pointer */
} discoverModuleData;

extern discoverModuleData *getDiscoverModuleData(pmDiscoverModule *);

#ifdef HAVE_LIBUV
extern int pmDiscoverRegister(const char *,
		pmDiscoverModule *, pmDiscoverCallBacks *, void *);
extern void pmDiscoverUnregister(int);

#else
#define pmDiscoverRegister(path, module, callbacks, data)	(-EOPNOTSUPP)
#define pmDiscoverUnregister(handle)	do { } while (0)
#endif

#endif /* SERIES_DISCOVER_H */
