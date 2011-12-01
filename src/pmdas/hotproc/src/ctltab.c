/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/procfs.h>
#include "pmapi.h"
#include "ctltab.h"
#include "pglobal.h"
#include "pmemory.h"
#include "pracinfo.h"
#include "pscred.h"
#include "psinfo.h"
#include "pstatus.h"
#include "psusage.h"
#include "pcpu.h"
#include "ppred_values.h"

ctltab_entry ctltab[] = {
    { CLUSTER_GLOBAL, 1,
      pglobal_init,  pglobal_getdesc,  pglobal_setatom,  pglobal_getinfo,  pglobal_allocbuf },
    { 1, 1,
      psinfo_init,   psinfo_getdesc,   psinfo_setatom,   psinfo_getinfo,   psinfo_allocbuf },
    { 2, 1,
      pstatus_init,  pstatus_getdesc,  pstatus_setatom,  pstatus_getinfo,  pstatus_allocbuf },
    { 3, 1,
      pscred_init,   pscred_getdesc,   pscred_setatom,   pscred_getinfo,   pscred_allocbuf },
    { 4, 1,
      psusage_init,  psusage_getdesc,  psusage_setatom,  psusage_getinfo,  psusage_allocbuf },
    { 5, 1,
      pmem_init,     pmem_getdesc,     pmem_setatom,     pmem_getinfo,     pmem_allocbuf },
    { 6, 1,
      pracinfo_init, pracinfo_getdesc, pracinfo_setatom, pracinfo_getinfo, pracinfo_allocbuf },
    { CLUSTER_CPU, 1,
      pcpu_init,     pcpu_getdesc,     pcpu_setatom,     pcpu_getinfo,     pcpu_allocbuf },
    { CLUSTER_PRED, 1,
      ppred_init,    ppred_getdesc,    ppred_setatom,    ppred_getinfo,    ppred_allocbuf },
};


int nctltab = sizeof(ctltab) / sizeof (ctltab[0]);

int
lookup_ctltab(int cluster)
{
    int		i;

    for (i = 0; i < nctltab; i++) {
	if (ctltab[i].cluster == cluster)
	    return i;
    }
    return PM_ERR_PMID;
}
