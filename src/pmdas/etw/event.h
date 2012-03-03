/*
 * Event queue support for the ETW PMDA
 *
 * Copyright (c) 2011 Nathan Scott.  All rights reversed.
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
#ifndef _EVENT_H
#define _EVENT_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <tdh.h>
#include <evntrace.h>

enum {
    CLUSTER_KERNEL_PROCESS		=  0,
    CLUSTER_KERNEL_THREAD		=  1,
    CLUSTER_KERNEL_IMAGE_LOAD		=  2,
    CLUSTER_KERNEL_DISK_IO		=  3,
    CLUSTER_KERNEL_DISK_FILE_IO		=  4,
    CLUSTER_KERNEL_MEMORY_PAGE_FAULTS	=  5,
    CLUSTER_KERNEL_MEMORY_HARD_FAULTS	=  6,
    CLUSTER_KERNEL_NETWORK_TCPIP	=  7,
    CLUSTER_KERNEL_REGISTRY		=  8,
    CLUSTER_KERNEL_DBGPRINT		=  9,
    CLUSTER_KERNEL_PROCESS_COUNTERS	= 10,
    CLUSTER_KERNEL_CSWITCH		= 11,
    CLUSTER_KERNEL_DPC			= 12,
    CLUSTER_KERNEL_INTERRUPT		= 13,
    CLUSTER_KERNEL_SYSTEMCALL		= 14,
    CLUSTER_KERNEL_DISK_IO_INIT		= 15,
    CLUSTER_KERNEL_ALPC			= 16,
    CLUSTER_KERNEL_SPLIT_IO		= 17,
    CLUSTER_KERNEL_DRIVER		= 18,
    CLUSTER_KERNEL_PROFILE		= 19,
    CLUSTER_KERNEL_FILE_IO		= 20,
    CLUSTER_KERNEL_FILE_IO_INIT		= 21,
    CLUSTER_KERNEL_DISPATCHER		= 22,
    CLUSTER_KERNEL_VIRTUAL_ALLOC	= 23,
    CLUSTER_KERNEL_EXTENSION		= 24,
    CLUSTER_KERNEL_FORWARD_WMI		= 25,
    CLUSTER_KERNEL_ENABLE_RESERVE	= 26,

    CLUSTER_SQLSERVER_RPC_STARTING	= 30,
    CLUSTER_SQLSERVER_BATCH_STARTING	= 31,

    CLUSTER_CONFIGURATION		= 250
};

enum {
    EVENT_PROCESS_START,
    EVENT_PROCESS_EXIT,
    EVENT_THREAD_START,
    EVENT_THREAD_STOP,
    EVENT_IMAGE_LOAD,
    EVENT_IMAGE_UNLOAD,
};

#define DEFAULT_MAXMEM	(128 * 1024)	/* 128K per event queue */

typedef struct {
    int			queueid;
    int			eventid;
    int			version;
    ULONG		flags;
    HANDLE		mutex;
    char		pmnsname[64];
    const GUID		*provider;
    LPTSTR		pname;
    ULONG		plen;
} etw_event_t;

extern etw_event_t eventtab[];
extern int event_init(void);
extern void event_shutdown(void);
extern int event_decoder(int arrayid, void *buffer, size_t size,
			 struct timeval *timestamp, void *data);

extern etw_event_t *event_table_lookup(LPGUID guid, int eventid, int version);

void event_queue_lock(etw_event_t *entry);
void event_queue_unlock(etw_event_t *entry);

#endif /* _EVENT_H */
