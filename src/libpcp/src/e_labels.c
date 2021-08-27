/*
 * Copyright (c) 2012-2017,2020-2021 Red Hat.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021, Ken McDonell.  All Rights Reserved.
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
 * Thread-safe notes:
 *
 * None.
 */

#include <inttypes.h>
#include <assert.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "internal.h"

/*
 * On-Disk __pmLogLabelSet Record, Version 3
 */
typedef struct {
    __int32_t	len;		/* header */
    __int32_t	type;
    __int32_t	sec[2];		/* __pmTimestamp */
    __int32_t	nsec;
    __int32_t	labeltype;
    __int32_t	ident;
    __int32_t	nsets;
    __int32_t	data[0];	/* labelsets */
    				/* will be expanded if nsets > 0 */
} __pmExtLabelSet_v3;

/*
 * On-Disk  __pmLogLabelSet Record, Version 2
 */
typedef struct {
    __int32_t	len;		/* header */
    __int32_t	type;
    __int32_t	sec;		/* pmTimeval */
    __int32_t	usec;
    __int32_t	labeltype;
    __int32_t	ident;
    __int32_t	nsets;
    __int32_t	data[0];	/* labelsets */
    				/* will be expanded if nsets > 0 */
} __pmExtLabelSet_v2;

int
__pmLogPutLabels(__pmArchCtl *acp, unsigned int type, unsigned int ident,
		int nsets, pmLabelSet *labelsets, const __pmTimestamp * const tsp)
{
    __pmLogCtl		*lcp = acp->ac_log;
    char		*ptr;
    int			sts = 0;
    int			i, j;
    size_t		len;
    __int32_t		*lenp;
    int			inst;
    int			nlabels;
    int			jsonlen;
    int			convert;
    pmLabel		label;
    
    void		*out;

    /*
     * Common leader fields on disk (before instances) ...
     * V2: 32-bits for len, type, usec, labeltype, ident, nsets
     *     + 32 bits for sec
     * V3: 32-bits for len, type, nsec, labeltype, ident, nsets
     *     + 64 bits for sec
     */
    len = 6 * sizeof(__int32_t);
    if (__pmLogVersion(lcp) == PM_LOG_VERS03)
	len += sizeof(__int64_t);
    else if (__pmLogVersion(lcp) == PM_LOG_VERS02)
	len += sizeof(__int32_t);
    else
	return PM_ERR_LABEL;

    for (i = 0; i < nsets; i++) {
	len += sizeof(unsigned int);	/* instance identifier */
	len += sizeof(int) + labelsets[i].jsonlen; /* json */
	len += sizeof(int);		/* count or error code */
	if (labelsets[i].nlabels > 0)
	    len += (labelsets[i].nlabels * sizeof(pmLabel));
    }
    len += sizeof(__int32_t);

PM_FAULT_POINT("libpcp/" __FILE__ ":12", PM_FAULT_ALLOC);
    if (__pmLogVersion(lcp) == PM_LOG_VERS03) {
	__pmExtLabelSet_v3	*v3;
	if ((v3 = (__pmExtLabelSet_v3 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v3->len = htonl(len);
	v3->type = htonl(TYPE_LABEL);
	__pmLogPutTimestamp(tsp, &v3->sec[0]);
	v3->labeltype = htonl(type);
	v3->ident = htonl(ident);
	v3->nsets = htonl(nsets);
	out = (void *)v3;
	ptr = (char *)&v3->data;
	lenp = &v3->len;
    }
    else if (__pmLogVersion(lcp) == PM_LOG_VERS02) {
	__pmExtLabelSet_v2	*v2;
	if ((v2 = (__pmExtLabelSet_v2 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v2->len = htonl(len);
	v2->type = htonl(TYPE_LABEL_V2);
	__pmLogPutTimeval(tsp, &v2->sec);
	v2->labeltype = htonl(type);
	v2->ident = htonl(ident);
	v2->nsets = htonl(nsets);
	out = (void *)v2;
	ptr = (char *)&v2->data;
	lenp = &v2->len;
    }
    else
	return PM_ERR_LABEL;

    for (i = 0; i < nsets; i++) {
    	/* label inst */
    	inst = htonl(labelsets[i].inst);
	memmove((void *)ptr, (void *)&inst, sizeof(inst));
	ptr += sizeof(inst);

	/* label jsonlen */
	jsonlen = labelsets[i].jsonlen;
	convert = htonl(jsonlen);
	memmove((void *)ptr, (void *)&convert, sizeof(jsonlen));
	ptr += sizeof(jsonlen);

	/* label string */
	memmove((void *)ptr, (void *)labelsets[i].json, jsonlen);
	ptr += jsonlen;

	/* label nlabels */
	nlabels = labelsets[i].nlabels;
	convert = htonl(nlabels);
	memmove((void *)ptr, (void *)&convert, sizeof(nlabels));
	ptr += sizeof(nlabels);

	/* label pmLabels */
	for (j = 0; j < nlabels; j++) {
	    label = labelsets[i].labels[j];
	    __htonpmLabel(&label);
	    memmove((void *)ptr, (void *)&label, sizeof(label));
	    ptr += sizeof(label);
	}
    }

    memcpy((void *)ptr, lenp, sizeof(*lenp));

    if ((sts = __pmFwrite(out, 1, len, lcp->l_mdfp)) != len) {
	char	errmsg[PM_MAXERRMSGLEN];

	pmprintf("__pmLogPutLabels(...,type=%d,ident=%d): write failed: returned %d expecting %zd: %s\n",
		type, ident, sts, len, osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	free(out);
	return -oserror();
    }
    free(out);

    return addlabel(acp, type, ident, nsets, labelsets, tsp);
}
