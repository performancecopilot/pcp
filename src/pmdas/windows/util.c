/*
 * Utility routines for shim.exe
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "./shim.h"
#include "./pmdbg.h"

/*
 * This block of functionality is required to map counter types from
 * their Windows semantics to equivalent PCP semantics.
 */

static struct {
    int		type;
    char	*desc;
} ctypetab[] = {
    { PERF_100NSEC_MULTI_TIMER, "PERF_100NSEC_MULTI_TIMER" },
    { PERF_100NSEC_MULTI_TIMER_INV, "PERF_100NSEC_MULTI_TIMER_INV" },
    { PERF_100NSEC_TIMER, "PERF_100NSEC_TIMER" },
    { PERF_100NSEC_TIMER_INV, "PERF_100NSEC_TIMER_INV" },
    { PERF_AVERAGE_BASE, "PERF_AVERAGE_BASE" },
    { PERF_AVERAGE_BULK, "PERF_AVERAGE_BULK" },
    { PERF_AVERAGE_TIMER, "PERF_AVERAGE_TIMER" },
    { PERF_COUNTER_100NS_QUEUELEN_TYPE, "PERF_COUNTER_100NS_QUEUELEN_TYPE" },
    { PERF_COUNTER_BULK_COUNT, "PERF_COUNTER_BULK_COUNT" },
    { PERF_COUNTER_COUNTER, "PERF_COUNTER_COUNTER" },
    { PERF_COUNTER_DELTA, "PERF_COUNTER_DELTA" },
    { PERF_COUNTER_LARGE_DELTA, "PERF_COUNTER_LARGE_DELTA" },
    { PERF_COUNTER_LARGE_QUEUELEN_TYPE, "PERF_COUNTER_LARGE_QUEUELEN_TYPE" },
    { PERF_COUNTER_LARGE_RAWCOUNT, "PERF_COUNTER_LARGE_RAWCOUNT" },
    { PERF_COUNTER_LARGE_RAWCOUNT_HEX, "PERF_COUNTER_LARGE_RAWCOUNT_HEX" },
    { PERF_COUNTER_MULTI_BASE, "PERF_COUNTER_MULTI_BASE" },
    { PERF_COUNTER_MULTI_TIMER, "PERF_COUNTER_MULTI_TIMER" },
    { PERF_COUNTER_MULTI_TIMER_INV, "PERF_COUNTER_MULTI_TIMER_INV" },
    { PERF_COUNTER_NODATA, "PERF_COUNTER_NODATA" },
    { PERF_COUNTER_QUEUELEN_TYPE, "PERF_COUNTER_QUEUELEN_TYPE" },
    { PERF_COUNTER_RAWCOUNT, "PERF_COUNTER_RAWCOUNT" },
    { PERF_COUNTER_RAWCOUNT_HEX, "PERF_COUNTER_RAWCOUNT_HEX" },
    { PERF_COUNTER_TEXT, "PERF_COUNTER_TEXT" },
    { PERF_COUNTER_TIMER, "PERF_COUNTER_TIMER" },
    { PERF_COUNTER_TIMER_INV, "PERF_COUNTER_TIMER_INV" },
    { PERF_ELAPSED_TIME, "PERF_ELAPSED_TIME" },
    { PERF_LARGE_RAW_BASE, "PERF_LARGE_RAW_BASE" },
    { PERF_OBJ_TIME_TIMER, "PERF_OBJ_TIME_TIMER" },
    { PERF_PRECISION_100NS_TIMER, "PERF_PRECISION_100NS_TIMER" },
    { PERF_PRECISION_OBJECT_TIMER, "PERF_PRECISION_OBJECT_TIMER" },
    { PERF_PRECISION_SYSTEM_TIMER, "PERF_PRECISION_SYSTEM_TIMER" },
    { PERF_RAW_BASE, "PERF_RAW_BASE" },
    { PERF_RAW_FRACTION, "PERF_RAW_FRACTION" },
    { PERF_SAMPLE_BASE, "PERF_SAMPLE_BASE" },
    { PERF_SAMPLE_COUNTER, "PERF_SAMPLE_COUNTER" },
    { PERF_SAMPLE_FRACTION, "PERF_SAMPLE_FRACTION" }
};

static int num_ctypetab = sizeof(ctypetab) / sizeof(ctypetab[0]);

char *
decode_ctype(DWORD ctype)
{
    static char	unknown[20];
    int		i;

    for (i = 0; i < num_ctypetab; i++) {
	if (ctype == ctypetab[i].type) {
	    return ctypetab[i].desc;
	}
    }
    sprintf(unknown, "0x%08x unknown", (int)ctype);
    return unknown;
}

/*
 * Copied from libpcp ... it is easier to include 'em here than to figure
 * out how to link this code with libpcp!
 */

const char *
pmIDStr(pmID pmid)
{
    static char	pbuf[20];
    __pmID_int*	p = (__pmID_int*)&pmid;
    if (pmid == PM_ID_NULL)
	return "PM_ID_NULL";
    sprintf(pbuf, "%d.%d.%d", p->domain, p->cluster, p->item);
    return pbuf;
}

const char *
pmInDomStr(pmInDom indom)
{
    static char	pbuf[20];
    __pmInDom_int*	p = (__pmInDom_int*)&indom;
    if (indom == PM_INDOM_NULL)
	return "PM_INDOM_NULL";
    sprintf(pbuf, "%d.%d", p->domain, p->serial);
    return pbuf;
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
		sprintf(sbuf, "space-%d", pu->scaleSpace);
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
		sprintf(tbuf, "time-%d", pu->scaleTime);
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
		sprintf(cbuf, "count x 10");
		countstr = cbuf;
		break;
	    default:
		sprintf(cbuf, "count x 10^%d", pu->scaleCount);
		countstr = cbuf;
		break;
	}
    }

    p = buf;

    if (pu->dimSpace > 0) {
	if (pu->dimSpace == 1)
	    sprintf(p, "%s", spacestr);
	else
	    sprintf(p, "%s^%d", spacestr, pu->dimSpace);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimTime > 0) {
	if (pu->dimTime == 1)
	    sprintf(p, "%s", timestr);
	else
	    sprintf(p, "%s^%d", timestr, pu->dimTime);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimCount > 0) {
	if (pu->dimCount == 1)
	    sprintf(p, "%s", countstr);
	else
	    sprintf(p, "%s^%d", countstr, pu->dimCount);
	while (*p) p++;
	*p++ = ' ';
    }
    if (pu->dimSpace < 0 || pu->dimTime < 0 || pu->dimCount < 0) {
	*p++ = '/';
	*p++ = ' ';
	if (pu->dimSpace < 0) {
	    if (pu->dimSpace == -1)
		sprintf(p, "%s", spacestr);
	    else
		sprintf(p, "%s^%d", spacestr, -pu->dimSpace);
	    while (*p) p++;
	    *p++ = ' ';
	}
	if (pu->dimTime < 0) {
	    if (pu->dimTime == -1)
		sprintf(p, "%s", timestr);
	    else
		sprintf(p, "%s^%d", timestr, -pu->dimTime);
	    while (*p) p++;
	    *p++ = ' ';
	}
	if (pu->dimCount < 0) {
	    if (pu->dimCount == -1)
		sprintf(p, "%s", countstr);
	    else
		sprintf(p, "%s^%d", countstr, -pu->dimCount);
	    while (*p) p++;
	    *p++ = ' ';
	}
    }

    if (buf[0] == '\0') {
	if (pu->scaleCount == 1)
	    sprintf(buf, "x 10");
	else if (pu->scaleCount != 0)
	    sprintf(buf, "x 10^%d", pu->scaleCount);
    }
    else {
	p--;
	*p = '\0';
    }

    return buf;
}

int
strcasecmp(const char *p, const char *q)
{
    /* quick and dirty! */
    int		sts = 0;

    while (sts == 0 && *p && *q) {
	if (isascii(*p) && isascii(*q)) {
	    if (tolower(*p) > tolower(*q))
		sts = 1;
	    else if (tolower(*p) < tolower(*q))
		sts = -1;
	}
	if (sts == 0) {
	    p++;
	    q++;
	    if (*p == '\0') {
		if (*q)
		    sts = -1;
	    }
	    else {
		if (*q == '\0')
		    sts = 1;
	    }
	}
    }

    return sts;
}

int
__pmParseDebug(const char *spec)
{
#ifdef PCP_DEBUG
    int		val = 0;
    int		tmp;
    const char	*p;
    char	*pend;
    int		i;

    for (p = spec; *p; ) {
	tmp = (int)strtol(p, &pend, 10);
	if (tmp == -1)
	    /* special case ... -1 really means set all the bits! */
	    tmp = 0x7ffffff;
	if (*pend == '\0') {
	    val |= tmp;
	    break;
	}
	else if (*pend == ',') {
	    val |= tmp;
	    p = pend + 1;
	}
	else {
	    pend = strchr(p, ',');
	    if (pend != NULL)
		*pend = '\0';

	    if (strcasecmp(p, "ALL") == 0) {
		val |= 0x7ffffff;
		if (pend != NULL) {
		    *pend = ',';
		    p = pend + 1;
		}
		else
		    p = "";		/* force termination of outer loop */
		break;
	    }

	    for (i = 0; i < num_debug; i++) {
		if (strcasecmp(p, debug_map[i].name) == 0) {
		    val |= debug_map[i].bit;
		    if (pend != NULL) {
			*pend = ',';
			p = pend + 1;
		    }
		    else
			p = "";		/* force termination of outer loop */
		    break;
		}
	    }

	    if (i == num_debug) {
		if (pend != NULL)
		    *pend = ',';
		return PM_ERR_CONV;
	    }
	}
    }

    return val;
#else
    return PM_ERR_NYI;
#endif
}

/*
 * There has to be something like this hiding in the Windows DLLs,
 * but I cannot find it ... so roll your own.
 */
void
errmsg(void)
{
    LPVOID bufp;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&bufp,
        0,
	NULL);

    fprintf(stderr, "%s\n", bufp);

    LocalFree(bufp);
}

void
shm_dump_hdr(FILE *f, char *msg, shm_hdr_t *smp)
{
    int		i;
    int		*ip;

    fprintf(f, "[shim.exe] Dump shared memory header: %s\n", msg);
    fprintf(f, "magic    0x%8x\n", smp->magic);
    fprintf(f, "size       %8d\n", smp->size);
    fprintf(f, "nseg       %8d\n", smp->nseg);
    fprintf(f, "segment     base     nelt elt_size\n");
    for (i = 0; i < smp->nseg; i++) {
	fprintf(f, "[%5d] %8d %8d %8d\n", i, smp->segment[i].base,
		smp->segment[i].nelt, smp->segment[i].elt_size);
    }
    ip = (int *)&((char *)shm)[shm->segment[smp->nseg-1].base];
    fprintf(f, "end      0x%8x\n", *ip);
    fflush(f);
}

/*
 * Only called after initialization, so shm is valid.
 * This is the shim.exe version ... there is also a PMDA version that
 * is semantically equivalent.
 */
void
shm_remap(int newsize)
{
    int		*ip;
#ifdef PCP_DEBUG
    static int	first = 1;
#endif

    UnmapViewOfFile((LPCVOID)shm);
    CloseHandle(shm_hmap);

    shm_hmap = CreateFileMapping(
		shm_hfile,		// our file
		NULL,			// MappingAttributes ignored
		PAGE_READWRITE,		// read+write access
		0,			// assume size < 2^32-1
		newsize,		// low 32-bits of size (all of file)
		NULL);			// don't care about name of
					// mapping object
    if (shm_hmap == NULL) {
	fprintf(stderr, "shm_remap: CreateFileMapping() failed: ");
	errmsg();
	fflush(stderr);
	exit(1);
    }
    shm = (shm_hdr_t *)MapViewOfFile(
		shm_hmap,		// our map
		FILE_MAP_ALL_ACCESS,	// read+write access
		0,			// offset is start of map
		0,			// ditto
		0);			// map whole file
    if (shm == NULL) {
	fprintf(stderr, "shm_remap: MapViewOfFile() failed: ");
	errmsg();
	fflush(stderr);
	exit(1);
    }
    shm_oldsize = newsize;

#ifdef PCP_DEBUG
    if (first && (pmDebug & DBG_TRACE_APPL1)) {
	shm_dump_hdr(stderr, "intial shm_remap", shm);
	first = 0;
    }
#endif

    /*
     * Integrity checks
     */
    
    if (shm->magic != SHM_MAGIC) {
	shm_dump_hdr(stderr, "shm_remap: Error: bad magic!", shm);
	exit(1);
    }

    ip = (int *)&((char *)shm)[shm->segment[shm->nseg-1].base];
    if (*ip != SHM_MAGIC) {
	fprintf(stderr, "shm_remap: Error: bad end segment: 0x%x not 0x%x\n", *ip, SHM_MAGIC);
	shm_dump_hdr(stderr, "shm_remap", shm);
	exit(1);
    }

    /* re-map pointers into shm structs */
    shm_metrictab = (shm_metric_t *)&((char *)shm)[shm->segment[SEG_METRICS].base];
}

/*
 * Only called after initialization, so shm is valid
 * This is the shim.exe version ... there is also a PMDA version that
 * is semantically equivalent.
 *
 * new contains just the header with the desired shape elt_size and
 * nelt entries for each segment.
 *
 * Note, each segment in the shm region is only allowed to exand ...
 * contraction would make the re-shaping horribly complicated in the
 * presence of concurrent expansion.  So if the total size is unchanged,
 * none of the segments have changed.  When the total size increases, one
 * or more of the segments have increased.
 */
void
shm_reshape(shm_hdr_t *new)
{
    int		i;
    int		sts;
    int		base;
    char	*src;
    char	*dst;

    if (new->size == shm->size)
	/* do nothing */
	return;

    /*
     * compute new base offsets and check for any shrinking segments
     */
    base = SHM_ROUND(hdr_size);
    for (i = 0; i < new->nseg; i++) {
	if (new->segment[i].elt_size * new->segment[i].nelt <
	    shm->segment[i].elt_size * shm->segment[i].nelt) {
	    fprintf(stderr, "shm_reshape: Botch: segment[%d] shrank!\n", i);
	    shm_dump_hdr(stderr, "Old", shm);
	    shm_dump_hdr(stderr, "New", new);
	    exit(1);
	}
	new->segment[i].base = base;
	base = SHM_ROUND(base + new->segment[i].elt_size * new->segment[i].nelt);
    }
    new->size = base;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	shm_dump_hdr(stderr, "shm_reshape - Old", shm);
    }
#endif

    shm->size = new->size;
    shm_remap(new->size);

    /*
     * shift segments from last to first to avoid clobbering good data
     */
    for (i = new->nseg-1; i >=0; i--) {
	if (new->segment[i].base > shm->segment[i].base) {
	    src = (char *)&((char *)shm)[shm->segment[i].base];
	    dst = (char *)&((char *)shm)[new->segment[i].base];
	    /* note, may overlap so memmove() not memcpy() */
	    memmove(dst, src, new->segment[i].nelt * new->segment[i].elt_size);
	}
	else {
	    /* this and earlier ones are not moving */
	    break;
	}
    }

    /* update header */
    for (i = 0; i < shm->nseg; i++) {
	shm->segment[i].base = new->segment[i].base;
	shm->segment[i].elt_size = new->segment[i].elt_size;
	shm->segment[i].nelt = new->segment[i].nelt;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	shm_dump_hdr(stderr, "shm_reshape - New", shm);
    }
#endif
}

