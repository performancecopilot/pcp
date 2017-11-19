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
 */

#include "pmapi.h"
#include "libpcp.h"
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#include <float.h>
#include <math.h>

#ifndef HAVE_STRTOLL
static __int64_t
strtoll(char *p, char **endp, int base)
{
    return (__int64_t)strtol(p, endp, base);
}
#endif

#ifndef HAVE_STRTOULL
static __uint64_t
strtoull(char *p, char **endp, int base)
{
    return (__uint64_t)strtoul(p, endp, base);
}
#endif

#define	IS_STRING	1
#define IS_INTEGER	2
#define IS_FLOAT	4
#define IS_HEX		8
#define	IS_UNKNOWN	15

int
__pmStringValue(const char *buf, pmAtomValue *avp, int type)
{
    const char	*p = buf;
    char	*endbuf;
    int		vtype = IS_UNKNOWN;
    int		seendot = 0;
    int		base;
    double	d;
    __int64_t	temp_l;
    __uint64_t	temp_ul;

    /*
     * for strtol() et al, start with optional white space, then
     * optional sign, then optional hex prefix, then stuff ...
     */
    while (*p && isspace((int)*p)) p++;
    if (*p && *p == '-') p++;

    if (*p && *p == '0' && p[1] && tolower((int)p[1]) == 'x') {
	p += 2;
    }
    else {
	vtype &= ~IS_HEX; /* hex MUST start with 0x or 0X */
    }

    /*
     * does it smell like a hex number or a floating point number?
     */
    while (*p) {
	if (!isdigit((int)*p)) {
	    vtype &= ~IS_INTEGER;
	    if (!isxdigit((int)*p) ) {
		vtype &= ~IS_HEX;
		if (*p == '.')
		    seendot++;
	    }
	}
	p++;
    }

    if (seendot != 1)
	/* more or less than one '.' and it is not a floating point number */
	vtype &= ~IS_FLOAT;

    endbuf = (char *)buf;
    base = (vtype & IS_HEX) ? 16:10;

    switch (type) {
	case PM_TYPE_32:
		temp_l = strtol(buf, &endbuf, base);
		if (oserror() != ERANGE) {
		    /*
		     * ugliness here is for cases where pmstore is compiled
		     * 64-bit (e.g. on ia64) and then strtol() may return
		     * values larger than 32-bits with no error indication
		     * ... if this is being compiled 32-bit, then the
		     * condition will be universally false, and a smart
		     * compiler may notice and warn.
		     */
#ifdef HAVE_64BIT_LONG
		    if (temp_l > 0x7fffffffLL || temp_l < (-0x7fffffffLL - 1))
			setoserror(ERANGE);
		    else 
#endif
		    {
			avp->l = (__int32_t)temp_l;
		    }
		}
		break;

	case PM_TYPE_U32:
		temp_ul = strtoul(buf, &endbuf, base);
		if (oserror() != ERANGE) {
#ifdef HAVE_64BIT_LONG
		    if (temp_ul > 0xffffffffLL)
			setoserror(ERANGE);
		    else 
#endif
		    {
			avp->ul = (__uint32_t)temp_ul;
		    }
		}
		break;

	case PM_TYPE_64:
		avp->ll = strtoll(buf, &endbuf, base);
		/* trust library to set error code to ERANGE as appropriate */
		break;

	case PM_TYPE_U64:
		/* trust library to set error code to ERANGE as appropriate */
		avp->ull = strtoull(buf, &endbuf, base);
		break;

	case PM_TYPE_FLOAT:
		if (vtype & IS_HEX) {
		    /*
		     * strtod from GNU libc would try to convert it using an
		     * algorithm we don't want used here
		     */
		    endbuf = (char *)buf;
		}
		else {
		    d = strtod(buf, &endbuf);
		    if (fabs(d) < FLT_MIN || fabs(d) > FLT_MAX) {
			setoserror(ERANGE);
		    } else {
			avp->f = (float)d;
		    }
		}
		break;

	case PM_TYPE_DOUBLE:
		if (vtype & IS_HEX) {
		    /*
		     * strtod from GNU libc would try to convert it using an
		     * algorithm we don't want used here
		     */
		    endbuf = (char *)buf;
		}
		else {
		    avp->d = strtod(buf, &endbuf);
		}
		break;

	case PM_TYPE_STRING:
		if ((avp->cp = strdup(buf)) == NULL)
		    return -ENOMEM;
		endbuf = "";
		break;

    }
    if (*endbuf != '\0')
	return PM_ERR_TYPE;
    if (oserror() == ERANGE)
	return -oserror();
    return 0;
}

int
__pmStuffValue(const pmAtomValue *avp, pmValue *vp, int type)
{
    void	*src;
    size_t	need, body;

    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vp->value.lval = avp->ul;
	    return PM_VAL_INSITU;

	case PM_TYPE_FLOAT:
	    body = sizeof(float);
	    src  = (void *)&avp->f;
	    break;

	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_DOUBLE:
	    body = sizeof(__int64_t);
	    src  = (void *)&avp->ull;
	    break;

	case PM_TYPE_AGGREGATE:
	    /*
	     * vbp field of pmAtomValue points to a dynamically allocated
	     * pmValueBlock ... the vlen and vtype fields MUST have been
	     * already set up.
	     * A new pmValueBlock header will be allocated below, so adjust
	     * the length here (PM_VAL_HDR_SIZE will be added back later).
	     */
	    body = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	    src  = avp->vbp->vbuf;
	    break;
	    
	case PM_TYPE_STRING:
	    body = strlen(avp->cp) + 1;
	    src  = (void *)avp->cp;
	    break;

	case PM_TYPE_AGGREGATE_STATIC:
	case PM_TYPE_EVENT:
	case PM_TYPE_HIGHRES_EVENT:
	    /*
	     * vbp field of pmAtomValue points to a statically allocated
	     * pmValueBlock ... the vlen and vtype fields MUST have been
	     * already set up and are not modified here
	     *
	     * DO NOT make a copy of the value in this case
	     */
	    vp->value.pval = avp->vbp;
	    return PM_VAL_SPTR;

	default:
	    return PM_ERR_TYPE;
    }
    need = body + PM_VAL_HDR_SIZE;
    vp->value.pval = (pmValueBlock *)malloc( 
	    (need < sizeof(pmValueBlock)) ? sizeof(pmValueBlock) : need);
    if (vp->value.pval == NULL)
	return -oserror();
    vp->value.pval->vlen = (int)need;
    vp->value.pval->vtype = type;
    memcpy((void *)vp->value.pval->vbuf, (void *)src, body);
    return PM_VAL_DPTR;
}
