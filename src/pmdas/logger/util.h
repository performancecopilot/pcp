/*
 * Utility routines.
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

#ifndef _UTIL_H
#define _UTIL_H

extern char *lstrip(char *str);
extern void rstrip(char *str);
extern int start_cmd(const char *cmd, pid_t *ppid);
extern int stop_cmd(pid_t pid);

#endif /* _UTIL_H */
