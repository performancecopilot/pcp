/*
 * Event Trace Consumer for Windows events on the current platform.
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
#include <pmapi.h>
#include <impl.h>
#include <tdh.h>
#include <tdhmsg.h>
#include <evntrace.h>
#include <inttypes.h>
#include "util.h"

#define PROPERTY_BUFFER	1024
#define MAX_SESSIONS	64
#define PCP_SESSION	"PCP Collector Set"

static int verbose;

/* Get the event metadata */
static DWORD
GetEventInformation(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO *pInfoPointer)
{
    PTRACE_EVENT_INFO pInfo = NULL;
    DWORD sts = ERROR_SUCCESS;
    DWORD size = 0;

    sts = TdhGetEventInformation(pEvent, 0, NULL, pInfo, &size);
    if (sts == ERROR_INSUFFICIENT_BUFFER) {
	pInfo = (TRACE_EVENT_INFO *)malloc(size);
	if (pInfo == NULL) {
	    fprintf(stderr,
		"Failed to allocate memory for event info (size=%lu).\n", size);
	    sts = ERROR_OUTOFMEMORY;
	} else {
	    /* Retrieve event metadata */
	    sts = TdhGetEventInformation(pEvent, 0, NULL, pInfo, &size);
	}
    } else if (sts != ERROR_SUCCESS && verbose) {
	fprintf(stderr, "TdhGetEventInformation failed: %s (%lu)\n",
		tdherror(sts), sts);
    }
    *pInfoPointer = pInfo;
    return sts;
}

static void
PrintMapString(PEVENT_MAP_INFO pMapInfo, PBYTE pData)
{
    BOOL MatchFound = FALSE;
    DWORD i;

    if ((pMapInfo->Flag & EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP) ==
		EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP ||
	((pMapInfo->Flag & EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) ==
		EVENTMAP_INFO_FLAG_WBEM_VALUEMAP &&
	(pMapInfo->Flag & (~EVENTMAP_INFO_FLAG_WBEM_VALUEMAP)) !=
		EVENTMAP_INFO_FLAG_WBEM_FLAG)) {
	if ((pMapInfo->Flag & EVENTMAP_INFO_FLAG_WBEM_NO_MAP) ==
		EVENTMAP_INFO_FLAG_WBEM_NO_MAP) {
	    printf("%s\n", ((PBYTE)pMapInfo +
			pMapInfo->MapEntryArray[*(PULONG)pData].OutputOffset));
	} else {
	    for (i = 0; i < pMapInfo->EntryCount; i++) {
		if (pMapInfo->MapEntryArray[i].Value == *(PULONG)pData) {
		    printf("%s\n", ((PBYTE)pMapInfo +
		   		 pMapInfo->MapEntryArray[i].OutputOffset));
		    MatchFound = TRUE;
		    break;
		}
	    }

	    if (MatchFound == FALSE)
		printf("%lu\n", *(PULONG)pData);
	}
    }
    else if ((pMapInfo->Flag & EVENTMAP_INFO_FLAG_MANIFEST_BITMAP) ==
		EVENTMAP_INFO_FLAG_MANIFEST_BITMAP ||
	(pMapInfo->Flag & EVENTMAP_INFO_FLAG_WBEM_BITMAP) ==
		EVENTMAP_INFO_FLAG_WBEM_BITMAP ||
	((pMapInfo->Flag & EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) ==
		EVENTMAP_INFO_FLAG_WBEM_VALUEMAP &&
	(pMapInfo->Flag & (~EVENTMAP_INFO_FLAG_WBEM_VALUEMAP)) ==
		EVENTMAP_INFO_FLAG_WBEM_FLAG)) {
	if ((pMapInfo->Flag & EVENTMAP_INFO_FLAG_WBEM_NO_MAP) ==
		EVENTMAP_INFO_FLAG_WBEM_NO_MAP) {
	    DWORD BitPosition = 0;

	    for (i = 0; i < pMapInfo->EntryCount; i++) {
		BitPosition = (1 << i);
		if ((*(PULONG)pData & BitPosition) == BitPosition) {
		    printf("%s%s", (MatchFound) ? " | " : "", 
			((PBYTE)pMapInfo +
				pMapInfo->MapEntryArray[i].OutputOffset));
		    MatchFound = TRUE;
		}
	    }
	} else {
	    for (i = 0; i < pMapInfo->EntryCount; i++) {
		if ((pMapInfo->MapEntryArray[i].Value & *(PULONG)pData) ==
			pMapInfo->MapEntryArray[i].Value) {
		    printf("%s%s", (MatchFound) ? " | " : "", 
			((PBYTE)pMapInfo +
				pMapInfo->MapEntryArray[i].OutputOffset));
		    MatchFound = TRUE;
		}
	    }
	}

	if (MatchFound) {
	    printf("\n");
	} else {
	    printf("%lu\n", *(PULONG)pData);
	}
    }
}

static DWORD
FormatAndPrintData(PEVENT_RECORD pEvent, USHORT InType, USHORT OutType,
		   PBYTE pData, DWORD DataSize, PEVENT_MAP_INFO pMapInfo)
{
    DWORD i, sts = ERROR_SUCCESS;
    size_t StringLength = 0;

    switch (InType) {
	case TDH_INTYPE_UNICODESTRING:
	case TDH_INTYPE_COUNTEDSTRING:
	case TDH_INTYPE_REVERSEDCOUNTEDSTRING:
	case TDH_INTYPE_NONNULLTERMINATEDSTRING:
	    if (TDH_INTYPE_COUNTEDSTRING == InType)
		StringLength = *(PUSHORT)pData;
	    else if (TDH_INTYPE_REVERSEDCOUNTEDSTRING == InType)
		StringLength = MAKEWORD(
			HIBYTE((PUSHORT)pData), LOBYTE((PUSHORT)pData));
	    else if (TDH_INTYPE_NONNULLTERMINATEDSTRING == InType)
		StringLength = DataSize;
	    else
		StringLength = wcslen((LPWSTR)pData);
	    printf("%.*s\n", StringLength, pData);
	    break;

	case TDH_INTYPE_ANSISTRING:
	case TDH_INTYPE_COUNTEDANSISTRING:
	case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING:
	case TDH_INTYPE_NONNULLTERMINATEDANSISTRING:
	    if (TDH_INTYPE_COUNTEDANSISTRING == InType)
		StringLength = *(PUSHORT)pData;
	    else if (TDH_INTYPE_REVERSEDCOUNTEDANSISTRING == InType)
		StringLength = MAKEWORD(
			HIBYTE((PUSHORT)pData), LOBYTE((PUSHORT)pData));
	    else if (TDH_INTYPE_NONNULLTERMINATEDANSISTRING == InType)
		StringLength = DataSize;
	    else
		StringLength = strlen((LPSTR)pData);
	    printf("%.*s\n", StringLength, pData);
	    break;

	case TDH_INTYPE_INT8:
	    printf("%hd\n", *(PCHAR)pData);
	    break;

	case TDH_INTYPE_UINT8:
	    if (TDH_OUTTYPE_HEXINT8 == OutType)
		printf("0x%x\n", *(PBYTE)pData);
	    else
		printf("%hu\n", *(PBYTE)pData);
	    break;

	case TDH_INTYPE_INT16:
	    printf("%hd\n", *(PSHORT)pData);
	    break;

	case TDH_INTYPE_UINT16:
	    if (TDH_OUTTYPE_HEXINT16 == OutType)
		printf("0x%x\n", *(PUSHORT)pData);
	    else if (TDH_OUTTYPE_PORT == OutType)
		printf("%hu\n", ntohs(*(PUSHORT)pData));
	    else
		printf("%hu\n", *(PUSHORT)pData);
	    break;

	case TDH_INTYPE_INT32:
	    if (TDH_OUTTYPE_HRESULT == OutType)
		printf("0x%lx\n", *(PLONG)pData);
	    else
		printf("%ld\n", *(PLONG)pData);
	    break;

	case TDH_INTYPE_UINT32:
	    if (TDH_OUTTYPE_HRESULT == OutType ||
		TDH_OUTTYPE_WIN32ERROR == OutType ||
		TDH_OUTTYPE_NTSTATUS == OutType ||
		TDH_OUTTYPE_HEXINT32 == OutType)
		printf("0x%lx\n", *(PULONG)pData);
	    else if (TDH_OUTTYPE_IPV4 == OutType)
		printf("%ld.%ld.%ld.%ld\n", (*(PLONG)pData >>  0) & 0xff,
					(*(PLONG)pData >>  8) & 0xff,
					(*(PLONG)pData >>  16) & 0xff,
					(*(PLONG)pData >>  24) & 0xff);
	    else if (pMapInfo)
		PrintMapString(pMapInfo, pData);
	    else
		printf("%lu\n", *(PULONG)pData);
	    break;

	case TDH_INTYPE_INT64:
	    printf("%I64d\n", *(PLONGLONG)pData);
	    break;

	case TDH_INTYPE_UINT64:
	    if (TDH_OUTTYPE_HEXINT64 == OutType)
		printf("0x%I64x\n", *(PULONGLONG)pData);
	    else
		printf("%I64u\n", *(PULONGLONG)pData);
	    break;

	case TDH_INTYPE_FLOAT:
	    printf("%f\n", *(PFLOAT)pData);
	    break;

	case TDH_INTYPE_DOUBLE:
	    printf("%f\n", *(double*)pData);
	    break;

	case TDH_INTYPE_BOOLEAN:
	    printf("%s\n", ((PBOOL)pData == 0) ? "false" : "true");
	    break;

	case TDH_INTYPE_BINARY:
	    if (TDH_OUTTYPE_IPV6 == OutType)
		break;
	    else {
		for (i = 0; i < DataSize; i++)
		    printf("%.2x", pData[i]);
		printf("\n");
	    }
	    break;

	case TDH_INTYPE_GUID:
	    printf("%s\n", strguid((GUID *)pData));
	    break;

	case TDH_INTYPE_POINTER:
	case TDH_INTYPE_SIZET:
	    if (EVENT_HEADER_FLAG_32_BIT_HEADER ==
		(pEvent->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER))
		printf("0x%I32x\n", *(PULONG)pData);
	    else
		printf("0x%I64x\n", *(PULONGLONG)pData);

	case TDH_INTYPE_FILETIME:
	    break;

	case TDH_INTYPE_SYSTEMTIME:
	    break;

	case TDH_INTYPE_SID:
	    break;

	case TDH_INTYPE_HEXINT32:
	    printf("0x%I32x\n", *(PULONG)pData);
	    break;

	case TDH_INTYPE_HEXINT64:
	    printf("0x%I64x\n", *(PULONGLONG)pData);
	    break;

	case TDH_INTYPE_UNICODECHAR:
	    printf("%c\n", *(PWCHAR)pData);
	    break;

	case TDH_INTYPE_ANSICHAR:
	    printf("%C\n", *(PCHAR)pData);
	    break;

	case TDH_INTYPE_WBEMSID:
	    break;

    default:
	sts = ERROR_NOT_FOUND;
    }

    return sts;
}

/*
 * Get the size of the array.
 * For MOF-based events, the size is specified in the declaration or using 
 * the MAX qualifier.  For manifest-based events, the property can specify
 * the size of the array using the count attribute.
 * The count attribue can specify the size directly or specify the name 
 * of another property in the event data that contains the size.
 */
static void
GetArraySize(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO pInfo,
		USHORT i, PUSHORT ArraySize)
{
    PROPERTY_DATA_DESCRIPTOR DataDescriptor;
    DWORD size = 0;

    if ((pInfo->EventPropertyInfoArray[i].Flags & PropertyParamCount) ==
	PropertyParamCount) {
	DWORD cnt = 0;  /* expecting count to be defined as uint16 or uint32 */
	DWORD j = pInfo->EventPropertyInfoArray[i].countPropertyIndex;

	ZeroMemory(&DataDescriptor, sizeof(PROPERTY_DATA_DESCRIPTOR));
	DataDescriptor.PropertyName =
				(ULONGLONG)((PBYTE)(pInfo) +
				pInfo->EventPropertyInfoArray[j].NameOffset);
	DataDescriptor.ArrayIndex = ULONG_MAX;
	TdhGetPropertySize(pEvent, 0, NULL, 1, &DataDescriptor, &size);
	TdhGetProperty(pEvent, 0, NULL, 1, &DataDescriptor, size, (PBYTE)&cnt);
	*ArraySize = (USHORT)cnt;
    } else {
	*ArraySize = pInfo->EventPropertyInfoArray[i].count;
    }
}

/*
 * Mapped string values defined in a manifest will contain a trailing space
 * in the EVENT_MAP_ENTRY structure. Replace the trailing space with a null-
 * terminating character, so that bit mapped strings are correctly formatted.
 */
static void
RemoveTrailingSpace(PEVENT_MAP_INFO pMapInfo)
{
    SIZE_T ByteLength = 0;
    DWORD i;

    for (i = 0; i < pMapInfo->EntryCount; i++) {
	ByteLength = (wcslen((LPWSTR)((PBYTE)pMapInfo +
			pMapInfo->MapEntryArray[i].OutputOffset)) - 1) * 2;
	*((LPWSTR)((PBYTE)pMapInfo +
		(pMapInfo->MapEntryArray[i].OutputOffset +
		ByteLength))) = L'\0';
    }
}

/*
 * Both MOF-based events and manifest-based events can specify name/value maps.
 * The map values can be integer values or bit values.
 * If the property specifies a value map, get the map.
 */
static DWORD
GetMapInfo(PEVENT_RECORD pEvent, LPWSTR pMapName, DWORD DecodingSource,
	   PEVENT_MAP_INFO pMapInfo)
{
    DWORD sts = ERROR_SUCCESS;
    DWORD size = 0;

    /* Retrieve required buffer size for map info */
    sts = TdhGetEventMapInformation(pEvent, pMapName, pMapInfo, &size);
    if (sts == ERROR_INSUFFICIENT_BUFFER) {
	pMapInfo = (PEVENT_MAP_INFO)malloc(size);
	if (pMapInfo == NULL) {
	    fprintf(stderr, "Failed to allocate map info memory (size=%lu).\n",
			size);
	    return ERROR_OUTOFMEMORY;
	}

	/* Retrieve the map info */
	sts = TdhGetEventMapInformation(pEvent, pMapName, pMapInfo, &size);
    }

    if (sts == ERROR_SUCCESS) {
	if (DecodingSourceXMLFile == DecodingSource)
	    RemoveTrailingSpace(pMapInfo);
    } else if (sts == ERROR_NOT_FOUND) {
	sts = ERROR_SUCCESS;
    } else {
	fprintf(stderr, "TdhGetEventMapInformation failed: %s (%ld)\n",
		tdherror(sts), sts);
    }
    return sts;
}

static DWORD
PrintProperties(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO pInfo,
		USHORT i, LPWSTR pStructureName, USHORT StructIndex)
{
    DWORD sts = ERROR_SUCCESS;
    DWORD LastMember = 0;
    USHORT ArraySize = 0;
    PEVENT_MAP_INFO pMapInfo = NULL;
    PROPERTY_DATA_DESCRIPTOR DataDescriptors[2];
    ULONG DescriptorsCount = 0;
    USHORT k, j;

    static DWORD size;
    static PBYTE pData;

    if (pData == NULL) {
	if ((pData = malloc(PROPERTY_BUFFER)) != NULL)
	    size = PROPERTY_BUFFER;
    }

    /* Get the size of the array (if the property is an array) */
    GetArraySize(pEvent, pInfo, i, &ArraySize);

    for (k = 0; k < ArraySize; k++) {
	wprintf(L"%*s%s: ", (pStructureName) ? 4 : 0, L"", (LPWSTR)
		((PBYTE)(pInfo) + pInfo->EventPropertyInfoArray[i].NameOffset));

	/* If the property is a structure, print the members of the structure */
	if ((pInfo->EventPropertyInfoArray[i].Flags & PropertyStruct) == PropertyStruct) {
	    printf("\n");

	    LastMember =
		pInfo->EventPropertyInfoArray[i].structType.StructStartIndex + 
		pInfo->EventPropertyInfoArray[i].structType.NumOfStructMembers;

	    for (j = pInfo->EventPropertyInfoArray[i].structType.StructStartIndex; j < LastMember; j++) {
		sts = PrintProperties(pEvent, pInfo, j,
(LPWSTR)((PBYTE)(pInfo) + pInfo->EventPropertyInfoArray[i].NameOffset), k);
		if (sts != ERROR_SUCCESS) {
		    fprintf(stderr,
			    "Printing the members of the structure failed\n");
		    goto cleanup;
		}
	    }
	} else {
	    ZeroMemory(&DataDescriptors, sizeof(DataDescriptors));

	    /*
	     * To retrieve a member of a structure, you need to specify
	     * an array of descriptors.  The first descriptor in the array
	     * identifies the name of the structure and the second 
	     * descriptor defines the member of the structure whose data
	     * you want to retrieve. 
	     */
	    if (pStructureName) {
		DataDescriptors[0].PropertyName = (ULONGLONG)pStructureName;
		DataDescriptors[0].ArrayIndex = StructIndex;
		DataDescriptors[1].PropertyName = (ULONGLONG)
		((PBYTE)(pInfo) + pInfo->EventPropertyInfoArray[i].NameOffset);
		DataDescriptors[1].ArrayIndex = k;
		DescriptorsCount = 2;
	    } else {
		DataDescriptors[0].PropertyName = (ULONGLONG)
		((PBYTE)(pInfo) + pInfo->EventPropertyInfoArray[i].NameOffset);
		DataDescriptors[0].ArrayIndex = k;
		DescriptorsCount = 1;
	    }

	    /*
	     * TDH API does not support IPv6 addresses.
	     * If the output type is TDH_OUTTYPE_IPV6, you will be unable
	     * to consume the rest of the event.
	     * If you try to consume the remainder of the event, you will
	     * get ERROR_EVT_INVALID_EVENT_DATA.
	     */
	    if (TDH_INTYPE_BINARY ==
		pInfo->EventPropertyInfoArray[i].nonStructType.InType &&
		TDH_OUTTYPE_IPV6 ==
		pInfo->EventPropertyInfoArray[i].nonStructType.OutType) {
		fprintf(stderr, "Event contains an IPv6 address. Skipping.\n");
		sts = ERROR_EVT_INVALID_EVENT_DATA;
		break;
	    } else {
retry:
		sts = TdhGetProperty(pEvent, 0, NULL,
			DescriptorsCount, &DataDescriptors[0], size, pData);
		if (sts == ERROR_INSUFFICIENT_BUFFER) {
		    /* TdhGetPropertySize failing on Win2008, so do this: */
		    pData = realloc(pData, size *= 2);
		    goto retry;
		} else if (sts != ERROR_SUCCESS) {
		    fprintf(stderr, "TdhGetProperty failed: %s (%ld)\n",
				tdherror(sts), sts);
		    goto cleanup;
		}

		/*
		 * Get the name/value map if the property specifies a value map.
		 */
		sts = GetMapInfo(pEvent, (PWCHAR) ((PBYTE)(pInfo) +
		    pInfo->EventPropertyInfoArray[i].nonStructType.MapNameOffset),
		    pInfo->DecodingSource,
		    pMapInfo);
		if (sts != ERROR_SUCCESS) {
		    fprintf(stderr, "GetMapInfo failed\n");
		    goto cleanup;
		}

		sts = FormatAndPrintData(pEvent, 
		    pInfo->EventPropertyInfoArray[i].nonStructType.InType,
		    pInfo->EventPropertyInfoArray[i].nonStructType.OutType,
		    pData, size, pMapInfo);
		if (sts != ERROR_SUCCESS) {
		    fprintf(stderr, "FormatAndPrintData failed\n");
		    goto cleanup;
		}

		if (pMapInfo) {
		    free(pMapInfo);
		    pMapInfo = NULL;
		}
	    }
	}
    }

cleanup:

    if (pMapInfo) {
	free(pMapInfo);
	pMapInfo = NULL;
    }

    return sts;
}

void
PrintHeader(PEVENT_RECORD pEvent)
{
    /* Note: EventHeader.ProcessId is defined as ULONG */
    printf("Event HEADER (size=%u) flags=%s type=%s\npid=%lu tid=%ld eid=%u\n",
		pEvent->EventHeader.Size,
		eventHeaderFlags(pEvent->EventHeader.Flags),
		eventPropertyFlags(pEvent->EventHeader.EventProperty),
		pEvent->EventHeader.ThreadId, pEvent->EventHeader.ProcessId,
		pEvent->EventHeader.EventDescriptor.Id);
    if (pEvent->EventHeader.Flags &
	(EVENT_HEADER_FLAG_PRIVATE_SESSION|EVENT_HEADER_FLAG_NO_CPUTIME)) {
	printf("Time processor=%"PRIu64"\n", pEvent->EventHeader.ProcessorTime);
    } else {
	printf("Time: sys=%lu usr=%lu\n",
		pEvent->EventHeader.KernelTime, pEvent->EventHeader.UserTime);
    }
    printf("Event PROVIDER %s\n", strguid(&pEvent->EventHeader.ProviderId));
    printf("Event ACTIVITY %s\n", strguid(&pEvent->EventHeader.ActivityId));
}

void
PrintTimestamp(PEVENT_RECORD pEvent)
{
    ULONGLONG TimeStamp = 0;
    ULONGLONG Nanoseconds = 0;
    SYSTEMTIME st;
    SYSTEMTIME stLocal;
    FILETIME ft;

    /* Print the time stamp for when the event occurred */
    ft.dwHighDateTime = pEvent->EventHeader.TimeStamp.HighPart;
    ft.dwLowDateTime = pEvent->EventHeader.TimeStamp.LowPart;

    FileTimeToSystemTime(&ft, &st);
    SystemTimeToTzSpecificLocalTime(NULL, &st, &stLocal);
    TimeStamp = pEvent->EventHeader.TimeStamp.QuadPart;
    Nanoseconds = (TimeStamp % 10000000) * 100;

    printf("Event TIMESTAMP: %02d/%02d/%02d %02d:%02d:%02d.%I64u\n", 
	    stLocal.wMonth, stLocal.wDay, stLocal.wYear, stLocal.wHour,
	    stLocal.wMinute, stLocal.wSecond, Nanoseconds);
}

void
PrintEventInfo(PTRACE_EVENT_INFO pInfo)
{
    if (DecodingSourceWbem == pInfo->DecodingSource)
	printf("EventInfo: MOF class event\n");
    else if (DecodingSourceXMLFile == pInfo->DecodingSource)
	printf("EventInfo: XML manifest event\n");
    else if (DecodingSourceWPP == pInfo->DecodingSource)
	printf("EventInfo: WPP event\n");

    printf("Event GUID: %s\n", strguid(&pInfo->EventGuid));

    if (pInfo->ProviderNameOffset > 0)
	wprintf(L"Provider name: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->ProviderNameOffset));

    if (DecodingSourceXMLFile == pInfo->DecodingSource)
	wprintf(L"Event ID: %hu\n", pInfo->EventDescriptor.Id);

    wprintf(L"Version: %d\n", pInfo->EventDescriptor.Version);

    if (pInfo->ChannelNameOffset > 0)
	wprintf(L"Channel name: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->ChannelNameOffset));

    if (pInfo->LevelNameOffset > 0)
	wprintf(L"Level name: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->LevelNameOffset));
    else
	wprintf(L"Level: %hu\n", pInfo->EventDescriptor.Level);

    if (DecodingSourceXMLFile == pInfo->DecodingSource) {
	if (pInfo->OpcodeNameOffset > 0)
	    wprintf(L"Opcode name: %s\n",
		    (LPWSTR)((PBYTE)(pInfo) + pInfo->OpcodeNameOffset));
    } else
	wprintf(L"Type: %hu\n", pInfo->EventDescriptor.Opcode);

    if (DecodingSourceXMLFile == pInfo->DecodingSource) {
	if (pInfo->TaskNameOffset > 0)
	    wprintf(L"Task name: %s\n",
		    (LPWSTR)((PBYTE)(pInfo) + pInfo->TaskNameOffset));
    } else
	wprintf(L"Task: %hu\n", pInfo->EventDescriptor.Task);

    wprintf(L"Keyword mask: 0x%x\n", pInfo->EventDescriptor.Keyword);
    if (pInfo->KeywordsNameOffset) {
	LPWSTR pKeyword = (LPWSTR)((PBYTE)(pInfo) + pInfo->KeywordsNameOffset);

	for (; *pKeyword != 0; pKeyword += (wcslen(pKeyword) + 1))
	    wprintf(L"  Keyword name: %s\n", pKeyword);
    }

    if (pInfo->EventMessageOffset > 0)
	wprintf(L"Event message: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->EventMessageOffset));

    if (pInfo->ActivityIDNameOffset > 0)
	wprintf(L"Activity ID name: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->ActivityIDNameOffset));

    if (pInfo->RelatedActivityIDNameOffset > 0)
	wprintf(L"Related activity ID name: %s\n",
		(LPWSTR)((PBYTE)(pInfo) + pInfo->RelatedActivityIDNameOffset));
}

/* Callback that receives the events */
VOID WINAPI
ProcessEvent(PEVENT_RECORD pEvent)
{
    DWORD sts = ERROR_SUCCESS;
    PTRACE_EVENT_INFO pInfo = NULL;
    USHORT i;

    PrintHeader(pEvent);
    PrintTimestamp(pEvent);

    /*
     * Process the event.
     * The pEvent->UserData member is a pointer to the event specific data,
     * if any exists.
     */
    sts = GetEventInformation(pEvent, &pInfo);
    if (sts != ERROR_SUCCESS)
	goto cleanup;

    PrintEventInfo(pInfo);

    if (DecodingSourceWPP == pInfo->DecodingSource)
	/* Not handling the WPP case, unless just an inline string */
	if (!(pEvent->EventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY))
	    goto cleanup;

    /*
     * Print the event data for all the top-level properties.
     * Metadata for all the top-level properties comes before structure
     * member properties in the property information array.
     * If the EVENT_HEADER_FLAG_STRING_ONLY flag is set, the event data
     * is a null-terminated string, so just print it.
     */
    if (EVENT_HEADER_FLAG_STRING_ONLY ==
	(pEvent->EventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY)) {
	printf("Embedded: %s\n", (char *)pEvent->UserData);
    } else {
	for (i = 0; i < pInfo->TopLevelPropertyCount; i++) {
	    sts = PrintProperties(pEvent, pInfo, i, NULL, 0);
	    if (sts != ERROR_SUCCESS) {
		fprintf(stderr,
			"Printing top level properties failed, property %u.\n",
			i);
	    }
	}
    }

cleanup:
    fflush(stdout);
    if (pInfo)
	free(pInfo);
}

static struct {
    EVENT_TRACE_PROPERTIES *properties;
    TRACEHANDLE session;
    LPTSTR name;
} sessions[MAX_SESSIONS];
static int sCount;

static void
stopSession(TRACEHANDLE session, LPTSTR name,
	    PEVENT_TRACE_PROPERTIES properties)
{
    if (session != INVALID_PROCESSTRACE_HANDLE) {
	    ULONG sts = ControlTrace(session, name, properties,
				EVENT_TRACE_CONTROL_STOP);
	if (sts != ERROR_SUCCESS)
	    fprintf(stderr, "ControlTrace failed (%s): %s\n", name,
		    tdherror(sts));
	else
	    fprintf(stderr, "Stopped %s event tracing session\n", name);
    }
}

static void __attribute__((constructor)) initTracing()
{
    int	i;

    for (i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++)
	sessions[i].session = INVALID_PROCESSTRACE_HANDLE;
}

static void __attribute__((destructor)) stopTracing()
{
    int	i;

    for (i = 0; i < sCount; i++) {
	sessions[i].properties->EnableFlags = 0;
	stopSession(sessions[i].session, sessions[i].name, sessions[i].properties);
    }
}

void
enableEventTrace(LPTSTR name, ULONG namelen, const GUID *guid,
		 ULONG enableFlags, BOOL useGlobalSequence)
{
    TRACEHANDLE session;
    EVENT_TRACE_PROPERTIES *properties;
    ULONG size, sts = ERROR_SUCCESS;
    int retryStartTrace = 0;

    size = sizeof(EVENT_TRACE_PROPERTIES) + namelen;
    properties = malloc(size);
    if (properties == NULL) {
	fprintf(stderr, "Insufficient memory: %lu bytes\n", size);
	exit(1);
    }

    sessions[sCount].properties = properties;
    sessions[sCount].session = session;
    sessions[sCount].name = name;
    sCount++;

retrySession:
    ZeroMemory(properties, size);
    properties->Wnode.BufferSize = size;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1;
    properties->Wnode.Guid = *guid;
    properties->EnableFlags = enableFlags;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    if (useGlobalSequence == TRUE)
	properties->LogFileMode |= EVENT_TRACE_USE_GLOBAL_SEQUENCE;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    sts = StartTrace(&session, name, properties);
    if (sts != ERROR_SUCCESS) {
	if (retryStartTrace == 1) {
	    fprintf(stderr, "Cannot start %s session (in use)\n", name);
	} else if (sts == ERROR_ALREADY_EXISTS) {
	    fprintf(stderr, "%s session is in use - retry ... (flags=%lx)\n",
			name, enableFlags);
	    stopTracing();
	    retryStartTrace = 1;
	    goto retrySession;
	}
	else {
	    fprintf(stderr, "StartTrace: %s\n", tdherror(sts));
	}
	exit(1);
    }
}

ULONG bufferCount, buffersRead, bufferTotal, bufferBytes, eventsLost, sysCount;

ULONG WINAPI
BufferCallback(PEVENT_TRACE_LOGFILE mode)
{
    buffersRead += mode->BuffersRead;
    bufferTotal += mode->BufferSize;
    bufferBytes += mode->Filled;
    eventsLost += mode->EventsLost;
    sysCount += mode->IsKernelTrace;
    bufferCount++;
    printf("Event BUFFER (size=%lu) all reads=%lu bytes=%lu lost=%lu sys=%lu\n",
		mode->Filled, buffersRead, bufferBytes, eventsLost, sysCount);
    return TRUE;
}

LPGUID
LookupGuidInBuffer(LPTSTR name, PPROVIDER_ENUMERATION_INFO buffer)
{
    PTRACE_PROVIDER_INFO traceProviderInfo;
    PWCHAR stringPointer;
    char s[1024];
    LPGUID guidPointer;
    PBYTE bufferPointer = (PBYTE)buffer;
    ULONG i;

    for (i = 0; i < buffer->NumberOfProviders; i++) {
	traceProviderInfo = &buffer->TraceProviderInfoArray[i];
	if (traceProviderInfo->ProviderNameOffset == 0)
	    continue;
	guidPointer = &traceProviderInfo->ProviderGuid;
	stringPointer = (PWCHAR)
		(bufferPointer + traceProviderInfo->ProviderNameOffset);
	WideCharToMultiByte(CP_ACP, 0, stringPointer, -1, s, 1024, NULL, NULL);
  	if (strcmp(s, name) == 0)
  	    return guidPointer;
    }
    return NULL;
}

ULONG
ProviderGuid(LPTSTR name, LPGUID guidPointer)
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
	    guidPointer = LookupGuidInBuffer(name, buffer);
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

TRACEHANDLE
openTraceHandle(LPTSTR name)
{
    TRACEHANDLE handle;
    EVENT_TRACE_LOGFILE traceMode;

    ZeroMemory(&traceMode, sizeof(EVENT_TRACE_LOGFILE));
    traceMode.LoggerName = name;
    traceMode.BufferCallback = (PEVENT_TRACE_BUFFER_CALLBACK)BufferCallback;
    traceMode.EventRecordCallback = (PEVENT_RECORD_CALLBACK)ProcessEvent;
    traceMode.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME;

    handle = OpenTrace(&traceMode);
    if (handle == INVALID_PROCESSTRACE_HANDLE) {
	ULONG sts = GetLastError();
	fprintf(stderr, "OpenTrace: %s (%lu)\n", tdherror(sts), sts);
	CloseTrace(handle);
	exit(1);
    }
    return handle;
}

static char *options = "k:v?";
static char usage[] =
    "Usage: %s [options] tracename\n\n"
    "Options:\n"
    "  -k subsys     kernel subsystem to trace\n"
    "  -g            use global sequence numbers\n"
    "  -v            verbose diagnostics (errors)\n";

int
main(int argc, LPTSTR *argv)
{
    BOOL useGlobalSequence = FALSE;
    TRACEHANDLE session;
    ULONG sts, sysFlags = 0;
    int c;

    while ((c = getopt(argc, argv, options)) != EOF) {
	switch (c) {
	case 'g':
	    useGlobalSequence = TRUE;
	    break;
	case 'k':
	    sysFlags |= kernelTraceFlag(optarg);
	    break;
	case 'v':
	    verbose++;
	    break;
	case '?':
	default:
	    fprintf(stderr, usage, argv[0]);
	    exit(1);
	}
    }

    if (sysFlags) {
	enableEventTrace(KERNEL_LOGGER_NAME, sizeof(KERNEL_LOGGER_NAME),
			 &SystemTraceControlGuid, sysFlags, useGlobalSequence);
	session = openTraceHandle(KERNEL_LOGGER_NAME);
    } else {
	session = openTraceHandle(PCP_SESSION);
    }

    sts = ProcessTrace(&session, 1, NULL, NULL);
    if (sts == ERROR_CANCELLED)
	sts = ERROR_SUCCESS;
    if (sts != ERROR_SUCCESS)
	fprintf(stderr, "ProcessTrace: %s (%lu)\n", tdherror(sts), sts);
    return sts;
}
