/*
 * Copyright (c) 2012-2013 Red Hat.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "fault.h"
#include "internal.h"

#define LIMIT_CTXNUM	2048

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
__pmSendProfile(int fd, int from, int ctxnum, __pmProfile *instprof)
{
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p;
    size_t		need;
    __pmPDU		*pdubuf;
    int			sts;

    /* work out how much space we need and then alloc a pdu buf */
    need = sizeof(profile_t) + instprof->profile_len * sizeof(instprof_t);
    for (prof = instprof->profile, p_end = prof + instprof->profile_len;
	 prof < p_end;
	 prof++)
	need += prof->instances_len * sizeof(int);

    if ((pdubuf = __pmFindPDUBuf((int)need)) == NULL)
	return -oserror();

    p = (__pmPDU *)pdubuf;

    /* First the profile itself */
    pduProfile = (profile_t *)p;
    pduProfile->hdr.len = (int)need;
    pduProfile->hdr.type = PDU_PROFILE;
    /* 
     * note: context id may be sent twice due to protocol evolution and
     * backwards compatibility issues
     */
    pduProfile->hdr.from = from;
    pduProfile->ctxnum = htonl(ctxnum);
    pduProfile->g_state = htonl(instprof->state);
    pduProfile->numprof = htonl(instprof->profile_len);
    pduProfile->pad = 0;

    p += sizeof(profile_t) / sizeof(__pmPDU);

    if (instprof->profile_len) {
	/* Next all the profile entries (if any) in one block */
	for (prof = instprof->profile, p_end = prof + instprof->profile_len;
	     prof < p_end;
	     prof++) {
	    pduInstProf = (instprof_t *)p;
	    pduInstProf->indom = __htonpmInDom(prof->indom);
	    pduInstProf->state = htonl(prof->state);
	    pduInstProf->numinst = htonl(prof->instances_len);
	    pduInstProf->pad = 0;
	    p += sizeof(instprof_t) / sizeof(__pmPDU);
	}

	/* and then all the instances */
	for (prof = instprof->profile, p_end = prof+instprof->profile_len;
	     prof < p_end;
	     prof++) {
	    int j;

	    /* and then the instances themselves (if any) */
	    for (j = 0; j < prof->instances_len; j++, p++)
		*p = htonl(prof->instances[j]);
	}
    }
    sts = __pmXmitPDU(fd, pdubuf);
    __pmUnpinPDUBuf(pdubuf);
    return sts;
}

int
__pmDecodeProfile(__pmPDU *pdubuf, int *ctxnump, __pmProfile **resultp)
{
    __pmProfile		*instprof;
    __pmInDomProfile	*prof, *p_end;
    profile_t		*pduProfile;
    instprof_t		*pduInstProf;
    __pmPDU		*p = (__pmPDU *)pdubuf;
    char		*pdu_end;
    int			ctxnum;
    int			sts = 0;

    /* First the profile */
    pduProfile = (profile_t *)pdubuf;
    pdu_end = (char*)pdubuf + pduProfile->hdr.len;
    if (pdu_end - (char*)pdubuf < sizeof(profile_t))
	return PM_ERR_IPC;

    ctxnum = ntohl(pduProfile->ctxnum);
    if (ctxnum < 0 || ctxnum > LIMIT_CTXNUM)
	return PM_ERR_IPC;
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
    if ((instprof = (__pmProfile *)malloc(sizeof(__pmProfile))) == NULL)
	return -oserror();
    instprof->state = ntohl(pduProfile->g_state);
    instprof->profile = NULL;
    instprof->profile_len = ntohl(pduProfile->numprof);
    if (instprof->profile_len < 0) {
	sts = PM_ERR_IPC;
	goto fail;
    }

    p += sizeof(profile_t) / sizeof(__pmPDU);

    if (instprof->profile_len > 0) {
	if (instprof->profile_len >= INT_MAX / sizeof(__pmInDomProfile) ||
	    instprof->profile_len >= pduProfile->hdr.len) {
	    sts = PM_ERR_IPC;
	    goto fail;
	}
PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
	if ((instprof->profile = (__pmInDomProfile *)calloc(
	     instprof->profile_len, sizeof(__pmInDomProfile))) == NULL) {
	    sts = -oserror();
	    goto fail;
	}

	/* Next the profiles (if any) all together */
	for (prof = instprof->profile, p_end = prof + instprof->profile_len;
	     prof < p_end;
	     prof++) {
	    if ((char *)p >= pdu_end) {
		sts = PM_ERR_IPC;
		goto fail;
	    }
	    pduInstProf = (instprof_t *)p;
	    prof->indom = __ntohpmInDom(pduInstProf->indom);
	    prof->state = ntohl(pduInstProf->state);
	    prof->instances = NULL;
	    prof->instances_len = ntohl(pduInstProf->numinst);
	    p += sizeof(instprof_t) / sizeof(__pmPDU);
	}

	/* Finally, all the instances for all profiles (if any) together */
	for (prof = instprof->profile, p_end = prof+instprof->profile_len;
	     prof < p_end;
	     prof++) {
	    int j;

	    if (prof->instances_len > 0) {
		if (prof->instances_len >= INT_MAX / sizeof(int) ||
		    prof->instances_len >= pduProfile->hdr.len) {
		    sts = PM_ERR_IPC;
		    goto fail;
		}
PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
		prof->instances = (int *)calloc(prof->instances_len, sizeof(int));
		if (prof->instances == NULL) {
		    sts = -oserror();
		    goto fail;
		}
		for (j = 0; j < prof->instances_len; j++, p++) {
		    if ((char *)p >= pdu_end) {
			sts = PM_ERR_IPC;
			goto fail;
		    }
		    prof->instances[j] = ntohl(*p);
		}
	    }
	    else if (prof->instances_len < 0) {
		sts = PM_ERR_IPC;
		goto fail;
	    }
	    else {
		/*
		 * do nothing, prof->instances already NULL from initialization
		 * in earlier loop.
		 */
		;
	    }
	}
    }
    else {
	instprof->profile = NULL;
    }

    *resultp = instprof;
    *ctxnump = ctxnum;
    return 0;

fail:
    if (instprof != NULL) {
	if (instprof->profile != NULL) {
	    for (prof = instprof->profile, p_end = prof+instprof->profile_len;
		 prof < p_end;
		 prof++) {
		if (prof->instances != NULL)
		    free(prof->instances);
	    }
	    free(instprof->profile);
	}
	free(instprof);
    }
    return sts;
}
