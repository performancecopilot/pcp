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
#ifndef UTIL_H
#define UTIL_H

extern void dumpKernelTraceFlags(FILE *, const char *, const char *);
extern ULONG kernelTraceFlag(const char *);

extern const char *eventHeaderFlags(USHORT);
extern const char *eventPropertyFlags(USHORT);

extern const char *strguid(LPGUID);
extern const char *tdherror(ULONG);

extern void *BufferAllocate(ULONG);
extern void BufferFree(void *);

#endif
