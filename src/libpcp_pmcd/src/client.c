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
    __pmHostEnt		h;
    char		*hbuf, *sbuf;

    fprintf(f, "     fd  client connection from                    ipc ver  operations denied\n");
    fprintf(f, "     ==  ========================================  =======  =================\n");
    hbuf = __pmAllocHostEntBuffer();
    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;

	fprintf(f, "    %3d  ", client[i].fd);

	if (__pmGetHostByAddr(&client[i].addr, &h, hbuf) == NULL) {
	    sbuf = __pmSockAddrInToString(&client[i].addr);
	    fprintf(f, "%s", sbuf);
	    free(sbuf);
	} else {
	    fprintf(f, "%-40.40s", h.h_name);
	}
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
    __pmFreeHostEntBuffer(hbuf);
    fputc('\n', f);
}
