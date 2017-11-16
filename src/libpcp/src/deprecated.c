/*
 * Deprecated PMAPI routines ... someday these might all go away.
 *
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
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
#include "pmapi.h"
#include "libpcp.h"
#include "deprecated.h"
#include "internal.h"

int
__pmSetProgname(const char *program)
{
    pmSetProgname(program);
    return 0;
}

#undef __pmGetAPIConfig
const char *
__pmGetAPIConfig(const char *name)
{
    return pmGetAPIConfig(name);
}

#undef __pmOpenLog
FILE *
__pmOpenLog(const char *progname, const char *logname, FILE *oldstream, int *status)
{
    return pmOpenLog(progname, logname, oldstream, status);
}

#undef __pmNoMem
void
__pmNoMem(const char *where, size_t size, int fatal)
{
    return pmNoMem(where, size, fatal);
}

#undef __pmNotifyErr
void
__pmNotifyErr(int priority, const char *message, ...)
{
    va_list	arg;

    va_start(arg, message);
    notifyerr(priority, message, arg);
    va_end(arg);
}

#undef __pmSyslog
void
__pmSyslog(int onoff)
{
    pmSyslog(onoff);
}

#undef __pmPrintDesc
void
__pmPrintDesc(FILE *f, const pmDesc *desc)
{
    pmPrintDesc(f, desc);
}
