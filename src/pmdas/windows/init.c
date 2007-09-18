/*
 * One-trip initialization for shim.exe
 *
 * Parts of this file contributed by Ken McDonell
 * (kenj At internode DoT on DoT net)
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

static char *
_append(char *name, char *suff)
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


/*
 * Based on documentation from ...
 * http://msdn.microsoft.com/library/default.asp?
 * 		url=/library/en-us/sysinfo/base/osversioninfoex_str.asp
 */
static char *
_get_os_name()
{
    static char		*name = NULL;
    OSVERSIONINFOEX	osv;
    char		tbuf[80];
    int			have_ex;

    if (name != NULL)
      /* already been here! */
      return name;

    /*
     * Try to get the OSVERSIONINFOEX version, fall back to the
     * OSVERSIONINFO version if needs be
     */
    memset(&osv, 0, sizeof(OSVERSIONINFOEX));
    osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    have_ex = 1;
    if (!GetVersionEx((OSVERSIONINFO *)&osv)) {
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!GetVersionEx((OSVERSIONINFO *)&osv))
	    return NULL;
	have_ex = 0;
    }

    switch (osv.dwPlatformId) {

        case VER_PLATFORM_WIN32_NT:
	    if (osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0) {
		if (osv.wProductType == VER_NT_WORKSTATION)
		    name = _append(name, "Windows Vista");
		else
		    name = _append(name, "Windows Server \"Longhorn\"");
	    }
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 2)
		name = _append(name, "Windows Server 2003");
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 1)
		name = _append(name, "Windows XP");
	    else if (osv.dwMajorVersion == 5 && osv.dwMinorVersion == 0)
		name = _append(name, "Windows 2000");
	    else if (osv.dwMajorVersion <= 4)
		name = _append(name, "Windows NT");
            else {
		sprintf(tbuf, "Windows Unknown (%d.%d)",
		    osv.dwMajorVersion, osv.dwMinorVersion); 
		name = _append(name, tbuf);
    	    }

            /* service pack and build number etc */
            if (osv.szCSDVersion[0] != '\0') {
		name = _append(name, " ");
		name = _append(name, osv.szCSDVersion);
	    }
	    if (have_ex) {
		sprintf(tbuf, " Build %d", osv.dwBuildNumber & 0xFFFF);
		name = _append(name, tbuf);
	    }

	    break;

        default:
	    name = _append(name, "Windows - Platform Unknown");
	    break;
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

#ifdef PCP_DEBUG
char *
_ctypestr(int ctype)
{
    if (ctype == PERF_COUNTER_COUNTER)
	return "PERF_COUNTER_COUNTER";
    else if (ctype == PERF_RAW_FRACTION)
	return "PERF_RAW_FRACTION";
    else if (ctype ==PERF_COUNTER_LARGE_RAWCOUNT_HEX)
	return "PERF_COUNTER_LARGE_RAWCOUNT_HEX";
    else if (ctype ==PERF_COUNTER_LARGE_RAWCOUNT)
	return "PERF_COUNTER_LARGE_RAWCOUNT";
    else if (ctype ==PERF_PRECISION_100NS_TIMER)
	return "PERF_PRECISION_100NS_TIMER";
    else if (ctype ==PERF_100NSEC_TIMER)
	return "PERF_100NSEC_TIMER";
    else if (ctype ==PERF_COUNTER_BULK_COUNT)
	return "PERF_COUNTER_BULK_COUNT";
    else if (ctype ==PERF_COUNTER_RAWCOUNT_HEX)
	return "PERF_COUNTER_RAWCOUNT_HEX";
    else if (ctype ==PERF_COUNTER_RAWCOUNT)
	return "PERF_COUNTER_RAWCOUNT";
    else if (ctype ==PERF_COUNTER_COUNTER)
	return "PERF_COUNTER_COUNTER";
    else
    	return "UNKNOWN";
}
#endif


int
shim_init(void)
{
    int			i;
    int			j;
    static int		first = 1;
    PDH_STATUS  	pdhsts;
    static LPSTR	pattern = NULL;
    static DWORD	pattern_sz = 0;
    static LPSTR	info = NULL;
    static DWORD	info_sz = 0;
    static DWORD	result_sz;
    PDH_COUNTER_INFO	*infop;
    LPTSTR      	p;
    int			sts = -1;	/* assume failure */
    DWORD		index;
    char		*ctr_type;
    MEMORYSTATUS	mstat;
    char		*vp;
    char		*bp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "shim_init() called\n");
	fflush(stderr);
    }
#endif
    shm_hfile = CreateFile(
		SHM_FILENAME,
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,			// security attributes ignored
		OPEN_EXISTING,		// should be here, do not create
		FILE_ATTRIBUTE_NORMAL,
		NULL);			// template not used
    if (shm_hfile == INVALID_HANDLE_VALUE) {
	fprintf(stderr, "shim_init: CreateFile(%s) failed: ", SHM_FILENAME);
	errmsg();
	goto done;
    }
    shm_hmap = CreateFileMapping(
		shm_hfile,		// our file
		NULL,			// MappingAttributes ignored
		PAGE_READWRITE,		// read+write access
		0,			// assume size < 2^32-1
		0,			// low 32-bits of size (all of file)
		NULL);			// don't care about name of
					// mapping object
    if (shm_hmap == NULL) {
	fprintf(stderr, "shim_init: CreateFileMapping() failed: ");
	errmsg();
	goto done;
    }

    shm = (shm_hdr_t *)MapViewOfFile(
		shm_hmap,		// our map
		FILE_MAP_ALL_ACCESS,	// read+write access
		0,			// offset is start of map
		0,			// ditto
		0);			// map whole file
    if (shm == NULL) {
	fprintf(stderr, "shim_init: MapViewOfFile() failed: ");
	errmsg();
	goto done;
    }
    shm_oldsize = shm->size;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	fprintf(stderr, "shim_init: shm -> %p\n", shm);
	fprintf(stderr, " magic = 0x%x, size = %d, nseg = %d\n", shm->magic, shm->size, shm->nseg);
	for (i = 0; i < shm->nseg; i++) {
	    fprintf(stderr, "  segment[%d] base=%d elt_size=%d nelt=%d\n",
	    i, shm->segment[i].base, shm->segment[i].elt_size, shm->segment[i].nelt);
	}
	fflush(stderr);
    }
#endif

    /*
     * special metrics that do not use PDH ...
     */

    /* physical memory size (dwTotalPhys is in units of bytes) */
    GlobalMemoryStatus(&mstat);
    shm->physmem = mstat.dwTotalPhys / (1024*1024);

    /* version of Windows */
    vp = _get_os_name();
    bp = strstr(vp, " Build");
    if (bp != NULL) {
        *bp++ = '\0';
	strncpy(shm->build, bp, MAX_UNAME_SIZE);
    }
    else
	shm->build[0] = '\0';
    strncpy(shm->uname, vp, MAX_UNAME_SIZE);
    shm->uname[MAX_UNAME_SIZE-1] = '\0';

    hdr_size = sizeof(shm_hdr_t) + (shm->nseg-1)*sizeof(shm_seg_t);
    if ((new_hdr = (shm_hdr_t *)malloc(hdr_size)) == NULL) {
	fprintf(stderr, "shim_init: malloc new_hdr[%d]: ", hdr_size);
	errmsg();
	goto done;
    }

    metrictab_sz = shm->segment[SEG_METRICS].nelt;
    shm_metrictab = (shm_metric_t *)&((char *)shm)[shm->segment[SEG_METRICS].base];
    if ((shim_metrictab = (shim_metric_t *)malloc(metrictab_sz * sizeof(shim_metric_t))) == NULL) {
	fprintf(stderr, "shim_init: malloc shim_metrictab[%d]: ", metrictab_sz * sizeof(shim_metric_t));
	errmsg();
	goto done;
    }

    for (i = 0; i < metrictab_sz; i++) {
	if (shm_metrictab[i].m_qid > querytab_sz)
	    querytab_sz = shm_metrictab[i].m_qid;
	shim_metrictab[i].m_num_ctrs = 0;
	shim_metrictab[i].m_ctrs = NULL;
    }
    querytab_sz++;

    if ((querytab = (shim_query_t *)malloc(querytab_sz * sizeof(shim_query_t))) == NULL) {
	fprintf(stderr, "shim_init: malloc querytab[%d]: ", querytab_sz * sizeof(shim_query_t));
	errmsg();
	goto done;
    }

    for (i = 0; i < querytab_sz; i++) {
	pdhsts = PdhOpenQuery(NULL, 0, &querytab[i].q_hdl);
	if (pdhsts != ERROR_SUCCESS) {
	    querytab[i].q_hdl = NULL;
	    fprintf(stderr, "shim_init: Warning: PdhOpenQuery failed: %s\n", pdherrstr(pdhsts));
	}
	querytab[i].q_flags = Q_NONE;
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "shim_init: query[%d] handle=%p\n", i, querytab[i].q_hdl);
	    fflush(stderr);
	}
#endif
    }

    /*
     * Enumerate instances where appropriate and generally get ready to
     * do good works.
     */
    for (i = 0; i < metrictab_sz; i++) {
	shm_metric_t	*sp = &shm_metrictab[i];
	shim_metric_t	*pp = &shim_metrictab[i];

	result_sz = pattern_sz;
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "\nshim_init: calling PdhExpandCounterPath pat=\"%s\" (len=%d) result_sz=%d\n",
		sp->m_pat, strlen(sp->m_pat), result_sz);
	    fflush(stderr);
	}
#endif
	pdhsts = PdhExpandCounterPath(sp->m_pat, pattern, &result_sz);
	if (pdhsts == PDH_MORE_DATA) {
	    result_sz++;		// not sure if this is this necessary?
	    pattern_sz = result_sz;
	    if ((pattern = (LPSTR)realloc(pattern, pattern_sz)) == NULL) {
		fprintf(stderr, "shim_init: Error: PdhExpandCounterPath realloc (%d) failed @ metric %s: ",
		    (int)pattern_sz, pmIDStr(sp->m_desc.pmid));
		errmsg();
		goto done;
	    }
#ifdef PCP_DEBUG
	    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		/* desperate */
		fprintf(stderr, "shim_init: calling PdhExpandCounterPath again result_sz=%d\n", result_sz);
		fflush(stderr);
	    }
#endif
	    pdhsts = PdhExpandCounterPath(sp->m_pat, pattern, &result_sz);
	}
	if (pdhsts != ERROR_SUCCESS) {
	    if (sp->m_pat[0] == '\0') {
		/*
		 * Empty path string.  Used to denote metrics that are
		 * derived and do not have an explicit path or retrieval
		 * need, other than to make sure the m_qid value will
		 * force the corresponding query to be run before a PCP
		 * fetch ... do nothing here
		 */
		;
	    }
	    else {
		fprintf(stderr, "shim_init: Warning: PdhExpandCounterPath failed @ metric pmid=%s pattern=\"%s\": %s\n",
		    pmIDStr(sp->m_desc.pmid), sp->m_pat, pdherrstr(pdhsts));
            }
	    sp->m_flags |= M_NOVALUES;
	    continue;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "shim_init: %d bytes from PdhExpandCounterPath\n", result_sz);
	    fflush(stderr);
	}
#endif
	/*
	 * PdhExpandCounterPath is apparently busted ... the length
	 * returned includes one byte _after_ the last NULL byte
	 * string terminator, but the final byte is apparently
	 * not being set ... force the issue
	 */
	pattern[result_sz-1] = '\0';
	for (p = pattern; *p; p += lstrlen(p) + 1) {
	    shim_ctr_t		*wcp;
#ifdef PCP_DEBUG
	    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		/* desperate */
		fprintf(stderr, "shim_init: p-> pattern[%d] \"%s\" len=%d\n", p-pattern, p, lstrlen(p));
		fflush(stderr);
	    }
#endif
	    pp->m_num_ctrs++;
	    if ((pp->m_ctrs = (shim_ctr_t *)realloc(pp->m_ctrs, pp->m_num_ctrs * sizeof(shim_ctr_t))) == NULL) {
		fprintf(stderr, "shim_init: Error: m_ctrs realloc (%d x %d) failed @ metric %s [%s]: ",
		    pp->m_num_ctrs, sizeof(shim_ctr_t), pmIDStr(sp->m_desc.pmid), p);
		errmsg();
		goto done;
	    }
	    wcp = &pp->m_ctrs[pp->m_num_ctrs-1];
	    if (sp->m_desc.indom == PM_INDOM_NULL) {
		/* singular instance */
		wcp->c_inst = PM_IN_NULL;
		if (pp->m_num_ctrs > 1) {
		    /*
		     * report only once per pattern
		     */
		    char 	*q;
		    int		k;
		    fprintf(stderr, "shim_init: Warning: singular metric %s has more than one instance ...\n",
			pmIDStr(sp->m_desc.pmid));
		    fprintf(stderr, "  pattern: \"%s\"\n", sp->m_pat);
		    for (k = 0, q = pattern; *q; q += lstrlen(q) + 1, k++)  {
			fprintf(stderr, "  match[%d]: \"%s\"\n", k, q);
		    }
		    fprintf(stderr, "... skip this counter\n");
		    /* next realloc() will be a NOP */
		    pp->m_num_ctrs--;
		    /* no more we can do here, onto next metric-pattern */
		    break;
		}
	    }
	    else {
		/*
		 * if metric has instance domain, parse pattern using
		 * indom type to extract instance name and number, and
		 * add into indom control table as needed.
		 */
		int	ok;

		ok = check_instance(p, sp, &wcp->c_inst);
		/*
		 * check_instance() can force the shared memory segment
		 * to be re-shaped, which might move the base address of
		 * the shm region, so make sure pointers into shm are
		 * re-evaluated.
		 */
		sp = &shm_metrictab[i];

		if (!ok) {
		    /*
		     * error reported in check_instance() ...
		     * we cannot return any values for this instance if
		     * we don't recognize the name ... skip this one,
		     * the next realloc() (if any) will be a NOP
		     */
		    pp->m_num_ctrs--;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0) {
			fprintf(stderr, "shim_init: metric: %s: skip counter \"%s\"\n",
			    pmIDStr(sp->m_desc.pmid), p);
			fflush(stderr);
		    }
#endif
		    /* onto next instance */
		    continue;
		}
	    }

	    /*
	     * Note.  Passing c_inst down here does not help much, as
	     * I don't think we're ever going to navigate back from a PDH
	     * counter into our data structures, and even if we do,
	     * the instance id is not going to be unique enough.
	     */
	    pdhsts = PdhAddCounter(querytab[sp->m_qid].q_hdl,
			    p, wcp->c_inst, &wcp->c_hdl);
	    if (pdhsts != ERROR_SUCCESS) {
		fprintf(stderr, "shim_init: Warning: PdhAddCounter failed @ metric pmid=%s inst=%d pat=\"%s\" qid=%d qhdl=%p: %s\n",
		    pmIDStr(sp->m_desc.pmid), wcp->c_inst, p, sp->m_qid,
		    querytab[sp->m_qid].q_hdl, pdherrstr(pdhsts));
		sp->m_flags |= M_NOVALUES;
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "shim_init: PdhAddCounter: metric pmid=%s inst=%d pat=\"%s\"\n",
		    pmIDStr(sp->m_desc.pmid), wcp->c_inst, p);
		fflush(stderr);
	    }
#endif

	    if (p > pattern)
		continue;

	    /*
	     * check metric semantics (PCP) against PDH info
	     */
	    if (info_sz == 0) {
		/*
		 * on hugh.melbourne.sgi.com running SFU 3.5 on Windows NT
		 * the first call to PdhGetCounterInfo() hung with a zero
		 * sized buffer ... pander to this with an intial buffer
		 * allocation ... the size is a 100% guess
		 */
	    	info_sz = 256;
		if ((info = (LPSTR)malloc(info_sz)) == NULL) {
		    fprintf(stderr, "shim_init: Warning: PdhGetCounterInfo malloc (%d) failed @ metric %s: ",
			(int)info_sz, pmIDStr(sp->m_desc.pmid));
		    errmsg();
		    goto done;
		}
	    }
	    result_sz = info_sz;
#ifdef PCP_DEBUG
	    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		/* desperate */
		fprintf(stderr, "shim_init: calling PdhGetCounterInfo result_sz=%d\n", result_sz);
		fflush(stderr);
	    }
#endif
	    pdhsts = PdhGetCounterInfo(wcp->c_hdl, FALSE, &result_sz, (PDH_COUNTER_INFO *)info);
	    if (pdhsts == PDH_MORE_DATA) {
		info_sz = result_sz;
		if ((info = (LPSTR)realloc(info, info_sz)) == NULL) {
		    fprintf(stderr, "shim_init: Warning: PdhGetCounterInfo realloc (%d) failed @ metric %s: ",
			(int)info_sz, pmIDStr(sp->m_desc.pmid));
		    errmsg();
		    goto done;
		}
#ifdef PCP_DEBUG
		if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		    /* desperate */
		    fprintf(stderr, "shim_init: calling PdhGetCounterInfo again result_sz=%d\n", result_sz);
		    fflush(stderr);
		}
#endif
		pdhsts = PdhGetCounterInfo(wcp->c_hdl, FALSE, &result_sz, (PDH_COUNTER_INFO *)info);
	    }
	    if (pdhsts != ERROR_SUCCESS) {
		fprintf(stderr, "shim_init: Warning: PdhGetCounterInfo failed @ metric %s: %s\n",
		    pmIDStr(sp->m_desc.pmid), pdherrstr(pdhsts));
		continue;
	    }
	    infop = (PDH_COUNTER_INFO *)info;
	    pp->m_ctype = infop->dwType;
#ifdef PCP_DEBUG
	    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		/* desperate */
		fprintf(stderr, "shim_init: %d bytes from PdhGetCounterInfo -> type=0x%x %s\n", result_sz, pp->m_ctype, _ctypestr(pp->m_ctype));
		fflush(stderr);
	    }
#endif
	    switch (pp->m_ctype) {
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
		    if (sp->m_desc.type != PM_TYPE_32 &&
			sp->m_desc.type != PM_TYPE_U32) {
			fprintf(stderr, "shim_init: Warning: PERF_COUNTER_COUNTER: metric %s: rewrite type from %s to PM_TYPE_U32\n",
			    pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_U32;
		    }
		    if (sp->m_desc.sem != PM_SEM_COUNTER) {
			fprintf(stderr, "shim_init: Warning: PERF_COUNTER_COUNTER: metric %s: semantics %s (expected %s)\n",
			    pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem), _semstr(PM_SEM_COUNTER));
		    }
		    break;

		case PERF_COUNTER_RAWCOUNT:
		    ctr_type = "PERF_COUNTER_RAWCOUNT";
		    goto label_01;
		case PERF_COUNTER_RAWCOUNT_HEX:
		    ctr_type = "PERF_COUNTER_RAWCOUNT_HEX";
label_01:
		    /* 32-bit PM_SEM_INSTANT or PM_SEM_DISCRETE */
		    if (sp->m_desc.type != PM_TYPE_32 &&
			sp->m_desc.type != PM_TYPE_U32) {
			fprintf(stderr, "shim_init: Warning: %s: metric %s: rewrite type from %s to PM_TYPE_U32\n",
			    ctr_type, pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_U32;
		    }
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0) {
			fprintf(stderr, "shim_init: INFO: %s: metric %s: semantics %s\n",
			    ctr_type, pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem));
		    }
#endif
		    break;

		/*
		 * Known 64-bit counters
		 */
		case PERF_COUNTER_BULK_COUNT:
		    /* 64-bit PM_SEM_COUNTER */
		    if (sp->m_desc.type != PM_TYPE_64 &&
			sp->m_desc.type != PM_TYPE_U64) {
			fprintf(stderr, "shim_init: Warning: PERF_COUNTER_BULK_COUNT: metric %s: rewrite type from %s to PM_TYPE_U64\n",
			    pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_U64;
		    }
		    if (sp->m_desc.sem != PM_SEM_COUNTER) {
			fprintf(stderr, "shim_init: Warning: PERF_COUNTER_BULK_COUNT: metric %s: semantics %s (expected %s)\n",
			    pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem), _semstr(PM_SEM_COUNTER));
			sp->m_desc.sem = PM_SEM_COUNTER;
		    }
		    break;

		case PERF_100NSEC_TIMER:
		    ctr_type = "PERF_100NSEC_TIMER";
		    goto label_02;
		case PERF_PRECISION_100NS_TIMER:
		    ctr_type = "PERF_PRECISION_100NS_TIMER";
label_02:
		    /*
		     * 64-bit PM_SEM_COUNTER, units are 100's of nanosecs,
		     * we shall export 'em as microseconds
		     */
		    if (sp->m_desc.type != PM_TYPE_64 &&
			sp->m_desc.type != PM_TYPE_U64) {
			fprintf(stderr, "shim_init: Warning: %s: metric %s: rewrite type from %s to PM_TYPE_U64\n",
			    ctr_type, pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_U64;
		    }
		    if (sp->m_desc.sem != PM_SEM_COUNTER) {
			fprintf(stderr, "shim_init: Warning: %s: metric %s: semantics %s (expected %s)\n",
			    ctr_type, pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem), _semstr(PM_SEM_COUNTER));
			sp->m_desc.sem = PM_SEM_COUNTER;
		    }
		    if (sp->m_desc.units.dimSpace != 0 ||
			sp->m_desc.units.dimTime != 1 ||
			sp->m_desc.units.dimCount != 0 ||
			sp->m_desc.units.scaleTime != PM_TIME_USEC) {
			fprintf(stderr, "shim_init: Warning: %s: metric %s: rewrite dimension and scale from %s",
			    ctr_type, pmIDStr(sp->m_desc.pmid), pmUnitsStr(&sp->m_desc.units));
			sp->m_desc.units.dimSpace = sp->m_desc.units.dimCount = 0;
			sp->m_desc.units.scaleSpace = sp->m_desc.units.scaleCount = 0;
			sp->m_desc.units.dimTime = 1;
			sp->m_desc.units.scaleTime = PM_TIME_USEC;
			fprintf(stderr, " to %s\n", pmUnitsStr(&sp->m_desc.units));
		    }
		    break;

		case PERF_COUNTER_LARGE_RAWCOUNT:
		case PERF_COUNTER_LARGE_RAWCOUNT_HEX:
		    /* 64-bit PM_SEM_INSTANT or PM_SEM_DISCRETE */
		    if (sp->m_desc.type != PM_TYPE_64 &&
			sp->m_desc.type != PM_TYPE_U64) {
			fprintf(stderr, "shim_init: Warning: PERF_COUNTER_LARGE_RAWCOUNT: metric %s: rewrite type from %s to PM_TYPE_U64\n",
			    pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_U64;
		    }
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0) {
			fprintf(stderr, "shim_init: INFO: PERF_COUNTER_RAWCOUNT: metric %s: semantics %s\n",
			    pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem));
		    }
#endif
		    break;

		case PERF_RAW_FRACTION:
		    /* Float PM_SEM_INSTANT or PM_SEM_DISCRETE */
		    if (sp->m_desc.type != PM_TYPE_FLOAT) {
			fprintf(stderr, "shim_init: Warning: PERF_RAW_FRACTION: metric %s: rewrite type from %s to PM_TYPE_FLOAT\n",
			    pmIDStr(sp->m_desc.pmid), _typestr(sp->m_desc.type));
			sp->m_desc.type = PM_TYPE_FLOAT;
		    }
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0) {
			fprintf(stderr, "shim_init: INFO: PERF_RAW_FRACTION: metric %s: semantics %s\n",
			    pmIDStr(sp->m_desc.pmid), _semstr(sp->m_desc.sem));
		    }
#endif
		    break;


		default:
		    fprintf(stderr, "shim_init: Warning: metric %s: unexpected counter type: %s\n",
			pmIDStr(sp->m_desc.pmid), decode_ctype(infop->dwType));
		    break;
	    }
	}
	sp->m_flags |= M_EXPANDED;

    }

    sts = 0;		/* success */

done:
    fflush(stderr);
    return sts;
}

