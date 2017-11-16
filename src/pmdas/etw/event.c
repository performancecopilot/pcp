/*
 * Event queue support for the ETW PMDA
 *
 * Copyright (c) 2011 Nathan Scott.  All rights reserved.
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
#include "util.h"

static struct {
    TRACEHANDLE			session;
    EVENT_TRACE_PROPERTIES	properties;
    EVENT_TRACE_LOGFILE		tracemode;
    BOOL			sequence;
    ULONG			enabled;

    /* session stats */
    ULONG			buffer_count;
    ULONG			buffer_size;
    ULONG			buffer_bytes;
    ULONG			buffer_reads;
    ULONG			events_lost;
} sys;

ULONG WINAPI
event_buffer_callback(PEVENT_TRACE_LOGFILE mode)
{
    sys.buffer_count++;
    sys.buffer_size = mode->BufferSize;
    sys.buffer_reads += mode->BuffersRead;
    sys.buffer_bytes += mode->Filled;
    sys.events_lost += mode->EventsLost;
    return TRUE;
}

/* Difference in seconds between 1/1/1970 and 1/1/1601 */
#define EPOCH_DELTA_IN_MICROSEC	11644473600000000ULL

static void
event_decode_timestamp(PEVENT_RECORD event, struct timeval *tv)
{
    FILETIME ft;	/* 100-nanosecond intervals since 1/1/1601 */
    ft.dwHighDateTime = event->EventHeader.TimeStamp.HighPart;
    ft.dwLowDateTime = event->EventHeader.TimeStamp.LowPart;

    __uint64_t tmp = 0;
    tmp |= ft.dwHighDateTime;
    tmp <<= 32;
    tmp |= ft.dwLowDateTime;
    tmp /= 10;				/* convert to microseconds */
    tmp -= EPOCH_DELTA_IN_MICROSEC;	/* convert Win32 -> Unix epoch */
 
    tv->tv_sec = tmp / 1000000UL;
    tv->tv_usec = tmp % 1000000UL;
}

void
event_duplicate_record(PEVENT_RECORD source, PEVENT_RECORD target)
{
    memcpy(source, target, sizeof(EVENT_RECORD));
}

void
event_duplicate_buffer(PEVENT_RECORD source, char *buffer, size_t *bytes)
{
    PTRACE_EVENT_INFO pinfo = (PTRACE_EVENT_INFO)buffer;
    DWORD size = DEFAULT_MAXMEM, sts;

    sts = TdhGetEventInformation(source, 0, NULL, pinfo, &size);
    if (sts == ERROR_INSUFFICIENT_BUFFER) {
	*bytes = 0;	/* too large for us, too bad, bail out */
    } else {
	*bytes = size;
    }
}

/*
 * This is the main event callback routine.  It uses a fixed
 * maximally sized buffer to capture and do minimal decoding
 * of the event, before passing it into a queue.  Avoid any
 * memory allocation and/or copying here where possible, as
 * this is the event arrival fast path.
 * Note: if there are no clients, the queuing code will drop
 * this event (within pmdaEventQueueAppend), so don't waste
 * time here - basically just lookup the queue and dispatch.
 */
VOID WINAPI
event_record_callback(PEVENT_RECORD event)
{
    size_t bytes;
    struct timeval timestamp;
    struct {
	EVENT_RECORD	record;
	char		buffer[DEFAULT_MAXMEM];
    } localevent;
    etw_event_t *entry;

    LPGUID guid = &event->EventHeader.ProviderId;
    int eventid = event->EventHeader.EventDescriptor.Id;
    int version = event->EventHeader.EventDescriptor.Version; 

    if ((entry = event_table_lookup(guid, eventid, version)) != NULL) {

	event_decode_timestamp(event, &timestamp);
	event_duplicate_record(event, &localevent.record);
	event_duplicate_buffer(event, &localevent.buffer[0], &bytes);
	bytes += sizeof(EVENT_RECORD);

	event_queue_lock(entry);
	pmdaEventQueueAppend(entry->queueid, &localevent, bytes, &timestamp);
	event_queue_unlock(entry);

    } else {
	pmNotifyErr(LOG_ERR, "failed to enqueue event %d:%d from provider %s",
			eventid, version, strguid(guid));
    }
}

static void
event_sys_setup(void)
{
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);

    sys.properties.Wnode.BufferSize = size;
    sys.properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    sys.properties.Wnode.ClientContext = 1;
    sys.properties.Wnode.Guid = SystemTraceControlGuid;
    sys.properties.EnableFlags = sys.enabled;
    sys.properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    if (sys.sequence)
	sys.properties.LogFileMode |= EVENT_TRACE_USE_GLOBAL_SEQUENCE;
    sys.properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    sys.tracemode.LoggerName = KERNEL_LOGGER_NAME;
    sys.tracemode.BufferCallback = event_buffer_callback;
    sys.tracemode.EventRecordCallback = event_record_callback;
    sys.tracemode.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME;
}

/* Kernel tracing thread main() */
static DWORD WINAPI
event_trace_sys(LPVOID ptr)
{
    ULONG sts, retried = 0;
    TRACEHANDLE trace;

retry_session:
    event_sys_setup();

    sts = StartTrace(&sys.session, KERNEL_LOGGER_NAME, &sys.properties);
    if (sts != ERROR_SUCCESS) {
	if (retried) {
	    pmNotifyErr(LOG_ERR, "Cannot start session (in use)");
	} else if (sts == ERROR_ALREADY_EXISTS) {
	    pmNotifyErr(LOG_WARNING, "%s session in use, retry.. (flags=%lx)",
				    KERNEL_LOGGER_NAME, sys.enabled);
	    sys.properties.EnableFlags = 0;
	    ControlTrace(sys.session, KERNEL_LOGGER_NAME,
			 &sys.properties, EVENT_TRACE_CONTROL_STOP);
	    retried = 1;
	    goto retry_session;
	}
	return sts;
    }

    trace = OpenTrace(&sys.tracemode);
    if (trace == INVALID_PROCESSTRACE_HANDLE) {
	sts = GetLastError();
	pmNotifyErr(LOG_ERR, "failed to open kernel trace: %s (%lu)",
			tdherror(sts), sts);
	return sts;
    }

    sts = ProcessTrace(&trace, 1, NULL, NULL);	/* blocks, awaiting events */
    if (sts == ERROR_CANCELLED)
	sts = ERROR_SUCCESS;
    if (sts != ERROR_SUCCESS)
	pmNotifyErr(LOG_ERR, "failed to process kernel traces: %s (%lu)",
			tdherror(sts), sts);
    return sts;
}

void
event_queue_lock(etw_event_t *entry)
{
    WaitForSingleObject(entry->mutex, INFINITE);
}

void
event_queue_unlock(etw_event_t *entry)
{
    ReleaseMutex(entry->mutex);
}

int
event_init(void)
{
    HANDLE	thread;

    pmNotifyErr(LOG_INFO, "%s: Starting up tracing ...", __FUNCTION__);
    thread = CreateThread(NULL, 0, event_trace_sys, &sys, 0, NULL);
    if (thread == NULL)
	return -ECHILD;
    CloseHandle(thread);	/* no longer need the handle */
    return 0;
}

void __attribute__((constructor))
event_startup(void)
{
    sys.session = INVALID_PROCESSTRACE_HANDLE;
}

void __attribute__((destructor))
event_shutdown(void)
{
    pmNotifyErr(LOG_INFO, "%s: Shutting down tracing ...", __FUNCTION__);
    if (sys.session != INVALID_PROCESSTRACE_HANDLE) {
	sys.properties.EnableFlags = 0;
	ControlTrace(sys.session, 0, &sys.properties, EVENT_TRACE_CONTROL_STOP);
	sys.session = INVALID_PROCESSTRACE_HANDLE;
    }
}

int
event_decoder(int eventarray, void *buffer, size_t size,
		struct timeval *timestamp, void *data)
{
    int sts; /* , handle = *(int *)data; */
    pmAtomValue atom;
    pmID pmid = 0;	/* TODO */

    sts = pmdaEventAddRecord(eventarray, timestamp, PM_EVENT_FLAG_POINT);
    if (sts < 0)
	return sts;
    atom.cp = buffer;
    sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
    if (sts < 0)
	return sts;
    return 1;	/* simple decoder, added just one event array */
}
