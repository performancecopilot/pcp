/*
 * List Event Traces Providers or sessions for Windows events.
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

#include <inttypes.h>
#include <windows.h>
#include <wmistr.h>
#include <stdio.h>
#include <errno.h>
#include <tdh.h>
#include "util.h"

#define MAX_SESSIONS	(ULONG)64
#define MAX_SESSION_NAME_LEN 1024
#define MAX_LOGFILE_PATH_LEN 1024

void
PrintFieldInfo(PBYTE buffer,
	PPROVIDER_FIELD_INFO fieldInfo, int id, EVENT_FIELD_TYPE eventFieldType)
{
    PWCHAR stringBuffer;

    printf("\tField %u\n", id);
    switch (eventFieldType) {
	case EventKeywordInformation:
	    printf("\t\tType: KeywordInformation\n");
	    break;
	case EventLevelInformation:
	    printf("\t\tType: LevelInformation\n");
	    break;
	case EventChannelInformation:
	    printf("\t\tType: ChannelInformation\n");
	    break;
	case EventTaskInformation:
	    printf("\t\tType: TaskInformation\n");
	    break;
	case EventOpcodeInformation:
	    printf("\t\tType: OpcodeInformation\n");
	    break;
	default:
	    break;
    }
    printf("\t\tValue: %" PRIu64 "\n", fieldInfo->Value);
    if (fieldInfo->NameOffset) {
	stringBuffer = (PWCHAR)(buffer + fieldInfo->NameOffset);
	printf("\t\tField: %ls\n", stringBuffer);
    }
    if (fieldInfo->DescriptionOffset) {
	stringBuffer = (PWCHAR)(buffer + fieldInfo->DescriptionOffset);
	printf("\t\tDescription: %ls\n", stringBuffer);
    }
}

void
PrintFieldElements(PPROVIDER_FIELD_INFOARRAY buffer, EVENT_FIELD_TYPE eventFieldType)
{
    PPROVIDER_FIELD_INFO traceFieldInfo;
    ULONG i;

    for (i = 0; i < buffer->NumberOfElements; i++) {
	traceFieldInfo = &buffer->FieldInfoArray[i];
	PrintFieldInfo((PBYTE)buffer, traceFieldInfo, i, eventFieldType);
    }
}

void
EnumerateProviderFieldInformation(LPGUID guidPointer, EVENT_FIELD_TYPE eventFieldType)
{
    PPROVIDER_FIELD_INFOARRAY buffer = NULL;
    ULONG sts, size = 0;

    sts = TdhEnumerateProviderFieldInformation(guidPointer,
   			 eventFieldType, buffer, &size);
    do {
	if (sts == ERROR_INSUFFICIENT_BUFFER) {
	    BufferFree(buffer);
	    buffer = (PPROVIDER_FIELD_INFOARRAY)BufferAllocate(size);
	    if (!buffer)
		return;
	    sts = TdhEnumerateProviderFieldInformation(guidPointer,
				eventFieldType, buffer, &size);
	}
	else if (sts == ERROR_SUCCESS) {
	    PrintFieldElements(buffer, eventFieldType);
	    break;
	}
	else if (sts == ERROR_NOT_FOUND) {
	    break;
	}
	else {
	    fprintf(stderr, "TdhEnumerateProviderFieldInformation: %s (%lu)\n",
			tdherror(sts), sts);
	    break;
	}
    } while (1);

    BufferFree(buffer);
}

void
PrintTraceProviderInfo(PBYTE buffer, PTRACE_PROVIDER_INFO traceProviderInfo)
{
    LPGUID guidPointer = &traceProviderInfo->ProviderGuid;
    PWCHAR stringBuffer;
    ULONG i;

    if (traceProviderInfo->ProviderNameOffset) {
	stringBuffer = (PWCHAR)(buffer + traceProviderInfo->ProviderNameOffset);
	printf("Name: %ls\n", stringBuffer);
    }

    printf("Guid: %s\n", strguid(guidPointer));
    printf("SchemaSource: %ld (%s)\n", traceProviderInfo->SchemaSource,
	traceProviderInfo->SchemaSource==0 ? "XML manifest" : "WMI MOF class");

    for (i = EventKeywordInformation; i <= EventChannelInformation; i++) {
	EnumerateProviderFieldInformation(guidPointer, (EVENT_FIELD_TYPE)i);
    }
}

void
PrintProviderEnumeration(PPROVIDER_ENUMERATION_INFO buffer)
{
    PTRACE_PROVIDER_INFO traceProviderInfo;
    ULONG i;

    for (i = 0; i < buffer->NumberOfProviders; i++) {
	traceProviderInfo = &buffer->TraceProviderInfoArray[i];
	PrintTraceProviderInfo((PBYTE)buffer, traceProviderInfo);
    }
}

ULONG
EnumerateProviders(void)
{
    PROVIDER_ENUMERATION_INFO providerEnumerationInfo;
    PPROVIDER_ENUMERATION_INFO buffer;
    ULONG size, sts;

    buffer = &providerEnumerationInfo;
    size = sizeof(providerEnumerationInfo);
    sts = TdhEnumerateProviders(buffer, &size);
    do {
	if (sts == ERROR_INSUFFICIENT_BUFFER) {
	    if (buffer != &providerEnumerationInfo)
		BufferFree(buffer);
	    buffer = (PPROVIDER_ENUMERATION_INFO)BufferAllocate(size);
	    if (!buffer)
		return ERROR_NOT_ENOUGH_MEMORY;
	    sts = TdhEnumerateProviders(buffer, &size);
	}
	else if (sts == ERROR_SUCCESS) {
	    PrintProviderEnumeration(buffer);
	    break;
	}
	else {
	    fprintf(stderr, "TdhEnumerateProviders failed: %s (=%lu)\n",
		    tdherror(sts), sts);
	    break;
	}
    } while (1);

    if (buffer != &providerEnumerationInfo)
	BufferFree(buffer);

    return sts;
}

ULONG
EnumerateSessions(void)
{
    PEVENT_TRACE_PROPERTIES pSessions[MAX_SESSIONS];
    PEVENT_TRACE_PROPERTIES pBuffer = NULL;
    ULONG PropertiesSize = 0;
    ULONG SessionCount = 0;
    ULONG BufferSize = 0;
    ULONG i, sts = ERROR_SUCCESS;

    PropertiesSize = sizeof(EVENT_TRACE_PROPERTIES) +
	(MAX_SESSION_NAME_LEN * sizeof(WCHAR)) +
	(MAX_LOGFILE_PATH_LEN * sizeof(WCHAR));
    BufferSize = PropertiesSize * MAX_SESSIONS;

    pBuffer = (PEVENT_TRACE_PROPERTIES) malloc(BufferSize);
    if (pBuffer) {
	ZeroMemory(pBuffer, BufferSize);
	for (i = 0; i < MAX_SESSIONS; i++) {
	    pSessions[i] = (EVENT_TRACE_PROPERTIES *)((BYTE *)pBuffer +
			    (i * PropertiesSize));
	    pSessions[i]->Wnode.BufferSize = PropertiesSize;
	    pSessions[i]->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
	    pSessions[i]->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) +
					(MAX_SESSION_NAME_LEN * sizeof(WCHAR));
	}
    } else {
	fprintf(stderr, "Error allocating memory for properties.\n");
	return ENOMEM;
    }

    sts = QueryAllTraces(pSessions, MAX_SESSIONS, &SessionCount);
    if (sts == ERROR_SUCCESS || sts == ERROR_MORE_DATA) {
	printf("Requested session count, %ld. Actual session count, %ld.\n\n",
		MAX_SESSIONS, SessionCount);
	for (i = 0; i < SessionCount; i++) {
	    LPCSTR l, f;
	    l = (LPCSTR)((char *)pSessions[i] + pSessions[i]->LoggerNameOffset);
	    f = (LPCSTR)((char*)pSessions[i] + pSessions[i]->LogFileNameOffset);
	    printf("Session GUID: %s\nSession ID: %"PRIu64"\n",
		    strguid(&pSessions[i]->Wnode.Guid),
		    pSessions[i]->Wnode.HistoricalContext);
	    if (pSessions[i]->LogFileNameOffset == 0)
		printf("Realtime session name: %s\n", l);
	    else
		printf("Log session name: %s\nLog file: %s\n", l, f);
	    if (memcmp(&SystemTraceControlGuid,
		       &pSessions[i]->Wnode.Guid, sizeof(GUID)) == 0) {
		printf("Enable Flags: ");
		dumpKernelTraceFlags(stdout, "", ",");
		printf("\n");
	    }
	    printf("flush timer: %ld\n"
		   "min buffers: %ld\n"
		   "max buffers: %ld\n"
		   "buffers: %ld\n"
		   "buffers written: %ld\n"
		   "buffers lost: %ld\n"
		   "events lost: %ld\n\n",
		    pSessions[i]->FlushTimer,
		    pSessions[i]->MinimumBuffers,
		    pSessions[i]->MaximumBuffers,
		    pSessions[i]->NumberOfBuffers,
		    pSessions[i]->BuffersWritten,
		    pSessions[i]->LogBuffersLost,
		    pSessions[i]->EventsLost);
	}
    } else {
	fprintf(stderr, "Error calling QueryAllTraces: %s (%ld)\n",
			tdherror(sts), sts);
    }

    free(pBuffer);
    return sts;
}

int 
main(int argc, char **argv)
{
    ULONG sts;

    if (argc > 1 && strcmp(argv[1], "-s") == 0)
	sts = EnumerateSessions();
    else
	sts = EnumerateProviders();
    return sts;
}
