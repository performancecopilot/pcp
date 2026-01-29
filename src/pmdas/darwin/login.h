/*
 * Login/session statistics types
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
#ifndef LOGIN_H
#define LOGIN_H

/*
 * Login statistics structure
 * Tracks user sessions via the utmpx database
 */
typedef struct login_info {
    __uint32_t	nusers;		/* user sessions (including root) */
    __uint32_t	nroots;		/* root user sessions only */
    __uint32_t	nsessions;	/* total utmpx session records */
} login_info_t;

extern int refresh_login(login_info_t *);

#endif /* LOGIN_H */
