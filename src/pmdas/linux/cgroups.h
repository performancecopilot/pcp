/*
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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

extern int refresh_cgroup_subsys(pmInDom);
extern char *cgroup_find_subsys(pmInDom indom, const char *);
extern void refresh_cgroup_groups(pmInDom indom, __pmnsTree **);
extern void cgroup_init(void);
