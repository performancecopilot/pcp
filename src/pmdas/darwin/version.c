/*
 * Copyright (c) 2025 Red Hat.  All Rights Reserved.
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

#include <mach/mach.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CoreFoundation.h>
#include "pmapi.h"

#define OSRELEASEFILE "/System/Library/CoreServices/SystemVersion.plist"

char *
macos_version(void)
{
    static char version_string[64];
    const UInt8* osfile = (const UInt8 *)OSRELEASEFILE;
    const size_t length = sizeof(OSRELEASEFILE) - 1;

    if (version_string[0])
	return version_string;

    pmsprintf(version_string, sizeof(version_string), "unknown");

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, osfile, length, false);
    if (!url)
	return version_string;

    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
    CFRelease(url);
    if (!stream || !CFReadStreamOpen(stream)) {
	if (stream) CFRelease(stream);
	return version_string;
    }

    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    UInt8 bytes[4096];
    CFIndex bytesRead;
    while ((bytesRead = CFReadStreamRead(stream, bytes, sizeof(bytes))) > 0)
	CFDataAppendBytes(data, bytes, bytesRead);
    CFReadStreamClose(stream);
    CFRelease(stream);
    if (bytesRead < 0) {
	CFRelease(data);
	return version_string;
    }

    CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(data);
    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
	if (plist) CFRelease(plist);
	return version_string;
    }

    char name[32], version[32];
    CFDictionaryRef dict = (CFDictionaryRef)plist;
    CFStringRef productName = CFDictionaryGetValue(dict, CFSTR("ProductName"));
    CFStringRef productVersion = CFDictionaryGetValue(dict, CFSTR("ProductVersion"));
    if (CFStringGetCString(productName, name, sizeof(name), kCFStringEncodingUTF8) &&
	CFStringGetCString(productVersion, version, sizeof(version), kCFStringEncodingUTF8))
	pmsprintf(version_string, sizeof(version_string), "%s %s", name, version);
}
