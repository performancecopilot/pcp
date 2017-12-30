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
    pmNoMem(where, size, fatal);
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

#undef __pmtimevalNow
void
__pmtimevalNow(struct timeval *tv)
{
    pmtimevalNow(tv);
}

#undef __pmtimevalInc
void
__pmtimevalInc(struct timeval *ap, const struct timeval *bp)
{
    pmtimevalInc(ap, bp);
}

#undef __pmtimevalDec
void
__pmtimevalDec(struct timeval *ap, const struct timeval *bp)
{
    pmtimevalDec(ap, bp);
}

#undef __pmtimevalAdd
double
__pmtimevalAdd(const struct timeval *ap, const struct timeval *bp)
{
    return pmtimevalAdd(ap, bp);
}

#undef __pmtimevalSub
double
__pmtimevalSub(const struct timeval *ap, const struct timeval *bp)
{
    return pmtimevalSub(ap, bp);
}

#undef __pmtimevalToReal
double
__pmtimevalToReal(const struct timeval *val)
{
    return pmtimevalToReal(val);
}

#undef __pmtimevalFromReal
void
__pmtimevalFromReal(double secs, struct timeval *val)
{
    pmtimevalFromReal(secs, val);
}

#undef __pmPrintStamp
void
__pmPrintStamp(FILE *f, const struct timeval *tp)
{
    pmPrintStamp(f, tp);
}

#undef __pmPrintHighResStamp
void
__pmPrintHighResStamp(FILE *f, const struct timespec *tp)
{
    pmPrintHighResStamp(f, tp);
}

#undef __pmPathSeparator
int
__pmPathSeparator(void)
{
    return pmPathSeparator();
}

#undef __pmGetUsername
int
__pmGetUsername(char **username)
{
    return pmGetUsername(username);
}

#undef __pmSetProcessIdentity
int
__pmSetProcessIdentity(const char *username)
{
    return pmSetProcessIdentity(username);
}

#undef pmFreeHighResResult
void
pmFreeHighResResult(pmHighResResult *result)
{
    __pmFreeHighResResult(result);
}

#undef __pmSpecLocalPMDA
char *
__pmSpecLocalPMDA(const char *spec)
{
    return pmSpecLocalPMDA(spec);
}
