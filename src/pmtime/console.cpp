/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#include "console.h"
#include "pmtime.h"
#include <stdarg.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

Console *console;

Console::Console() : QDialog()
{
    my.level = 0;
    if (pmDebugOptions.appl0)
	my.level |= PmTime::DebugApp;	// pmtime apps internals
    if (pmDebugOptions.appl1)
	my.level |= PmTime::DebugProtocol;	// trace pmtime protocol
    setupUi(this);
}

void Console::post(const char *fmt, ...)
{
    static char buffer[4096];
    va_list ap;
    int bytes;

    if (!(my.level & PmTime::DebugApp))
	return;

    va_start(ap, fmt);
    bytes = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    if (bytes >= (int)sizeof(buffer))
	buffer[sizeof(buffer)-1] = '\0';
    else if (bytes < 0)
	buffer[0] = '\0';

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}

void Console::post(int level, const char *fmt, ...)
{
    static char buffer[4096];
    va_list ap;
    int bytes;

    if (!(my.level & level))
	return;

    va_start(ap, fmt);
    bytes = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    if (bytes >= (int)sizeof(buffer))
	buffer[sizeof(buffer)-1] = '\0';
    else if (bytes < 0)
	buffer[0] = '\0';

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}
