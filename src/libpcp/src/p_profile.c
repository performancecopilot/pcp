/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"

/*
 * PDU used to transmit __pmProfile prior to pmFetch (PDU_PROFILE)
 */
typedef struct {
    pmInDom	indom;
    int		state;		/* include/exclude */
    int		numinst;	/* no. of instances to follow */
    int		pad;		/* protocol backward compatibility */
} instprof_t;

typedef struct {
    __pmPDUHdr	hdr;
    int		ctxnum;
    int		g_state;	/* global include/exclude */
    int		numprof;	/* no. of elts to follow */
    int		pad;		/* protocol backward compatibility */
} profile_t;

int
__pmSendProfile(int fd, int mode, int ctxnum, __pmProfile *instprof)
{
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p;
    size_t		need;
    __pmPDU		*pdubuf;

    if (mode == PDU_BINARY) {
	/* work out how much space we need and then alloc a pdu buf */
	need = sizeof(profile_t) + instprof->profile_len * sizeof(instprof_t);
	for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++)
	    need += prof->instances_len * sizeof(int);

	if ((pdubuf = __pmFindPDUBuf((int)need)) == NULL)
	    return -errno;

	p = (__pmPDU *)pdubuf;

	/* First the profile itself */
	pduProfile = (profile_t *)p;
	pduProfile->hdr.len = (int)need;
	pduProfile->hdr.type = PDU_PROFILE;
	pduProfile->ctxnum = htonl(ctxnum);
	pduProfile->g_state = htonl(instprof->state);
	pduProfile->numprof = htonl(instprof->profile_len);
	pduProfile->pad = 0;

	p += sizeof(profile_t) / sizeof(__pmPDU);

	if (instprof->profile_len) {
	    /* Next all the profile entries (if any) in one block */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		pduInstProf = (instprof_t *)p;
		pduInstProf->indom = __htonpmInDom(prof->indom);
		pduInstProf->state = htonl(prof->state);
		pduInstProf->numinst = htonl(prof->instances_len);
		pduInstProf->pad = 0;
		p += sizeof(instprof_t) / sizeof(__pmPDU);
	    }

	    /* and then all the instances */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len;
			prof < p_end; prof++) {
		int j;
		/* and then the instances themselves (if any) */
		for (j = 0; j < prof->instances_len; j++, p++)
		    *p = htonl(prof->instances[j]);
	    }
	}

	return __pmXmitPDU(fd, pdubuf);
    }
    else {
	/* assume PDU_ASCII */
	int			i, j, sts;
	__psint_t		nbytes;
	__pmInDomProfile		*idp;
	char			*q;

	snprintf(__pmAbuf, sizeof(__pmAbuf), "PROFILE %d %d %d\n", ctxnum, instprof->state, instprof->profile_len);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
	if (sts < 0)
	    return sts;
	for (i = 0; i < instprof->profile_len; i++) {
	    idp = &instprof->profile[i];
	    snprintf(__pmAbuf, sizeof(__pmAbuf), ". %d %d %d", idp->indom, idp->state, idp->instances_len);
	    q = &__pmAbuf[strlen(__pmAbuf)];
	    for (j = 0; j < idp->instances_len; j++) {
		if (j % 10 == 9) {
		    *q++ = ' ';
		    *q++ = '\\';
		    *q++ = '\n';
		    nbytes = q - __pmAbuf;
		    sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
		    if (sts < 0)
			return sts;
		    q = __pmAbuf;
		}
		snprintf(q, sizeof(__pmAbuf) - (q - __pmAbuf), " %d", idp->instances[j]);
		while (*q) q++;
	    }
	    *q++ = '\n';
	    nbytes = q - __pmAbuf;
	    sts = __pmXmitAscii(fd, __pmAbuf, (int)nbytes);
	    if (sts < 0)
		return sts;
	}
	return 0;
    }
}

int
__pmDecodeProfile(__pmPDU *pdubuf, int mode, int *ctxnum, __pmProfile **result)
{
    __pmProfile		*instprof;
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p;

    if (mode == PDU_BINARY) {
	p = (__pmPDU *)pdubuf;

	/* First the profile */
	pduProfile = (profile_t *)p;
	*ctxnum = ntohl(pduProfile->ctxnum);
	if ((instprof = (__pmProfile *)malloc(sizeof(__pmProfile))) == NULL)
	    return -errno;
	instprof->state = ntohl(pduProfile->g_state);
	instprof->profile_len = ntohl(pduProfile->numprof);
	p += sizeof(profile_t) / sizeof(__pmPDU);

	if (instprof->profile_len > 0) {
	    if ((instprof->profile = (__pmInDomProfile *)malloc(
		instprof->profile_len * sizeof(__pmInDomProfile))) == NULL)
		return -errno;

	    /* Next the profiles (if any) all together */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len; prof < p_end; prof++) {
		pduInstProf = (instprof_t *)p;
		prof->indom = __ntohpmInDom(pduInstProf->indom);
		prof->state = ntohl(pduInstProf->state);
		prof->instances_len = ntohl(pduInstProf->numinst);
		p += sizeof(instprof_t) / sizeof(__pmPDU);
	    }

	    /* Finally, all the instances for all profiles (if any) together */
	    for (prof=instprof->profile, p_end=prof+instprof->profile_len;
			prof < p_end; prof++) {
		int j;
		prof->instances = (int *)malloc(prof->instances_len * sizeof(int));
		if (prof->instances == NULL)
		    return -errno;
		for (j = 0; j < prof->instances_len; j++, p++)
		    prof->instances[j] = ntohl(*p);
	    }
	}
	else
	    instprof->profile = NULL;

	*result = instprof;
	return 0;
    }
    else {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }
}
