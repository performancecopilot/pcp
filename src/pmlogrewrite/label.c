/*
 * Label metadata support for pmlogrewrite
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

/* Stolen from libpcp. */
static void
_ntohpmLabel(pmLabel * const label)
{
    label->name = ntohs(label->name);
    /* label->namelen is one byte */
    /* label->flags is one byte */
    label->value = ntohs(label->value);
    label->valuelen = ntohs(label->valuelen);
}

/*
 * Reverse the logic of __pmLogPutLabel()
 *
 * Mostly stolen from __pmLogLoadMeta. There may be a chance for some
 * code factoring here.
 */
 static void
_pmUnpackLabelSet(__pmPDU *pdubuf, unsigned int *type, unsigned int *ident,
		  int *nsets, pmLabelSet **labelsets, pmTimeval *stamp)
{
    char	*tbuf;
    int		i, j, k;
    int		inst;
    int		jsonlen;
    int		nlabels;

    /* Walk through the record extracting the data. */
    tbuf = (char *)pdubuf;
    k = 0;
    k += sizeof(__pmLogHdr);

    *stamp = *((pmTimeval *)&tbuf[k]);
    stamp->tv_sec = ntohl(stamp->tv_sec);
    stamp->tv_usec = ntohl(stamp->tv_usec);
    k += sizeof(*stamp);

    *type = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(*type);

    *ident = ntohl(*((unsigned int*)&tbuf[k]));
    k += sizeof(*ident);

    *nsets = *((unsigned int *)&tbuf[k]);
    *nsets = ntohl(*nsets);
    k += sizeof(*nsets);

    *labelsets = NULL;
    if (*nsets > 0) {
	*labelsets = (pmLabelSet *)calloc(*nsets, sizeof(pmLabelSet));
	if (*labelsets == NULL) {
	    fprintf(stderr, "_pmUnpackLabelSet labellist malloc(%d) failed: %s\n",
		    (int)(*nsets * sizeof(pmLabelSet)), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}

	/* No offset to JSONB string as in logarchive(5)???? */
	for (i = 0; i < *nsets; i++) {
	    inst = *((unsigned int*)&tbuf[k]);
	    inst = ntohl(inst);
	    k += sizeof(inst);
	    (*labelsets)[i].inst = inst;

	    jsonlen = ntohl(*((unsigned int*)&tbuf[k]));
	    k += sizeof(jsonlen);
	    (*labelsets)[i].jsonlen = jsonlen;

	    if (((*labelsets)[i].json = (char *)malloc(jsonlen+1)) == NULL) {
		fprintf(stderr, "_pmUnpackLabelSet JSONB malloc(%d) failed: %s\n",
			jsonlen+1, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }

	    memcpy((void *)(*labelsets)[i].json, (void *)&tbuf[k], jsonlen);
	    (*labelsets)[i].json[jsonlen] = '\0';
	    k += jsonlen;

	    /* label nlabels */
	    nlabels = ntohl(*((unsigned int *)&tbuf[k]));
	    k += sizeof(nlabels);
	    (*labelsets)[i].nlabels = nlabels;

	    if (nlabels > 0) {
		(*labelsets)[i].labels = (pmLabel *)calloc(nlabels, sizeof(pmLabel));
		if ((*labelsets)[i].labels == NULL) {
		    fprintf(stderr, "_pmUnpackLabelSet label malloc(%lu) failed: %s\n",
			    nlabels * sizeof(pmLabel), strerror(errno));
		    abandon();
		    /*NOTREACHED*/
		}

		/* label pmLabels */
		for (j = 0; j < nlabels; j++) {
		    (*labelsets)[i].labels[j] = *((pmLabel *)&tbuf[k]);
		    _ntohpmLabel(&(*labelsets)[i].labels[j]);
		    k += sizeof(pmLabel);
		}
	    }
	}
    }
}


void
do_labelset(void)
{
    long		out_offset;
    unsigned int	type = 0;
    unsigned int	ident = 0;
    int			nsets = 0;
    pmLabelSet		*labellist = NULL;
    pmTimeval		stamp;
    //    labelspec_t	*ip;
    int			sts;
    //    int		i;
    //    int		j;
    //    int		need_alloc = 0;

    out_offset = __pmFtell(outarch.logctl.l_mdfp);

    _pmUnpackLabelSet(inarch.metarec, &type, &ident, &nsets, &labellist, &stamp);

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
     * libpcp, via __pmLogPutLabel(), assumes control of the storage pointed
     * to by labellist and inamelist.
     */
    if ((sts = __pmLogPutLabel(&outarch.archctl, type, ident, nsets, labellist, &stamp)) < 0) {
	char buf[1024];
	fprintf(stderr, "%s: Error: __pmLogPutLabel: %s: %s\n",
		pmGetProgname(),
		__pmLabelIdentString(ident, type, buf, sizeof(buf)),
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
	char buf[1024];
	fprintf(stderr, "Metadata: write LabelSet %s @ offset=%ld\n",
		__pmLabelIdentString(ident, type, buf, sizeof(buf)), out_offset);
    }
}
