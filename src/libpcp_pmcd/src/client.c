/*
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"

PMCD_INTERN ClientInfo	*client;
PMCD_INTERN int		nClients;	/* Number in array, (not all in use) */
PMCD_INTERN int		this_client_id;

void
ShowClients(FILE *f)
{
    int			i;
    __pmHostEnt		*hp;

    fprintf(f, "     fd  client connection from                    ipc ver  operations denied\n");
    fprintf(f, "     ==  ========================================  =======  =================\n");
    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;

	fprintf(f, "    %3d  ", client[i].fd);

	hp = __pmGetHostByAddr(&client[i].addr);
	if (hp == NULL) {
	    char	*p = (char *)&client[i].addr.sin_addr.s_addr;
	    int	k;

	    for (k = 0; k < 4; k++) {
		if (k > 0)
		    fputc('.', f);
		fprintf(f, "%d", p[k] & 0xff);
	    }
	}
	else
	    fprintf(f, "%-40.40s", hp->h_name);

	fprintf(f, "  %7d", __pmVersionIPC(client[i].fd));

	if (client[i].denyOps != 0) {
	    fprintf(f, "  ");
	    if (client[i].denyOps & PMCD_OP_FETCH)
		fprintf(f, "fetch ");
	    if (client[i].denyOps & PMCD_OP_STORE)
		fprintf(f, "store ");
	}

	fputc('\n', f);
    }
    fputc('\n', f);
}
