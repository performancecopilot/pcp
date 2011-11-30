/*
 * Copyright (C) 2010 Max Matveev. All rights reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* Extract information about ZFS' Adjustable Replacement Cache
 *
 * The stats are in the sections called "arc_stats" of module zfs
 */

#include <kstat.h>
#include "common.h"

static kstat_t *arcstats;
int arcstats_fresh;

void
arcstats_refresh(void)
{
    kstat_ctl_t *kc;
    arcstats_fresh = 0;
    if ((kc = kstat_ctl_update()) == NULL)
	return;
    if ((arcstats = kstat_lookup(kc, "zfs", -1, "arcstats")) != NULL)
        arcstats_fresh = kstat_read(kc, arcstats, NULL) != -1;
}

int
arcstats_fetch(pmdaMetric *pm, int inst, pmAtomValue *av)
{
    metricdesc_t *md = pm->m_user;
    char *metric = (char *)md->md_offset;
    kstat_named_t *kn;

    if (!arcstats_fresh)
	return 0;

    if ((kn = kstat_data_lookup(arcstats, metric)) != NULL) 
	return kstat_named_to_pmAtom(kn, av);

    return 0;
}
