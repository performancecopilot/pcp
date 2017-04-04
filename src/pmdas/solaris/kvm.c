/*
 * Copyright (C) 2009 Max Matveev. All Rights Reserved
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
#include <kvm.h>
#include <nlist.h>

#include "common.h"


static kvm_t *kvm;
struct nlist kvm_names[] = {
	{.n_name = "fsf_total"},
	{.n_name = NULL}
};

fsf_stat_t s = {0};
static int fresh;

void
kvm_init(int ignore)
{
    kvm = kvm_open(NULL, NULL, NULL, O_RDONLY, "pmdasolaris");
    if (kvm && kvm_nlist(kvm, kvm_names))
	fprintf(stderr, "Cannot extract addresses\n");
}

void
kvm_refresh(void)
{
    fresh =  kvm &&
	     (kvm_kread(kvm, kvm_names[0].n_value, &s, sizeof(s)) == sizeof(s));
}

int
kvm_fetch(pmdaMetric *pm, int inst, pmAtomValue *v)
{
	metricdesc_t *md = pm->m_user;
	char *p = (char *)&s;

	if (!fresh)
	    return 0;

	memcpy(&v->ull, p + md->md_offset, sizeof(v->ull));
	return 1;
}
