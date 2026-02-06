/*
 * Login/session statistics for macOS
 * Copyright (c) 2026 Red Hat, Paul Smith.
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
#include <utmpx.h>
#include "pmapi.h"
#include "login.h"

/*
 * Count the number of users, root users and sessions using utmpx(5)
 *
 * macOS uses the utmpx API (not utmp) for session tracking.
 * We iterate through all utmpx entries and count:
 * - nusers: USER_PROCESS entries with non-empty username (includes root)
 * - nroots: subset of nusers where username is "root"
 * - nsessions: total count of all utmpx records
 */
int
refresh_login(login_info_t *info)
{
    struct utmpx *ut;

    memset(info, 0, sizeof(login_info_t));

    setutxent();
    while ((ut = getutxent()) != NULL) {
	if (ut->ut_type == USER_PROCESS) {
	    if (ut->ut_user[0] == '\0')
		continue;
	    if (strcmp(ut->ut_user, "root") == 0)
		info->nroots++;
	    info->nusers++;
	}
	info->nsessions++;
    }
    endutxent();

    return 0;
}
