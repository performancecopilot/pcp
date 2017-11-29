/*
 * Event Trace for Windows utility routines.
 *
 * Copyright (c) 2011, Nathan Scott.  All Rights Reserved.
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

#define INITGUID
#include <pmapi.h>
#include <tdh.h>
#include <evntrace.h>
#include "util.h"

struct {
    ULONG flag;
    char  *name;
} kernelFlags[] = {
    { EVENT_TRACE_FLAG_PROCESS, "process" },
    { EVENT_TRACE_FLAG_THREAD, "thread" },
    { EVENT_TRACE_FLAG_IMAGE_LOAD, "image_load" },
    { EVENT_TRACE_FLAG_DISK_IO, "disk_io"  },
    { EVENT_TRACE_FLAG_DISK_FILE_IO, "disk_file_io" },
    { EVENT_TRACE_FLAG_MEMORY_PAGE_FAULTS, "memory_page_faults" },
    { EVENT_TRACE_FLAG_MEMORY_HARD_FAULTS, "memory_hard_faults" },
    { EVENT_TRACE_FLAG_NETWORK_TCPIP, "network_tcpip" },
    { EVENT_TRACE_FLAG_REGISTRY, "registry" },
    { EVENT_TRACE_FLAG_DBGPRINT, "dbgprint" },
    { EVENT_TRACE_FLAG_PROCESS_COUNTERS, "process_counters" },
    { EVENT_TRACE_FLAG_CSWITCH, "cswitch" },
    { EVENT_TRACE_FLAG_DPC, "dpc" },
    { EVENT_TRACE_FLAG_INTERRUPT, "interrupt" },
    { EVENT_TRACE_FLAG_SYSTEMCALL, "syscall" },
    { EVENT_TRACE_FLAG_DISK_IO_INIT, "disk_io_init" },
    { EVENT_TRACE_FLAG_ALPC, "alpc" },
    { EVENT_TRACE_FLAG_SPLIT_IO, "split_io" },
    { EVENT_TRACE_FLAG_DRIVER, "driver" },
    { EVENT_TRACE_FLAG_PROFILE, "profile" },
    { EVENT_TRACE_FLAG_FILE_IO, "file_io" },
    { EVENT_TRACE_FLAG_FILE_IO_INIT, "file_io_init" },
    { EVENT_TRACE_FLAG_DISPATCHER, "dispatcher" },
    { EVENT_TRACE_FLAG_VIRTUAL_ALLOC, "virtual_alloc" },
    { EVENT_TRACE_FLAG_EXTENSION, "extension" },
    { EVENT_TRACE_FLAG_FORWARD_WMI, "forward_wmi" },
    { EVENT_TRACE_FLAG_ENABLE_RESERVE, "enable_reserve" },
};

void
dumpKernelTraceFlags(FILE *output, const char *prefix, const char *suffix)
{
    int i;

    for (i = 0; i < sizeof(kernelFlags)/sizeof(kernelFlags[0]); i++)
	fprintf(output, "%s%s%s", prefix, kernelFlags[i].name,
		(i+1 == sizeof(kernelFlags)/sizeof(kernelFlags[0])) ?
		"\0" : suffix);
}

ULONG
kernelTraceFlag(const char *name)
{
    int i;

    for (i = 0; i < sizeof(kernelFlags)/sizeof(kernelFlags[0]); i++)
	if (strcmp(kernelFlags[i].name, name) == 0)
	    return kernelFlags[i].flag;
    fprintf(stderr, "Unrecognised kernel trace flag: %s\n", name);
    fprintf(stderr, "List of all known options:\n");
    dumpKernelTraceFlags(stderr, "\t", "\n");
    fprintf(stderr, "\n\n");
    exit(1);
}

const char *
eventPropertyFlags(USHORT flags)
{
    if (flags & EVENT_HEADER_PROPERTY_XML)
	return "XML";
    if (flags & EVENT_HEADER_PROPERTY_FORWARDED_XML)
	return "forwarded XML";
    if (flags & EVENT_HEADER_PROPERTY_LEGACY_EVENTLOG)
	return "legacy WMI MOF";
    return "none";
}

const char *
eventHeaderFlags(USHORT flags)
{
    static char buffer[128];
    char *p = &buffer[0];

    *p = '\0';
    if (flags & EVENT_HEADER_FLAG_EXTENDED_INFO)
	strcat(p, "extended info,");
    if (flags & EVENT_HEADER_FLAG_PRIVATE_SESSION)
	strcat(p, "private session,");
    if (flags & EVENT_HEADER_FLAG_STRING_ONLY)
	strcat(p, "string,");
    if (flags & EVENT_HEADER_FLAG_TRACE_MESSAGE)
	strcat(p, "TraceMessage,");
    if (flags & EVENT_HEADER_FLAG_NO_CPUTIME)
	strcat(p, "no cputime,");
    if (flags & EVENT_HEADER_FLAG_32_BIT_HEADER)
	strcat(p, "32bit,");
    if (flags & EVENT_HEADER_FLAG_64_BIT_HEADER)
	strcat(p, "64bit,");
    if (flags & EVENT_HEADER_FLAG_CLASSIC_HEADER)
	strcat(p, "classic,");
    buffer[strlen(buffer)-1] = '\0';
    return buffer;
}

const char *
tdherror(ULONG code)
{
    switch (code){
    case ERROR_ACCESS_DENIED:
    	return "Insufficient privileges for requested operation";
    case ERROR_ALREADY_EXISTS:
    	return "A sessions with the same name or GUID already exists";
    case ERROR_BAD_LENGTH:
        return "Insufficient space or size for a parameter";
    case ERROR_BAD_PATHNAME:
        return "Given path parameter is not valid";
    case ERROR_CANCELLED:
    	return "Consumer cancelled processing via buffer callback";
    case ERROR_FILE_NOT_FOUND:
    	return "Unable to find the requested file";
    case ERROR_INSUFFICIENT_BUFFER:
    	return "Size of buffer is too small";
    case ERROR_INVALID_HANDLE:
    	return "Element of array is not a valid event tracing session handle";
    case ERROR_INVALID_PARAMETER:
    	return "One or more of the parameters is not valid";
    case ERROR_INVALID_TIME:
    	return "EndTime is less than StartTime";
    case ERROR_NOACCESS:
    	return "An exception occurred in one of the event callback routines";
    case ERROR_NOT_FOUND:
    	return "Requested class or field type not found";
    case ERROR_NOT_SUPPORTED:
    	return "The requested field type is not supported";
    case ERROR_OUTOFMEMORY:
	return "Insufficient memory";
    case ERROR_WMI_INSTANCE_NOT_FOUND:
    	return "Session from which realtime events to be consumed not running";
    case ERROR_WMI_ALREADY_ENABLED:
    	return "Handle array contains more than one realtime session handle";
    case ERROR_SUCCESS:
    	return "Success";
    }
    return strerror(code);
}

const char *
strguid(LPGUID guidPointer)
{
    static char stringBuffer[64];

    pmsprintf(stringBuffer, sizeof(stringBuffer),
		"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
	       guidPointer->Data1, guidPointer->Data2, guidPointer->Data3,
	       guidPointer->Data4[0], guidPointer->Data4[1],
	       guidPointer->Data4[2], guidPointer->Data4[3],
	       guidPointer->Data4[4], guidPointer->Data4[5],
	       guidPointer->Data4[6], guidPointer->Data4[7]);
    return stringBuffer;
}

void *
BufferAllocate(ULONG size)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void
BufferFree(void *buffer)
{
    if (buffer)
	HeapFree(GetProcessHeap(), 0, buffer);
}
