/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include "main.h"
#include <stdarg.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

Console *console;

Console::Console() : QDialog()
{
    struct timeval now;

    my.level = 0;
    if (pmDebug & DBG_TRACE_APPL0)
	my.level |= KmChart::DebugApp;	// kmtime apps internals
    if (pmDebug & DBG_TRACE_APPL1)
	my.level |= KmChart::DebugProtocol;// trace time protocol
    if (pmDebug & DBG_TRACE_APPL2)
	my.level |= KmChart::DebugGUI;	// interesting GUI events
    setupUi(this);

    gettimeofday(&now, NULL);
    my.origin = tosec(now);
}

void Console::post(char *fmt, ...)
{
    static char buffer[4096];
    struct timeval now;
    va_list ap;

    if (!(my.level & KmChart::DebugApp))
	return;

    gettimeofday(&now, NULL);
    sprintf(buffer, "%6.2f: ", tosec(now) - my.origin);

    va_start(ap, fmt);
    vsnprintf(buffer+8, sizeof(buffer)-8, fmt, ap);
    va_end(ap);

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}

void Console::post(int level, char *fmt, ...)
{
    static char buffer[4096];
    struct timeval now;
    va_list ap;

    if (!(my.level & level))
	return;

    gettimeofday(&now, NULL);
    sprintf(buffer, "%6.2f: ", tosec(now) - my.origin);

    va_start(ap, fmt);
    vsnprintf(buffer+8, sizeof(buffer)-8, fmt, ap);
    va_end(ap);

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}
