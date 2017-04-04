/*
 * Copyright (c) 2014 Red Hat.
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
#include "impl.h"
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>

#if defined(HAVE_MATH_H)
#include <math.h>
#endif

#if !defined(ABS)
#define    ABS(a)          ((a) < 0 ? -(a) : (a))
#endif

#if defined(HAVE_CONST_LONGLONG)
#define SIGN_64_MASK 0x8000000000000000LL
#else
#define SIGN_64_MASK 0x8000000000000000
#endif

/*
 * pmAtomValue -> string, max length is 80 bytes
 *
 * To avoid alignment problems, avp _must_ be aligned appropriately
 * for a pmAtomValue pointer by the caller.
 */
char *
pmAtomStr_r(const pmAtomValue * avp, int type, char *buf, int buflen)
{
    int i;
    int vlen;
    char strbuf[40];

    switch (type) {
	case PM_TYPE_32:
	    snprintf(buf, buflen, "%d", avp->l);
	    break;
	case PM_TYPE_U32:
	    snprintf(buf, buflen, "%u", avp->ul);
	    break;
	case PM_TYPE_64:
	    snprintf(buf, buflen, "%" PRIi64, avp->ll);
	    break;
	case PM_TYPE_U64:
	    snprintf(buf, buflen, "%" PRIu64, avp->ull);
	    break;
	case PM_TYPE_FLOAT:
	    snprintf(buf, buflen, "%e", (double) avp->f);
	    break;
	case PM_TYPE_DOUBLE:
	    snprintf(buf, buflen, "%e", avp->d);
	    break;
	case PM_TYPE_STRING:
	    if (avp->cp == NULL)
		snprintf(buf, buflen, "<null>");
	    else {
		i = (int) strlen(avp->cp);
		if (i < 38)
		    snprintf(buf, buflen, "\"%s\"", avp->cp);
		else
		    snprintf(buf, buflen, "\"%34.34s...\"", avp->cp);
	    }
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	    if (avp->vbp == NULL) {
		snprintf(buf, buflen, "<null>");
		break;
	    }
	    vlen = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	    if (vlen == 0)
		snprintf(buf, buflen, "[type=%s len=%d]", pmTypeStr_r(avp->vbp->vtype, strbuf, sizeof(strbuf)), vlen);
	    else {
		char *cp;
		char *bp;
		snprintf(buf, buflen, "[type=%s len=%d]", pmTypeStr_r(avp->vbp->vtype, strbuf, sizeof(strbuf)), vlen);
		cp = (char *) avp->vbp->vbuf;
		for (i = 0; i < vlen && i < 12; i++) {
		    bp = &buf[strlen(buf)];
		    if ((i % 4) == 0)
			snprintf(bp, sizeof(buf) - (bp - buf), " %02x", *cp & 0xff);
		    else
			snprintf(bp, sizeof(buf) - (bp - buf), "%02x", *cp & 0xff);
		    cp++;
		}
		if (vlen > 12) {
		    bp = &buf[strlen(buf)];
		    snprintf(bp, sizeof(buf) - (bp - buf), " ...");
		}
	    }
	    break;
	case PM_TYPE_EVENT:{
	    /* have to assume alignment is OK in this case */
	    pmEventArray *eap = (pmEventArray *) avp->vbp;
	    if (eap->ea_nrecords == 1)
		snprintf(buf, buflen, "[1 event record]");
	    else
		snprintf(buf, buflen, "[%d event records]", eap->ea_nrecords);
	    break;
	}
	case PM_TYPE_HIGHRES_EVENT:{
	    /* have to assume alignment is OK in this case */
	    pmHighResEventArray *hreap = (pmHighResEventArray *) avp->vbp;
	    if (hreap->ea_nrecords == 1)
		snprintf(buf, buflen, "[1 event record]");
	    else
		snprintf(buf, buflen, "[%d event records]", hreap->ea_nrecords);
	    break;
	}
	default:
	    snprintf(buf, buflen, "Error: unexpected type: %s", pmTypeStr_r(type, strbuf, sizeof(strbuf)));
    }
    return buf;
}

/*
 * To avoid alignment problems, avp _must_ be aligned appropriately
 * for a pmAtomValue pointer by the caller.
 */
const char *
pmAtomStr(const pmAtomValue * avp, int type)
{
    static char abuf[80];
    pmAtomStr_r(avp, type, abuf, sizeof(abuf));
    return abuf;
}

/*
 * must be in agreement with ordinal values for PM_TYPE_* #defines
 */
static const char *typename[] = {
    "32", "U32", "64", "U64", "FLOAT", "DOUBLE", "STRING", "AGGREGATE", "AGGREGATE_STATIC", "EVENT", "HIGHRES_EVENT"
};

/* PM_TYPE_* -> string, max length is 20 bytes */
char *
pmTypeStr_r(int type, char *buf, int buflen)
{
    if (type >= 0 && type < sizeof(typename) / sizeof(typename[0]))
	snprintf(buf, buflen, "%s", typename[type]);
    else if (type == PM_TYPE_NOSUPPORT)
	snprintf(buf, buflen, "%s", "Not Supported");
    else if (type == PM_TYPE_UNKNOWN)
	snprintf(buf, buflen, "%s", "Unknown");
    else
	snprintf(buf, buflen, "Illegal type=%d", type);

    return buf;
}

const char *
pmTypeStr(int type)
{
    static char tbuf[20];
    pmTypeStr_r(type, tbuf, sizeof(tbuf));
    return tbuf;
}

/* scale+units -> string, max length is 60 bytes */
char *
pmUnitsStr_r(const pmUnits * pu, char *buf, int buflen)
{
    char *spacestr = NULL;
    char *timestr = NULL;
    char *countstr = NULL;
    char *p;
    char sbuf[20];
    char tbuf[20];
    char cbuf[20];

    /*
     * must be at least 60 bytes, then we don't need to pollute the code
     * below with a check every time we call snprintf() or increment p
     */
    if (buflen < 60)
	return NULL;

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
	    snprintf(p, buflen, "%s", spacestr);
	else
	    snprintf(p, buflen, "%s^%d", spacestr, pu->dimSpace);
	while (*p)
	    p++;
	*p++ = ' ';
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    snprintf(p, buflen - (p - buf), "%s", timestr);
	else
	    snprintf(p, buflen - (p - buf), "%s^%d", timestr, pu->dimTime);
	while (*p)
	    p++;
	*p++ = ' ';
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    snprintf(p, buflen - (p - buf), "%s", countstr);
	else
	    snprintf(p, buflen - (p - buf), "%s^%d", countstr, pu->dimCount);
	while (*p)
	    p++;
	*p++ = ' ';
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	*p++ = ' ';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		snprintf(p, buflen - (p - buf), "%s", spacestr);
	    else
		snprintf(p, buflen - (p - buf), "%s^%d", spacestr, -pu->dimSpace);
	    while (*p)
		p++;
	    *p++ = ' ';
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		snprintf(p, buflen - (p - buf), "%s", timestr);
	    else
		snprintf(p, buflen - (p - buf), "%s^%d", timestr, -pu->dimTime);
	    while (*p)
		p++;
	    *p++ = ' ';
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		snprintf(p, buflen - (p - buf), "%s", countstr);
	    else
		snprintf(p, buflen - (p - buf), "%s^%d", countstr, -pu->dimCount);
	    while (*p)
		p++;
	    *p++ = ' ';
	}
    }

    if (buf[0] == '\0') {
	/*
	 * dimension is all 0, but scale maybe specified ... small
	 * anomaly here as we would expect dimCount to be 1 not
	 * 0 for these cases, but support maintained for historical
	 * behaviour
	 */
	if (pu->scaleCount == 1)
	    snprintf(buf, buflen, "x 10");
	else if (pu->scaleCount != 0)
	    snprintf(buf, buflen, "x 10^%d", pu->scaleCount);
    }
    else {
	p--;
	*p = '\0';
    }

    return buf;
}

const char *
pmUnitsStr(const pmUnits * pu)
{
    static char ubuf[60];
    pmUnitsStr_r(pu, ubuf, sizeof(ubuf));
    return ubuf;
}

/* Scale conversion, based on value format, value type and scale */
int
pmConvScale(int type, const pmAtomValue * ival, const pmUnits * iunit, pmAtomValue * oval, const pmUnits * ounit)
{
    int sts;
    int k;
    __int64_t div, mult;
    __int64_t d, m;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	char strbuf[80];
	fprintf(stderr, "pmConvScale: %s", pmAtomStr_r(ival, type, strbuf, sizeof(strbuf)));
	fprintf(stderr, " [%s]", pmUnitsStr_r(iunit, strbuf, sizeof(strbuf)));
    }
#endif

    if (iunit->dimSpace != ounit->dimSpace || iunit->dimTime != ounit->dimTime || iunit->dimCount != ounit->dimCount) {
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
		m = (__int64_t) 1024 *1024 * 1024 * 1024;
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
		d *= (__int64_t) 1024 *1024 * 1024 * 1024;
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

    if (iunit->dimCount || (iunit->dimSpace == 0 && iunit->dimTime == 0)) {
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
	    oval->l = (__int32_t) ((ival->l * mult + div / 2) / div);
	    break;
	case PM_TYPE_U32:
	    oval->ul = (__uint32_t) ((ival->ul * mult + div / 2) / div);
	    break;
	case PM_TYPE_64:
	    oval->ll = (ival->ll * mult + div / 2) / div;
	    break;
	case PM_TYPE_U64:
	    oval->ull = (ival->ull * mult + div / 2) / div;
	    break;
	case PM_TYPE_FLOAT:
	    oval->f = ival->f * ((float) mult / (float) div);
	    break;
	case PM_TYPE_DOUBLE:
	    oval->d = ival->d * ((double) mult / (double) div);
	    break;
	default:
	    sts = PM_ERR_CONV;
	    goto bad;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	char strbuf[80];
	fprintf(stderr, " -> %s", pmAtomStr_r(oval, type, strbuf, sizeof(strbuf)));
	fprintf(stderr, " [%s]\n", pmUnitsStr_r(ounit, strbuf, sizeof(strbuf)));
    }
#endif
    return 0;

bad:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	char strbuf[60];
	char errmsg[PM_MAXERRMSGLEN];
	fprintf(stderr, " -> Error: %s", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	fprintf(stderr, " [%s]\n", pmUnitsStr_r(ounit, strbuf, sizeof(strbuf)));
    }
#endif
    return sts;
}

/* Value extract from pmValue and type conversion */
int
pmExtractValue(int valfmt, const pmValue * ival, int itype, pmAtomValue * oval, int otype)
{
    void *avp;
    pmAtomValue av;
    int sts = 0;
    int len;
    const char *vp;
    char buf[80];

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
	if (pmDebug & DBG_TRACE_VALUE) {
	    char strbuf[80];
	    vp = pmAtomStr_r(&av, itype, strbuf, sizeof(strbuf));
	}
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
			    oval->ul = (__uint32_t) av.l;
			break;
		    case PM_TYPE_64:
			oval->ll = (__int64_t) av.l;
			break;
		    case PM_TYPE_U64:
			if (av.l < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t) av.l;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float) av.l;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double) av.l;
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
			    oval->l = (__int32_t) av.ul;
			break;
		    case PM_TYPE_U32:
			oval->ul = (__uint32_t) av.ul;
			break;
		    case PM_TYPE_64:
			oval->ll = (__int64_t) av.ul;
			break;
		    case PM_TYPE_U64:
			oval->ull = (__uint64_t) av.ul;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float) av.ul;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double) av.ul;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

		/*
		 * Notes on conversion to FLOAT ... because of the limited
		 * precision of the mantissa, more than one integer value
		 * maps to the same floating point value ... hence the
		 * >= (float)max-int-value style of tests
		 */
	    case PM_TYPE_FLOAT:	/* old style insitu encoding */
		switch (otype) {
		    case PM_TYPE_32:
			if ((float) ABS(av.f) >= (float) 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t) av.f;
			break;
		    case PM_TYPE_U32:
			if (av.f >= (float) ((unsigned) 0xffffffff))
			    sts = PM_ERR_TRUNC;
			else if (av.f < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t) av.f;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f >= (float) 0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (av.f >= (float) 0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t) av.f;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (av.f >= (float) ((__uint64_t) 0xffffffffffffffffLL))
			    sts = PM_ERR_TRUNC;
#else
			if (av.f >= (float) ((__uint64_t) 0xffffffffffffffff))
			    sts = PM_ERR_TRUNC;
#endif
			else if (av.f < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t) av.f;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = av.f;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double) av.f;
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
	    case PM_TYPE_EVENT:
	    case PM_TYPE_HIGHRES_EVENT:
	    default:
		sts = PM_ERR_CONV;
	}
    }
    else if (valfmt == PM_VAL_DPTR || valfmt == PM_VAL_SPTR) {
	__int64_t src;
	__uint64_t usrc;
	double dsrc;
	float fsrc;
	switch (itype) {

	    case PM_TYPE_64:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(__int64_t)
		    || (ival->value.pval->vtype != PM_TYPE_64 && ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		avp = (void *) &ival->value.pval->vbuf;
		memcpy((void *) &av.ll, avp, sizeof(av.ll));
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    char strbuf[80];
		    vp = pmAtomStr_r(&av, itype, strbuf, sizeof(strbuf));
		}
#endif
		src = av.ll;
		switch (otype) {
		    case PM_TYPE_32:
			if (src > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t) src;
			break;
		    case PM_TYPE_U32:
			if (src > (unsigned) 0xffffffff)
			    sts = PM_ERR_TRUNC;
			else if (src < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t) src;
			break;
		    case PM_TYPE_64:
			oval->ll = src;
			break;
		    case PM_TYPE_U64:
			if (src < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t) src;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float) src;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (double) src;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_U64:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(__uint64_t)
		    || (ival->value.pval->vtype != PM_TYPE_U64 && ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		avp = (void *) &ival->value.pval->vbuf;
		memcpy((void *) &av.ull, avp, sizeof(av.ull));
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    char strbuf[80];
		    vp = pmAtomStr_r(&av, itype, strbuf, sizeof(strbuf));
		}
#endif
		usrc = av.ull;
		switch (otype) {
		    case PM_TYPE_32:
			if (usrc > 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t) usrc;
			break;
		    case PM_TYPE_U32:
			if (usrc > (unsigned) 0xffffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->ul = (__uint32_t) usrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (usrc > (__int64_t) 0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (usrc > (__int64_t) 0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t) usrc;
			break;
		    case PM_TYPE_U64:
			oval->ull = usrc;
			break;
		    case PM_TYPE_FLOAT:
#if !defined(HAVE_CAST_U64_DOUBLE)
			if (SIGN_64_MASK & usrc)
			    oval->f = (float) (__int64_t) (usrc & (~SIGN_64_MASK)) + (__uint64_t) SIGN_64_MASK;
			else
			    oval->f = (float) (__int64_t) usrc;
#else
			oval->f = (float) usrc;
#endif
			break;
		    case PM_TYPE_DOUBLE:
#if !defined(HAVE_CAST_U64_DOUBLE)
			if (SIGN_64_MASK & usrc)
			    oval->d = (double) (__int64_t) (usrc & (~SIGN_64_MASK)) + (__uint64_t) SIGN_64_MASK;
			else
			    oval->d = (double) (__int64_t) usrc;
#else
			oval->d = (double) usrc;
#endif
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_DOUBLE:
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(double)
		    || (ival->value.pval->vtype != PM_TYPE_DOUBLE && ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		avp = (void *) &ival->value.pval->vbuf;
		memcpy((void *) &av.d, avp, sizeof(av.d));
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    char strbuf[80];
		    vp = pmAtomStr_r(&av, itype, strbuf, sizeof(strbuf));
		}
#endif
		dsrc = av.d;
		switch (otype) {
		    case PM_TYPE_32:
			if (ABS(dsrc) >= (double) 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t) dsrc;
			break;
		    case PM_TYPE_U32:
			if (dsrc >= (double) ((unsigned) 0xffffffff))
			    sts = PM_ERR_TRUNC;
			else if (dsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t) dsrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (dsrc >= (double) 0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (dsrc >= (double) 0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t) dsrc;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (dsrc >= (double) ((__uint64_t) 0xffffffffffffffffLL))
			    sts = PM_ERR_TRUNC;
#else
			if (dsrc >= (double) ((__uint64_t) 0xffffffffffffffff))
			    sts = PM_ERR_TRUNC;
#endif
			else if (dsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t) dsrc;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = (float) dsrc;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = dsrc;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_FLOAT:	/* new style pmValueBlock encoding */
		if (ival->value.pval->vlen != PM_VAL_HDR_SIZE + sizeof(float)
		    || (ival->value.pval->vtype != PM_TYPE_FLOAT && ival->value.pval->vtype != 0)) {
		    sts = PM_ERR_CONV;
		    break;
		}
		avp = (void *) &ival->value.pval->vbuf;
		memcpy((void *) &av.f, avp, sizeof(av.f));
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    char strbuf[80];
		    vp = pmAtomStr_r(&av, itype, strbuf, sizeof(strbuf));
		}
#endif
		fsrc = av.f;
		switch (otype) {
		    case PM_TYPE_32:
			if ((float) ABS(fsrc) >= (float) 0x7fffffff)
			    sts = PM_ERR_TRUNC;
			else
			    oval->l = (__int32_t) fsrc;
			break;
		    case PM_TYPE_U32:
			if (fsrc >= (float) ((unsigned) 0xffffffff))
			    sts = PM_ERR_TRUNC;
			else if (fsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ul = (__uint32_t) fsrc;
			break;
		    case PM_TYPE_64:
#if defined(HAVE_CONST_LONGLONG)
			if (fsrc >= (float) 0x7fffffffffffffffLL)
			    sts = PM_ERR_TRUNC;
#else
			if (fsrc >= (float) 0x7fffffffffffffff)
			    sts = PM_ERR_TRUNC;
#endif
			else
			    oval->ll = (__int64_t) fsrc;
			break;
		    case PM_TYPE_U64:
#if defined(HAVE_CONST_LONGLONG)
			if (fsrc >= (float) ((__uint64_t) 0xffffffffffffffffLL))
			    sts = PM_ERR_TRUNC;
#else
			if (fsrc >= (float) ((__uint64_t) 0xffffffffffffffff))
			    sts = PM_ERR_TRUNC;
#endif
			else if (fsrc < 0)
			    sts = PM_ERR_SIGN;
			else
			    oval->ull = (__uint64_t) fsrc;
			break;
		    case PM_TYPE_FLOAT:
			oval->f = fsrc;
			break;
		    case PM_TYPE_DOUBLE:
			oval->d = (float) fsrc;
			break;
		    default:
			sts = PM_ERR_CONV;
		}
		break;

	    case PM_TYPE_STRING:
		if (ival->value.pval->vtype != PM_TYPE_STRING && ival->value.pval->vtype != 0) {
		    sts = PM_ERR_CONV;
		    break;
		}
		len = ival->value.pval->vlen - PM_VAL_HDR_SIZE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    if (ival->value.pval->vbuf[0] == '\0')
			vp = "<null>";
		    else {
			int i;
			i = (int) strlen(ival->value.pval->vbuf);
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
		if ((oval->cp = (char *) malloc(len + 1)) == NULL) {
		    __pmNoMem("pmExtractValue.string", len + 1, PM_FATAL_ERR);
		}
		memcpy(oval->cp, ival->value.pval->vbuf, len);
		oval->cp[len] = '\0';
		break;

	    case PM_TYPE_AGGREGATE:
		if (ival->value.pval->vtype != PM_TYPE_AGGREGATE && ival->value.pval->vtype != 0) {
		    sts = PM_ERR_CONV;
		    break;
		}
		len = ival->value.pval->vlen;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    int vlen;
		    int i;
		    vlen = ival->value.pval->vlen - PM_VAL_HDR_SIZE;
		    if (vlen == 0)
			snprintf(buf, sizeof(buf), "[len=%d]", vlen);
		    else {
			char *cp;
			char *bp;
			snprintf(buf, sizeof(buf), "[len=%d]", vlen);
			cp = (char *) ival->value.pval->vbuf;
			for (i = 0; i < vlen && i < 12; i++) {
			    bp = &buf[strlen(buf)];
			    if ((i % 4) == 0)
				snprintf(bp, sizeof(buf) - (bp - buf), " %02x", *cp & 0xff);
			    else
				snprintf(bp, sizeof(buf) - (bp - buf), "%02x", *cp & 0xff);
			    cp++;
			}
			if (vlen > 12) {
			    bp = &buf[strlen(buf)];
			    snprintf(bp, sizeof(buf) - (bp - buf), " ...");
			}
		    }
		    vp = buf;
		}
#endif
		if (otype != PM_TYPE_AGGREGATE) {
		    sts = PM_ERR_CONV;
		    break;
		}
		if ((oval->vbp = (pmValueBlock *) malloc(len)) == NULL) {
		    __pmNoMem("pmExtractValue.aggr", len, PM_FATAL_ERR);
		}
		memcpy(oval->vbp, ival->value.pval, len);
		break;

	    case PM_TYPE_32:
	    case PM_TYPE_U32:
	    case PM_TYPE_EVENT:
	    case PM_TYPE_HIGHRES_EVENT:
	    default:
		sts = PM_ERR_CONV;
	}
    }
    else
	sts = PM_ERR_CONV;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	char strbuf[80];
	char errmsg[PM_MAXERRMSGLEN];
	fprintf(stderr, " %s", vp);
	fprintf(stderr, " [%s]", pmTypeStr_r(itype, strbuf, sizeof(strbuf)));
	if (sts == 0)
	    fprintf(stderr, " -> %s", pmAtomStr_r(oval, otype, strbuf, sizeof(strbuf)));
	else
	    fprintf(stderr, " -> Error: %s", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	fprintf(stderr, " [%s]\n", pmTypeStr_r(otype, strbuf, sizeof(strbuf)));
    }
#endif

    return sts;
}



/* Parse a general "N $units" string into a pmUnits tuple and a multiplier. */
/* $units can be a series of SCALE-UNIT^EXPONENT, each unit dimension appearing */
/* at most once. */

/* An internal variant of pmUnits, but without the narrow bitfields. */
/* That way, we can tolerate intermediate arithmetic that goes out of */
/* range of the 4-bit bitfields. */
typedef struct pmUnitsBig
{
    int dimSpace;		/* space dimension */
    int dimTime;		/* time dimension */
    int dimCount;		/* event dimension */
    unsigned scaleSpace;	/* one of PM_SPACE_* below */
    unsigned scaleTime;		/* one of PM_TIME_* below */
    int scaleCount;		/* one of PM_COUNT_* below */
} pmUnitsBig;

static int
__pmParseUnitsStrPart(const char *str, const char *str_end, pmUnitsBig * out, double *multiplier, char **errMsg)
{
    int sts = 0;
    unsigned i;
    const char *ptr;		/* scanning along str */
    enum dimension_t
    { d_none, d_space, d_time, d_count } dimension;
    struct unit_keyword_t
    {
	const char *keyword;
	int scale;
    };
    static const struct unit_keyword_t time_keywords[] = {
	{"nanoseconds", PM_TIME_NSEC}, {"nanosecond", PM_TIME_NSEC},
	{"nanosec", PM_TIME_NSEC}, {"ns", PM_TIME_NSEC},
	{"microseconds", PM_TIME_USEC}, {"microsecond", PM_TIME_USEC},
	{"microsec", PM_TIME_USEC}, {"us", PM_TIME_USEC},
	{"milliseconds", PM_TIME_MSEC}, {"millisecond", PM_TIME_MSEC},
	{"millisec", PM_TIME_MSEC}, {"ms", PM_TIME_MSEC},
	{"seconds", PM_TIME_SEC}, {"second", PM_TIME_SEC},
	{"sec", PM_TIME_SEC},
	{"s", PM_TIME_SEC},
	{"minutes", PM_TIME_MIN}, {"minute", PM_TIME_MIN}, {"min", PM_TIME_MIN},
	{"hours", PM_TIME_HOUR}, {"hour", PM_TIME_HOUR}, {"hr", PM_TIME_HOUR},
	{"time-0", 0}, /* { "time-1", 1 }, */ {"time-2", 2}, {"time-3", 3},
	{"time-4", 4}, {"time-5", 5}, {"time-6", 6}, {"time-7", 7},
	{"time-8", 8}, {"time-9", 9}, {"time-10", 10}, {"time-11", 11},
	{"time-12", 12}, {"time-13", 13}, {"time-14", 14}, {"time-15", 15},
	{"time-1", 1},
    };
    const size_t num_time_keywords = sizeof(time_keywords) / sizeof(time_keywords[0]);
    static const struct unit_keyword_t space_keywords[] = {
	{"bytes", PM_SPACE_BYTE}, {"byte", PM_SPACE_BYTE},
	{"Kbytes", PM_SPACE_KBYTE}, {"Kbyte", PM_SPACE_KBYTE},
	{"Kilobytes", PM_SPACE_KBYTE}, {"Kilobyte", PM_SPACE_KBYTE},
	{"KB", PM_SPACE_KBYTE},
	{"Mbytes", PM_SPACE_MBYTE}, {"Mbyte", PM_SPACE_MBYTE},
	{"Megabytes", PM_SPACE_MBYTE}, {"Megabyte", PM_SPACE_MBYTE},
	{"MB", PM_SPACE_MBYTE},
	{"Gbytes", PM_SPACE_GBYTE}, {"Gbyte", PM_SPACE_GBYTE},
	{"Gigabytes", PM_SPACE_GBYTE}, {"Gigabyte", PM_SPACE_GBYTE},
	{"GB", PM_SPACE_GBYTE},
	{"Tbytes", PM_SPACE_TBYTE}, {"Tbyte", PM_SPACE_TBYTE},
	{"Terabytes", PM_SPACE_TBYTE}, {"Terabyte", PM_SPACE_TBYTE},
	{"TB", PM_SPACE_TBYTE},
	{"Pbytes", PM_SPACE_PBYTE}, {"Pbyte", PM_SPACE_PBYTE},
	{"Petabytes", PM_SPACE_PBYTE}, {"Petabyte", PM_SPACE_PBYTE},
	{"PB", PM_SPACE_PBYTE},
	{"Ebytes", PM_SPACE_EBYTE}, {"Ebyte", PM_SPACE_EBYTE},
	{"Exabytes", PM_SPACE_EBYTE}, {"Exabyte", PM_SPACE_EBYTE},
	{"EB", PM_SPACE_EBYTE},
	{"space-0", 0}, /* { "space-1", 1 }, */ {"space-2", 2}, {"space-3", 3},
	{"space-4", 4}, {"space-5", 5}, {"space-6", 6}, {"space-7", 7},
	{"space-8", 8}, {"space-9", 9}, {"space-10", 10}, {"space-11", 11},
	{"space-12", 12}, {"space-13", 13}, {"space-14", 14}, {"space-15", 15},
	{"space-1", 1},
    };
    const size_t num_space_keywords = sizeof(space_keywords) / sizeof(space_keywords[0]);
    static const struct unit_keyword_t count_keywords[] = {
	{"count x 10^-8", -8},
	{"count x 10^-7", -7},
	{"count x 10^-6", -6},
	{"count x 10^-5", -5},
	{"count x 10^-4", -4},
	{"count x 10^-3", -3},
	{"count x 10^-2", -2},
	{"count x 10^-1", -1},
	/* { "count", 0 }, { "counts", 0 }, */
	/* { "count x 10", 1 }, */
	{"count x 10^2", 2},
	{"count x 10^3", 3},
	{"count x 10^4", 4},
	{"count x 10^5", 5},
	{"count x 10^6", 6},
	{"count x 10^7", 7},
	{"count x 10", 1},
	{"counts", 0},
	{"count", 0},
	/* NB: we don't support the anomalous "x 10^SCALE" syntax for the dimCount=0 case. */
    };
    const size_t num_count_keywords = sizeof(count_keywords) / sizeof(count_keywords[0]);
    static const struct unit_keyword_t exponent_keywords[] = {
	{"^-8", -8}, {"^-7", -7}, {"^-6", -6}, {"^-5", -5},
	{"^-4", -4}, {"^-3", -3}, {"^-2", -2}, {"^-1", -1},
	{"^0", 0}, /*{ "^1", 1 }, */ {"^2", 2}, {"^3", 3},
	{"^4", 4}, {"^5", 5}, {"^6", 6}, {"^7", 7},
	/* NB: the following larger exponents are enabled by use of pmUnitsBig above. */
	/* They happen to be necessary because pmUnitsStr emits foo-dim=-8 as "/ foo^8", */
	/* so the denominator could encounter wider-than-bitfield exponents. */
	{"^8", 8}, {"^9", 9}, {"^10", 10}, {"^11", 11},
	{"^12", 12}, {"^13", 13}, {"^14", 14}, {"^15", 15},
	{"^1", 1},
    };
    const size_t num_exponent_keywords = sizeof(exponent_keywords) / sizeof(exponent_keywords[0]);

    *multiplier = 1.0;
    memset(out, 0, sizeof(*out));
    ptr = str;

    while (ptr != str_end) {	/* parse whole string */
	assert(*ptr != '\0');

	if (isspace((int)(*ptr))) {	/* skip whitespace */
	    ptr++;
	    continue;
	}

	if (*ptr == '-' || *ptr == '.' || isdigit((int)(*ptr))) {	/* possible floating-point number */
	    /* parse it with strtod(3). */
	    char *newptr;
	    errno = 0;
	    double m = strtod(ptr, &newptr);
	    if (errno || newptr == ptr || newptr > str_end) {
		sts = PM_ERR_CONV;
		/* my kingdom for asprintf, my kingdom! */
		*errMsg = strdup("invalid floating point literal");
		goto out;
	    }
	    ptr = newptr;
	    *multiplier *= m;
	    continue;
	}

	dimension = d_none;	/* classify dimension of base unit */

	/* match & skip over keyword (followed by space, ^, or EOL) */
#define streqskip(q) (((ptr+strlen(q) <= str_end) &&        \
                       (strncasecmp(ptr,q,strlen(q))==0) && \
                       ((isspace((int)(*(ptr+strlen(q))))) ||      \
                        (*(ptr+strlen(q))=='^') ||          \
                        (ptr+strlen(q)==str_end)))          \
                       ? (ptr += strlen(q), 1) : 0)

	/* parse base unit, only once per input string.  We don't support */
	/* "microsec millisec", as that would require arithmetic on the scales. */
	/* We could support "sec sec" (and turn that into sec^2) in the future. */
	if (dimension == d_none && out->dimTime == 0)
	    for (i = 0; i < num_time_keywords; i++)
		if (streqskip(time_keywords[i].keyword)) {
		    out->scaleTime = time_keywords[i].scale;
		    dimension = d_time;
		    break;
		}
	if (dimension == d_none && out->dimSpace == 0)
	    for (i = 0; i < num_space_keywords; i++)
		if (streqskip(space_keywords[i].keyword)) {
		    out->scaleSpace = space_keywords[i].scale;
		    dimension = d_space;
		    break;
		}
	if (dimension == d_none && out->dimCount == 0)
	    for (i = 0; i < num_count_keywords; i++)
		if (streqskip(count_keywords[i].keyword)) {
		    out->scaleCount = count_keywords[i].scale;
		    dimension = d_count;
		    break;
		}

	/* parse optional dimension exponent */
	switch (dimension) {
	    case d_none:
		/* my kingdom for asprintf, my kingdom! */
		*errMsg = strdup("unrecognized or duplicate base unit");
		sts = PM_ERR_CONV;
		goto out;

	    case d_time:
		if (ptr == str_end || isspace((int)(*ptr))) {
		    out->dimTime = 1;
		}
		else {
		    for (i = 0; i < num_exponent_keywords; i++)
			if (streqskip(exponent_keywords[i].keyword)) {
			    out->dimTime = exponent_keywords[i].scale;
			    break;
			}
		}
		break;

	    case d_space:
		if (ptr == str_end || isspace((int)(*ptr))) {
		    out->dimSpace = 1;
		}
		else {
		    for (i = 0; i < num_exponent_keywords; i++)
			if (streqskip(exponent_keywords[i].keyword)) {
			    out->dimSpace = exponent_keywords[i].scale;
			    break;
			}
		}
		break;

	    case d_count:
		if (ptr == str_end || isspace((int)(*ptr))) {
		    out->dimCount = 1;
		}
		else {
		    for (i = 0; i < num_exponent_keywords; i++)
			if (streqskip(exponent_keywords[i].keyword)) {
			    out->dimCount = exponent_keywords[i].scale;
			    break;
			}
		}
		break;
	}

	/* fall through to next unit^exponent bit, if any */
    }

out:
    return sts;
}



/* Parse a general "N $units / M $units" string into a pmUnits tuple and a multiplier. */
int
pmParseUnitsStr(const char *str, pmUnits * out, double *multiplier, char **errMsg)
{
    const char *slash;
    double dividend_mult, divisor_mult;
    pmUnitsBig dividend, divisor;
    int sts;
    int dim;

    assert(str);
    assert(out);
    assert(multiplier);

    memset(out, 0, sizeof(*out));

    /* Parse the dividend and divisor separately */
    slash = strchr(str, '/');
    if (slash == NULL) {
	sts = __pmParseUnitsStrPart(str, str + strlen(str), &dividend, &dividend_mult, errMsg);
	if (sts < 0)
	    goto out;
	/* empty string for nonexistent denominator */
	memset(&divisor, 0, sizeof(divisor));
	divisor_mult = 1.0;
    }
    else {
	sts = __pmParseUnitsStrPart(str, slash, &dividend, &dividend_mult, errMsg);
	if (sts < 0)
	    goto out;
	sts = __pmParseUnitsStrPart(slash + 1, str + strlen(str), &divisor, &divisor_mult, errMsg);
	if (sts < 0)
	    goto out;
    }

    /* Compute the quotient dimensionality, check for possible bitfield overflow. */
    dim = dividend.dimSpace - divisor.dimSpace;
    out->dimSpace = dim;	/* might get truncated */
    if (out->dimSpace != dim) {
	*errMsg = strdup("space dimension out of bounds");
	sts = PM_ERR_CONV;
	goto out;
    }
    dim = dividend.dimTime - divisor.dimTime;
    out->dimTime = dim;		/* might get truncated */
    if (out->dimTime != dim) {
	*errMsg = strdup("time dimension out of bounds");
	sts = PM_ERR_CONV;
	goto out;
    }
    dim = dividend.dimCount - divisor.dimCount;
    out->dimCount = dim;	/* might get truncated */
    if (out->dimCount != dim) {
	*errMsg = strdup("count dimension out of bounds");
	sts = PM_ERR_CONV;
	goto out;
    }

    /*
     * Compute the individual scales.  In theory, we have considerable
     * freedom here, because we are also outputting a multiplier.  We
     * could just set all out->scale* to 0, and accumulate the
     * compensating scaling multipliers there.  But in order to
     * fulfill the testing-oriented invariant:
     *
     * for all valid pmUnits u:
     *     pmParseUnitsStr(pmUnitsStr(u), out_u, out_multiplier) succeeds, and
     *     out_u == u, and
     *     out_multiplier == 1.0
     *
     * we need to propagate scales to some extent.  It is helpful to
     * realize that pmUnitsStr() never generates multiplier literals,
     *  nor the same dimension in the dividend and divisor.
     */

    *multiplier = divisor_mult / dividend_mult;	/* NB: note reciprocation */

    if (dividend.dimSpace == 0 && divisor.dimSpace != 0)
	out->scaleSpace = divisor.scaleSpace;
    else if (divisor.dimSpace == 0 && dividend.dimSpace != 0)
	out->scaleSpace = dividend.scaleSpace;
    else {			/* both have space dimension; must compute a scale/multiplier */
	out->scaleSpace = PM_SPACE_BYTE;
	*multiplier *= pow(pow(1024.0, (double) dividend.scaleSpace), -(double) dividend.dimSpace);
	*multiplier *= pow(pow(1024.0, (double) divisor.scaleSpace), (double) divisor.dimSpace);
	if (out->dimSpace == 0)	/* became dimensionless? */
	    out->scaleSpace = 0;
    }

    if (dividend.dimCount == 0 && divisor.dimCount != 0)
	out->scaleCount = divisor.scaleCount;
    else if (divisor.dimCount == 0 && dividend.dimCount != 0)
	out->scaleCount = dividend.scaleCount;
    else {			/* both have count dimension; must compute a scale/multiplier */
	out->scaleCount = PM_COUNT_ONE;
	*multiplier *= pow(pow(10.0, (double) dividend.scaleCount), -(double) dividend.dimCount);
	*multiplier *= pow(pow(10.0, (double) divisor.scaleCount), (double) divisor.dimCount);
	if (out->dimCount == 0)	/* became dimensionless? */
	    out->scaleCount = 0;
    }

    if (dividend.dimTime == 0 && divisor.dimTime != 0)
	out->scaleTime = divisor.scaleTime;
    else if (divisor.dimTime == 0 && dividend.dimTime != 0)
	out->scaleTime = dividend.scaleTime;
    else {			/* both have time dimension; must compute a scale/multiplier */
	out->scaleTime = PM_TIME_SEC;
	static const double time_scales[] = {[PM_TIME_NSEC] = 0.000000001,
	    [PM_TIME_USEC] = 0.000001,
	    [PM_TIME_MSEC] = 0.001,
	    [PM_TIME_SEC] = 1,
	    [PM_TIME_MIN] = 60,
	    [PM_TIME_HOUR] = 3600
	};
	/* guaranteed by __pmParseUnitsStrPart; ensure in-range array access */
	assert(dividend.scaleTime <= PM_TIME_HOUR);
	assert(divisor.scaleTime <= PM_TIME_HOUR);
	*multiplier *= pow(time_scales[dividend.scaleTime], -(double) dividend.dimTime);
	*multiplier *= pow(time_scales[divisor.scaleTime], (double) divisor.dimTime);
	if (out->dimTime == 0)	/* became dimensionless? */
	    out->scaleTime = 0;
    }

out:
    if (sts < 0) {
	memset(out, 0, sizeof(*out));	/* clear partially filled in pmUnits */
	*multiplier = 1.0;
    }
    return sts;
}
