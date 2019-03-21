/*
 * Text metadata support for pmlogrewrite
 *
 * Copyright (c) 2018 Red Hat.
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

#include <string.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"

#define __ntohpmInDom(a)	ntohl(a)
#define __ntohpmID(a)		ntohl(a)

/*
 * Find or create a new textspec_t
 */
textspec_t *
start_text(int type, int id, char *text)
{
    textspec_t	*tp;
    int		sts;

    if (pmDebugOptions.appl4) {
	if ((type & PM_TEXT_PMID))
	    fprintf(stderr, "start_text(%s)", pmIDStr(id));
	else
	    fprintf(stderr, "start_text(%s)", pmInDomStr(id));
	if ((type & PM_TEXT_ONELINE))
	    fprintf(stderr, " one line");
	else
	    fprintf(stderr, " full");
    }

    /* Search for this help text in the existing list of changes. */
    for (tp = text_root; tp != NULL; tp = tp->t_next) {
	if (type == tp->old_type) {
	    if (id == tp->old_id) {
		if (text == NULL ||
		    (tp->old_text != NULL && strcmp(text, tp->old_text) == 0)) {
		    if (pmDebugOptions.appl4) {
			if ((type & PM_TEXT_PMID))
			    fprintf(stderr, " -> %s", pmIDStr(tp->new_id));
			else
			    fprintf(stderr, " -> %s", pmInDomStr(tp->new_id));
			if ((type & PM_TEXT_ONELINE))
			    fprintf(stderr, " one line\n");
			else
			    fprintf(stderr, " full\n");
		    }
		    return tp;
		}
	    }
	}
    }

    /* The help text was not found. Create a new change spec. */
    tp = (textspec_t *)malloc(sizeof(textspec_t));
    if (tp == NULL) {
	fprintf(stderr, "textspec malloc(%d) failed: %s\n", (int)sizeof(textspec_t), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }

    if (text != NULL)
	tp->old_text = text;
    else if ((type & PM_TEXT_PMID)) {
	sts = pmLookupText(id, type, &tp->old_text);
	if (sts < 0) {
	    if (wflag) {
		pmsprintf(mess, sizeof(mess), "Text %s: pmLookupText: %s", pmIDStr(id), pmErrStr(sts));
		yywarn(mess);
	    }
	    free(tp);
	    return NULL;
	}
    }
    else {
	sts = pmLookupInDomText(id, type, &tp->old_text);
	if (sts < 0) {
	    if (wflag) {
		pmsprintf(mess, sizeof(mess), "Text %s: pmLookupInDomText: %s", pmInDomStr(id), pmErrStr(sts));
		yywarn(mess);
	    }
	    free(tp);
	    return NULL;
	}
    }

    /* Initialize and link. old_text has already been initialized. */
    tp->t_next = text_root;
    text_root = tp;
    tp->old_type = tp->new_type = type;
    tp->old_id = tp->new_id = id;
    tp->new_text = NULL;
    tp->flags = 0;
    tp->ip = NULL;

    if (pmDebugOptions.appl4)
	fprintf(stderr, " -> [new entry]\n");

    return tp;
}

/*
 * Reverse the logic of __pmLogPutText()
 *
 * Mostly stolen from __pmLogLoadMeta. There may be a chance for some
 * code factoring here.
 */
 static void
_pmUnpackText(__pmPDU *pdubuf, unsigned int *type, unsigned int *ident,
	      char **buffer)
{
    char	*tbuf;
    int		k;

    /* Walk through the record extracting the data. */
    tbuf = (char *)pdubuf;
    k = 0;
    k += sizeof(__pmLogHdr);

    *type = ntohl(*((unsigned int *)&tbuf[k]));
    k += sizeof(*type);

    if (!(*type & (PM_TEXT_ONELINE|PM_TEXT_HELP))) {
	fprintf(stderr, "_pmUnpackText: invalid text type %u\n", *type);
	abandon();
	/*NOTREACHED*/
    }
    else if ((*type & PM_TEXT_INDOM))
	*ident = __ntohpmInDom(*((unsigned int *)&tbuf[k]));
    else if ((*type & PM_TEXT_PMID))
	*ident = __ntohpmID(*((unsigned int *)&tbuf[k]));
    else {
	fprintf(stderr, "_pmUnpackText: invalid text type %u\n", *type);
	abandon();
	/*NOTREACHED*/
    }
    k += sizeof(*ident);

    *buffer = &tbuf[k];
}

void
do_text(void)
{
    long		out_offset;
    unsigned int	type = 0;
    unsigned int	ident = 0;
    char		*buffer = NULL;
    textspec_t		*tp;
    int			sts;

    out_offset = __pmFtell(outarch.logctl.l_mdfp);

    /* After this call, buffer will point into inarch.metarec */
    _pmUnpackText(inarch.metarec, &type, &ident, &buffer);

    /*
     * Global time stamp adjustment (if any) has already been done in the
     * PDU buffer, so this is reflected in the unpacked value of stamp.
     */
    for (tp = text_root; tp != NULL; tp = tp->t_next) {
	if (tp->old_id != ident)
	    continue;
	if (tp->old_type != type)
	    continue;
	if (tp->old_text != NULL && strcmp(tp->old_text, buffer) != 0)
	    continue;

	/* Delete the record? */
	if (tp->flags & TEXT_DELETE) {
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "Delete: %s help text for ",
			(tp->old_type & PM_TEXT_ONELINE) ? "one line" : "full");
		if ((tp->old_type & PM_TEXT_PMID))
		    fprintf(stderr, " metric %s\n", pmIDStr(tp->old_id));
		else
		    fprintf(stderr, " indom %s\n", pmInDomStr(tp->old_id));
	    }
	    return;
	}

	/* Rewrite the record as specified. */
	if ((tp->flags & TEXT_CHANGE_ID))
	    ident = tp->new_id;
	if ((tp->flags & TEXT_CHANGE_TYPE))
	    type = tp->new_type;
	if ((tp->flags & TEXT_CHANGE_TEXT))
	    buffer = tp->new_text;
	
	if (pmDebugOptions.appl1) {
	    if ((tp->flags & (TEXT_CHANGE_ID | TEXT_CHANGE_TYPE | TEXT_CHANGE_TEXT))) {
		fprintf(stderr, "Rewrite: %s help text for ",
			(tp->old_type & PM_TEXT_ONELINE) ? "one line" : "full");
		if ((tp->old_type & PM_TEXT_PMID))
		    fprintf(stderr, " metric %s", pmIDStr(tp->old_id));
		else
		    fprintf(stderr, " indom %s", pmInDomStr(tp->old_id));
	    }
	    if ((tp->flags & (TEXT_CHANGE_TEXT))) {
		fprintf(stderr, " \"%s\"", tp->old_text);
	    }
	    if ((tp->flags & (TEXT_CHANGE_ID | TEXT_CHANGE_TYPE | TEXT_CHANGE_TEXT))) {
		fprintf(stderr, " to\n%s help text for ",
			(tp->new_type & PM_TEXT_ONELINE) ? "one line" : "full");
		if ((tp->new_type & PM_TEXT_PMID))
		    fprintf(stderr, " metric %s", pmIDStr(tp->new_id));
		else
		    fprintf(stderr, " indom %s", pmInDomStr(tp->new_id));
	    }
	    if ((tp->flags & (TEXT_CHANGE_TEXT))) {
		fprintf(stderr, " \"%s\"", tp->new_text);
	    }
	    if ((tp->flags & (TEXT_CHANGE_ID | TEXT_CHANGE_TYPE | TEXT_CHANGE_TEXT)))
		fputc('\n', stderr);
	}
	break;
    }

    /*
     * libpcp, via __pmLogPutText(), makes a copy of the storage pointed
     * to by buffer.
     */

    if ((sts = __pmLogPutText(&outarch.archctl, ident, type, buffer, 1/*cached*/)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutText: %u %u: %s\n",
		pmGetProgname(), type, ident,
		pmErrStr(sts));
	abandon();
	/*NOTREACHED*/
    }
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Metadata: write help text %u %u @ offset=%ld\n",
		type, ident, out_offset);
    }
}
