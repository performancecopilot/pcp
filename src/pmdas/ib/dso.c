/*
 * Copyright (C) 2007,2008 Silicon Graphics, Inc. All Rights Reserved.
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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * DSO initialization routine
 */

#include <stdio.h>
#include <limits.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "ibpmda.h"

void
ib_init (pmdaInterface * dispatch)
{
    char helppath[MAXPATHLEN];
    int  sep = __pmPathSeparator();

    snprintf(helppath, sizeof(helppath), "%s%c" "ib" "%c" "help",
	     pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDSO(dispatch, PMDA_INTERFACE_3, "ibpmda", helppath);

    ibpmda_init(NULL, dispatch);
}
