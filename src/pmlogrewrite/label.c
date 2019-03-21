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
start_label(int type, int id, int instance, const char *instance_name, char *label, char *value)
{
    labelspec_t	*lp;
    char	buf[64];

    if (pmDebugOptions.appl4) {
	fprintf(stderr, "start_label(%s)",
		__pmLabelIdentString(id, type, buf, sizeof(buf)));
    }
    
    /*
     * If an instance name was specified, we need to convert it to
     * an instance id for this indom, if possible.
     */
    if (instance_name != NULL) {
	int		i;
	int		numinst;
	int		*instlist;
	char	**namelist;
	numinst = pmGetInDomArchive(id, &instlist, &namelist);
	if (numinst < 0) {
	    if (wflag) {
		pmsprintf(mess, sizeof(mess), "Instance domain %s: %s", pmInDomStr(id), pmErrStr(numinst));
		yywarn(mess);
	    }
	    return NULL;
	}
	for (i = 0; i < numinst; i++) {
	    if (inst_name_eq(namelist[i], instance_name) > 0) {
		instance = instlist[i];
		break;
	    }
	}
	free(instlist);
	free(namelist);
	if (i >= numinst) {
	    if (wflag) {
		pmsprintf(mess, sizeof(mess), "Instance domain %s: no instance \"%s\" found", pmInDomStr(id), instance_name);
		yywarn(mess);
	    }
	    return NULL;
	}
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
			if (pmDebugOptions.appl4) {
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

    if (pmDebugOptions.appl4)
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

static int
find_label(
    pmLabelSet *lsp,
    int label_ix,
    const char *label,
    const char *value
) {
    const pmLabel	*lp;
    int			label_len;
    int			value_len;

    /*
     * Find the specified label in the given label set beginning at the given
     * position.
     * - label_ix indicates where to begin the search.
     * - One of label or value may be NULL, but not both.
     * - If either is non-NULL, then we are looking for the particular label(s)
     *   with those names and values.
     * - Return the index of the found label or -1 if not found.
     *
     * We'll need these lengths later
     */
    label_len = label != NULL ? strlen(label) : 0;
    value_len = value != NULL ? strlen(value) : 0;

    /* Look for the next matching label. */
    for (/**/; label_ix < lsp->nlabels; ++label_ix) {
	lp = &lsp->labels[label_ix];

	/*
	 * Compare this label to the one we want.
	 * Careful: The offset and length of the label name in the label
	 * do not include the double quotes.
	 */
	if (label != NULL) {
	    if (label_len != lp->namelen + 2)
		continue; /* not this one */
	    if (memcmp(label, lsp->json + lp->name - 1, label_len) != 0)
		continue; /* not this one */
	}
	if (value != NULL) {
	    if (value_len != lp->valuelen)
		continue; /* not this one */
	    if (memcmp(value, lsp->json + lp->value, value_len) != 0)
		continue; /* not this one */
	}

	/* We've found our candidate. */
	return label_ix;
    }

    /* No matching label found. */
    return -1;
}

static void
extract_label(pmLabelSet *lsp, int label_ix)
{
    int		json_pos;
    int		json_size;
    pmLabel	*lp = & lsp->labels[label_ix];
    int		i;

    /*
     * If there is only one label remaining, then free the storage for the
     * label and the JSON.
     */
    if (lsp->nlabels == 1) {
	free(lsp->labels);
	free(lsp->json);
	/* Prevent double free by pmFreeLabelSets */
	lsp->nlabels = 0;
	lsp->json = NULL;
	return;
    }

    /*
     * There is more than one label remaining. We need to extract it from the
     * labelset.
     */

    /*
     * Shift the remaining data to fill the hole, if necessary.
     *
     * The size of the JSON being removed is the length of the name and
     * value plus the double quotes around the name, plus the colon, plus
     * The comma separating the previous/next label.
     */
    json_size = lp->namelen + lp->valuelen + 2 + 1 + 1;

    if (label_ix < lsp->nlabels - 1) {
	/*
	 * This label is not the last one in the list.
	 *
	 * The size of the JSON is the length of the name and
	 * value plus the double quotes around the name, plus the colon, plus
	 * the comma separating the next label.
	 */
	json_pos = lp->name - 1; /* one back for the double quote */
	memmove(lsp->json + json_pos, lsp->json + json_pos + json_size,
		lsp->jsonlen - (json_pos + json_size));

	/*
	 * Need to update the offsets of the names/values of the remaining
	 * labels.
	 */
	for (i = label_ix + 1; i < lsp->nlabels; ++i) {
	    lsp->labels[i].name -= json_size;
	    lsp->labels[i].value -= json_size;
	}

	/* Collapse the list of labels. */
	memmove(lsp->labels + label_ix, lsp->labels + label_ix + 1,
		(lsp->nlabels - label_ix - 1) * sizeof (*lsp->labels));

    }
    else {
	/*
	 * This is the last label in a list of more than one.
	 * We need only collapse the json string.
	 * The position of the data is two bytes back from the label
	 * name. One for the double quote and one for the preceding comma.
	 */
	assert(lsp->nlabels > 1);
	json_pos = lp->name - 2;
	memmove(lsp->json + json_pos, lsp->json + json_pos + json_size,
		lsp->jsonlen - (json_pos + json_size));
    }
    
    /* Now reallocate the remaining data to the new (smaller) size. */
    lsp->jsonlen -= json_size;
    lsp->json = realloc(lsp->json, lsp->jsonlen);
    if (lsp->json == NULL) {
	fprintf(stderr, "labelset json realloc(%d) failed: %s\n", lsp->jsonlen, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }

    --lsp->nlabels;
    lsp->labels = realloc(lsp->labels, lsp->nlabels * sizeof(*lsp->labels));
    if (lsp->labels == NULL) {
	fprintf(stderr, "labelset labels realloc(%d) failed: %s\n",
		(int)(lsp->nlabels * sizeof(*lsp->labels)), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
}

static pmLabelSet *
extract_labelset(int ls_ix, pmLabelSet **labellist, int *nsets)
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

static const char *
label_id_str(const labelspec_t *lp, int old)
{
    static char buf[1024];

    pmsprintf(buf, sizeof(buf), "label {");
    if (lp->old_label)
	pmsprintf(buf, sizeof(buf), "%s,", old ? lp->old_label : lp->new_label);
    else
	pmsprintf(buf, sizeof(buf), "ALL,");
    if (lp->old_value)
	pmsprintf(buf, sizeof(buf), "%s ", old ? lp->old_value : lp->new_value);
    else
	pmsprintf(buf, sizeof(buf), "ALL ");
    pmsprintf(buf, sizeof(buf), "}");

    return buf;
}

static const char *
label_association_str(const labelspec_t *lp, int old)
{
    static char buf[1024];

    if ((lp->old_type & PM_LABEL_CONTEXT))
	pmsprintf(buf, sizeof(buf), "context");
    else if ((lp->old_type & PM_LABEL_DOMAIN)) {
	pmsprintf(buf, sizeof(buf), "domain %d",
		  old ? pmID_domain(lp->old_id) : pmID_domain(lp->new_id));
    }
    else if ((lp->old_type & PM_LABEL_CLUSTER)) {
	pmsprintf(buf, sizeof(buf), "cluster %d.%d",
		  old ? pmID_domain(lp->old_id) : pmID_domain(lp->new_id),
		  old ? pmID_cluster (lp->old_id) : pmID_cluster (lp->new_id));
    }
    else if ((lp->old_type & PM_LABEL_ITEM)) {
	pmsprintf(buf, sizeof(buf), "item %s",
		  old ? pmIDStr(lp->old_id) : pmIDStr(lp->new_id));
    }
    else if ((lp->old_type & PM_LABEL_INDOM)) {
	pmsprintf(buf, sizeof(buf), "indom %s",
		  old ? pmInDomStr(lp->old_id) : pmInDomStr(lp->new_id));
    }
    else if ((lp->old_type & PM_LABEL_INSTANCES)) {
	if (old ) {
	    if (lp->old_instance)
		pmsprintf(buf, sizeof(buf), "instance %d ", lp->old_instance);
	    else
		pmsprintf(buf, sizeof(buf), "all instances ");
	    pmsprintf(buf, sizeof(buf), "of indom %s", pmInDomStr(lp->old_id));
	}
	else {
	    if (lp->new_instance)
		pmsprintf(buf, sizeof(buf), "instance %d ", lp->new_instance);
	    else
		pmsprintf(buf, sizeof(buf), "all instances ");
	    pmsprintf(buf, sizeof(buf), "of indom %s", pmInDomStr(lp->new_id));
	}
    }

    return buf;
}

static void
change_labels(pmLabelSet *lsp, const labelspec_t *lp)
{
    char	*current_name;
    pmLabel	*current_label;
    char	*new_json;
    int		target_label_len = 0;
    int		new_label_len;
    int		delta;
    int		i, j;

    /*
     * Change the labels in the given label set according to the given
     * change record.
     */
    if (lp->old_label != NULL)
	target_label_len = strlen(lp->old_label);

    for (i = 0; i < lsp->nlabels; ++i) {
	current_label = & lsp->labels[i];
	current_name = lsp->json + current_label->name;

	if (lp->old_label != NULL) {
	    /*
	     * A specific label has been specified on the change record.
	     * Make sure we have the matching one.
	     */
	    if (current_label->namelen != target_label_len ||
		memcmp (lp->old_label, current_name, target_label_len) != 0)
		continue; /* next label */
	}

	/*
	 * We have a matching label. Change its name. Do this by reallocating
	 * the JSON to the new size, transfering the old data as needed and then
	 * writing the new name. This code handles both the case where the size
	 * of the json grows and when it shrinks.
	 *
	 * Careful, the new_label includes the surrounding double quotes.
	 */
	new_label_len = strlen(lp->new_label) - 2;
	delta = new_label_len - current_label->namelen;
	if (delta != 0) {
	    /*
	     * Reallocate the JSON. We can't just use realloc(3) because
	     * the new size may be smaller, causing us to lose the data
	     * at the end before it can be shifted.
	     * Don't forget about the terminating nul.
	     */
	    new_json = malloc(lsp->jsonlen + 1 + delta);
	    if (new_json == NULL) {
		fprintf(stderr, "labelset JSON realloc malloc(%d) failed: %s\n", lsp->jsonlen + delta, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }

	    /*
	     * Transfer the existing data. This is always needed due to the
	     * JSON syntax surrounding the name/value pairs. Don't forget
	     * about the terminating nul byte.
	     */
	    memcpy(new_json, lsp->json, current_label->name);
	    memcpy(new_json + current_label->name + current_label->namelen + delta,
		   current_name + current_label->namelen,
		   lsp->jsonlen -
		   (current_label->name + current_label->namelen) +
		   1);
	    free(lsp->json);
	    lsp->json = new_json;
	    lsp->jsonlen += delta;
	    current_name = lsp->json + current_label->name;
	}

	/*
	 * Write the new name. Note that we don't need to worry about the
	 * double quotes, since the ones from the previous name have already
	 * been shifted into place, if necessary above.
	 *
	 * Careful, the new_label includes the surrounding double quotes, but
	 * the length has already been adjusted.
	 */
	memcpy(current_name, lp->new_label + 1, new_label_len);

	/*
	 * Update the length of the current name and the offsets of
	 * the names/values in subsequent labels.
	 */
	current_label->namelen += delta;
	for (j = i + 1; j < lsp->nlabels; ++j) {
	    lsp->labels[j].name += delta;
	    lsp->labels[j].value += delta;
	}

	if (lp->old_label != NULL)
	    break; /* there can only be one matching label */
    }
}

static void
change_values(pmLabelSet *lsp, const labelspec_t *lp)
{
    char	*current_value;
    pmLabel	*current_label;
    char	*new_json;
    int		target_value_len = 0;
    int		new_value_len;
    int		delta;
    int		i, j;

    /*
     * Change the values in the given label set according to the given
     * change record.
     */
    if (lp->old_value != NULL)
	target_value_len = strlen(lp->old_value);

    for (i = 0; i < lsp->nlabels; ++i) {
	current_label = & lsp->labels[i];
	current_value = lsp->json + current_label->value;

	if (lp->old_value != NULL) {
	    /*
	     * A specific value has been specified on the change record.
	     * Make sure we have the matching one.
	     */
	    if (current_label->valuelen != target_value_len ||
		memcmp (lp->old_value, current_value, target_value_len) != 0)
		continue; /* next label */
	}

	/*
	 * We have a matching label. Change its value. Do this by reallocating
	 * the JSON to the new size, transfering the old data as needed and then
	 * writing the new value. This code handles both the case where the size
	 * of the json grows and when it shrinks.
	 */
	new_value_len = strlen(lp->new_value);
	delta = new_value_len - current_label->valuelen;
	if (delta != 0) {
	    /*
	     * Reallocate the JSON. We can't just use realloc(3) because
	     * the new size may be smaller, causing us to lose the data
	     * at the end before it can be shifted.
	     * Don't forget about the terminating nul.
	     */
	    new_json = malloc(lsp->jsonlen + 1 + delta);
	    if (new_json == NULL) {
		fprintf(stderr, "labelset JSON realloc malloc(%d) failed: %s\n", lsp->jsonlen + delta, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }

	    /*
	     * Transfer the existing data. This is always needed due to the
	     * JSON syntax surrounding the name/value pairs. Don't forget
	     * about the terminating nul byte.
	     */
	    memcpy(new_json, lsp->json, current_label->value);
	    memcpy(new_json + current_label->value + current_label->valuelen + delta,
		   current_value + current_label->valuelen,
		   lsp->jsonlen -
		   (current_label->value + current_label->valuelen) +
		   1);
	    free(lsp->json);
	    lsp->json = new_json;
	    lsp->jsonlen += delta;
	    current_value = lsp->json + current_label->value;

	    /*
	     * Update the length of the current value and the offsets of
	     * the names/value in subsequent labels.
	     */
	    current_label->valuelen += delta;
	    for (j = i + 1; j < lsp->nlabels; ++j) {
		lsp->labels[j].name += delta;
		lsp->labels[j].value += delta;
	    }
	}

	/*
	 * Write the new value. Note that we don't need to worry about the
	 * double quotes, since the ones from the previous value have already
	 * been shifted into place, if necessary above.
	 */
	memcpy(current_value, lp->new_value, new_value_len);
    }
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
    int			label_ix;
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
	 * We can operate on the entire label record if no specific label/value
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
		    fprintf(stderr, "Delete: label sets for %s\n",
			    label_association_str(lp, 1/*old*/));
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
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: instance for label %s to %d\n",
				label_id_str(lp, 1/*old*/), lp->new_instance);
		    }
		    continue; /* next labelset */
		}

		/*
		 * All other operations require that we extract the current
		 * labelset from the label record.
		 */
		lsp = extract_labelset(ls_ix, &labellist, &nsets);

		if (flags & LABEL_DELETE) {
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Delete: labelset for %s\n",
				label_association_str(lp, 1/*old*/));
		    }
		    pmFreeLabelSets(lsp, 1);
		    if (nsets == 0)
			return; /* last labelset deleted */
		    --ls_ix; /* labelset was extracted */
		    continue; /* next labelset */
		}

		if ((flags & LABEL_CHANGE_ID)) {
		    /* The changed id will be written below. */
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: label set %s",
				__pmLabelIdentString(lp->old_id, lp->old_type,
						     buf, sizeof(buf)));
			fprintf(stderr, " to %s",
				__pmLabelIdentString(lp->new_id, lp->new_type,
						     buf, sizeof(buf)));
		    }
		}

		if ((flags & LABEL_CHANGE_INSTANCE)) {
		    lsp->inst = lp->new_instance;
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: instance for label %s to %d\n",
				label_id_str(lp, 1/*old*/), lp->new_instance);
		    }
		}

		if ((flags & LABEL_CHANGE_LABEL)) {
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: name for label %s to %s\n",
				label_id_str(lp, 1/*old*/), lp->new_label);
		    }
		    change_labels(lsp, lp);
		}

		if ((flags & LABEL_CHANGE_VALUE)) {
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: value for label %s to %s\n",
				label_id_str(lp, 1/*old*/), lp->new_value);
		    }
		    change_values(lsp, lp);
		}

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
	     * A specific label and/or value was specified. We need to operate
	     * on the individual labels selected. For all changes, it's probably
	     * easiest if we extract the label(s) from the current labelset
	     * and write the changed versions directly.
	     */
	    label_ix = 0;
	    while ((label_ix = find_label(lsp, label_ix, lp->old_label, lp->old_value)) != -1) {
		if (flags & LABEL_DELETE) {
		    /* Do nothing. The label will be extracted below.  */
		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Delete: label %s for %s\n",
				label_id_str(lp, 1/*old*/),
				label_association_str(lp, 1/*old*/));
		    }
		}
		else {
		    pmLabelSet	*new_labelset = NULL;
		    char	buf[PM_MAXLABELJSONLEN];
		    const char	*new_label;
		    const char	*new_value;
		    int		new_label_len;
		    int		new_value_len;

		    if (pmDebugOptions.appl1) {
			fprintf(stderr, "Rewrite: label %s for %s to %s\n",
				label_id_str(lp, 1/*old*/),
				label_association_str(lp, 1/*old*/),
				label_id_str(lp, 0/*new*/));
		    }

		    /*
		     * All other operations require that we contruct a new
		     * labelset containing the affected label.
		     */
		    if (lp->new_label != NULL) {
			new_label = lp->new_label;
			new_label_len = strlen(new_label);
		    }
		    else {
			new_label = lsp->json + lsp->labels[label_ix].name - 1;
			new_label_len = lsp->labels[label_ix].namelen + 2;
		    }
		    if (lp->new_value != NULL) {
			new_value = lp->new_value;
			pmsprintf(buf, sizeof(buf), "{%.*s:%s}",
				  new_label_len, new_label, new_value);
		    }
		    else {
			new_value = lsp->json + lsp->labels[label_ix].value;
			new_value_len = lsp->labels[label_ix].valuelen;
			pmsprintf(buf, sizeof(buf), "{%.*s:%.*s}",
				  new_label_len, new_label, new_value_len, new_value);
		    }
		    if ((sts = __pmAddLabels(&new_labelset, buf, lp->new_type)) < 0) {
			fprintf(stderr, "Unable to rewrite label %s: %s\n",
				buf, pmErrStr(sts));
			abandon();
			/*NOTREACHED*/
		    }
		    if ((flags & LABEL_CHANGE_INSTANCE))
			new_labelset->inst = lp->new_instance;
		    else
			new_labelset->inst = lsp->inst;

		    /*
		     * Write the new label.
		     * libpcp, via __pmLogPutLabel(), assumes control of the
		     * storage pointed to by new_labelset.
		     */
		    if ((sts = __pmLogPutLabel(&outarch.archctl, lp->new_type,
					       lp->new_id, 1, new_labelset,
					       &stamp)) < 0) {
			fprintf(stderr, "%s: Error: __pmLogPutLabel: %s: %s\n",
				pmGetProgname(),
				__pmLabelIdentString(lp->new_id, type,
						     buf, sizeof(buf)),
				pmErrStr(sts));
			abandon();
			/*NOTREACHED*/
		    }
		}

		/* Remove the old label from the original labeset */
		extract_label(lsp, label_ix);
	    }

	    /* Extract and free the original labelset if it is now empty. */
	    if (lsp->nlabels == 0) {
		lsp = extract_labelset(ls_ix, &labellist, &nsets);
		pmFreeLabelSets(lsp, 1);
		--ls_ix;
	    }

	} /* Loop over labelsets */
    } /* Loop over change records */

    /*
     * Write what remains of the label record, if anything.
     * libpcp, via __pmLogPutLabel(), assumes control of the storage pointed
     * to by labellist.
     * If nsets has been reduced to zero, then labellist is already NULL.
     */
    if (labellist != NULL) {
	assert(nsets > 0);
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
