/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <stdarg.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "qed_console.h"

QedConsole *console;

QedConsole::QedConsole(struct timeval origin) : QDialog()
{
    my.level = 0;
    if (pmDebugOptions.appl0) {
	my.level |= QedApp::DebugApp;		// general and UI tracing
	my.level |= QedApp::DebugUi;
    }
    if (pmDebugOptions.appl1)
	my.level |= QedApp::DebugProtocol;	// trace time protocol
    if (pmDebugOptions.appl2) {
	my.level |= QedApp::DebugView;		// config files, for QA
	my.level |= QedApp::DebugTimeless;
    }
    setupUi(this);

    my.origin = QedApp::timevalToSeconds(origin);
    post("Console available");
}

void QedConsole::post(const char *fmt, ...)
{
    static char buffer[4096];
    struct timeval now;
    va_list ap;
    int sts, offset = 0;

    if (!(my.level & QedApp::DebugApp))
	return;

    if (!(my.level & QedApp::DebugTimeless)) {
	gettimeofday(&now, NULL);
	pmsprintf(buffer, sizeof(buffer), "%6.2f: ",
			QedApp::timevalToSeconds(now) - my.origin);
	offset = 8;
    }

    va_start(ap, fmt);
    sts = vsnprintf(buffer+offset, sizeof(buffer)-offset, fmt, ap);
    va_end(ap);
    if (sts >= (int)sizeof(buffer) - offset)
	buffer[sizeof(buffer)-1] = '\0';

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}

bool QedConsole::logLevel(int level)
{
    if (!(my.level & level))
	return false;
    return true;
}

void QedConsole::post(int level, const char *fmt, ...)
{
    static char buffer[4096];
    struct timeval now;
    va_list ap;
    int sts, offset = 0;

    if (!(my.level & level) && !(level & QedApp::DebugForce))
	return;

    if (!(my.level & QedApp::DebugTimeless)) {
	gettimeofday(&now, NULL);
	pmsprintf(buffer, sizeof(buffer), "%6.2f: ",
			QedApp::timevalToSeconds(now) - my.origin);
	offset = 8;
    }

    va_start(ap, fmt);
    sts = vsnprintf(buffer+offset, sizeof(buffer)-offset, fmt, ap);
    va_end(ap);
    if (sts >= (int)sizeof(buffer) - offset)
	buffer[sizeof(buffer)-1] = '\0';

    fputs(buffer, stderr);
    fputc('\n', stderr);
    text->append(QString(buffer));
}
