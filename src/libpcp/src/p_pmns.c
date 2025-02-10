/*
 * Copyright (c) 2012-2015,2021 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * PDU for name list (PDU_PMNS_NAMES)
 */

typedef struct {
    int namelen;
    char name[sizeof(__pmPDU)]; /* variable length */
} name_t;

typedef struct {
    int status;
    int namelen;
    char name[sizeof(__pmPDU)]; /* variable length */
} name_status_t;

typedef struct {
    __pmPDUHdr	hdr;
    int		nstrbytes; /* number of str bytes including null terminators */
    int 	numstatus; /* = 0 if there is no status to be encoded */
    int		numnames;
    __pmPDU	names[1]; /* list of variable length name_t or name_status_t */
} namelist_t;

/*
 * NOTES:
 *
 * 1.
 * name_t's are padded to a __pmPDU boundary (if needed)
 * so that direct accesses of a following record
 * can be made on a word boundary.
 * i.e. the following "namelen" field will be accessible.
 *
 * 2.
 * Names are sent length prefixed as opposed to null terminated.
 * This can make copying at the decoding end simpler 
 * (and possibly more efficient using memcpy).
 *
 * 3.
 * nstrbytes is used by the decoding function to know how many
 * bytes to allocate.
 *
 * 4.
 * name_status_t was added for pmGetChildrenStatus.
 * It is a variant of the names pdu which encompasses status
 * data as well.
 */ 

void
__pmDumpNameList(FILE *f, int numnames, const char *namelist[])
{
    int i;

    fprintf(f, "namelist dump: numnames = %d\n", numnames);
    for (i = 0; i < numnames; i++)
	fprintf(f, "  name[%d]: \"%s\"\n", i, namelist[i]);
}

void
__pmDumpStatusList(FILE *f, int numstatus, const int statuslist[])
{
    int i;

    fprintf(f, "statuslist dump: numstatus = %d\n", numstatus);
    for (i = 0; i < numstatus; i++)
	fprintf(f, "  status[%d]: %d\n", i, statuslist[i]);
}

void
__pmDumpNameAndStatusList(FILE *f, int numnames, char *namelist[], int statuslist[])
{
    int i;

    fprintf(f, "namelist & statuslist dump: numnames = %d\n", numnames);
    for (i = 0; i < numnames; i++)
	fprintf(f, "  name[%d]: \"%s\" (%s)\n", i, namelist[i],
		statuslist[i] == PMNS_LEAF_STATUS ? "leaf" : "non-leaf");
}

/*
 * Send a PDU_PMNS_NAMES across socket.
 */
int
__pmSendNameList(int fd, int from, int numnames, const char *namelist[],
		 const int statuslist[])
{
    namelist_t		*nlistp;
    int			need;
    int 		nstrbytes=0;
    int 		i;
    name_t		*nt; 
    name_status_t	*nst; 
    int			sts;

    if (pmDebugOptions.pmns) {
        fprintf(stderr, "__pmSendNameList\n");
	__pmDumpNameList(stderr, numnames, namelist);
        if (statuslist != NULL)
	    __pmDumpStatusList(stderr, numnames, statuslist);
    }

    /* namelist_t + names rounded up to a __pmPDU boundary */
    need = sizeof(*nlistp) - sizeof(nlistp->names);
    for (i = 0; i < numnames; i++) {
	int len = (int)strlen(namelist[i]);
        nstrbytes += len+1;
	need += PM_PDU_SIZE_BYTES(len);
	if (statuslist == NULL) 
            need += sizeof(*nt) - sizeof(nt->name);
	else 
            need += sizeof(*nst) - sizeof(nst->name);
    }

    if ((nlistp = (namelist_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    nlistp->hdr.len = need;
    nlistp->hdr.type = PDU_PMNS_NAMES;
    nlistp->hdr.from = from;
    nlistp->nstrbytes = htonl(nstrbytes);
    nlistp->numnames = htonl(numnames);

    if (statuslist == NULL) {
        int j = 0, namelen;
        nlistp->numstatus = htonl(0);
	for(i=0; i<numnames; i++) {
	    nt = (name_t*)&nlistp->names[j/sizeof(__pmPDU)];
	    namelen = (int)strlen(namelist[i]);
	    memcpy(nt->name, namelist[i], namelen);
	    if ((namelen % sizeof(__pmPDU)) != 0) {
		/* clear the padding bytes, lest they contain garbage */
		int	pad;
		char	*padp = nt->name + namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
	    j += sizeof(namelen) + PM_PDU_SIZE_BYTES(namelen);
	    nt->namelen = htonl(namelen);
	}
    }
    else { /* include the status fields */
        int j = 0, namelen;
        nlistp->numstatus = htonl(numnames);
	for(i=0; i<numnames; i++) {
	    nst = (name_status_t*)&nlistp->names[j/sizeof(__pmPDU)];
	    nst->status = htonl(statuslist[i]);
	    namelen = (int)strlen(namelist[i]);
	    memcpy(nst->name, namelist[i], namelen);
	    if ((namelen % sizeof(__pmPDU)) != 0) {
		/* clear the padding bytes, lest they contain garbage */
		int	pad;
		char	*padp = nst->name + namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
	    j += sizeof(nst->status) + sizeof(namelen) +
	         PM_PDU_SIZE_BYTES(namelen);
	    nst->namelen = htonl(namelen);
	}
    }

    sts = __pmXmitPDU(fd, (__pmPDU *)nlistp);
    __pmUnpinPDUBuf(nlistp);
    return sts;
}

/*
 * Decode a PDU_PMNS_NAMES
 *
 * statuslist is optional ... if NULL, no status values will be returned
 */
int
__pmDecodeNameList(__pmPDU *pdubuf, int *numnamesp,
                  char*** namelist, int** statuslist)
{
    namelist_t	*namelist_pdu;
    char 	*pdu_end;
    char        **names;
    char	*dest, *dest_end;
    int		*status = NULL;
    int 	namesize, numnames;
    int 	statussize, numstatus;
    int 	nstrbytes;
    int		namelen;
    int		maxnames;
    int		i, j;

    namelist_pdu = (namelist_t *)pdubuf;
    pdu_end = (char *)pdubuf + namelist_pdu->hdr.len;

    *namelist = NULL;
    if (statuslist != NULL)
	*statuslist = NULL;

    if (pdu_end - (char*)namelist_pdu < sizeof(namelist_t) - sizeof(__pmPDU)) {
	if (pmDebugOptions.pmns || pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: bytes %d < min %d\n",
		(int)(pdu_end - (char*)namelist_pdu), (int)(sizeof(namelist_t) - sizeof(__pmPDU)));
	}
	return PM_ERR_IPC;
    }

    numnames = ntohl(namelist_pdu->numnames);
    numstatus = ntohl(namelist_pdu->numstatus);
    nstrbytes = ntohl(namelist_pdu->nstrbytes);

    if (numnames == 0) {
	*numnamesp = 0;
	if (pmDebugOptions.pmns)
	    fprintf(stderr, "__pmDecodeNameList: no names\n");
	return 0;
    }

    if (numnames < 0) {
	if (pmDebugOptions.pmns) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmDecodeNameList: numnames %d < 0: %s\n",
		numnames, pmErrStr_r(numnames, errmsg, sizeof(errmsg)));
	}
	/* error code in PDU, nothing more to do ... */
	goto done;
    }

    /* validity checks - none of these conditions should happen */
    if (nstrbytes < 0) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: numnames %d and nstrbytes %d < 0\n",
		numnames, nstrbytes);
	}
	return PM_ERR_IPC;
    }
    /* anti-DOS measure - limiting allowable memory allocations */
    if (nstrbytes > namelist_pdu->hdr.len) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: nstrbytes %d > PDU len %d\n",
		nstrbytes, namelist_pdu->hdr.len);
	}
	return PM_ERR_IPC;
    }
    /*
     * 2 or 3 __pmPDU's required for each name ... this is only
     * approximate because padding of each name to a __pmPDU boundary
     * may more than nstrbytes for the names, but the names are not
     * null byte terminated so it may be a little less than nstrbytes
     * ... leaving nstrbytes out of the calculation means maxnames is
     * potentially too large and the guard is too weak, but we have
     * specific checks below as each name is unpacked.
     */
    if (numstatus > 0)
	maxnames = (namelist_pdu->hdr.len - sizeof(namelist_t) + sizeof(__pmPDU)) / (3 * sizeof(__pmPDU));
    else
	maxnames = (namelist_pdu->hdr.len - sizeof(namelist_t) + sizeof(__pmPDU)) / (2 * sizeof(__pmPDU));
    if (numnames > maxnames) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: numname %d", numnames);
	    if (numstatus > 0)
		fprintf(stderr, " and numstatus %d", numstatus);
	    fprintf(stderr, " > max %d for PDU len %d\n",
		maxnames, namelist_pdu->hdr.len);
	}
	return PM_ERR_IPC;
    }
    /* numstatus must be one (and only one) of zero or numnames */
    if (numstatus != 0 && numstatus != numnames) {
	if (pmDebugOptions.pmns || pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: numstatus %d != 0 and != numnames %d\n",
		numstatus, numnames);
	}
	return PM_ERR_IPC;
    }

    namesize = numnames * ((int)sizeof(char*)) + nstrbytes;
    if ((names = (char**)malloc(namesize)) == NULL)
	return -oserror();

    if (statuslist != NULL && numstatus > 0) {
	statussize = numstatus * (int)sizeof(int);
	if ((status = (int*)malloc(statussize)) == NULL) {
	    free(names);
	    return -oserror();
	}
    }

    dest = (char*)&names[numnames];
    dest_end = (char*)names + namesize;

    /* copy over ptrs and characters */
    if (numstatus == 0) {
	name_t	*np;

	for (i = j = 0; i < numnames; i++) {
	    np = (name_t*)&namelist_pdu->names[j/sizeof(__pmPDU)];
	    names[i] = dest;

	    if (sizeof(np->namelen) > (size_t)(pdu_end - (char *)np)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] PDU too short remaining %d < required size %d for namelen\n",
			i, (int)(pdu_end - (char *)np), (int)sizeof(np->namelen));
		}
		goto corrupt;
	    }
	    namelen = ntohl(np->namelen);
	    if (namelen < 0) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] namelen %d < 0\n",
			    i, namelen);
		}
		goto corrupt;
	    }
	    /* ensure source buffer contains everything that we copy over */
	    if (sizeof(np->namelen) + namelen > (size_t)(pdu_end - (char *)np)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] PDU too short remaining %d < %d for namelen+name\n",
			i, (int)(pdu_end - (char *)np), (int)(sizeof(np->namelen) + namelen));
		}
		goto corrupt;
	    }
	    /* ensure space in destination; note null-terminator is added */
	    if (namelen + 1 > dest_end - dest) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] namelen %d + 1 > dst remainder %d\n",
			    i, namelen, (int)(dest_end - dest));
		}
		goto corrupt;
	    }

	    memcpy(dest, np->name, namelen);
	    *(dest + namelen) = '\0';
	    dest += namelen + 1; 

	    j += sizeof(namelen) + PM_PDU_SIZE_BYTES(namelen);
	}
    }
    else { /* status fields included in the PDU */
	name_status_t	*np;

	for (i = j = 0; i < numnames; i++) {
	    np = (name_status_t*)&namelist_pdu->names[j/sizeof(__pmPDU)];
	    names[i] = dest;

	    if (sizeof(np->status) + sizeof(np->namelen) > (size_t)(pdu_end - (char *)np)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] PDU too short remaining %d < required size %d for status+namelen\n",
			i, (int)(pdu_end - (char *)np), (int)(sizeof(np->status) + sizeof(np->namelen)));
		}
		goto corrupt;
	    }
	    namelen = ntohl(np->namelen);
	    if (namelen < 0) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] namelen %d < 0\n",
			    i, namelen);
		}
		goto corrupt;
	    }
	    /* ensure source buffer contains everything that we copy over */
	    if (sizeof(np->status) + sizeof(np->namelen) + namelen > (size_t)(pdu_end - (char *)np)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] PDU too short remaining %d < %d for namelen+status+name\n",
			i, (int)(pdu_end - (char *)np), (int)(sizeof(np->status) + sizeof(np->namelen) + namelen));
		}
		goto corrupt;
	    }
	    /* ensure space for null-terminated name in destination buffer */
	    if (namelen + 1 > dest_end - dest) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeNameList: PM_ERR_IPC: name[%d] namelen %d + 1 > dst remainder %d\n",
			    i, namelen, (int)(dest_end - dest));
		}
		goto corrupt;
	    }

	    if (status != NULL)
		status[i] = ntohl(np->status);

	    memcpy(dest, np->name, namelen);
	    *(dest + namelen) = '\0';
	    dest += namelen + 1; 

	    j += sizeof(np->status) + sizeof(namelen) + PM_PDU_SIZE_BYTES(namelen);
	}
    }

    if (pmDebugOptions.pmns) {
	fprintf(stderr, "__pmDecodeNameList\n");
	__pmDumpNameList(stderr, numnames, (const char **)names);
	if (status != NULL)
	    __pmDumpStatusList(stderr, numstatus, status);
    }

    *namelist = names;
    if (statuslist != NULL)
	*statuslist = status;
    *numnamesp = numnames;

done:
    return numnames;

corrupt:
    if (status != NULL)
	free(status);
    free(names);
    return PM_ERR_IPC;
}

/*********************************************************************/

/*
 * name request 
 */

typedef struct {
    __pmPDUHdr	hdr;
    int		subtype; 
    int		namelen;
    char	name[sizeof(int)];
} namereq_t;

/*
 * Send a PDU_PMNS_CHILD_REQ across socket.
 */
static int
SendNameReq(int fd, int from, const char *name, int pdu_type, int subtype)
{
    namereq_t	*nreq;
    int		need;
    int		namelen;
    int		alloc_len; /* length allocated for name */
    int		sts;
    char        *p;

    if (pmDebugOptions.pmns) {
	char	strbuf[20];
	fprintf(stderr, "SendNameReq: from=%d name=\"%s\" pdu=%s subtype=%d\n",
		from, name, __pmPDUTypeStr_r(pdu_type, strbuf, sizeof(strbuf)), subtype);
    }

    namelen = (int)strlen(name);
    alloc_len = (int)(sizeof(int)*((namelen-1 + sizeof(int))/sizeof(int)));
    need = (int)(sizeof(*nreq) - sizeof(nreq->name) + alloc_len);

    if ((nreq = (namereq_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    nreq->hdr.len = need;
    nreq->hdr.type = pdu_type;
    nreq->hdr.from = from;
    nreq->subtype = htonl(subtype);
    nreq->namelen = htonl(namelen);
    memcpy(&nreq->name[0], name, namelen);
    /* clear the padding bytes, lest they contain garbage */
    for (p = & (nreq->name[namelen]); p < & ((char*)nreq)[need]; p++)
        *p = '~';

    sts = __pmXmitPDU(fd, (__pmPDU *)nreq);
    __pmUnpinPDUBuf(nreq);
    return sts;
}

/*
 * Decode a name request
 */
static int
DecodeNameReq(__pmPDU *pdubuf, char **name_p, int *subtype)
{
    namereq_t	*namereq_pdu;
    char	*pdu_end;
    char	*name;
    int		namelen;

    namereq_pdu = (namereq_t *)pdubuf;
    pdu_end = (char *)pdubuf + namereq_pdu->hdr.len;

    if (pdu_end - (char *)namereq_pdu < sizeof(namereq_t) - sizeof(int)) {
	if (pmDebugOptions.pmns || pmDebugOptions.pdu) {
	    fprintf(stderr, "DecodeNameReq: PM_ERR_IPC: src remainder %d < sizeof(namereq_t) %d - sizeof(int) %d\n",
		(int)(pdu_end - (char *)namereq_pdu), (int)sizeof(namereq_t), (int)sizeof(int));
	}
	return PM_ERR_IPC;
    }

    /* only set it if you want it */
    if (subtype != NULL)
	*subtype = ntohl(namereq_pdu->subtype);
    namelen = ntohl(namereq_pdu->namelen);

    if (namelen < 0 || namelen > namereq_pdu->hdr.len) {
	if (pmDebugOptions.pmns || pmDebugOptions.pdu) {
	    fprintf(stderr, "DecodeNameReq: PM_ERR_IPC: namelen %d < 0 or > hdr.len %d\n",
		namelen, namereq_pdu->hdr.len);
	}
	return PM_ERR_IPC;
    }
    if (sizeof(namereq_t) - sizeof(int) + namelen > (size_t)(pdu_end - (char *)namereq_pdu)) {
	if (pmDebugOptions.pmns || pmDebugOptions.pdu) {
	    fprintf(stderr, "DecodeNameReq: PM_ERR_IPC: sizeof(namereq_t) %d - sizeof(int) %d + namelen %d >= src remainder %d\n",
		(int)sizeof(namereq_t), (int)sizeof(int), namelen, (int)(pdu_end - (char *)namereq_pdu));
	}
	return PM_ERR_IPC;
    }

    name = malloc(namelen+1);
    if (name == NULL)
	return -oserror(); 
    memcpy(name, namereq_pdu->name, namelen);
    name[namelen] = '\0';

    if (pmDebugOptions.pmns)
	fprintf(stderr, "DecodeNameReq: name=\"%s\"\n", name);

    *name_p = name;
    return 0;
}

/*********************************************************************/

/*
 * Send a PDU_PMNS_CHILD
 */
int
__pmSendChildReq(int fd, int from, const char *name, int subtype)
{
    return SendNameReq(fd, from, name, PDU_PMNS_CHILD, subtype);
}


/*
 * Decode a PDU_PMNS_CHILD
 */
int
__pmDecodeChildReq(__pmPDU *pdubuf, char **name_p, int *subtype)
{
    return DecodeNameReq(pdubuf, name_p, subtype);
}

/*********************************************************************/

/*
 * Send a PDU_PMNS_TRAVERSE
 */
int
__pmSendTraversePMNSReq(int fd, int from, const char *name)
{
    return SendNameReq(fd, from, name, PDU_PMNS_TRAVERSE, 0);
}


/*
 * Decode a PDU_PMNS_TRAVERSE
 */
int
__pmDecodeTraversePMNSReq(__pmPDU *pdubuf, char **name_p)
{
    return DecodeNameReq(pdubuf, name_p, 0);
}

/*********************************************************************/
