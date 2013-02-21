/*
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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
 */

#include <ctype.h>
#include "hypnotoad.h"

static char	*text;	/* filled in by iterator callback routine */
static char	texts[MAX_M_TEXT_LEN];	/* static callback buffer */

/*
 * Replace backslashes in the help string returned from Pdh APIs.
 * Everything done "in-place" so no change to size of the string.
 */
static char *
windows_fmt(char *text)
{
    char	*p;
    int		n;

    for (p = text, n = 0; p && *p != '\0'; p++, n++) {
	if (!isprint((int)*p))		/* toss any dodgey characters */
	    *p = '?';
	else if (*p == '\r')		/* remove Windows line ending */
	    *p = '\n';
	if (n < 70 || !isspace((int)*p))	/* very simple line wrapping */
	    continue;
	*p = '\n';
	n = 0;
    }
    return text;
}

static void
windows_helptext_metric(pdh_metric_t *mp, PDH_COUNTER_INFO_A *infop)
{
    text = infop->szExplainText;
}

static void
windows_helptext_callback(pdh_metric_t *pmp, LPSTR pat, pdh_value_t *pvp)
{
    windows_inform_metric(pmp, pat, pvp, TRUE, windows_helptext_metric);
}

int
windows_help(int ident, int type, char **buf, pmdaExt *pmda)
{
    pmID	pmid = (pmID)ident;
    int		i;

    if ((type & PM_TEXT_PMID) != PM_TEXT_PMID)
	return pmdaText(ident, type, buf, pmda);

    for (i = 0; i < metricdesc_sz; i++)
	if (pmid == metricdesc[i].desc.pmid)
	    break;
    if (i == metricdesc_sz)
	return PM_ERR_PMID;

    if (type & PM_TEXT_ONELINE) {
	if (metricdesc[i].pat[0] == '\0')
	    return pmdaText(ident, type, buf, pmda);
	*buf = windows_fmt(strncpy(texts, &metricdesc[i].pat[0], sizeof(texts)));
    } else {
	text = NULL;
	windows_visit_metric(&metricdesc[i], windows_helptext_callback);
	if (!text)
	    return pmdaText(ident, type, buf, pmda);
	*buf = windows_fmt(strncpy(texts, text, sizeof(texts)));
    }
    return 0;
}
