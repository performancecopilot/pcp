/*
 * List Event Trace for Windows events on the current platform.
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
#include <tdh.h>

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

void
PrintFieldInfo(PBYTE buffer,
	PPROVIDER_FIELD_INFO fieldInfo, EVENT_FIELD_TYPE eventFieldType)
{
    PWCHAR stringBuffer;

    printf("\tValue: %" PRIi64 "\n", fieldInfo->Value);
    if (fieldInfo->NameOffset) {
	stringBuffer = (PWCHAR)(buffer + fieldInfo->NameOffset);
	printf("\tField: %ls\n", stringBuffer);
    }
    if (fieldInfo->DescriptionOffset) {
	stringBuffer = (PWCHAR)(buffer + fieldInfo->DescriptionOffset);
	printf("\tDescription: %ls\n", stringBuffer);
    }
}

void
PrintFieldElements(PPROVIDER_FIELD_INFOARRAY buffer, EVENT_FIELD_TYPE eventFieldType)
{
    PPROVIDER_FIELD_INFO traceFieldInfo;
    ULONG i;

    for (i = 0; i < buffer->NumberOfElements; i++) {
	traceFieldInfo = &buffer->FieldInfoArray[i];
	PrintFieldInfo((PBYTE)buffer, traceFieldInfo, eventFieldType);
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
	    printf("TdhEnumerateProviderFieldInformation failed: code=%lu\n",
			sts);
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

    printf("Guid: {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
	       guidPointer->Data1, guidPointer->Data2, guidPointer->Data3,
	       guidPointer->Data4[0], guidPointer->Data4[1],
	       guidPointer->Data4[2], guidPointer->Data4[3],
	       guidPointer->Data4[4], guidPointer->Data4[5],
	       guidPointer->Data4[6], guidPointer->Data4[7]);

    /* SchemaSource: MOF/XML */
    /* printf("SchemaSource: %lu\n", traceProviderInfo->SchemaSource); */

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
	    printf("TdhEnumerateProviders failed: error=%lu\n", sts);
	    break;
	}
    } while (1);

    if (buffer != &providerEnumerationInfo)
	BufferFree(buffer);

    return sts;
}


int 
main(int argc, char *argv[])
{
    ULONG sts = EnumerateProviders();
    if (sts != ERROR_SUCCESS)
	printf("Error enumerating providers: error=%lu\n", sts);
    return sts;
}
