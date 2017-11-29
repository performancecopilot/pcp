/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * Bit field typedefs for endian translations to support little endian
 * hosts.
 *
 * For a typedef __X_foo, the big endian version will be "foo" or
 * The only structures that appear here are ones that
 * (a) may be encoded within a PDU, and/or
 * (b) may appear in a PCP archive
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

#ifndef __htonpmUnits
pmUnits
__htonpmUnits(pmUnits units)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

#ifndef __ntohpmUnits
pmUnits
__ntohpmUnits(pmUnits units)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

#ifndef __htonpmUnits
void
__htonpmLabel(pmLabel * const label)
{
    label->name = htons(label->name);
    /* label->namelen is one byte */
    /* label->flags is one byte */
    label->value = htons(label->value);
    label->valuelen = htons(label->valuelen);
}
#endif

#ifndef __ntohpmLabel
void
__ntohpmLabel(pmLabel * const label)
{
    label->name = ntohs(label->name);
    /* label->namelen is one byte */
    /* label->flags is one byte */
    label->value = ntohs(label->value);
    label->valuelen = ntohs(label->valuelen);
}
#endif

#ifndef __htonpmValueBlock
static void
htonEventArray(pmValueBlock * const vb, int highres)
{
    size_t		size;
    char		*base;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    int			nrecords;
    int			nparams;
    int			vtype;
    int			vlen;
    __uint32_t		*tp;	/* points to int holding vtype/vlen */

    /* ea_type and ea_len handled via *ip below */
    if (highres) {
	pmHighResEventArray *hreap = (pmHighResEventArray *)vb;
	base = (char *)&hreap->ea_record[0];
	nrecords = hreap->ea_nrecords;
	hreap->ea_nrecords = htonl(nrecords);
    }
    else {
	pmEventArray *eap = (pmEventArray *)vb;
	base = (char *)&eap->ea_record[0];
	nrecords = eap->ea_nrecords;
	eap->ea_nrecords = htonl(nrecords);
    }

    /* walk packed event record array */
    for (r = 0; r < nrecords; r++) {
	if (highres) {
	    pmHighResEventRecord *hrerp = (pmHighResEventRecord *)base;
	    size = sizeof(hrerp->er_timestamp) + sizeof(hrerp->er_flags) +
		    sizeof(hrerp->er_nparams);
	    if (hrerp->er_flags & PM_EVENT_FLAG_MISSED)
		nparams = 0;
	    else
		nparams = hrerp->er_nparams;
	    hrerp->er_nparams = htonl(nparams);
	    hrerp->er_flags = htonl(hrerp->er_flags);
	    __htonll((char *)&hrerp->er_timestamp.tv_sec);
	    __htonll((char *)&hrerp->er_timestamp.tv_nsec);
	}
	else {
	    pmEventRecord *erp = (pmEventRecord *)base;
	    size = sizeof(erp->er_timestamp) + sizeof(erp->er_flags) +
		    sizeof(erp->er_nparams);
	    if (erp->er_flags & PM_EVENT_FLAG_MISSED)
		nparams = 0;
	    else
		nparams = erp->er_nparams;
	    erp->er_nparams = htonl(erp->er_nparams);
	    erp->er_flags = htonl(erp->er_flags);
	    erp->er_timestamp.tv_sec = htonl(erp->er_timestamp.tv_sec);
	    erp->er_timestamp.tv_usec = htonl(erp->er_timestamp.tv_usec);
	}
	base += size;

	for (p = 0; p < nparams; p++) {
	    pmEventParameter *epp = (pmEventParameter *)base;

	    epp->ep_pmid = __htonpmID(epp->ep_pmid);
	    vtype = epp->ep_type;
	    vlen = epp->ep_len;
	    tp = (__uint32_t *)&epp->ep_pmid;
	    tp++;		/* now points to ep_type/ep_len */
	    *tp = htonl(*tp);
	    tp++;		/* now points to vbuf */
	    /* convert the types we're able to ... */
	    switch (vtype) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    *tp = htonl(*tp);
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    __htonll((void *)tp);
		    break;
		case PM_TYPE_DOUBLE:
		    __htond((void *)tp);
		    break;
		case PM_TYPE_FLOAT:
		    __htonf((void *)tp);
		    break;
	    }
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(vlen);
	}
    }
}

void
__htonpmValueBlock(pmValueBlock * const vb)
{
    unsigned int	*ip = (unsigned int *)vb;

    if (vb->vtype == PM_TYPE_U64 || vb->vtype == PM_TYPE_64)
	__htonll(vb->vbuf);
    else if (vb->vtype == PM_TYPE_DOUBLE)
	__htond(vb->vbuf);
    else if (vb->vtype == PM_TYPE_FLOAT)
	__htonf(vb->vbuf);
    else if (vb->vtype == PM_TYPE_EVENT)
	htonEventArray(vb, 0);
    else if (vb->vtype == PM_TYPE_HIGHRES_EVENT)
	htonEventArray(vb, 1);

    *ip = htonl(*ip);	/* vtype/vlen */
}
#endif

#ifndef __ntohpmValueBlock
static void
ntohEventArray(pmValueBlock * const vb, int highres)
{
    char		*base;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    int			nrecords;
    int			nparams;

    /* ea_type and ea_len handled via *ip above */
    if (highres) {
	pmHighResEventArray *hreap = (pmHighResEventArray *)vb;
	base = (char *)&hreap->ea_record[0];
	nrecords = hreap->ea_nrecords = ntohl(hreap->ea_nrecords);
    }
    else {
	pmEventArray *eap = (pmEventArray *)vb;
	base = (char *)&eap->ea_record[0];
	nrecords = eap->ea_nrecords = ntohl(eap->ea_nrecords);
    }

    /* walk packed event record array */
    for (r = 0; r < nrecords; r++) {
	unsigned int flags;
	size_t size;

	if (highres) {
	    pmHighResEventRecord *hrerp = (pmHighResEventRecord *)base;
	    size = sizeof(hrerp->er_timestamp) + sizeof(hrerp->er_flags) +
		    sizeof(hrerp->er_nparams);
	    __ntohll((char *)&hrerp->er_timestamp.tv_sec);
	    __ntohll((char *)&hrerp->er_timestamp.tv_nsec);
	    nparams = hrerp->er_nparams = ntohl(hrerp->er_nparams);
	    flags = hrerp->er_flags = ntohl(hrerp->er_flags);
	}
	else {
	    pmEventRecord *erp = (pmEventRecord *)base;
	    size = sizeof(erp->er_timestamp) + sizeof(erp->er_flags) +
		    sizeof(erp->er_nparams);
	    erp->er_timestamp.tv_sec = ntohl(erp->er_timestamp.tv_sec);
	    erp->er_timestamp.tv_usec = ntohl(erp->er_timestamp.tv_usec);
	    nparams = erp->er_nparams = ntohl(erp->er_nparams);
	    flags = erp->er_flags = ntohl(erp->er_flags);
	}

	if (flags & PM_EVENT_FLAG_MISSED)
	    nparams = 0;

	base += size;
	for (p = 0; p < nparams; p++) {
	    __uint32_t		*tp;	/* points to int holding vtype/vlen */
	    pmEventParameter	*epp = (pmEventParameter *)base;

	    epp->ep_pmid = __ntohpmID(epp->ep_pmid);
	    tp = (__uint32_t *)&epp->ep_pmid;
	    tp++;		/* now points to ep_type/ep_len */
	    *tp = ntohl(*tp);
	    tp++;		/* now points to vbuf */
	    /* convert the types we're able to ... */
	    switch (epp->ep_type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    *tp = ntohl(*tp);
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    __ntohll((void *)tp);
		    break;
		case PM_TYPE_DOUBLE:
		    __ntohd((void *)tp);
		    break;
		case PM_TYPE_FLOAT:
		    __ntohf((void *)tp);
		    break;
	    }
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
}

void
__ntohpmValueBlock(pmValueBlock * const vb)
{
    unsigned int	*ip = (unsigned int *)vb;

    /* Swab the first word, which contain vtype and vlen */
    *ip = ntohl(*ip);

    switch (vb->vtype) {
    case PM_TYPE_U64:
    case PM_TYPE_64:
	__ntohll(vb->vbuf);
	break;

    case PM_TYPE_DOUBLE:
	__ntohd(vb->vbuf);
	break;

    case PM_TYPE_FLOAT:
	__ntohf(vb->vbuf);
	break;

    case PM_TYPE_EVENT:
	ntohEventArray(vb, 0);
	break;

    case PM_TYPE_HIGHRES_EVENT:
	ntohEventArray(vb, 1);
	break;
    }
}
#endif

#ifndef __htonpmPDUInfo
__pmPDUInfo
__htonpmPDUInfo(__pmPDUInfo info)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&info);
    info = *(__pmPDUInfo *)&x;

    return info;
}
#endif

#ifndef __ntohpmPDUInfo
__pmPDUInfo
__ntohpmPDUInfo(__pmPDUInfo info)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&info);
    info = *(__pmPDUInfo *)&x;

    return info;
}
#endif

#ifndef __htonpmCred
__pmCred
__htonpmCred(__pmCred cred)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&cred);
    cred = *(__pmCred *)&x;

    return cred;
}
#endif

#ifndef __ntohpmCred
__pmCred
__ntohpmCred(__pmCred cred)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&cred);
    cred = *(__pmCred *)&x;

    return cred;
}
#endif


#ifndef __htonf
void
__htonf(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 2; i++) {
	c = p[i];
	p[i] = p[3-i];
	p[3-i] = c;
    }
}
#endif

#ifndef __htonll
void
__htonll(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 4; i++) {
	c = p[i];
	p[i] = p[7-i];
	p[7-i] = c;
    }
}
#endif
