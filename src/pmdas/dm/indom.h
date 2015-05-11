/*
 * Device Mapper PMDA instance domains
 *
 * Copyright (c) 2015 Red Hat.
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

#ifndef INDOM_H
#define INDOM_H

enum {
    DM_CACHE_INDOM = 0,		/* 0 -- Caches */
    DM_THIN_POOL_INDOM = 1,	/* 1 -- Thin Pools */
    DM_THIN_VOL_INDOM = 2,	/* 2 -- Thin Volumes */
    NUM_INDOMS
};

extern pmInDom dm_indom(int);

#endif /* INDOM_H */
