/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
 */

#include <ctype.h>
#include "hypnotoad.h"

/*
 * Replace backslashes in the help string returned from Pdh APIs.
 * Everything done "in-place" so no change to size of the string.
 */
static char *
windows_fmt(char *text)
{
    char *p;
    int n;

    for (p = text, n = 0; p && *p != '\0'; p++, n++) {
	if (!isprint(*p))		/* toss any dodgey characters */
	    *p = '?';
	else if (*p == '\r')		/* remove Windows line ending */
	    *p = '\n';
	if (n < 70 || !isspace(*p))	/* very simple line wrapping */
	    continue;
	*p = '\n';
	n = 0;
    }
    return text;
}

int
windows_help(int ident, int type, char **buf, pmdaExt *pmda)
{
    int			i;
    pmID		pmid;
    PDH_STATUS  	pdhsts;
    PDH_COUNTER_INFO_A	*infop;
    static LPSTR	info = NULL;
    static DWORD	info_sz = 0;
    static DWORD	result_sz;
    static char		*lastbuf;
    char		*text;
    int			sts;

    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "help(ident=%s, type=%d)\n", pmIDStr((pmID)ident), type);

    if ((type & PM_TEXT_PMID) != PM_TEXT_PMID)
	return PM_ERR_TEXT;

    pmid = (pmID)ident;
    for (i = 0; i < metricdesc_sz; i++) {
	if (pmid == metricdesc[i].desc.pmid) {
	    pdh_value_t *pvp = metricdesc[i].vals;
	    if (type & PM_TEXT_ONELINE) {
		if (metricdesc[i].pat[0] == '\0')
		    return PM_ERR_TEXT;
		text = &metricdesc[i].pat[0];
		goto found;
	    }

	    if (metricdesc[i].num_vals < 1)
		/* no counter handle, no help text */
		return sts;
	    if (info_sz == 0) {
		/*
		 * We've observed the initial call to PdhGetCounterInfoA
		 * hang with a zero sized buffer ... pander to this with
		 * an intial buffer allocation ... size is a 100% guess.
		 */
	    	info_sz = 256;
		if ((info = (LPSTR)malloc(info_sz)) == NULL) {
		    __pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
			    		"malloc (%d) failed @ metric %s: ",
				(int)info_sz, pmIDStr(metricdesc[i].desc.pmid));
		    errmsg();
		    return sts;
		}
	    }
	    result_sz = info_sz;
	    pdhsts = PdhGetCounterInfoA(pvp->hdl, TRUE, &result_sz,
					(PDH_COUNTER_INFO_A *)info);
	    if (pdhsts == PDH_MORE_DATA) {
		info_sz = result_sz;
		if ((info = (LPSTR)realloc(info, info_sz)) == NULL) {
		    __pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
			    		"failed @ metric %s: ", pmIDStr(pmid));
		    errmsg();
		    return sts;
		}
		pdhsts =  PdhGetCounterInfoA(pvp->hdl, TRUE, &result_sz,
					     (PDH_COUNTER_INFO_A *)info);
	    }
	    if (pdhsts != ERROR_SUCCESS) {
		__pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
				"failed @ metric %s: %s\n",
				pmIDStr(pmid), pdherrstr(pdhsts));
		return sts;
	    }
	    infop = (PDH_COUNTER_INFO_A *)info;
	    if (infop->szExplainText == NULL) {
		__pmNotifyErr(LOG_ERR, "windows_help: PdhGetCounterInfoA "
				"NULL help text @ metric %s\n", pmIDStr(pmid));
		return sts;
	    }
	    text = infop->szExplainText;
found:
	    if (lastbuf != NULL) {
		free(lastbuf);
		lastbuf = NULL;
	    }
	    lastbuf = *buf = windows_fmt(strdup(text));
	    if (*buf != NULL)
		sts = 0;
	    return sts;
	}
    }
    return PM_ERR_PMID;
}
