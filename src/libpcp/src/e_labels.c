/*
 * Copyright (c) 2012-2017,2020-2022 Red Hat.
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

/*
 * pack a set of labels into a physical metadata record
 * - lcp required to provide archive version
 * - caller must free allocated buf[]
 */
int
__pmLogEncodeLabels(__pmLogCtl *lcp, unsigned int type, unsigned int ident,
		int nsets, pmLabelSet *labelsets, const __pmTimestamp * const tsp,
		__int32_t **buf)
{
    char		*ptr;
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
	__pmPutTimestamp(tsp, &v3->sec[0]);
	v3->labeltype = htonl(type);
	v3->ident = htonl(ident);
	v3->nsets = htonl(nsets);
	out = (void *)v3;
	ptr = (char *)&v3->data;
	lenp = &v3->len;
    }
    else {
	/* __pmLogVersion(lcp) == PM_LOG_VERS02 */
	__pmExtLabelSet_v2	*v2;
	if ((v2 = (__pmExtLabelSet_v2 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v2->len = htonl(len);
	v2->type = htonl(TYPE_LABEL_V2);
	__pmPutTimeval(tsp, &v2->sec);
	v2->labeltype = htonl(type);
	v2->ident = htonl(ident);
	v2->nsets = htonl(nsets);
	out = (void *)v2;
	ptr = (char *)&v2->data;
	lenp = &v2->len;
    }

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

    /* trailer record length */
    memcpy((void *)ptr, lenp, sizeof(*lenp));

    *buf = out;
    return 0;
}

/*
 * output the metadata record for a set of labels
 */
int
__pmLogPutLabels(__pmArchCtl *acp, unsigned int type, unsigned int ident,
		int nsets, pmLabelSet *labelsets, const __pmTimestamp * const tsp)
{
    __pmLogCtl		*lcp = acp->ac_log;
    __int32_t		*buf;
    size_t		len;
    int			sts;

    sts = __pmLogEncodeLabels(lcp, type, ident, nsets, labelsets, tsp, &buf);
    if (sts < 0)
	return sts;

    len = ntohl(buf[0]);

    sts = acp->ac_write_cb(acp, PM_LOG_VOL_META, buf, len, __FUNCTION__);
    free(buf);
    if (sts < 0)
	return sts;

    return addlabel(acp, type, ident, nsets, labelsets, tsp);
}

int
__pmLogLoadLabelSet(char *tbuf, int rlen, int rtype, __pmTimestamp *stamp,
		int *typep, int *identp, int *nsetsp, pmLabelSet **labelsetsp)
{
    pmLabelSet		*labelsets = NULL;
    int			jsonlen, nlabels, nsets, inst;
    int			i, j, k, sts;

    *nsetsp = 0;
    *labelsetsp = NULL;

    k = 0;
    if (rtype == TYPE_LABEL_V2) {
	__pmLoadTimeval((__int32_t *)&tbuf[k], stamp);
	k += 2*sizeof(__int32_t);
    }
    else {
	__pmLoadTimestamp((__int32_t *)&tbuf[k], stamp);
	k += sizeof(__uint64_t) + sizeof(__int32_t);
    }

    *typep = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(*typep);

    *identp = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(*identp);

    nsets = ntohl(*((unsigned int *)&tbuf[k]));
    k += sizeof(*nsetsp);

    if (nsets < 0 || (size_t)nsets >= LONG_MAX / sizeof(pmLabelSet)) {
	if (pmDebugOptions.logmeta)
		fprintf(stderr, "%s: illegal nsets (%d)\n",
				"__pmLogLoadLabelSet", nsets);
	return PM_ERR_LOGREC;
    }

    if (nsets > 0 &&
	(labelsets = (pmLabelSet *)calloc(nsets, sizeof(pmLabelSet))) == NULL) {
	return -oserror();
    }

    for (i = 0; i < nsets; i++) {
	inst = *((unsigned int*)&tbuf[k]);
	inst = ntohl(inst);
	k += sizeof(inst);
	labelsets[i].inst = inst;

	jsonlen = ntohl(*((unsigned int*)&tbuf[k]));
	k += sizeof(jsonlen);
	labelsets[i].jsonlen = jsonlen;

	if (jsonlen < 0 || jsonlen > PM_MAXLABELJSONLEN) {
	    if (pmDebugOptions.logmeta)
		fprintf(stderr, "%s: corrupted json in labelset. jsonlen=%d\n",
				"__pmLogLoadLabelSet", jsonlen);
	    sts = PM_ERR_LOGREC;
	    free(labelsets);
	    return sts;
	}

	if ((labelsets[i].json = (char *)malloc(jsonlen+1)) == NULL) {
	    sts = -oserror();
	    free(labelsets);
	    return sts;
	}

	memcpy((void *)labelsets[i].json, (void *)&tbuf[k], jsonlen);
	labelsets[i].json[jsonlen] = '\0';
	k += jsonlen;

	/* label nlabels */
	nlabels = ntohl(*((unsigned int *)&tbuf[k]));
	k += sizeof(nlabels);
	labelsets[i].nlabels = nlabels;

	if (nlabels > 0) { /* nlabels < 0 is an error code. skip it here */
	    if (nlabels > PM_MAXLABELS || k + nlabels * sizeof(pmLabel) > rlen) {
		/* corrupt archive metadata detected. GH #475 */
		if (pmDebugOptions.logmeta)
		    fprintf(stderr, "%s: corrupted labelset. nlabels=%d\n",
				    "__pmLogLoadLabelSet", nlabels);
		sts = PM_ERR_LOGREC;
		free(labelsets);
		return sts;
	    }

	    if ((labelsets[i].labels = (pmLabel *)calloc(nlabels, sizeof(pmLabel))) == NULL) {
		sts = -oserror();
		free(labelsets);
		return sts;
	    }

	    /* label pmLabels */
	    for (j = 0; j < nlabels; j++) {
		labelsets[i].labels[j] = *((pmLabel *)&tbuf[k]);
		__ntohpmLabel(&labelsets[i].labels[j]);
		k += sizeof(pmLabel);
	    }
	}
    }
    *nsetsp = nsets;
    *labelsetsp = labelsets;
    return 0;
}
