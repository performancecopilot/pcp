/*
 * evntprov.h
 *
 * This file is part of the ReactOS PSDK package.
 *
 * Contributors:
 *   Created by Amine Khaldi.
 *
 * THIS SOFTWARE IS NOT COPYRIGHTED
 *
 * This source code is offered for use in the public domain. You may
 * use, modify or distribute it freely.
 *
 * This code is distributed in the hope that it will be useful but
 * WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 * DISCLAIMED. This includes but is not limited to warranties of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _EVNTPROV_H_
#define _EVNTPROV_H_

#ifndef EVNTAPI
#ifndef MIDL_PASS
#ifdef _EVNT_SOURCE_
#define EVNTAPI __stdcall
#else
#define EVNTAPI DECLSPEC_IMPORT __stdcall
#endif /* _EVNT_SOURCE_ */
#endif /* MIDL_PASS */
#endif /* EVNTAPI */

#ifdef __cplusplus
extern "C" {
#endif

#include <guiddef.h>

#define EVENT_MIN_LEVEL				0
#define EVENT_MAX_LEVEL				0xff

#define EVENT_ACTIVITY_CTRL_GET_ID		1
#define EVENT_ACTIVITY_CTRL_SET_ID		2
#define EVENT_ACTIVITY_CTRL_CREATE_ID		3
#define EVENT_ACTIVITY_CTRL_GET_SET_ID		4
#define EVENT_ACTIVITY_CTRL_CREATE_SET_ID	5

typedef ULONGLONG REGHANDLE, *PREGHANDLE;

#define MAX_EVENT_DATA_DESCRIPTORS		128
#define MAX_EVENT_FILTER_DATA_SIZE		1024

#define EVENT_FILTER_TYPE_SCHEMATIZED		0x80000000

typedef struct _EVENT_DESCRIPTOR {
  USHORT    Id;
  UCHAR     Version;
  UCHAR     Channel;
  UCHAR     Level;
  UCHAR     Opcode;
  USHORT    Task;
  ULONGLONG Keyword;
} EVENT_DESCRIPTOR, *PEVENT_DESCRIPTOR;
typedef const EVENT_DESCRIPTOR *PCEVENT_DESCRIPTOR;

typedef struct _EVENT_DATA_DESCRIPTOR {
  ULONGLONG Ptr;
  ULONG     Size;
  ULONG     Reserved;
} EVENT_DATA_DESCRIPTOR, *PEVENT_DATA_DESCRIPTOR;

struct _EVENT_FILTER_DESCRIPTOR {
  ULONGLONG Ptr;
  ULONG     Size;
  ULONG     Type;
};
#ifndef DEFINED_PEVENT_FILTER_DESC
typedef struct _EVENT_FILTER_DESCRIPTOR EVENT_FILTER_DESCRIPTOR, *PEVENT_FILTER_DESCRIPTOR;
#define DEFINED_PEVENT_FILTER_DESC	1
#endif	/* for  evntrace.h */

typedef struct _EVENT_FILTER_HEADER {
  USHORT    Id;
  UCHAR     Version;
  UCHAR     Reserved[5];
  ULONGLONG InstanceId;
  ULONG     Size;
  ULONG     NextOffset;
} EVENT_FILTER_HEADER, *PEVENT_FILTER_HEADER;


#ifndef _ETW_KM_ /* for wdm.h */

typedef VOID
(NTAPI *PENABLECALLBACK)(
  LPCGUID SourceId,
  ULONG IsEnabled,
  UCHAR Level,
  ULONGLONG MatchAnyKeyword,
  ULONGLONG MatchAllKeyword,
  PEVENT_FILTER_DESCRIPTOR FilterData,
  PVOID CallbackContext);

#if (_WIN32_WINNT >= 0x0600)
ULONG EVNTAPI EventRegister(
  LPCGUID ProviderId,
  PENABLECALLBACK EnableCallback,
  PVOID CallbackContext,
  PREGHANDLE RegHandle
);

ULONG EVNTAPI EventUnregister(
  REGHANDLE RegHandle
);

BOOLEAN EVNTAPI EventEnabled(
  REGHANDLE RegHandle,
  PCEVENT_DESCRIPTOR EventDescriptor
);

BOOLEAN EVNTAPI EventProviderEnabled(
  REGHANDLE RegHandle,
  UCHAR Level,
  ULONGLONG Keyword
);

ULONG EVNTAPI EventWrite(
  REGHANDLE RegHandle,
  PCEVENT_DESCRIPTOR EventDescriptor,
  ULONG UserDataCount,
  PEVENT_DATA_DESCRIPTOR UserData
);

ULONG EVNTAPI EventWriteTransfer(
  REGHANDLE RegHandle,
  PCEVENT_DESCRIPTOR EventDescriptor,
  LPCGUID ActivityId,
  LPCGUID RelatedActivityId,
  ULONG UserDataCount,
  PEVENT_DATA_DESCRIPTOR UserData
);

ULONG EVNTAPI EventWriteString(
  REGHANDLE RegHandle,
  UCHAR Level,
  ULONGLONG Keyword,
  PCWSTR String
);

ULONG EVNTAPI EventActivityIdControl(
  ULONG ControlCode,
  LPGUID ActivityId
);

#endif /*(_WIN32_WINNT >= 0x0600)*/

#if (_WIN32_WINNT >= 0x0601)
ULONG EVNTAPI EventWriteEx(
  REGHANDLE RegHandle,
  PCEVENT_DESCRIPTOR EventDescriptor,
  ULONG64 Filter,
  ULONG Flags,
  LPCGUID ActivityId,
  LPCGUID RelatedActivityId,
  ULONG UserDataCount,
  PEVENT_DATA_DESCRIPTOR UserData
);
#endif /*(_WIN32_WINNT >= 0x0601)*/

#endif /* _ETW_KM_ */

FORCEINLINE
VOID
EventDataDescCreate(
  PEVENT_DATA_DESCRIPTOR EventDataDescriptor,
  const VOID* DataPtr,
  ULONG DataSize)
{
  EventDataDescriptor->Ptr = (ULONGLONG)(ULONG_PTR)DataPtr;
  EventDataDescriptor->Size = DataSize;
  EventDataDescriptor->Reserved = 0;
}

FORCEINLINE
VOID
EventDescCreate(
  PEVENT_DESCRIPTOR EventDescriptor,
  USHORT Id,
  UCHAR Version,
  UCHAR Channel,
  UCHAR Level,
  USHORT Task,
  UCHAR Opcode,
  ULONGLONG Keyword)
{
  EventDescriptor->Id = Id;
  EventDescriptor->Version = Version;
  EventDescriptor->Channel = Channel;
  EventDescriptor->Level = Level;
  EventDescriptor->Task = Task;
  EventDescriptor->Opcode = Opcode;
  EventDescriptor->Keyword = Keyword;
}

FORCEINLINE
VOID
EventDescZero(
  PEVENT_DESCRIPTOR EventDescriptor)
{
  memset(EventDescriptor, 0, sizeof(EVENT_DESCRIPTOR));
}

FORCEINLINE
USHORT
EventDescGetId(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Id);
}

FORCEINLINE
UCHAR
EventDescGetVersion(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Version);
}

FORCEINLINE
USHORT
EventDescGetTask(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Task);
}

FORCEINLINE
UCHAR
EventDescGetOpcode(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Opcode);
}

FORCEINLINE
UCHAR
EventDescGetChannel(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Channel);
}

FORCEINLINE
UCHAR
EventDescGetLevel(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Level);
}

FORCEINLINE
ULONGLONG
EventDescGetKeyword(
  PCEVENT_DESCRIPTOR EventDescriptor)
{
  return (EventDescriptor->Keyword);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetId(
  PEVENT_DESCRIPTOR EventDescriptor,
  USHORT Id)
{
  EventDescriptor->Id = Id;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetVersion(
  PEVENT_DESCRIPTOR EventDescriptor,
  UCHAR Version)
{
  EventDescriptor->Version = Version;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetTask(
  PEVENT_DESCRIPTOR EventDescriptor,
  USHORT Task)
{
  EventDescriptor->Task = Task;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetOpcode(
  PEVENT_DESCRIPTOR EventDescriptor,
  UCHAR Opcode)
{
  EventDescriptor->Opcode = Opcode;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetLevel(
  PEVENT_DESCRIPTOR EventDescriptor,
  UCHAR  Level)
{
  EventDescriptor->Level = Level;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetChannel(
  PEVENT_DESCRIPTOR EventDescriptor,
  UCHAR Channel)
{
  EventDescriptor->Channel = Channel;
  return (EventDescriptor);
}

FORCEINLINE
PEVENT_DESCRIPTOR
EventDescSetKeyword(
  PEVENT_DESCRIPTOR EventDescriptor,
  ULONGLONG Keyword)
{
  EventDescriptor->Keyword = Keyword;
  return (EventDescriptor);
}


FORCEINLINE
PEVENT_DESCRIPTOR
EventDescOrKeyword(
  PEVENT_DESCRIPTOR EventDescriptor,
  ULONGLONG Keyword)
{
  EventDescriptor->Keyword |= Keyword;
  return (EventDescriptor);
}

#ifdef __cplusplus
}
#endif

#endif /* _EVNTPROV_H_ */

