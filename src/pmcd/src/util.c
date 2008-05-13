/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include "pmcd.h"

/* Based on Stevens (Unix Network Programming, p.83) */

void
StartDaemon()
{
    int childpid;

#if defined(HAVE_TERMIO_SIGNALS)
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
#endif

    if ((childpid = fork()) < 0)
	__pmNotifyErr(LOG_ERR, "StartDaemon: fork");
	/* but keep going */
    else if (childpid > 0) {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	fclose(stderr);
	exit(0);
    }

    /* not a process group leader, lose controlling tty */
    if (setsid() == -1)
	__pmNotifyErr(LOG_WARNING, "StartDaemon: setsid");
	/* but keep going */

    close(0);
    /* don't close other fd's -- we know that only good ones are open! */

    /* don't chdir("/") -- we still need to open pmcd.log */
}
