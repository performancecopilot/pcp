/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"

PMCD_DATA ClientInfo	*client;
PMCD_DATA int		nClients;	/* Number in array, (not all in use) */
PMCD_DATA int		this_client_id;

/*
 * Expose ClientInfo struct for client #n
 */
ClientInfo *
GetClient(int n)
{
    if (0 <= n && n < nClients && client[n].status.connected)
	return &client[n];
    return NULL;
}

void
ShowClients(FILE *f)
{
    int			i;
    char		*sbuf;
    char		*hostName;

    fprintf(f, "     fd  client connection from                    ipc ver  operations denied\n");
    fprintf(f, "     ==  ========================================  =======  =================\n");
    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;

	fprintf(f, "    %3d  ", client[i].fd);

	hostName = __pmGetNameInfo(client[i].addr);
	if (hostName == NULL) {
	    sbuf = __pmSockAddrToString(client[i].addr);
	    fprintf(f, "%s", sbuf);
	    free(sbuf);
	} else {
	    fprintf(f, "%-40.40s", hostName);
	    free(hostName);
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
    fputc('\n', f);
}
