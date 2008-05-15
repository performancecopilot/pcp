/*
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

/*
 * PDU for id list (PDU_PMNS_IDS)
 */
typedef struct {
    __pmPDUHdr   hdr;
    int		sts;      /* to encode status of pmns op */
    int		numids;
    pmID        idlist[1];
} idlist_t;

#ifdef PCP_DEBUG
void
__pmDumpIDList(FILE *f, int numids, const pmID idlist[])
{
  int i;
  fprintf(f, "IDlist dump: numids = %d\n", numids);
  for(i=0; i<numids; i++) {
    fprintf(f, "  PMID[%d]: 0x%08x %s\n", i, idlist[i], pmIDStr(idlist[i]));
  }/*for*/
}
#endif

/*
 * Send a PDU_PMNS_IDS across socket.
 */
int
__pmSendIDList(int fd, int mode, int numids, const pmID idlist[], int sts)
{
    idlist_t	*ip;
    int		need;
    int		j;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "__pmSendIDList\n");
	__pmDumpIDList(stderr, numids, idlist);
    }
#endif

    need = (int)(sizeof(idlist_t) + (numids-1) * sizeof(idlist[0]));

    if ((ip = (idlist_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    ip->hdr.len = need;
    ip->hdr.type = PDU_PMNS_IDS;
    ip->sts = htonl(sts);
    ip->numids = htonl(numids);
    for (j = 0; j < numids; j++) {
	ip->idlist[j] = __htonpmID(idlist[j]);
    }

    return __pmXmitPDU(fd, (__pmPDU *)ip);
}

/*
 * Decode a PDU_PMNS_IDS
 * Assumes that we have preallocated idlist prior to this call
 * (i.e. we know how many should definitely be coming over)
 * Returns 0 on success.
 */
int
__pmDecodeIDList(__pmPDU *pdubuf, int mode, 
                     int numids, pmID idlist[], int *sts)
{
    idlist_t	*idlist_pdu;
    int		j;

    if (mode == PDU_ASCII) {
	return PM_ERR_NOASCII;
    }

    idlist_pdu = (idlist_t *)pdubuf;

    *sts = ntohl(idlist_pdu->sts);
    for (j = 0; j < numids; j++) {
	idlist[j] = __ntohpmID(idlist_pdu->idlist[j]);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "__pmDecodeIDList\n");
	__pmDumpIDList(stderr, numids, idlist);
    }
#endif

    return 0;
}

/*********************************************************************/

/*
 * PDU for name list (PDU_PMNS_NAMES)
 */

#define NAME_ALLOC(len) (sizeof(__pmPDU) * (((len)-1 + sizeof(__pmPDU))/sizeof(__pmPDU)))

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
    __pmPDUHdr   hdr;
    int		nstrbytes; /* number of str bytes including null terminators */
    int 	numstatus; /* = 0 if there is no status to be encoded */
    int		numnames;
    __pmPDU      names[1]; /* list of variable length name_t or name_status_t */
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

#ifdef PCP_DEBUG
void
__pmDumpNameList(FILE *f, int numnames, char *namelist[])
{
  int i;
  fprintf(f, "namelist dump: numnames = %d\n", numnames);
  for(i=0; i<numnames; i++) {
    fprintf(f, "  name[%d]: \"%s\"\n", i, namelist[i]);
  }
}

void
__pmDumpStatusList(FILE *f, int numstatus, const int statuslist[])
{
  int i;
  fprintf(f, "statuslist dump: numstatus = %d\n", numstatus);
  for(i=0; i<numstatus; i++) {
    fprintf(f, "  status[%d]: %d\n", i, statuslist[i]);
  }
}
#endif

/*
 * Send a PDU_PMNS_NAMES across socket.
 */
int
__pmSendNameList(int fd, int mode, int numnames, char *namelist[],
		 const int statuslist[])
{
    namelist_t	*nlistp;
    int		need;
    int 	nstrbytes=0;
    int 	i;
    name_t	*nt; 
    name_status_t *nst; 

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "__pmSendNameList\n");
	__pmDumpNameList(stderr, numnames, namelist);
        if (statuslist != NULL)
	  __pmDumpStatusList(stderr, numnames, statuslist);
    }
#endif

    /* namelist_t + names rounded up to a __pmPDU boundary */
    need = sizeof(*nlistp) - sizeof(nlistp->names);
    for (i = 0; i < numnames; i++) {
	int len = (int)strlen(namelist[i]);
        nstrbytes += len+1;
        if (namelist != NULL)
	    need += NAME_ALLOC(len);
	if (statuslist == NULL) 
            need += sizeof(*nt) - sizeof(nt->name);
	else 
            need += sizeof(*nst) - sizeof(nst->name);
    }

    if ((nlistp = (namelist_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    nlistp->hdr.len = need;
    nlistp->hdr.type = PDU_PMNS_NAMES;
    nlistp->nstrbytes = htonl(nstrbytes);
    nlistp->numnames = htonl(numnames);

    if (statuslist == NULL) {
        int i, j = 0, namelen;
        nlistp->numstatus = htonl(0);
	for(i=0; i<numnames; i++) {
	    nt = (name_t*)&nlistp->names[j/sizeof(__pmPDU)];
	    namelen = (int)strlen(namelist[i]);
	    memcpy(nt->name, namelist[i], namelen);
#ifdef PCP_DEBUG
	    if ((namelen % sizeof(__pmPDU)) != 0) {
		/* for Purify */
		int	pad;
		char	*padp = nt->name + namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
#endif
	    j += sizeof(namelen) + NAME_ALLOC(namelen);
	    nt->namelen = htonl(namelen);
	}
    }
    else { /* include the status fields */
        int i, j = 0, namelen;
        nlistp->numstatus = htonl(numnames);
	for(i=0; i<numnames; i++) {
	    nst = (name_status_t*)&nlistp->names[j/sizeof(__pmPDU)];
	    nst->status = htonl(statuslist[i]);
	    namelen = (int)strlen(namelist[i]);
	    memcpy(nst->name, namelist[i], namelen);
#ifdef PCP_DEBUG
	    if ((namelen % sizeof(__pmPDU)) != 0) {
		/* for Purify */
		int	pad;
		char	*padp = nst->name + namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
#endif
	    j += sizeof(nst->status) + sizeof(namelen) +
	         NAME_ALLOC(namelen);
	    nst->namelen = htonl(namelen);
	}
    }

    return __pmXmitPDU(fd, (__pmPDU *)nlistp);
}

/*
 * Decode a PDU_PMNS_NAMES
 */
int
__pmDecodeNameList(__pmPDU *pdubuf, int mode, int *numnames, 
                  char*** namelist, int** statuslist)
{
    namelist_t	*namelist_pdu;
    char        **names;
    int		*status;
    int 	need;
    int		numstatus;

    if (mode == PDU_BINARY) {
	namelist_pdu = (namelist_t *)pdubuf;

        *namelist = NULL;
	if (statuslist)
	  *statuslist = NULL;

        *numnames = ntohl(namelist_pdu->numnames);
        numstatus = ntohl(namelist_pdu->numstatus);

        if (*numnames == 0) {
	  return 0;
	}

        /* need space for name ptrs and the name characters */
        need = *numnames * ((int)sizeof(char*)) + ntohl(namelist_pdu->nstrbytes);
        if ((names = (char**)malloc(need)) == NULL) {
          return -errno;
        }

        /* need space for status values */
	if (numstatus > 0) {
            need = numstatus * (int)sizeof(int);
            if ((status = (int*)malloc(need)) == NULL) {
	        free(names);
            	return -errno;
            }
        }

        /* copy over ptrs and characters */
	if (numstatus == 0) {
	  int i,j=0;
          char *dest = (char*)&names[*numnames];
	  name_t *np;
	  int namelen;

          for(i=0; i<*numnames; i++) {
	     np = (name_t*)&namelist_pdu->names[j/sizeof(__pmPDU)];
             names[i] = dest;
	     namelen = ntohl(np->namelen);

             memcpy(dest, np->name, namelen);
	     *(dest + namelen) = '\0';
             dest += namelen + 1; 

             j += sizeof(namelen) + NAME_ALLOC(namelen);
          }/*for*/
        }
        else { /* include the status fields */
	  int i,j=0;
          char *dest = (char*)&names[*numnames];
	  name_status_t *np;
	  int namelen;

          for(i=0; i<*numnames; i++) {
	     np = (name_status_t*)&namelist_pdu->names[j/sizeof(__pmPDU)];
             names[i] = dest;
	     namelen = ntohl(np->namelen);
	     status[i] = ntohl(np->status);

             memcpy(dest, np->name, namelen);
	     *(dest + namelen) = '\0';
             dest += namelen + 1; 

             j += sizeof(np->status) + sizeof(namelen) + 
	          NAME_ALLOC(namelen);
          }/*for*/
	}
    }
    else {
	return PM_ERR_NOASCII;
    }


#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "__pmDecodeNameList\n");
        __pmDumpNameList(stderr, *numnames, names);
	if (numstatus == 0)
	  __pmDumpStatusList(stderr, numstatus, status);
    }
#endif

    *namelist = names;
    if (numstatus > 0)
      *statuslist = status;
    return *numnames;
}


/*********************************************************************/

/*
 * name request 
 */

typedef struct {
    __pmPDUHdr   hdr;
    int		subtype; 
    int		namelen;
    char	name[sizeof(int)];
} namereq_t;

/*
 * Send a PDU_PMNS_CHILD_REQ across socket.
 */
static int
SendNameReq(int fd, int mode, const char *name, int pdu_type, int subtype)
{
    namereq_t	*nreq;
    int		need;
    int		namelen;
    int		alloc_len; /* length allocated for name */

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "SendNameReq: name=\"%s\"\n", name);
    }
#endif

    namelen = (int)strlen(name);
    alloc_len = (int)(sizeof(int)*((namelen-1 + sizeof(int))/sizeof(int)));
    need = (int)(sizeof(*nreq) - sizeof(nreq->name) + alloc_len);

    if ((nreq = (namereq_t *)__pmFindPDUBuf(need)) == NULL)
	return -errno;
    nreq->hdr.len = need;
    nreq->hdr.type = pdu_type;
    nreq->subtype = htonl(subtype);
    nreq->namelen = htonl(namelen);
    memcpy(&nreq->name[0], name, namelen);

    return __pmXmitPDU(fd, (__pmPDU *)nreq);
}

/*
 * Decode a name request
 */
static int
DecodeNameReq(__pmPDU *pdubuf, int mode, char** name_p, int *subtype)
{
    namereq_t *namereq_pdu;
    char *name;
    int namelen;

    if (mode == PDU_ASCII) {
	return PM_ERR_NOASCII;
    }

    namereq_pdu = (namereq_t *)pdubuf;

    /* only set it if you want it */
    if (subtype != NULL)
        *subtype = ntohl(namereq_pdu->subtype);

    namelen = ntohl(namereq_pdu->namelen);
    name = malloc(namelen+1);
    if (name == NULL)
      return -errno; 
    memcpy(name, namereq_pdu->name, namelen);
    name[namelen] = '\0';

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
        fprintf(stderr, "DecodeNameReq: name=\"%s\"\n", name);
    }
#endif

    *name_p = name;

    return 0;
}

/*********************************************************************/

/*
 * Send a PDU_PMNS_CHILD
 */
int
__pmSendChildReq(int fd, int mode, const char *name, int subtype)
{
  return SendNameReq(fd, mode, name, PDU_PMNS_CHILD, subtype);
}


/*
 * Decode a PDU_PMNS_CHILD
 */
int
__pmDecodeChildReq(__pmPDU *pdubuf, int mode, char** name_p, int *subtype)
{
  return DecodeNameReq(pdubuf, mode, name_p, subtype);
}

/*********************************************************************/

/*
 * Send a PDU_PMNS_TRAVERSE
 */
int
__pmSendTraversePMNSReq(int fd, int mode, const char *name)
{
  return SendNameReq(fd, mode, name, PDU_PMNS_TRAVERSE, 0);
}


/*
 * Decode a PDU_PMNS_TRAVERSE
 */
int
__pmDecodeTraversePMNSReq(__pmPDU *pdubuf, int mode, char** name_p)
{
  return DecodeNameReq(pdubuf, mode, name_p, 0);
}

/*********************************************************************/
