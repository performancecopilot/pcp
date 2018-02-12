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

    *buffer = strdup(&tbuf[k]);
    if (*buffer == NULL) {
	fprintf(stderr, "_pmUnpackText: malloc(%d) failed: %s\n",
		(int)strlen(&tbuf[k]), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
}

void
do_text(void)
{
    long		out_offset;
    unsigned int	type = 0;
    unsigned int	ident = 0;
    char		*buffer = NULL;
    //    textspec_t	*ip;
    int			sts;
    //    int		i;
    //    int		j;
    //    int		need_alloc = 0;

    out_offset = __pmFtell(outarch.logctl.l_mdfp);

    _pmUnpackText(inarch.metarec, &type, &ident, &buffer);

#if 0 /* no rewriting yet */
    /*
     * global time stamp adjustment (if any) has already been done in the
     * PDU buffer, so this is reflected in the unpacked value of stamp.
     */
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (ip->old_indom != indom)
	    continue;
	if (ip->indom_flags & INDOM_DUPLICATE) {
	    /*
	     * Save the old indom without changes, then operate on the
	     * duplicate.
	     */
	    if ((sts = __pmLogPutInDom(&outarch.archctl, indom, &stamp, numinst, instlist, inamelist)) < 0) {
		fprintf(stderr, "%s: Error: __pmLogPutInDom: %s: %s\n",
				pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
		abandon();
		/*NOTREACHED*/
	    }

	    /*
	     * If the old indom was not a duplicate, then libpcp, via
	     * __pmLogPutInDom(), assumes control of the storage pointed to by
	     * instlist and inamelist. In that case, we need to operate on copies
	     * from this point on.
	     */
	    if (sts != PMLOGPUTINDOM_DUP)
		_pmDupInDomData(numinst, &instlist, &inamelist);

	    if (pmDebugOptions.appl0) {
		fprintf(stderr, "Metadata: write pre-duplicate InDom %s @ offset=%ld\n", pmInDomStr(indom), out_offset);
	    }
	    out_offset = __pmFtell(outarch.logctl.l_mdfp);
	}
	if (ip->new_indom != ip->old_indom)
	    indom = ip->new_indom;
	for (i = 0; i < ip->numinst; i++) {
	    for (j = 0; j < numinst; j++) {
		if (ip->old_inst[i] == instlist[j])
		    break;
	    }
	    if (j == numinst)
		continue;
	    if (ip->inst_flags[i] & INST_DELETE) {
		if (pmDebugOptions.appl1)
		    fprintf(stderr, "Delete: instance %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], pmInDomStr(ip->old_indom));
		j++;
		while (j < numinst) {
		    instlist[j-1] = instlist[j];
		    inamelist[j-1] = inamelist[j];
		    j++;
		}
		need_alloc = 1;
		numinst--;
	    }
	    else {
		if (ip->inst_flags[i] & INST_CHANGE_INST)
		    instlist[j] = ip->new_inst[i];
		if (ip->inst_flags[i] & INST_CHANGE_INAME) {
		    inamelist[j] = ip->new_iname[i];
		    need_alloc = 1;
		}
		if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) && pmDebugOptions.appl1) {
		    if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) == (INST_CHANGE_INST | INST_CHANGE_INAME))
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->new_iname[i], ip->new_inst[i], pmInDomStr(ip->old_indom));
		    else if ((ip->inst_flags[i] & (INST_CHANGE_INST | INST_CHANGE_INAME)) == INST_CHANGE_INST)
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->old_iname[i], ip->new_inst[i], pmInDomStr(ip->old_indom));
		    else
			fprintf(stderr, "Rewrite: instance %s (%d) -> %s (%d) for indom %s\n", ip->old_iname[i], ip->old_inst[i], ip->new_iname[i], ip->old_inst[i], pmInDomStr(ip->old_indom));
		}
	    }
	}
    }

    if (need_alloc) {
	/*
	 * __pmLogPutInDom assumes the elements of inamelist[] point into
	 * of a contiguous allocation starting at inamelist[0] ... if we've
	 * changed an instance name or moved instance names about, then we
	 * need to reallocate the strings for inamelist[]
	 */
	int	need = 0;
	char	*new;
	char	*p;

	for (j = 0; j < numinst; j++)
	    need += strlen(inamelist[j]) + 1;
	new = (char *)malloc(need);
	if (new == NULL) {
	    fprintf(stderr, "inamelist[] malloc(%d) failed: %s\n", need, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	p = new;
	for (j = 0; j < numinst; j++) {
	    strcpy(p, inamelist[j]);
	    inamelist[j] = p;
	    p += strlen(p) + 1;
	}
    }
#endif /* no rewriting yet */

    /*
     * libpcp, via __pmLogPutText(), assumes control of the storage pointed
     * to by buffer.
     */

    if ((sts = __pmLogPutText(&outarch.archctl, ident, type, buffer, 1/*cached*/)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutText: %u %u: %s\n",
		pmGetProgname(), type, ident,
		pmErrStr(sts));
	abandon();
	/*NOTREACHED*/
    }
#if 0 /* don't handle duplicates yet. */
    /*
     * If the indom was a duplicate, then we are responsible for freeing the
     * associated storage.
     */
    if (sts == PMLOGPUTINDOM_DUP) {
	if (need_alloc)
	    free(inamelist[0]);
	free(inamelist);
	free(instlist);
    }
#endif
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Metadata: write help text %u %u @ offset=%ld\n",
		type, ident, out_offset);
    }
}
