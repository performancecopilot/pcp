/*
 * Copyright (c) 2018 Red Hat.
 * Portions Copyright (c) International Business Machines Corp., 2002
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

#include <string.h>
#include <utmp.h>
#include "login.h"

/*
 * Count the number of users, root users and sessions using utmp(5)
 */
void
refresh_login_info(struct login_info *up)
{
    struct utmp		*ut;

    memset(up, 0, sizeof(login_info_t));

    setutent();
    while ((ut = getutent())) {
	if (ut->ut_type == USER_PROCESS) {
	    if (ut->ut_user[0] == '\0')
		continue;
	    if (strcmp(ut->ut_user, "root") == 0)
		up->nroots++;
	    up->nusers++;
	}
	up->nsessions++;
    }
    endutent();
}
