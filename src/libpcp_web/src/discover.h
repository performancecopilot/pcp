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
 *
 * The DISCOVER_FLAGS_META_IN_PROGRESS flag indicates a metadata record
 * read is in-progress. This can span multiple callbacks. Until this completes,
 * we avoid processing logvol records. If a logvol callback is received whilst
 * DISCOVER_FLAGS_META_IN_PROGRESS is set, set DISCOVER_FLAGS_DATAVOL_READY
 * so we know to process the log volume callback once the metadata read has
 * completed.
 */

/*
 * Discovery state flags for a given path
 */
typedef enum pmDiscoverFlags { 
    DISCOVER_FLAGS_NONE			= 0,

    DISCOVER_FLAGS_NEW			= 1 << 0, /* new path (stays set until cleared) */
    DISCOVER_FLAGS_DELETED		= 1 << 1, /* deleted (may have been compressed) */
    DISCOVER_FLAGS_COMPRESSED		= 1 << 2, /* file is compressed */
    DISCOVER_FLAGS_MONITORED		= 1 << 3, /* path is monitored */
    DISCOVER_FLAGS_DIRECTORY		= 1 << 4, /* directory path */
    DISCOVER_FLAGS_DATAVOL		= 1 << 5, /* archive data volume */
    DISCOVER_FLAGS_INDEX		= 1 << 6, /* archive index file */
    DISCOVER_FLAGS_META			= 1 << 7, /* archive metadata */
    DISCOVER_FLAGS_DATAVOL_READY	= 1 << 8, /* flag: datavol data available */
    DISCOVER_FLAGS_META_IN_PROGRESS	= 1 << 9, /* flag: metadata read in progress */

    DISCOVER_FLAGS_ALL			= (unsigned int)~DISCOVER_FLAGS_NONE
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
    __pmTimestamp		timestamp;	
    int				ctx;		/* PMAPI context handle */
    int				fd;		/* meta file descriptor */
    sds				metavol;	/* partial metadata buffer */
    sds				datavol;	/* partial data volume buffer */
#ifdef HAVE_LIBUV
    uv_fs_event_t		*event_handle;	/* uv fs_notify event handle */ 
#endif
    time_t			lastcb;		/* time last callback processed */
    struct stat			statbuf;	/* stat buffer (metadata file) */
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

extern void pmSearchDiscoverMetric(pmDiscoverEvent *,
				pmDesc *, int, char **, void *);
extern void pmSearchDiscoverInDom(pmDiscoverEvent *,
				pmInResult *, void *);
extern void pmSearchDiscoverText(pmDiscoverEvent *,
				int, int, char *, void *);

extern pmDiscover *pmDiscoverStreamLabel(const char *, __pmLogLabel *, pmDiscoverModule *, void *);
extern int pmDiscoverStreamMeta(pmDiscover *, const char *, size_t);
extern int pmDiscoverStreamData(pmDiscover *, const char *, size_t);
extern void pmDiscoverStreamEnd(const char *);

enum {
    DISCOVER_MONITORED,
    DISCOVER_PURGED,
    DISCOVER_META_CALLBACKS,
    DISCOVER_META_LOOPS,
    DISCOVER_DECODE_DESC,
    DISCOVER_DECODE_INDOM,
    DISCOVER_DECODE_LABEL,
    DISCOVER_DECODE_HELPTEXT,
    DISCOVER_LOGVOL_CALLBACKS,
    DISCOVER_LOGVOL_LOOPS,
    DISCOVER_LOGVOL_CHANGE_VOL,
    DISCOVER_DECODE_RESULT,
    DISCOVER_DECODE_RESULT_PMIDS,
    DISCOVER_DECODE_MARK_RECORD,
    DISCOVER_LOGVOL_NEW_CONTEXTS,
    DISCOVER_ARCHIVE_END_FAILED,
    DISCOVER_CHANGED_CALLBACKS,
    DISCOVER_THROTTLE_CALLBACKS,
    DISCOVER_THROTTLE,
    DISCOVER_META_PARTIAL_READS,
    DISCOVER_DECODE_RESULT_ERRORS,
    DISCOVER_META_STREAMING,
    DISCOVER_LOGVOL_STREAMING,
    NUM_DISCOVER_METRIC
};

/*
 * Module internals data structure
 */
typedef struct discoverModuleData {
    unsigned int		handle;		/* callbacks context handle */
    unsigned int		shareslots;	/* boolean, sharing 'slots' */

    mmv_registry_t		*registry;	/* metrics */
    pmAtomValue			*metrics[NUM_DISCOVER_METRIC];
    void			*map;

    struct dict			*config;	/* configuration dict */
    uv_loop_t			*events;	/* event library loop */
    keySlots			*slots;		/* key server slots data */

    unsigned int		exclude_names;	/* exclude metric names */
    sds				*patterns;	/* metric name patterns */
    struct dict			*pmids;		/* dict of excluded PMIDs */
    unsigned int		exclude_indoms;	/* exclude instance domains */
    struct dict			*indoms;	/* dict of excluded InDoms */

    void			*data;		/* user-supplied pointer */
} discoverModuleData;

extern discoverModuleData *getDiscoverModuleData(pmDiscoverModule *);

extern int pmDiscoverRegister(const char *,
		pmDiscoverModule *, pmDiscoverCallBacks *, void *);
extern void pmDiscoverUnregister(int);

#endif /* SERIES_DISCOVER_H */
