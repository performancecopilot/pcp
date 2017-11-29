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

/* Extract per-link network information via kstat.
 *
 * Link stats are in the sections called "link" and the stat name
 * is the same as the link names */

#include <kstat.h>
#include "common.h"

int
netlink_fetch(pmdaMetric *pm, int inst, pmAtomValue *av)
{
    char *lname;
    metricdesc_t *md = pm->m_user;
    kstat_t *k;
    char *stat = (char *)md->md_offset;

    if (pmdaCacheLookup(indomtab[NETLINK_INDOM].it_indom, inst, &lname,
                        (void **)&k) != PMDA_CACHE_ACTIVE)
        return PM_ERR_INST;

    if (k) {
	kstat_named_t *kn = kstat_data_lookup(k, stat);

	if (kn == NULL) {
	    fprintf(stderr, "No kstat called %s for %s\n", stat, lname);
	    return 0;
	}

        switch (pm->m_desc.type) {
        case PM_TYPE_32:
	    if (kn->data_type == KSTAT_DATA_INT32) {
		av->l = kn->value.i32;
		return 1;
	    }
	    break;
        case PM_TYPE_U32:
	    if (kn->data_type == KSTAT_DATA_UINT32) {
		av->ul = kn->value.ui32;
		return 1;
	    }
	    break;
        case PM_TYPE_64:
	    if (kn->data_type == KSTAT_DATA_INT64) {
		av->ll = kn->value.i64;
		return 1;
	    }
	    break;
        case PM_TYPE_U64:
	    if (kn->data_type == KSTAT_DATA_UINT64) {
		av->ull = kn->value.ui64;
		return 1;
	    }
	    break;
	}
    }

    return 0;
}

void
netlink_update_stats(int fetch)
{
    kstat_t *k;
    kstat_ctl_t *kc;
    pmInDom indom = indomtab[NETLINK_INDOM].it_indom;

    if ((kc = kstat_ctl_update()) == NULL)
	return;

    for (k = kc->kc_chain; k != NULL; k = k->ks_next) {
	if (strcmp(k->ks_module, "link") == 0) {
	    int rv;
	    kstat_t *cached;

	    if (pmdaCacheLookupName(indom, k->ks_name, &rv,
				    (void **)&cached) != PMDA_CACHE_ACTIVE) {
		rv = pmdaCacheStore(indom, PMDA_CACHE_ADD, k->ks_name, k);
		if (rv < 0) {
		    pmNotifyErr(LOG_WARNING,
				  "Cannot create instance for "
				  "network link '%s': %s\n",
				  k->ks_name, pmErrStr(rv));
		    continue;
		}
	    }

	    if (fetch)
		kstat_read(kc, k, NULL);
	}
    }
}

void
netlink_refresh(void)
{
    pmdaCacheOp(indomtab[NETLINK_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    netlink_update_stats(1);
    pmdaCacheOp(indomtab[NETLINK_INDOM].it_indom, PMDA_CACHE_SAVE);
}

void
netlink_init(int first)
{
    pmdaCacheOp(indomtab[NETLINK_INDOM].it_indom, PMDA_CACHE_LOAD);
    netlink_update_stats(0);
    pmdaCacheOp(indomtab[NETLINK_INDOM].it_indom, PMDA_CACHE_SAVE);
}
