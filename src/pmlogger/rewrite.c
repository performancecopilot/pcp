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

#include "logger.h"

/*
 * PDU for pmResult (PDU_RESULT) -- from libpcp/src/p_fetch.c
 */

typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;

typedef struct {
    __pmPDUHdr		hdr;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

__pmPDU *
rewrite_pdu(__pmPDU *pb, int version)
{
    result_t		*pp;
    int			numpmid;
    int			valfmt;
    int			numval;
    int			vsize;
    vlist_t		*vlp;
    pmValueSet		*vsp;
    pmValueBlock	*vbp;
    int			i;
    int			j;

    if (version == PM_LOG_VERS02)
	return pb;

    if (version == PM_LOG_VERS01) {
	pp = (result_t *)pb;
	numpmid = ntohl(pp->numpmid);
	vsize = 0;
	for (i = 0; i < numpmid; i++) {
	    vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	    numval = ntohl(vlp->numval);
	    valfmt = ntohl(vlp->valfmt);
	    for (j = 0; j < numval; j++) {
		vsp = (pmValueSet *)vlp;
		if (valfmt == PM_VAL_INSITU)
		    continue;
		vbp = (pmValueBlock *)&pb[ntohl(vsp->vlist[j].value.lval)];
		if (vbp->vtype == PM_TYPE_FLOAT) {
		    /* suck FLOAT back from pmValueBlock and make INSITU */
		    vlp->valfmt = htonl(PM_VAL_INSITU);
		    memcpy(&vsp->vlist[j].value.lval, &vbp->vbuf, sizeof(float));
		}
		vbp->vtype = 0;
	    }
	    vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	    if (numval > 0)
		vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
	    if (numval < 0)
		vlp->numval = htonl(XLATE_ERR_2TO1(numval));
	}

	return pb;
    }

    fprintf(stderr, "Errors: do not know how to re-write the PDU buffer for a version %d archive\n", version);
    exit(1);
}
