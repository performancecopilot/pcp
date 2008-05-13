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

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

#if defined(HAVE_CONST_LONGLONG)
#define SIGN_64_MASK 0x8000000000000000LL
#else
#define SIGN_64_MASK 0x8000000000000000
#endif

extern int	errno;

/* pmAtomValue -> string, max length is 40 bytes */
const char *
pmAtomStr(const pmAtomValue *avp, int type)
{
    int		i;
    __int32_t	*lp;
    static char	buf[60];
    pmAtomValue	av;

    /* avoid alignment problems ... avp may be unaligned! */
    memcpy((void *)&av, (void *)avp, sizeof(av));

    switch (type) {
	case PM_TYPE_32:
	    snprintf(buf, sizeof(buf), "%d", av.l);
	    break;
	case PM_TYPE_U32:
	    snprintf(buf, sizeof(buf), "%u", av.ul);
	    break;
	case PM_TYPE_64:
	    snprintf(buf, sizeof(buf), "%lld", (long long)av.ll);
	    break;
	case PM_TYPE_U64:
	    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)av.ull);
	    break;
	case PM_TYPE_FLOAT:
	    snprintf(buf, sizeof(buf), "%e", (double)av.f);
	    break;
	case PM_TYPE_DOUBLE:
	    snprintf(buf, sizeof(buf), "%e", av.d);
	    break;
	case PM_TYPE_STRING:
	    if (av.cp == NULL)
		snprintf(buf, sizeof(buf), "<null>");
	    else {
		i = (int)strlen(av.cp);
		if (i < 38)
		    snprintf(buf, sizeof(buf), "\"%s\"", av.cp);
		else
		    snprintf(buf, sizeof(buf), "\"%34.34s...\"", av.cp);
	    }
	    break;
	case PM_TYPE_AGGREGATE:
	    lp = av.vp;
	    if (lp == NULL)
		snprintf(buf, sizeof(buf), "<null>");
	    else
		snprintf(buf, sizeof(buf), "%08x %08x %08x...", lp[0], lp[1], lp[2]);
	    break;
	case PM_TYPE_NOSUPPORT:
	    snprintf(buf, sizeof(buf), "bogus value, metric Not Supported");
	    break;
	default:
	    snprintf(buf, sizeof(buf), "botched type=%d", type);
    }
    return buf;
}

/*
 * must be in agreement with ordinal values for PM_TYPE_* #defines
 */
static char *typename[] = {
    "32", "U32", "64", "U64", "FLOAT", "DOUBLE", "STRING", "AGGREGATE"
};

/* PM_TYPE_* -> string, max length is 20 bytes */
const char *
pmTypeStr(int type)
{
    static char	buf[30];
    if (type >= 0 && type < sizeof(typename)/sizeof(typename[0]))
	snprintf(buf, sizeof(buf), "%s", typename[type]);
    else if (type == PM_TYPE_NOSUPPORT)
	strcpy(buf, "Not Supported");
    else
	snprintf(buf, sizeof(buf), "botched type=%d", type);

    return buf;
}

/* scale+units -> string, max length is 60 bytes */
const char *
pmUnitsStr(const pmUnits *pu)
{
    char	*spacestr;
    char	*timestr;
    char	*countstr;
    char	*p;
    char	sbuf[20];
    char	tbuf[20];
    char	cbuf[20];
    static char	buf[60];

    buf[0] = '\0';

    if (pu->dimSpace) {
	switch (pu->scaleSpace) {
	    case PM_SPACE_BYTE:
		spacestr = "byte";
		break;
	    case PM_SPACE_KBYTE:
		spacestr = "Kbyte";
		break;
	    case PM_SPACE_MBYTE:
		spacestr = "Mbyte";
		break;
	    case PM_SPACE_GBYTE:
		spacestr = "Gbyte";
		break;
	    case PM_SPACE_TBYTE:
		spacestr = "Tbyte";
		break;
	    case PM_SPACE_PBYTE:
		spacestr = "Pbyte";
		break;
	    case PM_SPACE_EBYTE:
		spacestr = "Ebyte";
		break;
	    default:
		snprintf(sbuf, sizeof(sbuf), "space-%d", pu->scaleSpace);
		spacestr = sbuf;
		break;
	}
    }
    if (pu->dimTime) {
	switch (pu->scaleTime) {
	    case PM_TIME_NSEC:
		timestr = "nanosec";
		break;
	    case PM_TIME_USEC:
		timestr = "microsec";
		break;
	    case PM_TIME_MSEC:
		timestr = "millisec";
		break;
	    case PM_TIME_SEC:
		timestr = "sec";
		break;
	    case PM_TIME_MIN:
		timestr = "min";
		break;
	    case PM_TIME_HOUR:
		timestr = "hour";
		break;
	    default:
		snprintf(tbuf, sizeof(tbuf), "time-%d", pu->scaleTime);
		timestr = tbuf;
		break;
	}
    }
    if (pu->dimCount) {
	switch (pu->scaleCount) {
	    case 0:
		countstr = "count";
		break;
	    case 1:
		snprintf(cbuf, sizeof(cbuf), "count x 10");
		countstr = cbuf;
		break;
	    default:
		snprintf(cbuf, sizeof(cbuf), "count x 10^%d", pu->scaleCount);
		countstr = cbuf;
		break;
	}
    }

    p = buf;

    if (pu->dimSpace > 0) {
	if (pu->dimSpace == 1)
	    snprintf(p, sizeof(buf), "%s", spacestr);
	else
	    snprintf(p, sizeof(buf), "%s^%d", spacestr, pu->dimSpace);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    snprintf(p, sizeof(buf) - (p - buf), "%s", timestr);
	else
	    snprintf(p, sizeof(buf) - (p - buf), "%s^%d", timestr, pu->dimTime);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    snprintf(p, sizeof(buf) - (p - buf), "%s", countstr);
	else
	    snprintf(p, sizeof(buf) - (p - buf), "%s^%d", countstr, pu->dimCount);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	*p++ = ' ';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		snprintf(p, sizeof(buf) - (p - buf), "%s", spacestr);
	    else
		snprintf(p, sizeof(buf) - (p - buf), "%s^%d", spacestr, -pu->dimSpace);
	    while (*p) p++;
	    *p++ = ' ';
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		snprintf(p, sizeof(buf) - (p - buf), "%s", timestr);
	    else
		snprintf(p, sizeof(buf) - (p - buf), "%s^%d", timestr, -pu->dimTime);
	    while (*p) p++;
	    *p++ = ' ';
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		snprintf(p, sizeof(buf) - (p - buf), "%s", countstr);
	    else
		snprintf(p, sizeof(buf) - (p - buf), "%s^%d", countstr, -pu->dimCount);
	    while (*p) p++;
	    *p++ = ' ';
	}
    }

    if (buf[0] == '\0') {
	if (pu->scaleCount == 1)
	    snprintf(buf, sizeof(buf), "x 10");
	else if (pu->scaleCount != 0)
	    snprintf(buf, sizeof(buf), "x 10^%d", pu->scaleCount);
    }
    else {
	p--;
	*p = '\0';
    }

    return buf;
}

/* Scale conversion, based on value format, value type and scale */
int
pmConvScale(int type, const pmAtomValue *ival, const pmUnits *iunit,
	    pmAtomValue *oval, pmUnits *ounit)
{
    int			sts;
    int			k;
    __int64_t		div, mult;
    __int64_t		d, m;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, "pmConvScale: %s", pmAtomStr(ival, type));
	fprintf(stderr, " [%s]", pmUnitsStr(iunit));
    }
#endif

    if (iunit->dimSpace != ounit->dimSpace ||
	iunit->dimTime != ounit->dimTime ||
	iunit->dimCount != ounit->dimCount) {
	    sts = PM_ERR_CONV;
	    goto bad;
    }

    div = mult = 1;

    if (iunit->dimSpace) {
	d = 1;
	m = 1;
	switch (iunit->scaleSpace) {
	    case PM_SPACE_BYTE:
		d = 1024 * 1024;
		break;
	    case PM_SPACE_KBYTE:
		d = 1024;
		break;
	    case PM_SPACE_MBYTE:
		/* the canonical unit */
		break;
	    case PM_SPACE_GBYTE:
		m = 1024;
		break;
	    case PM_SPACE_TBYTE:
		m = 1024 * 1024;
		break;
	    case PM_SPACE_PBYTE:
		m = 1024 * 1024 * 1024;
		break;
	    case PM_SPACE_EBYTE:
		m = (__int64_t)1024 * 1024 * 1024 * 1024;
		break;
	    default:
		sts = PM_ERR_UNIT;
		goto bad;
	}
	switch (ounit->scaleSpace) {
	    case PM_SPACE_BYTE:
		m *= 1024 * 1024;
		break;
	    case PM_SPACE_KBYTE:
		m *= 1024;
		break;
	    case PM_SPACE_MBYTE:
		/* the canonical unit */
		break;
	    case PM_SPACE_GBYTE:
		d *= 1024;
		break;
	    case PM_SPACE_TBYTE:
		d *= 1024 * 1024;
		break;
	    case PM_SPACE_PBYTE:
		d *= 1024 * 1024 * 1024;
		break;
	    case PM_SPACE_EBYTE:
		d *= (__int64_t)1024 * 1024 * 1024 * 1024;
		break;
	    default:
		sts = PM_ERR_UNIT;
		goto bad;
	}
	if (iunit->dimSpace > 0) {
	    for (k = 0; k < iunit->dimSpace; k++) {
		div *= d;
		mult *= m;
	    }
	}
	else {
	    for (k = iunit->dimSpace; k < 0; k++) {
		mult *= d;
		div *= m;
	    }
	}
    }

    if (iunit->dimTime) {
	d = 1;
	m = 1;
	switch (iunit->scaleTime) {
	    case PM_TIME_NSEC:
		d = 1000000000;
		break;
	    case PM_TIME_USEC:
		d = 1000000;
		break;
	    case PM_TIME_MSEC:
		d = 1000;
		break;
	    case PM_TIME_SEC:
		/* the canonical unit */
		break;
	    case PM_TIME_MIN:
		m = 60;
		break;
	    case PM_TIME_HOUR:
		m = 3600;
		break;
	    default:
		sts = PM_ERR_UNIT;
		goto bad;
	}
	switch (ounit->scaleTime) {
	    case PM_TIME_NSEC:
		m *= 1000000000;
		break;
	    case PM_TIME_USEC:
		m *= 1000000;
		break;
	    case PM_TIME_MSEC:
		m *= 1000;
		break;
	    case PM_TIME_SEC:
		/* the canonical unit */
		break;
	    case PM_TIME_MIN:
		d *= 60;
		break;
	    case PM_TIME_HOUR:
		d *= 3600;
		break;
	    default:
		sts = PM_ERR_UNIT;
		goto bad;
	}
	if (iunit->dimTime > 0) {
	    for (k = 0; k < iunit->dimTime; k++) {
		div *= d;
		mult *= m;
	    }
	}
	else {
	    for (k = iunit->dimTime; k < 0; k++) {
		mult *= d;
		div *= m;
	    }
	}
    }

    if (iunit->dimCount ||
	(iunit->dimSpace == 0 && iunit->dimTime == 0)) {
	d = 1;
	m = 1;
	if (iunit->scaleCount < 0) {
	    for (k = iunit->scaleCount; k < 0; k++)
		d *= 10;
	}
	else if (iunit->scaleCount > 0) {
	    for (k = 0; k < iunit->scaleCount; k++)
		m *= 10;
	}
	if (ounit->scaleCount < 0) {
	    for (k = ounit->scaleCount; k < 0; k++)
		m *= 10;
	}
	else if (ounit->scaleCount > 0) {
	    for (k = 0; k < ounit->scaleCount; k++)
		d *= 10;
	}
	if (iunit->dimCount > 0) {
	    for (k = 0; k < iunit->dimCount; k++) {
		div *= d;
		mult *= m;
	    }
	}
	else if (iunit->dimCount < 0) {
	    for (k = iunit->dimCount; k < 0; k++) {
		mult *= d;
		div *= m;
	    }
	}
	else {
	    mult = m;
	    div = d;
	}
    }

    if (mult % div == 0) {
	mult /= div;
	div = 1;
    }

    switch (type) {
	case PM_TYPE_32:
		oval->l = (__int32_t)((ival->l * mult + div/2) / div);
		break;
	case PM_TYPE_U32:
		oval->ul = (__uint32_t)((ival->ul * mult + div/2) / div);
		break;
	case PM_TYPE_64:
		oval->ll = (ival->ll * mult + div/2) / div;
		break;
	case PM_TYPE_U64:
		oval->ull = (ival->ull * mult + div/2) / div;
		break;
	case PM_TYPE_FLOAT:
		oval->f = ival->f * ((float)mult / (float)div);
		break;
	case PM_TYPE_DOUBLE:
		oval->d = ival->d * ((double)mult / (double)div);
		break;
	default:
		sts = PM_ERR_CONV;
		goto bad;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, " -> %s", pmAtomStr(oval, type));
	fprintf(stderr, " [%s]\n", pmUnitsStr(ounit));
    }
#endif
    return 0;

bad:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, " -> Error: %s", pmErrStr(sts));
	fprintf(stderr, " [%s]\n", pmUnitsStr(ounit));
    }
#endif
    return sts;
}

/* Value extract from pmValue and type conversion */
int
pmExtractValue(int valfmt, const pmValue *ival, int itype,
			   pmAtomValue *oval, int otype)
{
    pmAtomValue	*ap;
    pmAtomValue	av;
    int		sts = 0;
    int		len;
    const char	*vp;
    static char	buf[60];

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, "pmExtractValue: ");
	vp = "???";
    }
#endif

    oval->ll = 0;
    if (valfmt == PM_VAL_INSITU) {
	av.l = ival->value.lval;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_VALUE)
	    vp = pmAtomStr(&av, itype);
#endif
	switch (itype) {

	    case PM_TYPE_32:
	    case PM_TYPE_UNKNOWN:
		switch (otype) {
		    case PM_TYPE_32:
			oval->l = av.l;
			break;
		    case PM_TYPE_U32:
			if (av.l < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t)av.l;
			break;
		    case PM_TYPE_64:
			oval->ll = (__int64_t)av.l;
			break;
		    case PM_TYPE_U64:
			if (av.l < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t)av.l;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float)av.l;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double)av.l;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_U32:
		switch (otype) {
		    case PM_TYPE_32:
			if (av.ul > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)av.ul;
			break;
		    case PM_TYPE_U32:
			oval->ul = (__uint32_t)av.ul;
			break;
		    case PM_TYPE_64:
			oval->ll = (__int64_t)av.ul;
			break;
		    case PM_TYPE_U64:
			oval->ull = (__uint64_t)av.ul;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float)av.ul;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double)av.ul;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_FLOAT:		/* old style insitu encoding */
		switch (otype) {
		    case PM_TYPE_32:
			if (av.f > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)av.f;
			break;
		    case PM_TYPE_U32:
			if (av.f > (unsigned)0xffffffff)
			    sts = PM_ERR_TRUNC;
			else if (av.f < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t)av.f;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__int64_t)0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__int64_t)0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t)av.f;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__uint64_t)0xffffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__uint64_t)0xffffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else if (av.f < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t)av.f;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = av.f;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double)av.f;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_64:
	    case PM_TYPE_U64:
	    case PM_TYPE_DOUBLE:
	    case PM_TYPE_STRING:
	    case PM_TYPE_AGGREGATE:
	    default:
		sts = PM_ERR_CONV;
	}
    }
    else if (valfmt == PM_VAL_DPTR || valfmt == PM_VAL_SPTR) {
	__int64_t	src;
	__uint64_t	usrc;
	double		dsrc;
	float		fsrc;
	switch (itype) {

	    case PM_TYPE_64:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(__int64_t) ||
		    (ival->value.pval->vtype != PM_TYPE_64 &&
		     ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		ap = (pmAtomValue *)&ival->value.pval->vbuf;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE)
		    vp = pmAtomStr(ap, itype);
#endif
		memcpy((void *)&src, (void *)ap, sizeof(src));
		switch (otype) {
		    case PM_TYPE_32:
			if (src > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)src;
			break;
		    case PM_TYPE_U32:
			if (src > (unsigned)0xffffffff)
			    sts = PM_ERR_TRUNC;
			else if (src < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t)src;
			break;
		    case PM_TYPE_64:
			oval->ll = src;
			break;
		    case PM_TYPE_U64:
			if (src < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t)src;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float)src;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double)src;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_U64:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(__uint64_t) ||
		    (ival->value.pval->vtype != PM_TYPE_U64 &&
		     ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		ap = (pmAtomValue *)&ival->value.pval->vbuf;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE)
		    vp = pmAtomStr(ap, itype);
#endif
		memcpy((void *)&usrc, (void *)ap, sizeof(usrc));
		switch (otype) {
		    case PM_TYPE_32:
			if (src > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)usrc;
			break;
		    case PM_TYPE_U32:
			if (usrc > (unsigned)0xffffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->ul = (__uint32_t)usrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (usrc > (__int64_t)0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (usrc > (__int64_t)0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t)usrc;
			break;
		    case PM_TYPE_U64:
			oval->ull = usrc;
			break;
		    case PM_TYPE_FLOAT:
#if !defined(HAVE_CAST_U64_DOUBLE)
			if (SIGN_64_MASK & usrc)
			    oval->f = (float)(__int64_t)(usrc & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
			else
			    oval->f = (float)(__int64_t)usrc;
#else
			oval->f = (float)usrc;
#endif
			break;
		    case PM_TYPE_DOUBLE:
#if !defined(HAVE_CAST_U64_DOUBLE)
			if (SIGN_64_MASK & usrc)
			    oval->d = (double)(__int64_t)(usrc & (~SIGN_64_MASK)) + (__uint64_t)SIGN_64_MASK;
			else
			    oval->d = (double)(__int64_t)usrc;
#else
			oval->d = (double)usrc;
#endif
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_DOUBLE:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(double) ||
		    (ival->value.pval->vtype != PM_TYPE_DOUBLE &&
		     ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		ap = (pmAtomValue *)&ival->value.pval->vbuf;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE)
		    vp = pmAtomStr(ap, itype);
#endif
		memcpy((void *)&dsrc, (void *)ap, sizeof(dsrc));
		switch (otype) {
		    case PM_TYPE_32:
			if (dsrc > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)dsrc;
			break;
		    case PM_TYPE_U32:
			if (dsrc > (unsigned)0xffffffff)
			    sts = PM_ERR_TRUNC;
			else if (dsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t)dsrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__int64_t)0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__int64_t)0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t)dsrc;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__uint64_t)0xffffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__uint64_t)0xffffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else if (dsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t)dsrc;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float)dsrc;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = dsrc;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_FLOAT:		/* new style pmValueBlock encoding */
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(float) ||
		    (ival->value.pval->vtype != PM_TYPE_FLOAT &&
		     ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		ap = (pmAtomValue *)&ival->value.pval->vbuf;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE)
		    vp = pmAtomStr(ap, itype);
#endif
		memcpy((void *)&fsrc, (void *)ap, sizeof(fsrc));
		switch (otype) {
		    case PM_TYPE_32:
			if (fsrc > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t)fsrc;
			break;
		    case PM_TYPE_U32:
			if (fsrc > (unsigned)0xffffffff)
			    sts = PM_ERR_TRUNC;
			else if (fsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t)fsrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__int64_t)0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__int64_t)0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t)fsrc;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f > (__uint64_t)0xffffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f > (__uint64_t)0xffffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else if (fsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t)fsrc;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = fsrc;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (float)fsrc;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_STRING:
		if (ival->value.pval->vtype != PM_TYPE_STRING &&
		    ival->value.pval->vtype != 0) {
		    sts = PM_ERR_CONV;
		    break;
		}
		len = ival->value.pval->vlen - PM_VAL_HDR_SIZE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    if (ival->value.pval->vbuf == NULL)
			vp = "<null>";
		    else {
			int		i;
			i = (int)strlen(ival->value.pval->vbuf);
			if (i < 38)
			    snprintf(buf, sizeof(buf), "\"%s\"", ival->value.pval->vbuf);
			else
			    snprintf(buf, sizeof(buf), "\"%34.34s...\"", ival->value.pval->vbuf);
			vp = buf;
		    }
		}
#endif
		if (otype != PM_TYPE_STRING) {
		    sts = PM_ERR_CONV;
		    break;
		}
		if ((oval->cp = (char *)malloc(len + 1)) == NULL) {
		    __pmNoMem("pmConvValue.string", len + 1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		memcpy(oval->cp, ival->value.pval->vbuf, len);
		oval->cp[len] = '\0';
		break;

	    case PM_TYPE_AGGREGATE:
		if (ival->value.pval->vtype != PM_TYPE_AGGREGATE &&
		    ival->value.pval->vtype != 0) {
		    sts = PM_ERR_CONV;
		    break;
		}
		len = ival->value.pval->vlen - PM_VAL_HDR_SIZE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    __int32_t	*lp;
		    lp = (__int32_t *)ival->value.pval->vbuf;
		    if (lp == NULL)
			vp = "<null>";
		    else {
			snprintf(buf, sizeof(buf), "%08x %08x %08x...", lp[0], lp[1], lp[2]);
			vp = buf;
		    }
		}
#endif
		if (otype != PM_TYPE_AGGREGATE) {
		    sts = PM_ERR_CONV;
		    break;
		}
		if ((oval->vp = (void *)malloc(len)) == NULL) {
		    __pmNoMem("pmConvValue.aggr", len, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		memcpy(oval->vp, ival->value.pval->vbuf, len);
		break;

	    case PM_TYPE_32:
	    case PM_TYPE_U32:
	    default:
		sts = PM_ERR_CONV;
	}
    }
    else
	sts = PM_ERR_CONV;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, " %s", vp);
	fprintf(stderr, " [%s]", pmTypeStr(itype));
	if (sts == 0)
	    fprintf(stderr, " -> %s", pmAtomStr(oval, otype));
	else
	    fprintf(stderr, " -> Error: %s", pmErrStr(sts));
	fprintf(stderr, " [%s]\n", pmTypeStr(otype));
    }
#endif

    return sts;
}
