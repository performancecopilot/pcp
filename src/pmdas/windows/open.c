/*
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Parts of this file contributed by Ken McDonell
 * (kenj At internode DoT on DoT net)
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
 */

#include "hypnotoad.h"
#include <winbase.h>

char *windows_uname;
char *windows_build;
char *windows_machine;

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

static int ctypetab_sz = sizeof(ctypetab) / sizeof(ctypetab[0]);

char *
decode_ctype(DWORD ctype)
{
    static char	unknown[20];
    int		i;

    for (i = 0; i < ctypetab_sz; i++) {
	if (ctype == ctypetab[i].type) {
	    return ctypetab[i].desc;
	}
    }
    sprintf(unknown, "0x%08x unknown", (int)ctype);
    return unknown;
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

    fprintf(stderr, "%s\n", (const char *)bufp);

    LocalFree(bufp);
}

static char *
string_append(char *name, char *suff)
{
    if (name == NULL) {
	name = (char *)strdup(suff);
    }
    else {
	name = (char *)realloc(name, strlen(name)+strlen(suff)+1);
	strcat(name, suff);
    }
    return name;
}

static char *
_semstr(int sem)
{
    static char	msg[20];
    if (sem == PM_SEM_COUNTER)
	return "COUNTER";
    else if (sem == PM_SEM_INSTANT)
	return "INSTANT";
    else if (sem == PM_SEM_DISCRETE)
	return "DISCRETE";
    else {
	sprintf(msg, "UNKNOWN! (%d)", sem);
	return msg;
    }
}

static char *
_typestr(int type)
{
    static char msg[20];
    if (type == PM_TYPE_32)
	return "PM_TYPE_32";
    else if (type == PM_TYPE_U32)
	return "PM_TYPE_U32";
    else if (type == PM_TYPE_64)
	return "PM_TYPE_64";
    else if (type == PM_TYPE_U64)
	return "PM_TYPE_U64";
    else if (type == PM_TYPE_FLOAT)
	return "PM_TYPE_FLOAT";
    else if (type == PM_TYPE_DOUBLE)
	return "PM_TYPE_DOUBLE";
    else {
	sprintf(msg, "UNKNOWN! (%d)", type);
	return msg;
    }
}

char *
_ctypestr(int ctype)
{
    if (ctype == PERF_COUNTER_COUNTER)
	return "PERF_COUNTER_COUNTER";
    else if (ctype == PERF_RAW_FRACTION)
	return "PERF_RAW_FRACTION";
    else if (ctype == PERF_COUNTER_LARGE_RAWCOUNT_HEX)
	return "PERF_COUNTER_LARGE_RAWCOUNT_HEX";
    else if (ctype == PERF_COUNTER_LARGE_RAWCOUNT)
	return "PERF_COUNTER_LARGE_RAWCOUNT";
    else if (ctype == PERF_PRECISION_100NS_TIMER)
	return "PERF_PRECISION_100NS_TIMER";
    else if (ctype == PERF_100NSEC_TIMER)
	return "PERF_100NSEC_TIMER";
    else if (ctype == PERF_COUNTER_BULK_COUNT)
	return "PERF_COUNTER_BULK_COUNT";
    else if (ctype == PERF_COUNTER_RAWCOUNT_HEX)
	return "PERF_COUNTER_RAWCOUNT_HEX";
    else if (ctype == PERF_COUNTER_RAWCOUNT)
	return "PERF_COUNTER_RAWCOUNT";
    else if (ctype == PERF_COUNTER_COUNTER)
	return "PERF_COUNTER_COUNTER";
    else
    	return "UNKNOWN";
}

/*
 * Based on documentation from ...
 * http://msdn.microsoft.com/library/default.asp?
 * 		url=/library/en-us/sysinfo/base/osversioninfoex_str.asp
 */
static void
format_uname(OSVERSIONINFOEX osv)
{
    char		tbuf[80];

    switch (osv.dwPlatformId) {
        case VER_PLATFORM_WIN32_NT:
	    if (osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0) {
		if (osv.wProductType == VER_NT_WORKSTATION)
		    windows_name = string_append(name, "Windows 7");
		else
		    windows_name = string_append(name, "Windows Server 2008 R2");
	    }
	    else if (osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0) {
		if (osv.wProductType == VER_NT_WORKSTATION)
		    windows_name = string_append(name, "Windows Vista");
		else
		    windows_name = string_append(name, "Windows Server 2008");
	    }
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 2)
		windows_name = string_append(name, "Windows Server 2003");
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 1)
		windows_name = string_append(name, "Windows XP");
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 0)
		windows_name = string_append(name, "Windows 2000");
	    else if (osv.dwMajorVersion <= 4)
		windows_name = string_append(name, "Windows NT");
	    else {
		sprintf(tbuf, "Windows Unknown (%ld.%ld)",
		    osv.dwMajorVersion, osv.dwMinorVersion); 
		windows_name = string_append(name, tbuf);
	    }

	    /* service pack and build number etc */
	    if (osv.szCSDVersion[0] != '\0') {
		windows_name = string_append(name, " ");
		windows_name = string_append(name, osv.szCSDVersion);
	    }
	    sprintf(tbuf, " Build %ld", osv.dwBuildNumber & 0xFFFF);
	    windows_build = windows_name + strlen(windows_name) + 2;
	    windows_name = string_append(name, tbuf);
	    break;

        default:
	    windows_name = "Windows - Platform Unknown";
	    windows_build = "Unknown Build";
	    break;
    }
}

void
setup_globals(void)
{
    SYSTEM_INFO		sysinfo;
    OSVERSIONINFOEX	osversion;
#if (_WIN32_WINNT >= 0x0500)
    MEMORYSTATUSEX	memstat;
#else
    MEMORYSTATUS	memstat;
#endif

    /* physical memory size (in units of bytes) */
#if (_WIN32_WINNT >= 0x0500)
    ZeroMemory(&memstat, sizeof(MEMORYSTATUSEX));
    if (!GlobalMemoryStatusEx(&memstat))
	__pmNotifyErr(LOG_ERR, "GlobalMemoryStatusEx failed");
    else
	windows_physmem = memstat.ullTotalPhys / (1024*1024);
#else
    ZeroMemory(&memstat, sizeof(MEMORYSTATUS));
    GlobalMemoryStatus(&memstat);
    windows_physmem = memstat.dwTotalPhys / (1024*1024);
#endif

    ZeroMemory(&sysinfo, sizeof(SYSTEM_INFO));
    GetSystemInfo(&sysinfo);
    windows_pagesize = sysinfo.dwPageSize;
    switch (sysinfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
	windows_machine = "x86_64";
	break;
    case PROCESSOR_ARCHITECTURE_IA64:
	windows_machine = "ia64";
	break;
    case PROCESSOR_ARCHITECTURE_INTEL:
	windows_machine = "i686";
	break;
    default:
	windows_machine = "Unknown";
	break;
    }

    ZeroMemory(&osversion, sizeof(OSVERSIONINFOEX));
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx((OSVERSIONINFO *)&osversion))
	__pmNotifyErr(LOG_ERR, "GetVersionEx failed");
    else
	format_uname(osversion);
}

static int
check_semantics(pdh_metric_t *mp, pdh_value_t *vp)
{
    int		sts = 0;
    char	*ctr_type;

    switch (mp->ctype) {
	/*
	 * Pdh metric sematics ... from WinPerf.h
	 *
	 * SIZE
	 *	DWORD		32-bit
	 *	LARGE		64-bit
	 *	ZERO		no support here
	 *	VARIABLE_LEN	no support here
	 *
	 * TYPE
	 *	NUMBER		PM_SEM_INSTANT
	 *	    HEX		display in hex (no support here)
	 *	    DECIMAL	display as decimal
	 *	    DEC_1000	display as value / 1000
	 *			(no support here)
	 *	COUNTER		PM_SEM_COUNTER
	 *	    VALUE	display value (no support here)
	 *	    RATE	time rate converted
	 *	    FRACTION	divide value by BASE
	 *	    BASE	used for FRACTION
	 *	    ELAPSED	subtract from current time
	 *			(no support here)
	 *	    QUEUELEN	magic internal queuelen() routines
	 *			(you're joking, right?)
	 *	    HISTOGRAM	counter begins or ends a histo (?)
	 *			(definitely no support here)
	 *	    PRECISION	divide counter by private clock (?)
	 *			(definitely no support here)
	 *	TEXT		no support here
	 *	ZERO		no support here
	 */

	/*
	 * Known 32-bit counters
	 */
	case PERF_COUNTER_COUNTER:
	    /* 32-bit PM_SEM_COUNTER */
	    if (mp->desc.type != PM_TYPE_32 && mp->desc.type != PM_TYPE_U32) {
		__pmNotifyErr(LOG_ERR, "windows_open: PERF_COUNTER_COUNTER: "
			"metric %s: rewrite type from %s to PM_TYPE_U32\n",
			pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U32;
	    }
	    if (mp->desc.sem != PM_SEM_COUNTER) {
		__pmNotifyErr(LOG_ERR, "windows_open: PERF_COUNTER_COUNTER: "
			"metric %s: semantics %s (expected %s)\n",
			pmIDStr(mp->desc.pmid), _semstr(mp->desc.sem),
			_semstr(PM_SEM_COUNTER));
	    }
	    break;

	case PERF_COUNTER_RAWCOUNT:
	case PERF_COUNTER_RAWCOUNT_HEX:
	    if (mp->ctype == PERF_COUNTER_RAWCOUNT)
		ctr_type = "PERF_COUNTER_RAWCOUNT";
	    else
		ctr_type = "PERF_COUNTER_RAWCOUNT_HEX";
	    /* 32-bit PM_SEM_INSTANT or PM_SEM_DISCRETE */
	    if (mp->desc.type != PM_TYPE_32 && mp->desc.type != PM_TYPE_U32) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s: metric %s: "
					"rewrite type from %s to PM_TYPE_U32\n",
					ctr_type, pmIDStr(mp->desc.pmid),
					_typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U32;
	    }
	    break;

	/*
	 * Known 64-bit counters
	 */
	case PERF_COUNTER_BULK_COUNT:
	    /* 64-bit PM_SEM_COUNTER */
	    if (mp->desc.type != PM_TYPE_64 && mp->desc.type != PM_TYPE_U64) {
		__pmNotifyErr(LOG_ERR, "windows_open: PERF_COUNTER_BULK_COUNT:"
			" metric %s: rewrite type from %s to PM_TYPE_U64\n",
			pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U64;
	    }
	    if (mp->desc.sem != PM_SEM_COUNTER) {
		__pmNotifyErr(LOG_ERR, "windows_open: PERF_COUNTER_BULK_COUNT:"
			" metric %s: semantics %s (expected %s)\n",
			pmIDStr(mp->desc.pmid), _semstr(mp->desc.sem),
			_semstr(PM_SEM_COUNTER));
		mp->desc.sem = PM_SEM_COUNTER;
	    }
	    break;

	case PERF_100NSEC_TIMER:
	case PERF_PRECISION_100NS_TIMER:
	    if (mp->ctype == PERF_100NSEC_TIMER)
		ctr_type = "PERF_100NSEC_TIMER";
	    else
		ctr_type = "PERF_PRECISION_100NS_TIMER";
	    /*
	     * 64-bit PM_SEM_COUNTER, units are 100's of nanosecs,
	     * we shall export 'em as microseconds
	     */
	    if (mp->desc.type != PM_TYPE_64 && mp->desc.type != PM_TYPE_U64) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s: "
			"metric %s: rewrite type from %s to PM_TYPE_U64\n",
		    ctr_type, pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U64;
	    }
	    if (mp->desc.sem != PM_SEM_COUNTER) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s: "
			"metric %s: semantics %s (expected %s)\n",
		    ctr_type, pmIDStr(mp->desc.pmid), _semstr(mp->desc.sem),
		    _semstr(PM_SEM_COUNTER));
		mp->desc.sem = PM_SEM_COUNTER;
	    }
	    if (mp->desc.units.dimSpace != 0 ||
		mp->desc.units.dimTime != 1 ||
		mp->desc.units.dimCount != 0 ||
		mp->desc.units.scaleTime != PM_TIME_USEC) {
		pmUnits units = mp->desc.units;
		mp->desc.units.dimSpace = mp->desc.units.dimCount = 0;
		mp->desc.units.scaleSpace = mp->desc.units.scaleCount = 0;
		mp->desc.units.dimTime = 1;
		mp->desc.units.scaleTime = PM_TIME_USEC;
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s: "
			"metric %s: rewrite dimension and scale from %s to %s",
		    ctr_type, pmIDStr(mp->desc.pmid), pmUnitsStr(&units),
		    pmUnitsStr(&mp->desc.units));
	    }
	    break;

	case PERF_COUNTER_LARGE_RAWCOUNT:
	case PERF_COUNTER_LARGE_RAWCOUNT_HEX:
	    /* 64-bit PM_SEM_INSTANT or PM_SEM_DISCRETE */
	    if (mp->desc.type != PM_TYPE_64 &&
		mp->desc.type != PM_TYPE_U64) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: "
				"PERF_COUNTER_LARGE_RAWCOUNT: metric %s: "
				"rewrite type from %s to PM_TYPE_U64\n",
		    pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U64;
	    }
	    break;

	case PERF_RAW_FRACTION:	/* Float PM_SEM_INSTANT or PM_SEM_DISCRETE */
	    if (mp->desc.type != PM_TYPE_FLOAT) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: "
				"PERF_RAW_FRACTION: metric %s: "
				"rewrite type from %s to PM_TYPE_FLOAT\n",
		    pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_FLOAT;
	    }
	    break;

	case PERF_AVERAGE_BULK:
	case PERF_AVERAGE_TIMER:
	    /* 64-bit PM_SEM_INSTANT or PM_SEM_DISCRETE */
	    if (mp->desc.sem != PM_SEM_INSTANT && mp->desc.sem != PM_SEM_DISCRETE) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s: "
				       "metric %s: semantics %s (expected %s)\n",
					ctr_type, pmIDStr(mp->desc.pmid),
					_semstr(mp->desc.sem), _semstr(PM_SEM_INSTANT));
		mp->desc.sem = PM_SEM_INSTANT;
	    }
	    if (mp->desc.type != PM_TYPE_64 && mp->desc.type != PM_TYPE_U64) {
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: %s "
			"metric %s: rewrite type from %s to PM_TYPE_U64\n",
			ctr_type, pmIDStr(mp->desc.pmid), _typestr(mp->desc.type));
		mp->desc.type = PM_TYPE_U64;
	    }
	    break;

	default:
	    sts = -1;
    }
    mp->flags |= M_EXPANDED;
    return sts;
}

int
windows_check_metric(pdh_metric_t *pmp)
{
    int			i, id;
    size_t		size;
    PDH_STATUS  	pdhsts;
    static LPSTR	pattern = NULL;
    static DWORD	pattern_sz = 0;
    static LPSTR	info = NULL;
    static DWORD	info_sz = 0;
    static DWORD	result_sz;
    PDH_COUNTER_INFO_A	*infop;
    LPTSTR      	p;

    for (i = 0; i < pmp->num_vals; i++)
	PdhRemoveCounter(pmp->vals[i].hdl);
    if (pmp->num_vals)
	free(pmp->vals);
    pmp->num_vals = 0;
    pmp->flags &= ~(M_EXPANDED|M_NOVALUES);

    result_sz = pattern_sz;
    pdhsts = PdhExpandCounterPathA(pmp->pat, pattern, &result_sz);
    if (pdhsts == PDH_MORE_DATA) {
	result_sz++;		// not sure if this is necessary?
	pattern_sz = result_sz;
	if ((pattern = (LPSTR)realloc(pattern, pattern_sz)) == NULL) {
	    __pmNotifyErr(LOG_ERR, "windows_open: PdhExpandCounterPathA "
				   "realloc (%d) failed @ metric %s: ",
				(int)pattern_sz, pmIDStr(pmp->desc.pmid));
	    errmsg();
	    return -1;
	}
	pdhsts = PdhExpandCounterPathA(pmp->pat, pattern, &result_sz);
    }
    if (pdhsts != ERROR_SUCCESS) {
	if (pmp->pat[0] == '\0') {
	    /*
	     * Empty path string.  Used to denote metrics that are
	     * derived and do not have an explicit path or retrieval
	     * need, other than to make sure the qid value will
	     * force the corresponding query to be run before a PCP
	     * fetch ... do nothing here
	     */
	    ;
	}
	else if (pmp->flags & M_OPTIONAL) {
	    pmp->flags |= M_NOVALUES;
	    return 0;
	}
	else {
	    __pmNotifyErr(LOG_ERR, "windows_open: PdhExpandCounterPathA "
			"failed @ metric pmid=%s pattern=\"%s\": %s\n",
			pmIDStr(pmp->desc.pmid), pmp->pat, pdherrstr(pdhsts));
	}
	pmp->flags |= M_NOVALUES;
	return -1;
    }

    /*
     * PdhExpandCounterPathA is apparently busted ... the length
     * returned includes one byte _after_ the last NULL byte
     * string terminator, but the final byte is apparently
     * not being set ... force the issue
     */
    pattern[result_sz-1] = '\0';
    for (p = pattern; *p; p += lstrlen(p) + 1) {
	pdh_value_t	*pvp;

	pmp->num_vals++;
	size = pmp->num_vals * sizeof(pdh_value_t);
	if ((pmp->vals = (pdh_value_t *)realloc(pmp->vals, size)) == NULL) {
	    __pmNotifyErr(LOG_ERR, "windows_open: Error: values realloc "
				   "(%d x %d) failed @ metric %s [%s]: ",
				pmp->num_vals, sizeof(pdh_value_t),
				pmIDStr(pmp->desc.pmid), p);
	    errmsg();
	    return -1;
	}
	pvp = &pmp->vals[pmp->num_vals-1];
	if (pmp->desc.indom == PM_INDOM_NULL) {
	    /* singular instance */
	    pvp->inst = PM_IN_NULL;
	    if (pmp->num_vals > 1) {
		char 	*q;
		int	k;

		/*
		 * report only once per pattern
		 */
		__pmNotifyErr(LOG_ERR, "windows_open: Warning: singular "
				"metric %s has more than one instance ...\n",
				pmIDStr(pmp->desc.pmid));
		fprintf(stderr, "  pattern: \"%s\"\n", pmp->pat);
		for (k = 0, q = pattern; *q; q += lstrlen(q) + 1, k++)
		    fprintf(stderr, "  match[%d]: \"%s\"\n", k, q);
		fprintf(stderr, "... skip this counter\n");

		/* next realloc() will be a NOP */
		pmp->num_vals--;

		/* no more we can do here, onto next metric-pattern */
		break;
	    }
	}
	else {
	    /*
	     * if metric has instance domain, parse pattern using
	     * indom type to extract instance name and number, and
	     * add into indom cache data structures as needed.
	     */
	    if ((pvp->inst = windows_check_instance(p, pmp)) < 0) {
		/*
		 * error reported in windows_check_instance() ...
		 * we cannot return any values for this instance if
		 * we don't recognize the name ... skip this one,
		 * the next realloc() (if any) will be a NOP
		 */
		pmp->num_vals--;

		/* move onto next instance */
		continue;
	    }
	}

	/*
	 * Note.  Passing inst down here does not help much, as
	 * I don't think we're ever going to navigate back from a PDH
	 * counter into our data structures, and even if we do,
	 * the instance id is not going to be unique enough.
	 */
	id = pmp->qid;
	pdhsts = PdhAddCounterA(querydesc[id].hdl, p, pvp->inst, &pvp->hdl);
	if (pdhsts != ERROR_SUCCESS) {
	    __pmNotifyErr(LOG_ERR, "windows_open: Warning: PdhAddCounterA "
				"@ pmid=%s inst=%d pat=\"%s\" hdl=%p: %s\n",
			    pmIDStr(pmp->desc.pmid), pvp->inst, p, pvp->hdl,
			    pdherrstr(pdhsts));
	    pmp->flags |= M_NOVALUES;
	    break;
	}

	if (p > pattern)
	    continue;
	if (pvp->flags & V_VERIFIED)
	    continue;

	/*
	 * check PCP metric semantics against PDH info
	 */
	if (info_sz == 0) {
	    /*
	     * We've observed an initial call to PdhGetCounterInfoA()
	     * hang with a zero sized buffer ... pander to this with
	     * an initial buffer allocation ... (size is a 100% guess).
	     */
	    info_sz = 256;
	    if ((info = (LPSTR)malloc(info_sz)) == NULL) {
		__pmNotifyErr(LOG_ERR, "windows_open: PdhGetCounterInfoA "
					"malloc (%d) failed @ metric %s: ",
				(int)info_sz, pmIDStr(pmp->desc.pmid));
		errmsg();
		return -1;
	    }
	}
	result_sz = info_sz;
	pdhsts = PdhGetCounterInfoA(pvp->hdl, FALSE, &result_sz,
					(PDH_COUNTER_INFO_A *)info);
	if (pdhsts == PDH_MORE_DATA) {
	    info_sz = result_sz;
	    if ((info = (LPSTR)realloc(info, info_sz)) == NULL) {
		__pmNotifyErr(LOG_ERR, "windows_open: PdhGetCounterInfoA "
					"realloc (%d) failed @ metric %s: ",
				(int)info_sz, pmIDStr(pmp->desc.pmid));
		errmsg();
		return -1;
	    }
	    pdhsts = PdhGetCounterInfoA(pvp->hdl, FALSE, &result_sz,
					(PDH_COUNTER_INFO_A *)info);
	}
	if (pdhsts != ERROR_SUCCESS) {
	    __pmNotifyErr(LOG_ERR, "windows_open: PdhGetCounterInfoA "
					"failed @ metric %s: %s\n",
				pmIDStr(pmp->desc.pmid), pdherrstr(pdhsts));
	    continue;
	}
	infop = (PDH_COUNTER_INFO_A *)info;
	pmp->ctype = infop->dwType;

	if (check_semantics(pmp, pvp) == 0)
	    pvp->flags |= V_VERIFIED;
	else
	    __pmNotifyErr(LOG_ERR, "windows_open: Warning: metric %s: "
				   "unexpected counter type: %s\n",
			pmIDStr(pmp->desc.pmid), decode_ctype(pmp->ctype));
    }
    return 0;
}

void
windows_open(void)
{
    int			i, sts = 0;

    setup_globals();

    memset(querydesc, 0, sizeof(pdh_query_t) * Q_NUMQUERIES);
    for (i = 0; i < Q_NUMQUERIES; i++) {
	sts = PdhOpenQueryA(NULL, 0, &querydesc[i].hdl);
	if (sts != ERROR_SUCCESS) {
	    __pmNotifyErr(LOG_ERR, "windows_open: PdhOpenQueryA "
				    "failed @ query %d: %s\n",
				    i, pdherrstr(sts));
	    querydesc[i].flags |= Q_ERR_SEEN;
	}
    }

    for (i = 0; i < metricdesc_sz; i++) {
	windows_check_metric(&metricdesc[i]);
    }
}
