/*
 * Event queue support for the ETW PMDA
 *
 * Copyright (c) 2011 Nathan Scott.  All rights reserved.
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

#include "event.h"

int
event_init(void)
{
    __pmNotifyErr(LOG_INFO, "%s: Starting up tracing ...", __FUNCTION__);
    return 0;
}

void
event_shutdown(void)
{
    __pmNotifyErr(LOG_INFO, "%s: Shutting down tracing ...", __FUNCTION__);
}

int
event_decoder(int eventarray, void *buffer, size_t size, void *data)
{
    int sts; /* , handle = *(int *)data; */
    pmAtomValue atom;
    pmID pmid = 0;	/* TODO */

    atom.cp = buffer;
    sts = pmdaEventAddParam(eventarray, pmid, PM_TYPE_STRING, &atom);
    if (sts < 0)
	return sts;
    return 1;	/* simple decoder, added just one parameter into event array */
}

#if 0
... event callback ...
	pmdaEventQueueAppend(queueid, p, bytes, &timestamp);
#endif
