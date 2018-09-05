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
#include <assert.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"

/*
 * Find or create a new labelspec_t
 */
labelspec_t *
start_label(int type, int id, int instance, char *label, char *value)
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
		if (instance == -1 || instance == lp->old_instance) {
		    if ((label == NULL ||
			 (lp->old_label != NULL && strcmp (label, lp->old_label) == 0)) &&
			(value == NULL ||
			 (lp->old_value != NULL && strcmp (value, lp->old_value) == 0))) {
			if (pmDebugOptions.appl0 && pmDebugOptions.appl1) {
			    fprintf(stderr, " -> %s",
				    __pmLabelIdentString(lp->new_id, lp->new_type,
							 buf, sizeof(buf)));
			}
			return lp;
		    }
		}
	    }
	}
    }

    /* The label set was not found. Create a new change spec. */
    lp = create_label(type, id, instance, label, value);
    return lp;
}

labelspec_t *
create_label(int type, int id, int instance, char *label, char *value)
{
    labelspec_t	*lp;

    /* Create a new change spec. */
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
    lp->old_instance = lp->new_instance = instance;
    lp->old_label = label;
    lp->old_value = value;
    lp->new_label = NULL;
    lp->new_value = NULL;
    lp->new_labels = NULL;
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

#if 0
static int
find_label (
    const char *label,
    const char *value,
    int nsets,
    const pmLabelSet *labellist,
    int *labelset_ix,
    int *label_ix)
{
    const pmLabelSet	*lsp;
    const pmLabel	*lp;

    /*
     * Find the specified label in the given label set beginning at the given
     * position.
     * - When starting at the beginning, both labelset_ix and label_ix will be
     *   -1.
     * - When resuming the search, they will indicate the position of the last
     *   label found.
     * - One or both of label and value may be NULL
     *   - If both are NULL, then we're not looking for any label in particular.
     *     In this case, for the initial search  just return 1 for success, and
     *     for any subsequent search, return 0.
     *   - If either is non-NULL, then we are looking for the particular label(s)
     *     with those names and values.
     */
    if (*labelset_ix == -1) {
	*labelset_ix = 0;
	*label_ix = 0;
	if (label == NULL && value == NULL)
	    return 1; /* success */
    }
    else {
	if (label == NULL && value == NULL)
	    return 0; /* not found */
	++*label_ix;
    }

    /* We'll need these later */
    if (label != NULL)
	label_len = strlen(label);
    if (value != NULL)
	value_len = strlen(value);

    /* Look for the next matching label. */
    while (*labelset_ix < nsets) {
	lsp = &labellist[*labelset_ix];
	while (*label_ix < lsp->nlabels) {
	    lp = &lsp->labels[*label_ix];
	}

	/* Look in the next label set, if there is one. */
	++*labelset_ix;
    }

    return 0;
}
#endif

static pmLabelSet *
extract_labelset (int ls_ix, pmLabelSet **labellist, int *nsets)
{
    pmLabelSet *extracted;

    /* if there is only one label set in the list, then just return it. */
    if (*nsets == 1) {
	extracted = *labellist;
	*nsets = 0;
	*labellist = NULL;
	return extracted;
    }
    
    /*
     * Make a copy of the selected labelset. We can just use a struct assignment.
     */
    extracted = malloc(sizeof(*extracted));
    *extracted = (*labellist)[ls_ix];

    /* Now collapse the list, if necessary */
    if (ls_ix < *nsets - 1)
	memmove(*labellist + ls_ix, *labellist + ls_ix + 1,
		(*nsets - ls_ix - 1) * sizeof(**labellist));

    /* Reallocate the list to its new size */
    --*nsets;
    *labellist = realloc(*labellist, *nsets * sizeof(**labellist));
    if (*labellist == NULL) {
	fprintf(stderr, "labelset realloc(%d) failed: %s\n", (int)(*nsets * sizeof(**labellist)), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    
    return extracted;
}

void
do_labelset(void)
{
    long		out_offset;
    unsigned int	type = 0;
    unsigned int	ident = 0;
    int			nsets = 0;
    int			flags = 0;
    pmLabelSet		*labellist;
    pmLabelSet		*lsp;
    pmTimeval		stamp;
    labelspec_t		*lp;
    int			full_record;
    int			ls_ix;
    int			sts;
    char		buf[64];

    out_offset = __pmFtell(outarch.logctl.l_mdfp);

    _pmUnpackLabelSet(inarch.metarec, &type, &ident, &nsets, &labellist, &stamp);

    /*
     * Global time stamp adjustment (if any) has already been done in the
     * PDU buffer, so this is reflected in the unpacked value of stamp.
     */
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	/* Check the label type and id for a match. */
	if (lp->old_type != type)
	    continue;
	if (lp->old_id != ident)
	    continue;

	/* We are not interested in LABEL_NEW operations here. */
	flags = lp->flags & ~LABEL_NEW;
	if (flags == 0)
	    continue;

	/*
	 * We can Operate on the entire label record if no specific label/value
	 * was specified on the change record AND if no specific instance
	 * was specified or all of the instances in the labelsets match.
	 */
	full_record = 0;
	if (lp->old_label == NULL && lp->old_value == NULL) {
	    if (lp->old_type != PM_LABEL_INSTANCES)
		full_record = 1;
	    else if (lp->old_instance == -1)
		full_record = 1;
	    else {
		for (ls_ix = 0; ls_ix < nsets; ++ls_ix) {
		    lsp = & labellist[ls_ix];
		    if (lp->old_instance != lsp->inst)
			break;
		}
		if (ls_ix >= nsets)
		    full_record = 1;
	    }
	}

	/* Perform any full record operations here, if able. */
	if (full_record) {
	    if ((flags & LABEL_DELETE)) {
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "Delete: label set for ");
		    if ((lp->old_type & PM_LABEL_CONTEXT))
			fprintf(stderr, " context\n");
		    else if ((lp->old_type & PM_LABEL_DOMAIN))
			fprintf(stderr, " domain %d\n", pmID_domain(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_CLUSTER))
			fprintf(stderr, " item %d.%d\n", pmID_domain(lp->old_id), pmID_cluster (lp->old_id));
		    else if ((lp->old_type & PM_LABEL_ITEM))
			fprintf(stderr, " item %s\n", pmIDStr(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_INDOM))
			fprintf(stderr, " indom %s\n", pmInDomStr(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_INSTANCES))
			fprintf(stderr, " the instances of indom %s\n", pmInDomStr(lp->old_id));
		}
		pmFreeLabelSets(labellist, nsets);
		return;
	    }

	    /* Rewrite the id as specified. */
	    if ((flags & LABEL_CHANGE_ID))
		ident = lp->new_id;
	}

	/*
	 * We're operating on a specific labelsets and/or labels within the
	 * label record. We need to iterate over the affected labels in each
	 * affected labelset.
	 */
	for (ls_ix = 0; ls_ix < nsets; ++ls_ix) {
	    lsp = & labellist[ls_ix];
	    /*
	     * If the change record is for a specific indom instance, then the
	     * instance in the labelset must match.
	     */
	    if (lp->old_type == PM_LABEL_INSTANCES &&
		lp->old_instance != -1 && lp->old_instance != lsp->inst)
		continue;

	    /*
	     * If no specific label/value was specified, then we're operating
	     * on the entire labelset.
	     */
	    if (lp->old_label == NULL && lp->old_value == NULL) {
		/*
		 * If all we're doing is changing the instance for this label
		 * set, then it can be done in place.
		 */
		if (flags == LABEL_CHANGE_INSTANCE) {
		    lsp->inst = lp->new_instance;
		    continue; /* next labelset */
		}

		/*
		 * All other operations require that we extract the current
		 * labelset from the label record.
		 */
		lsp = extract_labelset (ls_ix, & labellist, & nsets);

		if (flags & LABEL_DELETE) {
		    pmFreeLabelSets(lsp, 1);
		    if (nsets == 1)
			return; /* last labelset deleted */
		    --ls_ix; /* labelset was extracted */
		    continue; /* next labelset */
		}

		if ((flags & LABEL_CHANGE_INSTANCE))
		    lsp->inst = lp->new_instance;

		/*
		 * We know that there is another operation to perform and that it
		 * is LABEL_CHANGE_ID. Otherwise the instance would have been
		 * changed in place aove.
		 * The changed id will be written below.
		 */
		assert((flags & LABEL_CHANGE_ID));

		/*
		 * Write the extracted labelset here.
		 * libpcp, via __pmLogPutLabel(), assumes control of the
		 * storage pointed to by lsp.
		 */
		if ((sts = __pmLogPutLabel(&outarch.archctl, type,
					   lp->new_id, 1, lsp,
					   &stamp)) < 0) {
		    fprintf(stderr, "%s: Error: __pmLogPutLabel: %s: %s\n",
			    pmGetProgname(),
			    __pmLabelIdentString(lp->new_id, type,
						 buf, sizeof(buf)),
			    pmErrStr(sts));
		    abandon();
		    /*NOTREACHED*/
		}
			    
		if (pmDebugOptions.appl0) {
		    fprintf(stderr, "Metadata: write LabelSet %s @ offset=%ld\n",
			    __pmLabelIdentString(lp->new_id, type,
						 buf, sizeof(buf)),
			    out_offset);
		}

		--ls_ix; /* labelset was extracted */
		continue; /* next labelset */
	    } /* operating on entire labelset */
	    
	    /*
	     * Iterate over a specific label and/or value was specified
	     */
#if 0
	    if (flags & LABEL_DELETE) {
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "Delete: label {");
		    if (lp->old_label)
			fprintf(stderr, "\"%s\",", lp->old_label);
		    else
			fprintf(stderr, "ALL,");
		    if (lp->old_value)
			fprintf(stderr, "\"%s\" ", lp->old_value);
		    else
			fprintf(stderr, "ALL ");
		    fprintf(stderr, "} for ");
		    if ((lp->old_type & PM_LABEL_CONTEXT))
			fprintf(stderr, "context");
		    else if ((lp->old_type & PM_LABEL_DOMAIN))
			fprintf(stderr, "domain %d", pmID_domain(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_CLUSTER))
			fprintf(stderr, "cluster %d.%d", pmID_domain(lp->old_id), pmID_cluster (lp->old_id));
		    else if ((lp->old_type & PM_LABEL_ITEM))
			fprintf(stderr, "item %s", pmIDStr(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_INDOM))
			fprintf(stderr, "indom %s", pmInDomStr(lp->old_id));
		    else if ((lp->old_type & PM_LABEL_INSTANCES)) {
			if (lp->old_instance)
			    fprintf(stderr, "instance %d ", lp->old_instance);
			else
			    fprintf(stderr, "all instances ");
			fprintf(stderr, "of indom %s", pmInDomStr(lp->old_id));
		    }
		    fputc('\n', stderr);
		}

		/* Delete the selected label(set)(s). */
		if (labelset_ix == -1) {
		    /* Free the entire label set. */
		    pmFreeLabelSets(labellist, nsets);
		    continue; /* next change record */
		}

		/*
		 * We're deleting an individual label. It needs to be extracted
		 * from the label set data structure.
		 */
		   
	    }

	    /* Rewrite the record as specified. */
	    if ((flags & LABEL_CHANGE_ID))
		ident = lp->new_id;
	
	    if (pmDebugOptions.appl1) {
		if ((flags & LABEL_CHANGE_ANY)) {
		    fprintf(stderr, "Rewrite: label set %s",
			    __pmLabelIdentString(lp->old_id, lp->old_type,
						 buf, sizeof(buf)));
		}
		if ((flags & (LABEL_CHANGE_LABEL | LABEL_CHANGE_VALUE))) {
		    fprintf(stderr, " \"%s\"", lp->old_label);
		}
		if ((flags & LABEL_CHANGE_ANY)) {
		    fprintf(stderr, " to\nlabel set %s",
			    __pmLabelIdentString(lp->new_id, lp->new_type,
						 buf, sizeof(buf)));
		}
		if ((flags & (LABEL_CHANGE_LABEL | LABEL_CHANGE_VALUE))) {
		    fprintf(stderr, " \"%s\"\"%s\"", lp->new_label, lp->new_value);
		}
		if ((flags & LABEL_CHANGE_ANY))
		    fputc('\n', stderr);
	    }
#endif
	} /* Loop over labelsets */
    } /* Loop over change records */

    /*
     * Write what remains of the label record, if anything.
     * libpcp, via __pmLogPutLabel(), assumes control of the storage pointed
     * to by labellist.
     */
    if (nsets > 0) {
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
}
