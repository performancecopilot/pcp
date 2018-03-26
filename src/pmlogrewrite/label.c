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

/*
 * Find or create a new labelspec_t
 */
labelspec_t *
start_label(int type, int id)
{
    labelspec_t	*lp;
    char	buf[64];

    if (pmDebugOptions.appl0 && pmDebugOptions.appl1) {
	fprintf(stderr, "start_label(%s)",
		__pmLabelIdentString(id, type, buf, sizeof(buf)));
    }

    /* Search for this help label in the existing list of changes. */
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (type == lp->old_type) {
	    if (id == lp->old_id) {
		if (pmDebugOptions.appl0 && pmDebugOptions.appl1) {
		    fprintf(stderr, " -> %s",
			    __pmLabelIdentString(lp->new_id, lp->new_type,
						 buf, sizeof(buf)));
		}
		return lp;
	    }
	}
    }

    /* The label set was not found. Create a new change spec. */
    lp = (labelspec_t *)malloc(sizeof(labelspec_t));
    if (lp == NULL) {
	fprintf(stderr, "labelspec malloc(%d) failed: %s\n", (int)sizeof(labelspec_t), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }

    /* Initialize and link. */
    lp->l_next = label_root;
    label_root = lp;
    lp->old_type = lp->new_type = type;
    lp->old_id = lp->new_id = id;
    lp->new_label = NULL;
    lp->flags = 0;
    lp->ip = NULL;

    if (pmDebugOptions.appl0 && pmDebugOptions.appl1)
	fprintf(stderr, " -> [new entry]\n");

    return lp;
}

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
		    fprintf(stderr, "_pmUnpackLabelSet label malloc(%d) failed: %s\n",
			    (int)(nlabels * sizeof(pmLabel)), strerror(errno));
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
    labelspec_t		*lp;
    int			sts;
    char		buf[64];

    out_offset = __pmFtell(outarch.logctl.l_mdfp);

    _pmUnpackLabelSet(inarch.metarec, &type, &ident, &nsets, &labellist, &stamp);

    /*
     * Global time stamp adjustment (if any) has already been done in the
     * PDU buffer, so this is reflected in the unpacked value of stamp.
     */
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (lp->old_id != ident)
	    continue;
	if (lp->old_type != type)
	    continue;

	/* Rewrite the record as specified. */
	if ((lp->flags & LABEL_CHANGE_ID))
	    ident = lp->new_id;
	if ((lp->flags & LABEL_CHANGE_TYPE))
	    type = lp->new_type;
#if 0 /* not supported yet */
	if ((lp->flags & LABEL_CHANGE_LABEL))
	    buffer = lp->new_label;
#endif
	
	if (pmDebugOptions.appl1) {
	    if ((lp->flags & (LABEL_CHANGE_ID | LABEL_CHANGE_TYPE | LABEL_CHANGE_LABEL))) {
		fprintf(stderr, "Rewrite: label set %s",
			__pmLabelIdentString(lp->old_id, lp->old_type,
					     buf, sizeof(buf)));
	    }
	    if ((lp->flags & (LABEL_CHANGE_LABEL))) {
		fprintf(stderr, " \"%s\"", lp->old_label);
	    }
	    if ((lp->flags & (LABEL_CHANGE_ID | LABEL_CHANGE_TYPE | LABEL_CHANGE_LABEL))) {
		fprintf(stderr, " to\nlabel set %s",
			__pmLabelIdentString(lp->new_id, lp->new_type,
					     buf, sizeof(buf)));
	    }
	    if ((lp->flags & (LABEL_CHANGE_LABEL))) {
		fprintf(stderr, " \"%s\"", lp->new_label);
	    }
	    if ((lp->flags & (LABEL_CHANGE_ID | LABEL_CHANGE_TYPE | LABEL_CHANGE_LABEL)))
		fputc('\n', stderr);
	}
    }

    /*
     * libpcp, via __pmLogPutLabel(), assumes control of the storage pointed
     * to by labellist and inamelist.
     */
    if ((sts = __pmLogPutLabel(&outarch.archctl, type, ident, nsets, labellist, &stamp)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutLabel: %s: %s\n",
		pmGetProgname(),
		__pmLabelIdentString(ident, type, buf, sizeof(buf)),
		pmErrStr(sts));
	abandon();
	/*NOTREACHED*/
    }

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Metadata: write LabelSet %s @ offset=%ld\n",
		__pmLabelIdentString(ident, type, buf, sizeof(buf)), out_offset);
    }
}
