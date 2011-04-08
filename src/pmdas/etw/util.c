/*
 * Trace Data Helper utility routines.
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
#include "util.h"

const char *tdherror(ULONG code)
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
    return "Unknown error";
}


const char *strguid(LPGUID guidPointer)
{
    static char stringBuffer[64];

    snprintf(stringBuffer, sizeof(stringBuffer),
		"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
	       guidPointer->Data1, guidPointer->Data2, guidPointer->Data3,
	       guidPointer->Data4[0], guidPointer->Data4[1],
	       guidPointer->Data4[2], guidPointer->Data4[3],
	       guidPointer->Data4[4], guidPointer->Data4[5],
	       guidPointer->Data4[6], guidPointer->Data4[7]);
    return stringBuffer;
}
