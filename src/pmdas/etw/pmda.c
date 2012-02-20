/*
 * Event Tracing for Windows PMDA
 *
 * Copyright (c) 2011 Nathan Scott.  All Rights Reserved.
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

#include "event.h"
#include "domain.h"
#include "util.h"

etw_event_t eventtab[] = {
    { EVENT_PROCESS_START, 0, 1, EVENT_TRACE_FLAG_PROCESS, NULL,
      "process.start", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
    { EVENT_PROCESS_EXIT, 2, 1, EVENT_TRACE_FLAG_PROCESS, NULL,
      "process.exit", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
    { EVENT_THREAD_START, 3, 1, EVENT_TRACE_FLAG_THREAD, NULL,
      "thread.start", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
    { EVENT_THREAD_STOP, 4, 1, EVENT_TRACE_FLAG_THREAD, NULL,
      "thread.stop", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
    { EVENT_IMAGE_LOAD, 5, 1, EVENT_TRACE_FLAG_IMAGE_LOAD, NULL,
      "image.load", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
    { EVENT_IMAGE_UNLOAD, 6, 1, EVENT_TRACE_FLAG_IMAGE_LOAD, NULL,
      "image.unload", &SystemTraceControlGuid,
      KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME) },
};

#if 0
static etw_event_t etwevents[] = {
    { CLUSTER_KERNEL_PROCESS, 0, EVENT_TRACE_FLAG_PROCESS,
    { CLUSTER_KERNEL_THREAD, 0,
	EVENT_TRACE_FLAG_THREAD, "kernel.thread" },
    { CLUSTER_KERNEL_IMAGE_LOAD, 0,
	EVENT_TRACE_FLAG_IMAGE_LOAD, "kernel.image_load" },
    { CLUSTER_KERNEL_DISK_IO, 0,
	EVENT_TRACE_FLAG_DISK_IO, "kernel.disk_io"  },
    { CLUSTER_KERNEL_DISK_FILE_IO, 0,
	EVENT_TRACE_FLAG_DISK_FILE_IO, "kernel.disk_file_io" },
    { CLUSTER_KERNEL_MEMORY_PAGE_FAULTS, 0,
	EVENT_TRACE_FLAG_MEMORY_PAGE_FAULTS, "kernel.memory_page_faults" },
    { CLUSTER_KERNEL_MEMORY_HARD_FAULTS, 0,
	EVENT_TRACE_FLAG_MEMORY_HARD_FAULTS, "kernel.memory_hard_faults" },
    { CLUSTER_KERNEL_NETWORK_TCPIP, 0,
	EVENT_TRACE_FLAG_NETWORK_TCPIP, "kernel.network_tcpip" },
    { CLUSTER_KERNEL_REGISTRY, 0,
	EVENT_TRACE_FLAG_REGISTRY, "kernel.registry" },
    { CLUSTER_KERNEL_DBGPRINT, 0,
	EVENT_TRACE_FLAG_DBGPRINT, "kernel.dbgprint" },
    { CLUSTER_KERNEL_PROCESS_COUNTERS, 0,
	EVENT_TRACE_FLAG_PROCESS_COUNTERS, "kernel.process_counters" },
    { CLUSTER_KERNEL_CSWITCH, 0,
	EVENT_TRACE_FLAG_CSWITCH, "kernel.cswitch" },
    { CLUSTER_KERNEL_DPC, 0,
	EVENT_TRACE_FLAG_DPC, "kernel.dpc" },
    { CLUSTER_KERNEL_INTERRUPT, 0,
	EVENT_TRACE_FLAG_INTERRUPT, "kernel.interrupt" },
    { CLUSTER_KERNEL_SYSTEMCALL, 0,
	EVENT_TRACE_FLAG_SYSTEMCALL, "kernel.syscall" },
    { CLUSTER_KERNEL_DISK_IO_INIT, 0,
	EVENT_TRACE_FLAG_DISK_IO_INIT, "kernel.disk_io_init" },
    { CLUSTER_KERNEL_ALPC, 0,
	EVENT_TRACE_FLAG_ALPC, "kernel.alpc" },
    { CLUSTER_KERNEL_SPLIT_IO, 0,
	EVENT_TRACE_FLAG_SPLIT_IO, "kernel.split_io" },
    { CLUSTER_KERNEL_DRIVER, 0,
	EVENT_TRACE_FLAG_DRIVER, "kernel.driver" },
    { CLUSTER_KERNEL_PROFILE, 0,
	EVENT_TRACE_FLAG_PROFILE, "kernel.profile" },
    { CLUSTER_KERNEL_FILE_IO, 0,
	EVENT_TRACE_FLAG_FILE_IO, "kernel.file_io" },
    { CLUSTER_KERNEL_FILE_IO_INIT, 0,
	EVENT_TRACE_FLAG_FILE_IO_INIT, "kernel.file_io_init" },
    { CLUSTER_KERNEL_DISPATCHER, 0,
	EVENT_TRACE_FLAG_DISPATCHER, "kernel.dispatcher" },
    { CLUSTER_KERNEL_VIRTUAL_ALLOC, 0,
	EVENT_TRACE_FLAG_VIRTUAL_ALLOC, "kernel.virtual_alloc" },
    { CLUSTER_KERNEL_EXTENSION, 0,
	EVENT_TRACE_FLAG_EXTENSION, "kernel.extension" },
    { CLUSTER_KERNEL_FORWARD_WMI, 0,
	EVENT_TRACE_FLAG_FORWARD_WMI, "kernel.forward_wmi" },
    { CLUSTER_KERNEL_ENABLE_RESERVE, 0,
	EVENT_TRACE_FLAG_ENABLE_RESERVE, "kernel.enable_reserve" },

    { CLUSTER_SQLSERVER_RPC_STARTING, 0, 0, "sqlserver.rpc.starting" },
    { CLUSTER_SQLSERVER_BATCH_STARTING, 0, 0, "sqlserver.sql.batch_starting" },
};
static int numqueues = sizeof(etwevents)/sizeof(etw_event_t);
#endif


static pmdaMetric metrictab[] = {
/* etw.kernel.process.start.count */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,0), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.start.records */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,1), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.start.numclients */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,2), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.start.queuemem */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,3), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.start.params.activityid */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,10), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.start.params.pid */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,11), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.start.params.parentid */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,12), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.start.params.starttime */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,13), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
/* etw.kernel.process.start.params.session */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,14), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.start.params.image_name */
    { &eventtab[EVENT_PROCESS_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,15), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* etw.kernel.process.exit.count */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,20), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.exit.records */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,21), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.numclients */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,22), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.exit.queuemem */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,23), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.exit.params.activityid */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,30), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.pid */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,31), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.parentid */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,32), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.starttime */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,33), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
/* etw.kernel.process.exit.params.exittime */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,34), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
/* etw.kernel.process.exit.params.exitcode */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,35), PM_TYPE_32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.handle_count */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,36), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.commit_charge */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,37), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.commit_peak */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,38), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.exit.params.image_name */
    { &eventtab[EVENT_PROCESS_EXIT],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,39), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* etw.kernel.process.thread.start.count */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,50), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.thread.start.records */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,51), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.start.numclients */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,52), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.thread.start.queuemem */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,53), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.thread.start.params.activityid */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,60), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.start.params.tid */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,61), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.start.params.pid */
    { &eventtab[EVENT_THREAD_START],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,62), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* etw.kernel.process.thread.stop.count */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,70), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.thread.stop.records */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,71), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.stop.numclients */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,72), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.thread.stop.queuemem */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,73), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.thread.stop.params.activityid */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,80), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.stop.params.tid */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,81), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.thread.stop.params.pid */
    { &eventtab[EVENT_THREAD_STOP],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,82), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* etw.kernel.process.image_load.count */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,90), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.image_load.records */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,91), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_load.numclients */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,92), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.image_load.queuemem */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,93), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.image_load.params.activityid */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,100), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_load.params.pid */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,101), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_load.params.name */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,102), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_load.params.size */
    { &eventtab[EVENT_IMAGE_LOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,103), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* etw.kernel.process.image_unload.count */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,110), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.image_unload.records */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,111), PM_TYPE_EVENT, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_unload.numclients */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,112), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* etw.kernel.process.image_unload.queuemem */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,113), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* etw.kernel.process.image_unload.params.activityid */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,120), PM_TYPE_AGGREGATE, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_unload.params.pid */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,121), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_unload.params.name */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,122), PM_TYPE_STRING, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* etw.kernel.process.image_unload.params.size */
    { &eventtab[EVENT_IMAGE_UNLOAD],
      { PMDA_PMID(CLUSTER_KERNEL_PROCESS,123), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static int
etw_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    etw_event_t	*etw;
    int		sts = PMDA_FETCH_STATIC;

    __pmNotifyErr(LOG_WARNING, "called %s, mdesc=%p", __FUNCTION__, mdesc);

    switch (idp->cluster) {
    case CLUSTER_KERNEL_PROCESS:
	if ((etw = ((mdesc != NULL) ? mdesc->m_user : NULL)) == NULL)
	    return PM_ERR_PMID;

	switch (idp->item) {
	    case 0:		/* etw.kernel.process.start.count */
	    case 20:		/* etw.kernel.process.exit.count */
	    case 50:		/* etw.kernel.thread.start.count */
	    case 70:		/* etw.kernel.thread.stop.count */
	    case 90:		/* etw.kernel.process.image_load.count */
	    case 110:		/* etw.kernel.process.image_unload.count */
		sts = pmdaEventQueueCounter(etw->queueid, atom);
		break;
	    case 1:		/* etw.kernel.process.start.records */
	    case 21:		/* etw.kernel.process.exit.records */
	    case 51:		/* etw.kernel.thread.start.records */
	    case 71:		/* etw.kernel.thread.stop.records */
	    case 91:		/* etw.kernel.process.image_load.records */
	    case 111:		/* etw.kernel.process.image_unload.records */
		event_queue_lock(etw);
		sts = pmdaEventQueueRecords(etw->queueid, atom,
					    pmdaGetContext(),
					    event_decoder, &etw);
		event_queue_unlock(etw);
		break;
	    case 2:		/* etw.kernel.process.start.numclients */
	    case 22:		/* etw.kernel.process.exit.numclients */
	    case 52:		/* etw.kernel.thread.start.numclients */
	    case 72:		/* etw.kernel.thread.stop.numclients */
	    case 92:		/* etw.kernel.process.image_load.numclients */
	    case 112:		/* etw.kernel.process.image_unload.numclients */
		sts = pmdaEventQueueClients(etw->queueid, atom);
		break;
	    case 3:		/* etw.kernel.process.start.queuemem */
	    case 23:		/* etw.kernel.process.exit.queuemem */
	    case 53:		/* etw.kernel.thread.start.queuemem */
	    case 73:		/* etw.kernel.thread.stop.queuemem */
	    case 93:		/* etw.kernel.process.image_load.queuemem */
	    case 113:		/* etw.kernel.process.image_unload.queuemem */
		sts = pmdaEventQueueMemory(etw->queueid, atom);
		break;
	    default:
		return PM_ERR_PMID;
	}
	break;

    case CLUSTER_CONFIGURATION:
	switch (idp->item) {
	    case 0:			/* etw.numclients */
		sts = pmdaEventClients(atom);
		break;
	    default:
		return PM_ERR_PMID;
	}
    	break;

    default:
    	break;
    }

    return sts;
}

static int
etw_profile(__pmProfile *prof, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return 0;
}

static int
etw_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    __pmNotifyErr(LOG_WARNING, "called %s", __FUNCTION__);
    pmdaEventNewClient(pmda->e_context);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
etw_store(pmResult *result, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return PM_ERR_PERMISSION;
}

static void
etw_end_contextCallBack(int context)
{
    __pmNotifyErr(LOG_WARNING, "called %s", __FUNCTION__);
    pmdaEventEndClient(context);
}

static int
etw_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    pmdaEventNewClient(pmda->e_context);
    return pmdaText(ident, type, buffer, pmda);
}

static int
event_equal(etw_event_t *event, LPGUID provider, int eventid, int version)
{
    if (memcmp(event->provider, provider, sizeof(GUID)) != 0)
	return 0;
    return (event->eventid == eventid && event->version == version);
}

etw_event_t *
event_table_lookup(LPGUID guid, int eventid, int version)
{
    static int last;	/* punt on previous type of event reoccurring */
    int i = last;

    if (event_equal(&eventtab[i], guid, eventid, version))
	return &eventtab[i];

    for (i = 0; i < sizeof(eventtab)/sizeof(eventtab[0]); i++) {
	etw_event_t *e = &eventtab[i];
	if (event_equal(e, guid, eventid, version)) {
	    last = i;
	    return e;
	}
    }
    return NULL;
}

int
event_table_init(void)
{
    int		i, id;

    for (i = 0; i < sizeof(eventtab)/sizeof(eventtab[0]); i++) {
	HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
	etw_event_t *e = &eventtab[i];

	if (!mutex) {
	    __pmNotifyErr(LOG_WARNING, "failed to create mutex for event %s",
				    e->pmnsname);
	} else if ((id = pmdaEventNewQueue(e->pmnsname, DEFAULT_MAXMEM)) < 0) {
	    __pmNotifyErr(LOG_WARNING, "failed to create queue for event %s",
				    e->pmnsname);
	} else {
	    e->queueid = id;
	    e->mutex = mutex;
	}
    }
    return 0;
}

void 
etw_init(pmdaInterface *dp, const char *configfile)
{
    char	helppath[MAXPATHLEN];
    int		sep = __pmPathSeparator();

    snprintf(helppath, sizeof(helppath), "%s%c" "etw" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDSO(dp, PMDA_INTERFACE_5, "etw DSO", helppath);
    if (dp->status != 0)
	return;
    if (event_table_init() < 0)
	return;
    if (event_init() < 0)
	return;

    dp->version.four.fetch = etw_fetch;
    dp->version.four.store = etw_store;
    dp->version.four.profile = etw_profile;
    dp->version.four.text = etw_text;

    pmdaSetFetchCallBack(dp, etw_fetchCallBack);
    pmdaSetEndContextCallBack(dp, etw_end_contextCallBack);

    pmdaInit(dp, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}
