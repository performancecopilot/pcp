/*
 * Scan user accounting records to determine nusers.
 *
 * Copyright (c) 2026 Ken McDonell.  All Rights Reserved.
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

#include <stdio.h>
#include <utmpx.h>
#include <sys/stat.h>

#include "pmapi.h"
#include "pmda.h"
#include "freebsd.h"

int
refresh_nusers(void)
{
    struct utmpx	*utp;
    int			nusers = 0;
    struct stat		sbuf;
    char		tty[MAXPATHLEN];

    setutxent();
    while ((utp = getutxent()) != NULL) {
	if (utp->ut_type != USER_PROCESS)
	    continue;
	snprintf(tty, sizeof(tty), "/dev/%s", utp->ut_line);
	if (stat(tty, &sbuf) != 0 || !S_ISCHR(sbuf.st_mode))
	    continue;
	nusers++;
    }
    return nusers;
}
