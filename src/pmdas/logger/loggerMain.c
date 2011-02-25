/*
 * logger main loop function
 *
 * Copyright (c) 2011 Red Hat Inc.
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

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"
#include "event.h"

void
loggerMain(pmdaInterface *dispatch)
{
    int pmcdfd = __pmdaInFd(dispatch);
    int maxfd;
    fd_set fds;
    fd_set readyfds;
    int nready;
    int monitorfd;

    event_init(dispatch->domain);

    /* Try to open logfile to monitor */
    monitorfd = open(monitor_path, O_RDONLY);
    if (monitorfd < 0) {
	__pmNotifyErr(LOG_ERR, "open failure on %s", monitor_path);
	exit(1);
    }

    /* Skip to the end. */
    //(void)lseek(monitorfd, 0, SEEK_END);

    /* We can't really select on the logfile.  Why?  If the logfile
     * is a normal file, select will (continually) return EOF after
     * we've read all the data.  So, instead, we'll try to read data
     * before handling any message we get on the control channel. */
    FD_ZERO(&fds);
    FD_SET(pmcdfd, &fds);
    maxfd = pmcdfd;

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);

	if (nready == 0)
	    continue;
	else if (nready < 0) {
	    if (errno != EINTR) {
		__pmNotifyErr(LOG_ERR, "select failure");
		exit(1);
	    }
	    continue;
	}

	if (FD_ISSET(pmcdfd, &readyfds)) {
	    if (event_create(monitorfd) < 0)
		exit(1);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "processing pmcd request [fd=%d]", pmcdfd);
#endif
	    if (__pmdaMainPDU(dispatch) < 0) {
		exit(1);	/* fatal if we lose pmcd */
	    }
	}
    }
}
