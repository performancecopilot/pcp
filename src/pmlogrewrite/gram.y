/*
 * Copyright (c) 2013-2018 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

%{
/*
 *  pmlogrewrite parser
 */
#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"
#include <errno.h>
#include <assert.h>

#define PM_TEXT_TYPE_MASK (PM_TEXT_ONELINE | PM_TEXT_HELP)

static indomspec_t	*current_indomspec;
static int		current_star_indom;
static int		do_walk_indom;
static int		star_domain;

static metricspec_t	*current_metricspec;
static int		current_star_metric;
static int		star_cluster;
static int		do_walk_metric;
static int		output = OUTPUT_ALL;
static int		one_inst;
static char		*one_name;

static textspec_t	*current_textspec;
static int		do_walk_text;

static int		current_label_id;
static char *		current_label_name;
static char *		current_label_value;
static int		current_label_instance;
static char *		current_label_instance_name;
static labelspec_t	*current_labelspec;
static int		do_walk_label;

indomspec_t *
walk_indom(int mode)
{
    static indomspec_t	*ip;

    if (do_walk_indom) {
	if (mode == W_START)
	    ip = indom_root;
	else
	    ip = ip->i_next;
	while (ip != NULL && pmInDom_domain(ip->old_indom) != star_domain)
	    ip = ip->i_next;
    }
    else {
	if (mode == W_START)
	    ip = current_indomspec;
	else
	    ip = NULL;
    }

    return ip;
}

metricspec_t *
walk_metric(int mode, int flag, char *which, int dupok)
{
    static metricspec_t	*mp;

    if (do_walk_metric) {
	if (mode == W_START)
	    mp = metric_root;
	else
	    mp = mp->m_next;
	while (mp != NULL) {
	    if (pmID_domain(mp->old_desc.pmid) == star_domain &&
		(star_cluster == PM_ID_NULL || star_cluster == pmID_cluster(mp->old_desc.pmid)))
		break;
	    mp = mp->m_next;
	}
    }
    else {
	if (mode == W_START)
	    mp = current_metricspec;
	else
	    mp = NULL;
    }

    if (mp != NULL) {
	if (!dupok && (mp->flags & flag)) {
	    pmsprintf(mess, sizeof(mess), "Duplicate %s clause for metric %s", which, mp->old_name);
	    yyerror(mess);
	}
	if (flag != METRIC_DELETE) {
	    if (mp->flags & METRIC_DELETE) {
		pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted metric %s", which, mp->old_name);
		yyerror(mess);
	    }
	}
	else {
	    if (mp->flags & (~METRIC_DELETE)) {
		pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for metric %s", mp->old_name);
		yyerror(mess);
	    }
	}
    }

    return mp;
}

static const char *
textTypeStr(int type)
{
    switch(type)
	{
	case PM_TEXT_ONELINE:
	    return "one line";
	case PM_TEXT_HELP:
	    return "help";
	default:
	    break;
	}

    return "unknown";
}
 
textspec_t *
walk_text(int mode, int flag, char *which, int dupok)
{
    static textspec_t	*tp;

    if (do_walk_text) {
	if (mode == W_START)
	    tp = text_root;
	else
	    tp = tp->t_next;

	/* Only consider the active specs. */
	while (tp != NULL && (tp->flags & TEXT_ACTIVE) == 0)
	    tp = tp->t_next;
    }
    else {
	if (mode == W_START) {
	    tp = current_textspec;
	    if (tp)
		assert ((tp->flags & TEXT_ACTIVE));
	}
	else
	    tp = NULL;
    }

    if (tp != NULL) {
	tp->flags &= ~TEXT_ACTIVE;
	
	if (!dupok && (tp->flags & flag)) {
	    if ((tp->old_type & PM_TEXT_PMID)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for %s text for metric %s",
			  which, textTypeStr(tp->old_type), pmIDStr(tp->old_id));
	    }
	    else {
		assert((tp->old_type & PM_TEXT_INDOM));
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for %s text for indom %s",
			  which, textTypeStr(tp->old_type), pmInDomStr(tp->old_id));
	    }
	    yyerror(mess);
	}
	if (flag != TEXT_DELETE) {
	    if (tp->flags & TEXT_DELETE) {
		if ((tp->old_type & PM_TEXT_PMID)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted %s text for metric %s",
			      which, textTypeStr(tp->old_type), pmIDStr(tp->old_id));
		}
		else {
		    assert((tp->old_type & PM_TEXT_INDOM));
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted %s text for indom %s",
			      which, textTypeStr(tp->old_type), pmInDomStr(tp->old_id));
		}
		yyerror(mess);
	    }
	}
	else {
	    if (tp->flags & (~TEXT_DELETE)) {
		if ((tp->old_type & PM_TEXT_PMID)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for %s text for metric %s",
			      textTypeStr(tp->old_type), pmIDStr(tp->old_id));
		}
		else {
		    assert((tp->old_type & PM_TEXT_INDOM));
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for %s text for indom %s",
			      textTypeStr(tp->old_type), pmInDomStr(tp->old_id));
		}
		yyerror(mess);
	    }
	}
    }

    return tp;
}

labelspec_t *
walk_label(int mode, int flag, char *which, int dupok)
{
    static labelspec_t	*lp;

    if (do_walk_label > 1) {
	if (mode == W_START)
	    lp = label_root;
	else
	    lp = lp->l_next;

	/* Only consider the active specs. */
	while (lp != NULL && (lp->flags & LABEL_ACTIVE) == 0)
	    lp = lp->l_next;
    }
    else {
	if (mode == W_START) {
	    lp = current_labelspec;
	    if (lp)
		assert ((lp->flags & LABEL_ACTIVE));
	}
	else
	    lp = NULL;
    }

    if (lp != NULL) {
	lp->flags &= ~LABEL_ACTIVE;
	
	if (!dupok && (lp->flags & flag)) {
	    if ((lp->old_type & PM_LABEL_CONTEXT)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for context label",
			  which);
	    }
	    else if ((lp->old_type & PM_LABEL_DOMAIN)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for label for domain %d",
			  which, pmID_domain(lp->old_id));
	    }
	    else if ((lp->old_type & PM_LABEL_CLUSTER)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for label for cluster %d.%d",
			  which, pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
	    }
	    else if ((lp->old_type & PM_LABEL_ITEM)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for label for metric %s",
			  which, pmIDStr(lp->old_id));
	    }
	    else if ((lp->old_type & PM_LABEL_INDOM)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for label for indom %s",
			  which, pmInDomStr(lp->old_id));
	    }
	    else if ((lp->old_type & PM_LABEL_INSTANCES)) {
		pmsprintf(mess, sizeof(mess), "Duplicate %s clause for label for the instances of indom %s",
			  which, pmInDomStr(lp->old_id));
	    }
	    yyerror(mess);
	}
	if (flag != LABEL_DELETE) {
	    if (lp->flags & LABEL_DELETE) {
		if ((lp->old_type & PM_LABEL_CONTEXT)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted context label",
			      which);
		}
		else if ((lp->old_type & PM_LABEL_DOMAIN)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted label for domain %d",
			      which, pmID_domain(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_CLUSTER)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted label for cluster %d.%d",
			      which, pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_ITEM)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted label for metric %s",
			      which, pmIDStr(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_INDOM)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted label for indom %s",
			      which, pmInDomStr(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_INSTANCES)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted label for the instances of indom %s",
			      which, pmInDomStr(lp->old_id));
		}
		yyerror(mess);
	    }
	}
	else {
	    if (lp->flags & (~LABEL_DELETE)) {
		if ((lp->old_type & PM_LABEL_CONTEXT)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for context label");
		}
		else if ((lp->old_type & PM_LABEL_DOMAIN)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for label for domain %d",
			      pmID_domain(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_CLUSTER)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for label for cluster %d.%d",
			      pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_ITEM)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for label for metric %s",
			      pmIDStr(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_INDOM)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for label for indom %s",
			      pmInDomStr(lp->old_id));
		}
		else if ((lp->old_type & PM_LABEL_INSTANCES)) {
		    pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for label for the instances of indom %s",
			      pmInDomStr(lp->old_id));
		}
		yyerror(mess);
	    }
	}
    }

    return lp;
}

void
deactivate_labels(void)
{
    labelspec_t	*lp;
    for (lp = walk_label(W_START, LABEL_ACTIVE, "active", 0); lp != NULL; lp = walk_label(W_NEXT, 0, "", 0)) {
	lp->flags &= ~LABEL_ACTIVE;
    }
}

void
new_context_label()
{
    labelspec_t	*lp;
    int		sts;
    char	buf[PM_MAXLABELJSONLEN];

    /*
     * Ignore the other change specs which were identified and
     * add the new label to the generic context label change spec.
     * Search for an existing one first.
     */
    deactivate_labels();
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (lp->old_type == PM_LABEL_CONTEXT) {
	    assert (lp->old_id == PM_ID_NULL);
	    if (lp->old_label == NULL)
		break;
	}
    }

    /* Create one if none exists. */
    if (lp == NULL)
	lp = create_label(PM_LABEL_CONTEXT, PM_ID_NULL, 0, NULL, NULL);

    /* Add the new label to the label change spec. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);
    if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_CONTEXT)) < 0) {
	pmsprintf(mess, sizeof(mess),
		  "Unable to add new context label %s: %s",
		  buf, pmErrStr(sts));
	yyerror(mess);
    }
    lp->new_labels->inst = 0;
    lp->flags |= LABEL_NEW;
}

void
new_domain_label(int domain)
{
    labelspec_t		*lp;
    int			sts;
    int			found = 0;
    __pmContext		*ctxp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    char		buf[PM_MAXLABELJSONLEN];

    /*
     * Ignore the change specs which were previously identified and instead
     * search the metadata for metrics in the given domain.
     * Create or re-use the change record for each one found. 
     */
    deactivate_labels();

    ctxp = __pmHandleToPtr(pmWhichContext());
    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    /* Prepare the JSON for the new label. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);

    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	if (pmID_domain(node->key) != domain)
	    continue;

	/*
	 * A metric in this domain exists.
	 * Add the new label to the generic label change spec for this
	 * domain. Search for an existing one first.
	 */
	found = 1;
	for (lp = label_root; lp != NULL; lp = lp->l_next) {
	    if (lp->old_type != PM_LABEL_DOMAIN)
		continue;
	    if (lp->old_id != domain)
		continue;
	    if (lp->old_label == NULL)
		break;
	}

	/* Create one if none exists. */
	if (lp == NULL)
	    lp = create_label(PM_LABEL_DOMAIN, domain, 0, NULL, NULL);

	/* Add the new label to the label change spec. */
	if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_DOMAIN)) < 0) {
	    pmsprintf(mess, sizeof(mess),
		      "Unable to add new domain label %s: %s",
		      buf, pmErrStr(sts));
	    yyerror(mess);
	}
	lp->new_labels->inst = 0;
	lp->flags |= LABEL_NEW;

	/* We only need to find one metric in the specified domain. */
	break;
    }

    /* Did we find any clusters matching the spec? */
    if (! found) {
	pmsprintf(mess, sizeof(mess),
		  "No matching domain for new label %s", buf);
	yywarn(mess);
    }
}

void
new_cluster_label(int cluster)
{
    labelspec_t		*lp;
    int			sts;
    int			found = 0;
    __pmContext		*ctxp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    char		buf[PM_MAXLABELJSONLEN] = "";

    /*
     * Ignore the change specs which were previously identified and instead
     * search the metadata for metrics in the given domain.
     * Create or re-use the change record for each one found. 
     */
    deactivate_labels();

    ctxp = __pmHandleToPtr(pmWhichContext());
    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    /* Prepare the JSON for the new label. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);

    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	if (pmID_domain(node->key) != pmID_domain(cluster))
	    continue;
	if (! current_star_metric && 
	    pmID_cluster(node->key) != pmID_cluster(cluster))
	    continue;

	/*
	 * A metric in this cluster exists.
	 * Add the new label to the generic label change spec for the
	 * identified cluster. Search for an existing one first.
	 */
	found = 1;
	for (lp = label_root; lp != NULL; lp = lp->l_next) {
	    if (lp->old_type != PM_LABEL_CLUSTER)
		continue;
	    if (pmID_domain(lp->old_id) != pmID_domain((pmID)(node->key)))
		continue;
	    if (pmID_cluster(lp->old_id) != pmID_cluster((pmID)(node->key)))
		continue;
	    if (lp->old_label == NULL)
		break;
	}

	if (lp == NULL) {
	    pmID current_cluster =
		pmID_build(pmID_domain(node->key), pmID_cluster(node->key), 0);
	    lp = create_label(PM_LABEL_CLUSTER, current_cluster, 0, NULL, NULL);
	}

	/* Add the new label to the label change spec. */
	if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_CLUSTER)) < 0) {
	    pmsprintf(mess, sizeof(mess),
		      "Unable to add new cluster label %s: %s",
		      buf, pmErrStr(sts));
	    yyerror(mess);
	}
	lp->new_labels->inst = 0;
	lp->flags |= LABEL_NEW;

	/* Do we need to look for more clusters? */
	if (! current_star_metric)
	    break;
    }

    /* Did we find any clusters matching the spec? */
    if (! found) {
	pmsprintf(mess, sizeof(mess),
		  "No matching cluster for new label %s", buf);
	yywarn(mess);
    }
}

void
new_item_label(int item)
{
    labelspec_t		*lp;
    int			sts;
    int			found = 0;
    __pmContext		*ctxp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    char		buf[PM_MAXLABELJSONLEN] = "";

    /*
     * Ignore the change specs which were previously identified and instead
     * search the metadata for metrics in the given domain.
     * Create or re-use the change record for each one found. 
     */
    deactivate_labels();

    ctxp = __pmHandleToPtr(pmWhichContext());
    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    /* Prepare the JSON for the new label. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);

    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	if (pmID_domain(node->key) != pmID_domain(item))
	    continue;
	if (current_star_metric < 2) {
	    if (pmID_cluster(node->key) != pmID_cluster(item))
		continue;
	    if (! current_star_metric && (pmID)(node->key) != (pmID)item)
		continue;
	}

	/*
	 * A metric with this pmID exists.
	 * Add the new label to the generic label change spec for the
	 * identified item. Search for an existing one first.
	 */
	found = 1;
	for (lp = label_root; lp != NULL; lp = lp->l_next) {
	    if (lp->old_type != PM_LABEL_ITEM)
		continue;
	    if ((pmID)(lp->old_id) != (pmID)(node->key))
		continue;
	    if (lp->old_label == NULL)
		break;
	}

	if (lp == NULL)
	    lp = create_label(PM_LABEL_ITEM, (pmID)node->key, 0, NULL, NULL);

	/* Add the new label to the label change spec. */
	if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_ITEM)) < 0) {
	    pmsprintf(mess, sizeof(mess),
		      "Unable to add new metric item label %s: %s",
		      buf, pmErrStr(sts));
	    yyerror(mess);
	}
	lp->new_labels->inst = 0;
	lp->flags |= LABEL_NEW;

	/* Do we need to look for more items? */
	if (! current_star_metric)
	    break;
    }

    /* Did we find any items matching the spec? */
    if (! found) {
	pmsprintf(mess, sizeof(mess),
		  "No matching metric for new label %s", buf);
	yywarn(mess);
    }
}

void
new_indom_label(int indom)
{
    labelspec_t		*lp;
    int			sts;
    int			found = 0;
    __pmContext		*ctxp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    char		buf[PM_MAXLABELJSONLEN] = "";

    /*
     * Ignore the change specs which were previously identified and instead
     * search the metadata for metrics in the given domain.
     * Create or re-use the change record for each one found. 
     */
    deactivate_labels();

    ctxp = __pmHandleToPtr(pmWhichContext());
    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    /* Prepare the JSON for the new label. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);

    hcp = &ctxp->c_archctl->ac_log->l_hashindom;
    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	if (pmInDom_domain(node->key) != pmInDom_domain(indom))
	    continue;
	if (! current_star_indom && 
	    pmInDom_serial(node->key) != pmInDom_serial(indom))
	    continue;

	/*
	 * A metric in this indom exists.
	 * Add the new label to the generic label change spec for the
	 * identified indom. Search for an existing one first.
	 */
	found = 1;
	for (lp = label_root; lp != NULL; lp = lp->l_next) {
	    if (lp->old_type != PM_LABEL_INDOM)
		continue;
	    if (pmInDom_domain(lp->old_id) != pmInDom_domain((pmInDom)(node->key)))
		continue;
	    if (pmInDom_serial(lp->old_id) != pmInDom_serial((pmInDom)(node->key)))
		continue;
	    if (lp->old_instance != -1)
		continue;
	    if (lp->old_label == NULL)
		break;
	}

	if (lp == NULL) {
	    pmInDom current_indom =
		pmInDom_build(pmInDom_domain(node->key), pmInDom_serial(node->key));
	    lp = create_label(PM_LABEL_INDOM, current_indom, 0, NULL, NULL);
	}

	/* Add the new label to the label change spec. */
	if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_INDOM)) < 0) {
	    pmsprintf(mess, sizeof(mess),
		      "Unable to add new indom label %s: %s",
		      buf, pmErrStr(sts));
	    yyerror(mess);
	}
	lp->new_labels->inst = 0;
	lp->flags |= LABEL_NEW;

	/* Do we need to look for more indoms? */
	if (! current_star_indom)
	    break;
    }

    /* Did we find any indoms matching the spec? */
    if (! found) {
	pmsprintf(mess, sizeof(mess),
		  "No matching indom for new label %s", buf);
	yywarn(mess);
    }
}

void
new_indom_instance_label(int indom)
{
    labelspec_t		*lp;
    int			sts;
    int			found = 0;
    __pmContext		*ctxp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    __pmLogInDom	*idp;
    char		buf[PM_MAXLABELJSONLEN] = "";

    /*
     * Ignore the change specs which were previously identified and instead
     * search the metadata for metrics in the given domain.
     * Create or re-use the change record for each one found. 
     */
    deactivate_labels();

    ctxp = __pmHandleToPtr(pmWhichContext());
    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    /* Prepare the JSON for the new label. */
    pmsprintf(buf, sizeof(buf), "{%s:%s}",
	      current_label_name, current_label_value);

    hcp = &ctxp->c_archctl->ac_log->l_hashindom;
    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	 node != NULL;
	 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	if (pmInDom_domain(node->key) != pmInDom_domain(indom))
	    continue;
	if (! current_star_indom && 
	    pmInDom_serial(node->key) != pmInDom_serial(indom))
	    continue;

	/*
	 * A metric in this indom exists.
	 * Add the new label to the generic label change specs for the
	 * identified indom instances. Search for existing ones first.
	 */
	for (idp = (__pmLogInDom *)node->data; idp != NULL; idp = idp->next) {
	    int indom_ix;
	    for (indom_ix = 0; indom_ix < idp->numinst; ++indom_ix) {
		int instance = idp->instlist[indom_ix];
		if (current_label_instance != -1 && instance != current_label_instance)
		    continue;
		found = 1;
		for (lp = label_root; lp != NULL; lp = lp->l_next) {
		    if (lp->old_type != PM_LABEL_INSTANCES)
			continue;
		    if (pmInDom_domain(lp->old_id) != pmInDom_domain((pmInDom)(node->key)))
			continue;
		    if (pmInDom_serial(lp->old_id) != pmInDom_serial((pmInDom)(node->key)))
			continue;
		    if (lp->old_instance != instance)
			continue;
		    if (lp->old_label == NULL)
			break;
		}

		if (lp == NULL) {
		    pmInDom current_indom =
			pmInDom_build(pmInDom_domain(node->key), pmInDom_serial(node->key));
		    lp = create_label(PM_LABEL_INSTANCES, current_indom,
				      instance, NULL, NULL);
		}

		/* Add the new label to the label change spec. */
		if ((sts = __pmAddLabels(&lp->new_labels, buf, PM_LABEL_INSTANCES)) < 0) {
		    pmsprintf(mess, sizeof(mess),
			      "Unable to add new indom instance label %s: %s",
			      buf, pmErrStr(sts));
		    yyerror(mess);
		}
		lp->new_labels->inst = instance;
		lp->flags |= LABEL_NEW;
	    }
	}

	/* Do we need to look for more indoms? */
	if (! current_star_indom)
	    break;
    }

    /* Did we find any indom instances matching the spec? */
    if (! found) {
	pmsprintf(mess, sizeof(mess),
		  "No matching indom instance for new label %s", buf);
	yywarn(mess);
    }
}

%}

%union {
    char		*str;
    int			ival;
    double		dval;
    pmInDom		indom;
    pmID		pmid;
}

%token	TOK_LBRACE
	TOK_RBRACE
	TOK_PLUS
	TOK_MINUS
	TOK_COLON
	TOK_COMMA
	TOK_ASSIGN
	TOK_GLOBAL
	TOK_INDOM
	TOK_DUPLICATE
	TOK_METRIC
	TOK_HOSTNAME
	TOK_TZ
	TOK_TIME
	TOK_NAME
	TOK_INST
	TOK_INAME
	TOK_DELETE
	TOK_PMID
	TOK_NULL_INT
	TOK_TYPE
	TOK_IF
	TOK_SEM
	TOK_UNITS
	TOK_OUTPUT
	TOK_RESCALE
	TOK_TEXT
	TOK_ONELINE
	TOK_HELP
	TOK_TEXT_STAR
	TOK_LABEL
	TOK_CONTEXT
	TOK_LABEL_STAR
	TOK_DOMAIN
	TOK_CLUSTER
	TOK_ITEM
	TOK_INSTANCES
	TOK_INSTANCE
	TOK_NEW
	TOK_VALUE

%token<str>	TOK_GNAME TOK_NUMBER TOK_STRING TOK_TEXT_STRING TOK_HNAME TOK_FLOAT
%token<str>	TOK_JSON_STRING TOK_JSON_NUMBER
%token<str>	TOK_JSON_TRUE TOK_JSON_FALSE TOK_JSON_NULL
%token<str>	TOK_INDOM_STAR TOK_PMID_INT TOK_PMID_STAR
%token<ival>	TOK_TYPE_NAME TOK_SEM_NAME TOK_SPACE_NAME TOK_TIME_NAME
%token<ival>	TOK_COUNT_NAME TOK_OUTPUT_TYPE

%type<str>	hname
%type<indom>	indom_int null_or_indom
%type<pmid>	pmid_int pmid_or_name
%type<ival>	signnumber number rescaleopt duplicateopt texttype texttypes opttexttypes pmid_domain pmid_cluster
%type<dval>	float
%type<str>	textstring opttextvalue jsonname jsonvalue jsonnumber

%%

config		: speclist
    		;

speclist	: spec
		| spec speclist
		;

spec		: globalspec
		| indomspec
		| metricspec
		| textspec
		| labelspec
		;

globalspec	: TOK_GLOBAL TOK_LBRACE globaloptlist TOK_RBRACE
		| TOK_GLOBAL TOK_LBRACE TOK_RBRACE
		;

globaloptlist	: globalopt
		| globalopt globaloptlist
		;

globalopt	: TOK_HOSTNAME TOK_ASSIGN hname
		    {
			if (global.flags & GLOBAL_CHANGE_HOSTNAME) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global hostname clause");
			    yyerror(mess);
			}
			if (strcmp(inarch.label.ll_hostname, $3) == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global hostname (%s): No change", inarch.label.ll_hostname);
				yywarn(mess);
			    }
			}
			else {
			    strncpy(global.hostname, $3, sizeof(global.hostname));
			    global.flags |= GLOBAL_CHANGE_HOSTNAME;
			}
			free($3);
		    }
		| TOK_TZ TOK_ASSIGN TOK_STRING
		    {
			if (global.flags & GLOBAL_CHANGE_TZ) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global tz clause");
			    yyerror(mess);
			}
			if (strcmp(inarch.label.ll_tz, $3) == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global timezone (%s): No change", inarch.label.ll_tz);
				yywarn(mess);
			    }
			}
			else {
			    strncpy(global.tz, $3, sizeof(global.tz));
			    global.flags |= GLOBAL_CHANGE_TZ;
			}
			free($3);
		    }
		| TOK_TIME TOK_ASSIGN signtime
		    {
			if (global.flags & GLOBAL_CHANGE_TIME) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global time clause");
			    yyerror(mess);
			}
			if (global.time.tv_sec == 0 && global.time.tv_usec == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global time: No change");
				yywarn(mess);
			    }
			}
			else
			    global.flags |= GLOBAL_CHANGE_TIME;
		    }
		| TOK_HOSTNAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting hostname in hostname clause");
			yyerror(mess);
		    }
		| TOK_HOSTNAME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in hostname clause");
			yyerror(mess);
		    }
		| TOK_TZ TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting timezone string in tz clause");
			yyerror(mess);
		    }
		| TOK_TZ
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in tz clause");
			yyerror(mess);
		    }
		| TOK_TIME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting delta of the form [+-][HH:[MM:]]SS[.d...] in time clause");
			yyerror(mess);
		    }
		| TOK_TIME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in time clause");
			yyerror(mess);
		    }
		;

	/*
	 * ambiguity in lexical scanner ... handle here
	 * abc.def - is TOK_HNAME or TOK_GNAME
	 * 123 - is TOK_HNAME or TOK_NUMBER
	 * 123.456 - is TOK_HNAME or TOK_FLOAT
	 */
hname		: TOK_HNAME
		| TOK_GNAME
		| TOK_NUMBER
		| TOK_FLOAT
		;

signnumber	: TOK_PLUS TOK_NUMBER
		    {
			$$ = atoi($2);
			free($2);
		    }
		| TOK_MINUS TOK_NUMBER
		    {
			$$ = -atoi($2);
			free($2);
		    }
		| TOK_NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

number		: TOK_NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

float		: TOK_FLOAT
		    {
			$$ = atof($1);
			free($1);
		    }
		;

signtime	: TOK_PLUS time
		| TOK_MINUS time { global.time.tv_sec = -global.time.tv_sec; }
		| time
		;

time		: number TOK_COLON number TOK_COLON float	/* HH:MM:SS.d.. format */
		    { 
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + (int)$5;
			global.time.tv_usec = (int)(1000000*(($5 - (int)$5))+0.5);
		    }
		| number TOK_COLON number TOK_COLON number	/* HH:MM:SS format */
		    { 
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + $5;
		    }
		| number TOK_COLON float		/* MM:SS.d.. format */
		    { 
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + (int)$3;
			global.time.tv_usec = (int)(1000000*(($3 - (int)$3))+0.5);
		    }
		| number TOK_COLON number		/* MM:SS format */
		    { 
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + $3;
		    }
		| float			/* SS.d.. format */
		    {
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = (int)$1;
			global.time.tv_usec = (int)(1000000*(($1 - (int)$1))+0.5);
		    }
		| number		/* SS format */
		    {
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = $1;
			global.time.tv_usec = 0;
		    }
		;

indomspec	: TOK_INDOM indom_int
		    {
			if (current_star_indom) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*node;

			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			    PM_UNLOCK(ctxp->c_lock);
			    hcp = &ctxp->c_archctl->ac_log->l_hashindom;
			    star_domain = pmInDom_domain($2);
			    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
				 node != NULL;
				 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
				if (pmInDom_domain((pmInDom)(node->key)) == star_domain)
				    current_indomspec = start_indom((pmInDom)(node->key));
			    }
			    do_walk_indom = 1;
			}
			else {
			    current_indomspec = start_indom($2);
			    do_walk_indom = 0;
			}
		    }
			TOK_LBRACE optindomopt TOK_RBRACE
		| TOK_INDOM
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or <domain>.* in indom rule");
			yyerror(mess);
		    }
		;

indom_int	: TOK_FLOAT
		    {
			int		domain;
			int		serial;
			int		sts;
			sts = sscanf($1, "%d.%d", &domain, &serial);
			if (sts < 2) {
			    pmsprintf(mess, sizeof(mess), "Missing serial field for indom");
			    yyerror(mess);
			}
			if (domain < 1 || domain >= DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
			    yyerror(mess);
			}
			if (serial < 0 || serial >= 4194304) {
			    pmsprintf(mess, sizeof(mess), "Illegal serial field (%d) for indom", serial);
			    yyerror(mess);
			}
			current_star_indom = 0;
			free($1);
			$$ = pmInDom_build(domain, serial);
		    }
		| TOK_INDOM_STAR
		    {
			int		domain;
			sscanf($1, "%d.", &domain);
			if (domain < 1 || domain >= DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
			    yyerror(mess);
			}
			current_star_indom = 1;
			free($1);
			$$ = pmInDom_build(domain, 0);
		    }
		;

optindomopt	: indomoptlist
		|
		;

indomoptlist	: indomopt
		| indomopt indomoptlist
		;

indomopt	: TOK_INDOM TOK_ASSIGN duplicateopt indom_int
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    pmInDom	indom;
			    if (indom_root->new_indom != indom_root->old_indom) {
				pmsprintf(mess, sizeof(mess), "Duplicate indom clause for indom %s", pmInDomStr(indom_root->old_indom));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($4), pmInDom_serial(ip->old_indom));
			    else
				indom = $4;
			    if (indom != ip->old_indom)
				ip->new_indom = indom;
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Instance domain %s: indom: No change", pmInDomStr(ip->old_indom));
				    yywarn(mess);
				}
			    }
			    ip->indom_flags |= $3;
			}
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN TOK_STRING
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
			free($2);
			/* Note: $4 referenced from new_iname[] */
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN TOK_DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, NULL) < 0)
			    	yyerror(mess);
			}
			free($2);
		    }
		| TOK_INST number TOK_ASSIGN number
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
		    }
		| TOK_INST number TOK_ASSIGN TOK_DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, PM_IN_NULL) < 0)
				yyerror(mess);
			}
		    }
		| TOK_INDOM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or <domain>.* in indom clause");
			yyerror(mess);
		    }
		| TOK_INDOM
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in indom clause");
			yyerror(mess);
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting new external instance name string or DELETE in iname clause");
			yyerror(mess);
		    }
		| TOK_INAME TOK_STRING
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in iname clause");
			yyerror(mess);
		    }
		| TOK_INAME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting old external instance name string in iname clause");
			yyerror(mess);
		    }
		| TOK_INST number TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting new internal instance identifier or DELETE in inst clause");
			yyerror(mess);
		    }
		| TOK_INST number
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in inst clause");
			yyerror(mess);
		    }
		| TOK_INST
		    {
			pmsprintf(mess, sizeof(mess), "Expecting old internal instance identifier in inst clause");
			yyerror(mess);
		    }
		;

duplicateopt	: TOK_DUPLICATE 	{ $$ = INDOM_DUPLICATE; }
		|			{ $$ = 0; }
		;

metricspec	: TOK_METRIC pmid_or_name
		    {
			if (current_star_metric) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*node;

			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			    PM_UNLOCK(ctxp->c_lock);
			    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
			    star_domain = pmID_domain($2);
			    if (current_star_metric == 1)
				star_cluster = pmID_cluster($2);
			    else
				star_cluster = PM_ID_NULL;
			    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
				 node != NULL;
				 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
				if (pmID_domain((pmID)(node->key)) == star_domain &&
				    (star_cluster == PM_ID_NULL ||
				     star_cluster == pmID_cluster((pmID)(node->key))))
				    current_metricspec = start_metric((pmID)(node->key));
			    }
			    do_walk_metric = 1;
			}
			else {
			    if ($2 == PM_ID_NULL)
				/* metric not in archive */
				current_metricspec = NULL;
			    else
				current_metricspec = start_metric($2);
			    do_walk_metric = 0;
			}
		    }
			TOK_LBRACE optmetricoptlist TOK_RBRACE
		| TOK_METRIC
		    {
			pmsprintf(mess, sizeof(mess), "Expecting metric name or <domain>.<cluster>.<item> or <domain>.<cluster>.* or <domain>.*.* in metric rule");
			yyerror(mess);
		    }
		;

pmid_or_name	: pmid_int
		|  TOK_GNAME
		    {
			int	sts;
			pmID	pmid;
			sts = pmLookupName(1, &$1, &pmid);
			if (sts < 0) {
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Metric: %s: %s", $1, pmErrStr(sts));
				yywarn(mess);
			    }
			    pmid = PM_ID_NULL;
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid;
		    }
		;

pmid_int	: TOK_PMID_INT
		    {
			int	domain;
			int	cluster;
			int	item;
			int	sts;
			sts = sscanf($1, "%d.%d.%d", &domain, &cluster, &item);
			assert(sts == 3);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			if (cluster < 0 || cluster >= 4096) {
			    pmsprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
			    yyerror(mess);
			}
			if (item < 0 || item >= 1024) {
			    pmsprintf(mess, sizeof(mess), "Illegal item field (%d) for pmid", item);
			    yyerror(mess);
			}
			current_star_metric = 0;
			free($1);
			$$ = pmID_build(domain, cluster, item);
		    }
		| TOK_PMID_STAR
		    {
			int	domain;
			int	cluster;
			int	sts;
			sts = sscanf($1, "%d.%d.", &domain, &cluster);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			if (sts == 2) {
			    if (cluster >= 4096) {
				pmsprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
				yyerror(mess);
			    }
			    current_star_metric = 1;
			}
			else {
			    cluster = 0;
			    current_star_metric = 2;
			}
			free($1);
			$$ = pmID_build(domain, cluster, 0);
		    }
		;

optmetricoptlist	: metricoptlist
			| /* nothing */
			;

metricoptlist	: metricopt
		| metricopt metricoptlist
		;

metricopt	: TOK_PMID TOK_ASSIGN pmid_int
		    {
			metricspec_t	*mp;
			pmID		pmid;
			for (mp = walk_metric(W_START, METRIC_CHANGE_PMID, "pmid", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_PMID, "pmid", 0)) {
			    if (current_star_metric == 1)
				pmid = pmID_build(pmID_domain($3), pmID_cluster($3), pmID_item(mp->old_desc.pmid));
			    else if (current_star_metric == 2)
				pmid = pmID_build(pmID_domain($3), pmID_cluster(mp->old_desc.pmid), pmID_item(mp->old_desc.pmid));
			    else
				pmid = $3;
			    if (pmid == mp->old_desc.pmid) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): pmid: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.pmid = pmid;
				mp->flags |= METRIC_CHANGE_PMID;
			    }
			}
		    }
		| TOK_NAME TOK_ASSIGN TOK_GNAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_NAME, "name", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_NAME, "name", 0)) {
			    if (strcmp($3, mp->old_name) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): name: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				int	sts;
				pmID	pmid;
				sts = pmLookupName(1, &$3, &pmid);
				if (sts >= 0) {
				    pmsprintf(mess, sizeof(mess), "Metric name %s already assigned for PMID %s", $3, pmIDStr(pmid));
				    yyerror(mess);
				}
				mp->new_name = $3;
				mp->flags |= METRIC_CHANGE_NAME;
			    }
			}
		    }
		| TOK_TYPE TOK_ASSIGN TOK_TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type", 0)) {
			    if ($3 == mp->old_desc.type) {
				/* old == new, so no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): type: PM_TYPE_%s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmTypeStr(mp->old_desc.type));
				    yywarn(mess);
				}
			    }
			    else {
				if (mp->old_desc.type == PM_TYPE_32 ||
				    mp->old_desc.type == PM_TYPE_U32 ||
				    mp->old_desc.type == PM_TYPE_64 ||
				    mp->old_desc.type == PM_TYPE_U64 ||
				    mp->old_desc.type == PM_TYPE_FLOAT ||
				    mp->old_desc.type == PM_TYPE_DOUBLE) {
				    mp->new_desc.type = $3;
				    mp->flags |= METRIC_CHANGE_TYPE;
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Old type (PM_TYPE_%s) must be numeric", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
			    }
			}
		    }
		| TOK_TYPE TOK_IF TOK_TYPE_NAME TOK_ASSIGN TOK_TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type", 1); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type", 1)) {
			    if (mp->old_desc.type != $3) {
				if (wflag) {
				    char	tbuf0[20];
				    char	tbuf1[20];
				    pmTypeStr_r(mp->old_desc.type, tbuf0, sizeof(tbuf0));
				    pmTypeStr_r($5, tbuf1, sizeof(tbuf1));
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): type: PM_TYPE_%s: No conditional change to PM_TYPE_%s", mp->old_name, pmIDStr(mp->old_desc.pmid), tbuf0, tbuf1);
				    yywarn(mess);
				}
			    }
			    else {
				if (mp->old_desc.type == PM_TYPE_32 ||
				    mp->old_desc.type == PM_TYPE_U32 ||
				    mp->old_desc.type == PM_TYPE_64 ||
				    mp->old_desc.type == PM_TYPE_U64 ||
				    mp->old_desc.type == PM_TYPE_FLOAT ||
				    mp->old_desc.type == PM_TYPE_DOUBLE) {
				    mp->new_desc.type = $5;
				    mp->flags |= METRIC_CHANGE_TYPE;
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Old type (PM_TYPE_%s) must be numeric", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
			    }
			}
		    }
		| TOK_INDOM TOK_ASSIGN null_or_indom pick
		    {
			metricspec_t	*mp;
			pmInDom		indom;
			for (mp = walk_metric(W_START, METRIC_CHANGE_INDOM, "indom", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_INDOM, "indom", 0)) {
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(mp->old_desc.indom));
			    else
				indom = $3;
			    if (indom == mp->old_desc.indom) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): indom: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmInDomStr(mp->old_desc.indom));
				    yywarn(mess);
				}
			    }
			    else {
				if ((output == OUTPUT_MIN ||
					  output == OUTPUT_MAX ||
					  output == OUTPUT_SUM ||
					  output == OUTPUT_AVG) &&
					 mp->old_desc.type != PM_TYPE_32 &&
					 mp->old_desc.type != PM_TYPE_U32 &&
					 mp->old_desc.type != PM_TYPE_64 &&
					 mp->old_desc.type != PM_TYPE_U64 &&
					 mp->old_desc.type != PM_TYPE_FLOAT &&
					 mp->old_desc.type != PM_TYPE_DOUBLE) {
				    pmsprintf(mess, sizeof(mess), "OUTPUT option MIN, MAX, AVG or SUM requires type to be numeric, not PM_TYPE_%s", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
				mp->new_desc.indom = indom;
				mp->flags |= METRIC_CHANGE_INDOM;
				mp->output = output;
				if (output == OUTPUT_ONE) {
				    mp->one_name = one_name;
				    mp->one_inst = one_inst;
				    if (mp->old_desc.indom == PM_INDOM_NULL)
					/*
					 * singular input, pick first (only)
					 * value, not one_inst matching ...
					 * one_inst used for output instance
					 * id
					 */
					mp->output = OUTPUT_FIRST;
				}
				if (output == OUTPUT_ALL) {
				    /*
				     * No OUTPUT clause, set up the defaults
				     * based on indom types:
				     * non-NULL -> NULL
				     *		OUTPUT_FIRST, inst PM_IN_NULL
				     * NULL -> non-NULL
				     *		OUTPUT_FIRST, inst 0
				     * non-NULL -> non-NULL
				     * 		all instances selected
				     *		(nothing to do for defaults)
				     * NULL -> NULL
				     *		caught above in no change case
				     */
				    if (mp->old_desc.indom != PM_INDOM_NULL &&
				        mp->new_desc.indom == PM_INDOM_NULL) {
					mp->output = OUTPUT_FIRST;
					mp->one_inst = PM_IN_NULL;
				    }
				    else if (mp->old_desc.indom == PM_INDOM_NULL &&
				             mp->new_desc.indom != PM_INDOM_NULL) {
					mp->output = OUTPUT_FIRST;
					mp->one_inst = 0;
				    }
				}
			    }
			}
			output = OUTPUT_ALL;	/* for next time */
		    }
		| TOK_SEM TOK_ASSIGN TOK_SEM_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_SEM, "sem", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_SEM, "sem", 0)) {
			    if ($3 == mp->old_desc.sem) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): sem: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), SemStr(mp->old_desc.sem));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.sem = $3;
				mp->flags |= METRIC_CHANGE_SEM;
			    }
			}
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA TOK_TIME_NAME TOK_COMMA TOK_COUNT_NAME rescaleopt
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_UNITS, "units", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_UNITS, "units", 0)) {
			    if ($3 == mp->old_desc.units.dimSpace &&
			        $5 == mp->old_desc.units.dimTime &&
			        $7 == mp->old_desc.units.dimCount &&
			        $9 == mp->old_desc.units.scaleSpace &&
			        $11 == mp->old_desc.units.scaleTime &&
			        $13 == mp->old_desc.units.scaleCount) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): units: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmUnitsStr(&mp->old_desc.units));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.units.dimSpace = $3;
				mp->new_desc.units.dimTime = $5;
				mp->new_desc.units.dimCount = $7;
				mp->new_desc.units.scaleSpace = $9;
				mp->new_desc.units.scaleTime = $11;
				mp->new_desc.units.scaleCount = $13;
				mp->flags |= METRIC_CHANGE_UNITS;
				if ($14 == 1) {
				    if ($3 == mp->old_desc.units.dimSpace &&
					$5 == mp->old_desc.units.dimTime &&
					$7 == mp->old_desc.units.dimCount)
					/* OK, no dim changes */
					mp->flags |= METRIC_RESCALE;
				    else {
					if (wflag) {
					    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): Dimension changed, cannot rescale", mp->old_name, pmIDStr(mp->old_desc.pmid));
					    yywarn(mess);
					}
				    }
				}
				else if (sflag) {
				    if ($3 == mp->old_desc.units.dimSpace &&
					$5 == mp->old_desc.units.dimTime &&
					$7 == mp->old_desc.units.dimCount)
					mp->flags |= METRIC_RESCALE;
				}
			    }
			}
		    }
		| TOK_DELETE
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_DELETE, "delete", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_DELETE, "delete", 0)) {
			    mp->flags |= METRIC_DELETE;
			}
		    }
		| TOK_PMID TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<cluster>.<item> or <domain>.<cluster>.* or <domain>.*.* in pmid clause");
			yyerror(mess);
		    }
		| TOK_NAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting metric name in iname clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) in type clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_IF
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) after if in type clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_IF TOK_TYPE_NAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) in type clause");
			yyerror(mess);
		    }
		| TOK_INDOM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or NULL in indom clause");
			yyerror(mess);
		    }
		| TOK_SEM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_SEM_XXX) in sem clause");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN 
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 3 numeric values for dim* fields of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_SPACE_XXX) for scaleSpace field of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_TIME_XXX) for scaleTime field of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA TOK_TIME_NAME TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or ONE for scaleCount field of units");
			yyerror(mess);
		    }
		;

null_or_indom	: indom_int
		| TOK_NULL_INT
		    {
			$$ = PM_INDOM_NULL;
		    }
		;

pick		: TOK_OUTPUT TOK_INST number
		    {
			output = OUTPUT_ONE;
			one_inst = $3;
			one_name = NULL;
		    }
		| TOK_OUTPUT TOK_INAME TOK_STRING
		    {
			output = OUTPUT_ONE;
			one_inst = PM_IN_NULL;
			one_name = $3;
		    }
		| TOK_OUTPUT TOK_OUTPUT_TYPE
		    {
			output = $2;
		    }
		| TOK_OUTPUT
		    {
			pmsprintf(mess, sizeof(mess), "Expecting FIRST or LAST or INST or INAME or MIN or MAX or AVG for OUTPUT instance option");
			yyerror(mess);
		    }
		| /* nothing */
		;

rescaleopt	: TOK_RESCALE { $$ = 1; }
		| /* nothing */
		    { $$ = 0; }
		;

textspec	: TOK_TEXT textmetricorindomspec
		;

textmetricorindomspec	: textmetricspec 
			| textindomspec
			;

textmetricspec	: TOK_METRIC pmid_or_name opttexttypes opttextvalue
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;
			int		target_types;
			int		this_type;
			int		type;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_textspec = NULL;
			if ($2 == PM_ID_NULL) {
			    /* Metric referenced by name is not in the archive */
			    do_walk_text = 0;
			}
			else {
			    int found = 0;
			    
			    if (current_star_metric) {
				/* Set up for metrics specified using globbing. */
				star_domain = pmID_domain($2);
				if (current_star_metric == 1)
				    star_cluster = pmID_cluster($2);
				else
				    star_cluster = PM_ID_NULL;
			    }

			    /* We're looking for text of the specified type(s) for metric(s). */
			    target_types = PM_TEXT_PMID | $3;
			    hcp1 = &ctxp->c_archctl->ac_log->l_hashtext;
			    for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
				 node1 != NULL;
				 node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
				__pmHashCtl	*hcp2;
				__pmHashNode	*node2;

				/* Was this object type selected? */
				this_type = (int)(node1->key);
				if ((this_type & target_types) != this_type)
				    continue;

				/*
				 * Collect the text records associated with the specified
				 * metric(s).
				 */
				hcp2 = (__pmHashCtl *)(node1->data);
				for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				     node2 != NULL;
				     node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				    if (current_star_metric) {
					/* Match the globbed metric spec and keep looking. */
					if (pmID_domain((pmID)(node2->key)) == star_domain &&
					    (star_cluster == PM_ID_NULL ||
					     star_cluster == pmID_cluster((pmID)(node2->key)))) {
					    current_textspec = start_text(this_type, (pmID)(node2->key), $4);
					    if (current_textspec) {
						current_textspec->flags |= TEXT_ACTIVE;
						++found;
					    }

					}
				    }
				    else {
					/* Match the exact metric PMID. */
					if ((pmID)(node2->key) == $2) {
					    current_textspec = start_text(this_type, (pmID)(node2->key), $4);
					    if (current_textspec) {
						current_textspec->flags |= TEXT_ACTIVE;
						++found;
					    }
					    /*
					     * Stop looking if we have found all of the specified
					     * types.
					     */
					    type = this_type & PM_TEXT_TYPE_MASK;
					    target_types ^= type;
					    if ((target_types & PM_TEXT_TYPE_MASK) == 0)
						break;
					}
				    }
				}
				if ((target_types & PM_TEXT_TYPE_MASK) == 0)
				    break;
			    }
			    do_walk_text = (found > 1);
			}
		    }
		  TOK_LBRACE opttextmetricoptlist TOK_RBRACE
		;

opttexttypes	: texttypes 
		    { $$ = $1; }
		| /* nothing */
		    { $$ = PM_TEXT_ONELINE; } /* The default */
		;

texttypes	: texttype
		    { $$ = $1; }
		| texttype texttypes
		    { $$ = $1 | $2; } /* Accumulate */
		;

texttype	: TOK_TEXT_STAR
		    { $$ = PM_TEXT_ONELINE | PM_TEXT_HELP; }
		| TOK_HELP
		    { $$ = PM_TEXT_HELP; }
		| TOK_ONELINE
		    { $$ = PM_TEXT_ONELINE; }
		;

opttextvalue	: textstring
		    { $$ = $1; }
		| /* nothing */
		    { $$ = NULL; }
		;

opttextmetricoptlist	: textmetricoptlist
			| /* nothing */
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_ACTIVE, "active", 0); tp != NULL; tp = walk_text(W_NEXT, 0, "", 0)) {
			    tp->flags &= ~TEXT_ACTIVE;
			}
		    }
			;

textmetricoptlist	: textmetricopt
			| textmetricopt textmetricoptlist
			;

textmetricopt	: TOK_DELETE
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_DELETE, "delete", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_DELETE, "delete", 0)) {
			    tp->flags |= TEXT_DELETE;
			}
		    }
		| TOK_TEXT TOK_ASSIGN textstring
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_CHANGE_TEXT, "text", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_CHANGE_TEXT, "text", 0)) {
			    if (tp->new_text) {
				if (strcmp(tp->new_text, $3) == 0) {
				    pmsprintf(mess, sizeof(mess), "Duplicate text change clause");
				    yyerror(mess);
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Conflicting text change clause");
				    yyerror(mess);
				}
				free($3);
			    }
			    else if (strcmp(tp->old_text, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Help text: No change");
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				tp->new_text = $3;
				tp->flags |= TEXT_CHANGE_TEXT;
			    }
			}
		    }
		| TOK_METRIC TOK_ASSIGN pmid_int
		    {
			textspec_t	*tp;
			pmID		pmid;
			for (tp = walk_text(W_START, TEXT_CHANGE_ID, "id", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_CHANGE_ID, "id", 0)) {
			    if (current_star_metric == 1)
				pmid = pmID_build(pmID_domain($3), pmID_cluster($3), pmID_item(tp->old_id));
			    else if (current_star_metric == 2)
				pmid = pmID_build(pmID_domain($3), pmID_cluster(tp->old_id), pmID_item(tp->old_id));
			    else
				pmid = $3;
			    if (pmid == tp->old_id) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Text for metric: %s: metric: No change", pmIDStr(tp->old_id));
				    yywarn(mess);
				}
			    }
			    else {
				const textspec_t *tp1;
				/* Warn if the target metric already has text of this type. */
				/*
				 * Search all of the change specs for others targeting
				 * the same metric.
				 */
				for (tp1 = text_root; tp1 != NULL; tp1 = tp1->t_next) {
				    if (! (tp1->flags & TEXT_CHANGE_ID))
					continue; /* not an id change spec */
				    if (tp1->new_type != tp->new_type)
					continue; /* target is a different type */
				    if (tp1 == tp)
					continue; /* same spec */
				    if (tp1->new_id == pmid) {
					/* conflict with another change spec. */
					pmsprintf(mess, sizeof(mess), "Text for metric: %s: metric: Conflicting metric change", pmIDStr(tp->old_id));
					yyerror(mess);
					break;
				    }
				}
				if (tp1 == NULL) {
				    /* No conflict */
				    tp->new_id = pmid;
				    tp->flags |= TEXT_CHANGE_ID;
				}
			    }
			}
		    }
		;

textstring	: TOK_STRING
		    { $$ = $1; }
		| TOK_TEXT_STRING
		    { $$ = $1; }
		;

textindomspec	: TOK_INDOM indom_int opttexttypes opttextvalue
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;
			int		target_types;
			int		this_type;
			int		type;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_textspec = NULL;
			if ($2 == PM_ID_NULL) {
			    /* Indom is not in the archive */
			    do_walk_text = 0;
			}
			else {
			    int found = 0;
			    
			    if (current_star_indom) {
				/* Set up for indoms specified using globbing. */
				star_domain = pmInDom_domain($2);
			    }

			    /* We're looking for text of the specified type(s) for indom(s). */
			    target_types = PM_TEXT_INDOM | $3;
			    hcp1 = &ctxp->c_archctl->ac_log->l_hashtext;
			    for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
				 node1 != NULL;
				 node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
				__pmHashCtl	*hcp2;
				__pmHashNode	*node2;

				/* Was this object type selected? */
				this_type = (int)(node1->key);
				if ((this_type & target_types) != this_type)
				    continue;

				/*
				 * Collect the text records associated with the specified
				 * metric(s).
				 */
				hcp2 = (__pmHashCtl *)(node1->data);
				for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				     node2 != NULL;
				     node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				    if (current_star_indom) {
					/* Match the globbed metric spec and keep looking. */
					if (pmInDom_domain((pmID)(node2->key)) == star_domain) {
					    current_textspec = start_text(this_type, (pmID)(node2->key), $4);
					    if (current_textspec) {
						current_textspec->flags |= TEXT_ACTIVE;
						++found;
					    }
					}
				    }
				    else {
					/* Match the exact indom id. */
					if ((pmID)(node2->key) == $2) {
					    current_textspec = start_text(this_type, (pmID)(node2->key), $4);
					    if (current_textspec) {
						current_textspec->flags |= TEXT_ACTIVE;
						++found;
					    }
					    /*
					     * Stop looking if we have found all of the specified
					     * types.
					     */
					    type = this_type & PM_TEXT_TYPE_MASK;
					    target_types ^= type;
					    if ((target_types & PM_TEXT_TYPE_MASK) == 0)
						break;
					}
				    }
				}
				if ((target_types & PM_TEXT_TYPE_MASK) == 0)
				    break;
			    }
			    do_walk_text = (found > 1);
			}
		    }
		  TOK_LBRACE opttextindomoptlist TOK_RBRACE
		;

opttextindomoptlist	: textindomoptlist
			| /* nothing */
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_ACTIVE, "active", 0); tp != NULL; tp = walk_text(W_NEXT, 0, "", 0)) {
			    tp->flags &= ~TEXT_ACTIVE;
			}
		    }
			;

textindomoptlist	: textindomopt
			| textindomopt textindomoptlist
			;

textindomopt	: TOK_DELETE
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_DELETE, "delete", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_DELETE, "delete", 0)) {
			    tp->flags |= TEXT_DELETE;
			}
		    }
		| TOK_TEXT TOK_ASSIGN textstring
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_CHANGE_TEXT, "text", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_CHANGE_TEXT, "text", 0)) {
			    if (tp->new_text) {
				if (strcmp(tp->new_text, $3) == 0) {
				    pmsprintf(mess, sizeof(mess), "Duplicate text change clause");
				    yyerror(mess);
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Conflicting text change clause");
				    yyerror(mess);
				}
				free($3);
			    }
			    else if (strcmp(tp->old_text, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Help text: No change");
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				tp->new_text = $3;
				tp->flags |= TEXT_CHANGE_TEXT;
			    }
			}
		    }
		| TOK_INDOM TOK_ASSIGN indom_int
		    {
			textspec_t	*tp;
			for (tp = walk_text(W_START, TEXT_CHANGE_ID, "id", 0); tp != NULL; tp = walk_text(W_NEXT, TEXT_CHANGE_ID, "id", 0)) {
			    pmInDom	indom;
			    if (tp->new_id != tp->old_id) {
				pmsprintf(mess, sizeof(mess), "Duplicate text clause for indom %s", pmInDomStr(tp->old_id));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(tp->old_id));
			    else
				indom = $3;
			    if (indom != tp->old_id) {
				tp->new_id = indom;
				tp->flags |= TEXT_CHANGE_ID;
			    }
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Text for instance domain %s: indom: No change", pmInDomStr(tp->old_id));
				    yywarn(mess);
				}
			    }
			}
		    }
		;

labelspec	: TOK_LABEL labelcontextormetricorindomspec
		;

labelcontextormetricorindomspec	: labelcontextspec
				| labeldomainspec
				| labelclusterspec
				| labelitemspec
				| labelindomspec
				| labelinstancesspec
				;

labelcontextspec	: TOK_CONTEXT optlabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;
			int		found = 0;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			/* We're looking for context labels. */
			current_labelspec = NULL;
			hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
			     node1 != NULL;
			     node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
			    __pmHashCtl	*hcp2;
			    __pmHashNode	*node2;
			    int		this_type;

			    /* Was this object type selected? */
			    this_type = (int)(node1->key);
			    if (this_type != PM_LABEL_CONTEXT)
				continue;

			    /*
			     * Collect the label records associated with the specified
			     * metric(s).
			     */
			    hcp2 = (__pmHashCtl *)(node1->data);
			    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				 node2 != NULL;
				 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				/* We want all of the context labels. */
				current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
								current_label_name, current_label_value);
				if (current_labelspec) {
				    current_labelspec->flags |= LABEL_ACTIVE;
				    ++found;
				}
			    }
			}
			do_walk_label = found;
		    }
		  TOK_LBRACE optlabelcontextoptlist TOK_RBRACE
		;

optlabeldetails	: jsonname optlabelvalue
		    { current_label_name = $1; }
		| TOK_LABEL_STAR optlabelvalue
		    { current_label_name = NULL; }
		| /* nothing */
		    {
			current_label_name = NULL;
			current_label_value = NULL;
		    }
		;

jsonname	: TOK_JSON_STRING
		    { $$ = $1; }
		| TOK_STRING
		    {
			$$ = add_quotes($1);
			free($1);
		    }
		;

optlabelvalue	: jsonvalue
		    { current_label_value = $1; }
		| TOK_LABEL_STAR
		    { current_label_value = NULL; }
		| /* nothing */
		    { current_label_value = NULL; }
		;

jsonvalue	: jsonname
		    { $$ = $1; }
		| jsonnumber
		    { $$ = $1; }
		| TOK_JSON_TRUE
		    { $$ = $1; }
		| TOK_JSON_FALSE
		    { $$ = $1; }
		| TOK_JSON_NULL
		    { $$ = $1; }
		;

jsonnumber	: TOK_MINUS TOK_NUMBER
		    {
			$$ = dupcat("-", $2);
			free($2);
		    }
		| TOK_NUMBER
		    { $$ = $1; }
		| TOK_MINUS TOK_JSON_NUMBER
		    {
			$$ = dupcat("-", $2);
			free($2);
		    }
		| TOK_JSON_NUMBER
		    { $$ = $1; }
		;

optlabelcontextoptlist	: labelcontextoptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labelcontextoptlist	: labelcontextopt
			| labelcontextopt labelcontextoptlist
			;

labelcontextopt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for context");
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for context: label: No change");
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for cluster %d.%d", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for cluster %d.%d: value: No change", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_context_label();
		    }
		
newlabelspec	: TOK_NEW jsonname jsonvalue
		    {
			/* The current label name and value must both NOT be specified. */
			if (current_label_name || current_label_value) {
			    pmsprintf(mess, sizeof(mess), "The target label name and value must both not be specified for a NEW label and will be ignored");
			    yywarn(mess);
			    if (current_label_name)
				free(current_label_name);
			    if (current_label_value)
				free(current_label_value);
			}
			current_label_name = $2;
			current_label_value = $3;
		    }

labeldomainspec	: TOK_DOMAIN pmid_domain optlabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;
			int		 found = 0;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_labelspec = NULL;
			assert ($2 != PM_ID_NULL);
			current_label_id = $2;
			    
			/* We're looking for domain labels. */
			hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
			     node1 != NULL;
			     node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
			    __pmHashCtl	*hcp2;
			    __pmHashNode	*node2;
			    int		this_type;

			    /* Was this object type selected? */
			    this_type = (int)(node1->key);
			    if (this_type != PM_LABEL_DOMAIN)
				continue;

			    /*
			     * Collect the label records associated with the specified
			     * metric domain.
			     */
			    hcp2 = (__pmHashCtl *)(node1->data);
			    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				 node2 != NULL;
				 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				/* Match the exact metric domain. */
				if ((pmID)(node2->key) == $2) {
				    current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
								    current_label_name, current_label_value);
				    if (current_labelspec) {
					current_labelspec->flags |= LABEL_ACTIVE;
					++found;
				    }
				}
			    }
			}
			do_walk_label = found;
		    }
		  TOK_LBRACE optlabeldomainoptlist TOK_RBRACE
		;

pmid_domain	: TOK_NUMBER
		    {
			int	domain;
			int	sts;
			sts = sscanf($1, "%d", &domain);
			assert(sts == 1);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			current_star_metric = 0;
			free($1);
			$$ = domain;
		    }
		;

optlabeldomainoptlist	: labeldomainoptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labeldomainoptlist	: labeldomainopt
			| labeldomainopt labeldomainoptlist
			;

labeldomainopt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_DOMAIN TOK_ASSIGN TOK_NUMBER
		    {
			labelspec_t	*lp;
			int		domain;
			int		sts;
			sts = sscanf($3, "%d", &domain);
			assert(sts == 1);
			free($3);
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    if (domain == lp->old_id) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for metric domain: %d: No change", lp->old_id);
				    yywarn(mess);
				}
			    }
			    else {
				/*
				 * No need to check for conflicts. Multiple label set records for
				 * the same domain are ok.
				 */
				lp->new_id = domain;
				lp->flags |= LABEL_CHANGE_ID;
			    }
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for domain %d", lp->old_id);
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for domain %d: label: No change", lp->old_id);
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for cluster %d.%d", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for cluster %d.%d: value: No change", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_domain_label(current_label_id);
		    }
		;

labelclusterspec	: TOK_CLUSTER pmid_cluster optlabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;
			int		found = 0;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_labelspec = NULL;
			assert ($2 != PM_ID_NULL);
			current_label_id = $2;
			    
			if (current_star_metric) {
			    /* Set up for metrics specified using globbing. */
			    star_domain = pmID_domain($2);
			    star_cluster = PM_ID_NULL;
			}

			/* We're looking for cluster labels. */
			hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
			     node1 != NULL;
			     node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
			    __pmHashCtl		*hcp2;
			    __pmHashNode	*node2;
			    int			this_type;

			    /* Was this object type selected? */
			    this_type = (int)(node1->key);
			    if (this_type != PM_LABEL_CLUSTER)
				continue;

			    /*
			     * Collect the label records associated with the specified
			     * metric(s).
			     */
			    hcp2 = (__pmHashCtl *)(node1->data);
			    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				 node2 != NULL;
				 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				if (current_star_metric) {
				    /* Match the globbed cluster spec and keep looking. */
				    if (pmID_domain((pmID)(node2->key)) == star_domain) {
					current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									current_label_name, current_label_value);
					if (current_labelspec) {
					    current_labelspec->flags |= LABEL_ACTIVE;
					    ++found;
					}
				    }
				}
				else {
				    /* Match the exact cluster spec. */
				    if ((pmID)(node2->key) == $2) {
					current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									current_label_name, current_label_value);
					if (current_labelspec) {
					    current_labelspec->flags |= LABEL_ACTIVE;
					    ++found;
					}
				    }
				}
			    }
			}
			do_walk_label = found;
		    }
		  TOK_LBRACE optlabelclusteroptlist TOK_RBRACE
		;

pmid_cluster	: TOK_FLOAT
		    {
			int	domain;
			int	cluster;
			int	sts;
			sts = sscanf($1, "%d.%d", &domain, &cluster);
			assert(sts == 2);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			if (cluster < 0 || cluster >= 4096) {
			    pmsprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
			    yyerror(mess);
			}
			current_star_metric = 0;
			free($1);
			$$ = pmID_build(domain, cluster, 0);
		    }
		| TOK_INDOM_STAR
		    {
			int	domain;
			int	sts;
			sts = sscanf($1, "%d.", &domain);
			assert (sts == 1);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			current_star_metric = 1;
			free($1);
			$$ = pmID_build(domain, 0, 0);
		    }
		;

optlabelclusteroptlist	: labelclusteroptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labelclusteroptlist	: labelclusteropt
			| labelclusteropt labelclusteroptlist
			;

labelclusteropt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_CLUSTER TOK_ASSIGN pmid_cluster
		    {
			labelspec_t	*lp;
			int		cluster;
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    if (current_star_metric)
				cluster = pmID_build(pmID_domain($3), pmID_cluster(lp->old_id), PM_ID_NULL);
			    else
				cluster = pmID_build(pmID_domain($3), pmID_cluster($3), PM_ID_NULL);
			    if (cluster == lp->old_id) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for cluster: %d.%d: No change", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				    yywarn(mess);
				}
			    }
			    else {
				/*
				 * No need to check for conflicts. Multiple label set records for
				 * the same cluster are ok.
				 */
				lp->new_id = cluster;
				lp->flags |= LABEL_CHANGE_ID;
			    }
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for cluster %d.%d", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for cluster %d.%d: label: No change", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for cluster %d.%d", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for cluster %d.%d: value: No change", pmID_domain(lp->old_id), pmID_cluster(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_cluster_label(current_label_id);
		    }
		;

labelitemspec	: TOK_ITEM pmid_or_name optlabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_labelspec = NULL;
			current_label_id = $2;
			if ($2 == PM_ID_NULL) {
			    /* Metric referenced by name is not in the archive */
			    do_walk_label = 0;
			}
			else {
			    int found = 0;
			    
			    if (current_star_metric) {
				/* Set up for metrics specified using globbing. */
				star_domain = pmID_domain($2);
				if (current_star_metric == 1)
				    star_cluster = pmID_cluster($2);
				else
				    star_cluster = PM_ID_NULL;
			    }

			    /* We're looking for item labels. */
			    hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			    for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
				 node1 != NULL;
				 node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
				__pmHashCtl	*hcp2;
				__pmHashNode	*node2;
				int		this_type;

				/* Was this object type selected? */
				this_type = (int)(node1->key);
				if (this_type != PM_LABEL_ITEM)
				    continue;

				/*
				 * Collect the label records associated with the specified
				 * metric(s).
				 */
				hcp2 = (__pmHashCtl *)(node1->data);
				for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				     node2 != NULL;
				     node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				    if (current_star_metric) {
					/* Match the globbed metric spec and keep looking. */
					if (pmID_domain((pmID)(node2->key)) == star_domain &&
					    (star_cluster == PM_ID_NULL ||
					     star_cluster == pmID_cluster((pmID)(node2->key)))) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }

					}
				    }
				    else {
					/* Match the exact metric PMID. */
					if ((pmID)(node2->key) == $2) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }
					}
				    }
				}
			    }
			    do_walk_label = found;
			}
		    }
		  TOK_LBRACE optlabelitemoptlist TOK_RBRACE
		;

optlabelitemoptlist	: labelitemoptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labelitemoptlist	: labelitemopt
			| labelitemopt labelitemoptlist
			;

labelitemopt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_ITEM TOK_ASSIGN pmid_int
		    {
			labelspec_t	*lp;
			pmID		pmid;
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    if (current_star_metric == 1)
				pmid = pmID_build(pmID_domain($3), pmID_cluster($3), pmID_item(lp->old_id));
			    else if (current_star_metric == 2)
				pmid = pmID_build(pmID_domain($3), pmID_cluster(lp->old_id), pmID_item(lp->old_id));
			    else
				pmid = $3;
			    if (pmid == lp->old_id) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for metric: %s: item: No change", pmIDStr(lp->old_id));
				    yywarn(mess);
				}
			    }
			    else {
				/*
				 * No need to check for conflicts. Multiple label set records for
				 * the same pmid are ok.
				 */
				lp->new_id = pmid;
				lp->flags |= LABEL_CHANGE_ID;
			    }
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for metric %s", pmIDStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for metric %s: label: No change", pmIDStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for metric %s", pmIDStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for metric %s: value: No change", pmIDStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_item_label(current_label_id);
		    }
		;

labelindomspec	: TOK_INDOM indom_int optlabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_labelspec = NULL;
			current_label_id = $2;
			if ($2 == PM_ID_NULL) {
			    /* Indom is not in the archive */
			    do_walk_label = 0;
			}
			else {
			    int found = 0;
			    
			    if (current_star_indom) {
				/* Set up for indoms specified using globbing. */
				star_domain = pmInDom_domain($2);
			    }

			    /* We're looking for label sets for indom(s). */
			    hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			    for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
				 node1 != NULL;
				 node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
				__pmHashCtl	*hcp2;
				__pmHashNode	*node2;
				int		this_type;

				/* Was this object type selected? */
				this_type = (int)(node1->key);
				if (this_type != PM_LABEL_INDOM)
				    continue;

				/*
				 * Collect the label records associated with the specified
				 * imdom(s).
				 */
				hcp2 = (__pmHashCtl *)(node1->data);
				for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				     node2 != NULL;
				     node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				    if (current_star_indom) {
					/* Match the globbed indom spec and keep looking. */
					if (pmInDom_domain((pmID)(node2->key)) == star_domain) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }
					}
				    }
				    else {
					/* Match the exact indom id. */
					if ((pmID)(node2->key) == $2) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key), 0, NULL,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }
					}
				    }
				}
			    }
			    do_walk_label = found;
			}
		    }
		  TOK_LBRACE optlabelindomoptlist TOK_RBRACE
		;

optlabelindomoptlist	: labelindomoptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labelindomoptlist	: labelindomopt
			| labelindomopt labelindomoptlist
			;

labelindomopt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_INDOM TOK_ASSIGN indom_int
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    pmInDom	indom;
			    if (lp->new_id != lp->old_id) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(lp->old_id));
			    else
				indom = $3;
			    if (indom != lp->old_id) {
				lp->new_id = indom;
				lp->flags |= LABEL_CHANGE_ID;
			    }
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instance domain %s: indom: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
			    }
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instance domain %s: label: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instance domain %s: value: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_indom_label(current_label_id);
		    }
		;

labelinstancesspec	: TOK_INSTANCES indom_int optinstancelabeldetails
		    {
			__pmContext	*ctxp;
			__pmHashCtl	*hcp1;
			__pmHashNode	*node1;

			ctxp = __pmHandleToPtr(pmWhichContext());
			assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp,
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			PM_UNLOCK(ctxp->c_lock);
			
			current_labelspec = NULL;
			current_label_id = $2;
			if ($2 == PM_ID_NULL) {
			    /* Indom is not in the archive */
			    do_walk_label = 0;
			}
			else {
			    int found = 0;
			    
			    if (current_star_indom) {
				/* Set up for indoms specified using globbing. */
				star_domain = pmInDom_domain($2);
			    }

			    /* We're looking for label sets for the instances of the indom(s). */
			    hcp1 = &ctxp->c_archctl->ac_log->l_hashlabels;
			    for (node1 = __pmHashWalk(hcp1, PM_HASH_WALK_START);
				 node1 != NULL;
				 node1 = __pmHashWalk(hcp1, PM_HASH_WALK_NEXT)) {
				__pmHashCtl	*hcp2;
				__pmHashNode	*node2;
				int		this_type;

				/* Was this object type selected? */
				this_type = (int)(node1->key);
				if (this_type != PM_LABEL_INSTANCES)
				    continue;

				/*
				 * Collect the label records associated with the specified
				 * indom instance(s).
				 */
				hcp2 = (__pmHashCtl *)(node1->data);
				for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
				     node2 != NULL;
				     node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
				    if (current_star_indom) {
					/* Match the globbed indom spec and keep looking. */
					if (pmInDom_domain((pmID)(node2->key)) == star_domain) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key),
									    current_label_instance, current_label_instance_name,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }
					}
				    }
				    else {
					/* Match the exact indom id. */
					if ((pmID)(node2->key) == $2) {
					    current_labelspec = start_label(this_type, (pmID)(node2->key),
									    current_label_instance, current_label_instance_name,
									    current_label_name, current_label_value);
					    if (current_labelspec) {
						current_labelspec->flags |= LABEL_ACTIVE;
						++found;
					    }
					}
				    }
				}
			    }
			    do_walk_label = found;
			}
		    }
		  TOK_LBRACE optlabelinstancesoptlist TOK_RBRACE
		;

optinstancelabeldetails	: TOK_STRING optlabeldetails
		    {
			current_label_instance = -1;
			current_label_instance_name = $1;
		    }
		| TOK_NUMBER optlabeldetails
		    {
			current_label_instance = atoi($1);
			free($1);
			current_label_instance_name = NULL;
		    }
		| TOK_LABEL_STAR optlabeldetails
		    {
			current_label_instance = -1;
			current_label_instance_name = NULL;
		    }
		| /* nothing */
		    {
			current_label_instance = -1;
			current_label_instance_name = NULL;
			current_label_name = NULL;
			current_label_value = NULL;
		    }
		;

optlabelinstancesoptlist	: labelinstancesoptlist
			| /* nothing */
		    {
			deactivate_labels();
		    }
			;

labelinstancesoptlist	: labelinstancesopt
			| labelinstancesopt labelinstancesoptlist
			;

labelinstancesopt	: TOK_DELETE
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_DELETE, "delete", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_DELETE, "delete", 0)) {
			    lp->flags |= LABEL_DELETE;
			}
		    }
		| TOK_INSTANCES TOK_ASSIGN indom_int
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    pmInDom	indom;
			    if (lp->new_id != lp->old_id) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for instances of indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(lp->old_id));
			    else
				indom = $3;
			    if (indom != lp->old_id) {
				lp->new_id = indom;
				lp->flags |= LABEL_CHANGE_ID;
			    }
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instance domain %s: indom: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
			    }
			}
		    }
		| TOK_INSTANCE TOK_ASSIGN TOK_NUMBER
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_ID, "id", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_ID, "id", 0)) {
			    int	instance;
			    if (lp->new_instance != lp->old_instance) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for instances of indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    instance = atoi($3);
			    free($3);
			    if (instance != lp->old_instance) {
				lp->new_instance = instance;
				lp->flags |= LABEL_CHANGE_INSTANCE;
			    }
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instance domain %s instance %d: instance: No change", pmInDomStr(lp->old_id), lp->old_instance);
				    yywarn(mess);
				}
				free($3);
			    }
			}
		    }
		| TOK_LABEL TOK_ASSIGN jsonname
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_LABEL, "label", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_LABEL, "label", 0)) {
			    if (lp->new_label != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate label clause for instances of indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_label != NULL &&
				strcmp(lp->old_label, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instances of instance domain %s: label: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_label = $3;
				lp->flags |= LABEL_CHANGE_LABEL;
			    }
			}
		    }
		;
		| TOK_VALUE TOK_ASSIGN jsonvalue
		    {
			labelspec_t	*lp;
			for (lp = walk_label(W_START, LABEL_CHANGE_VALUE, "value", 0); lp != NULL; lp = walk_label(W_NEXT, LABEL_CHANGE_VALUE, "value", 0)) {
			    if (lp->new_value != NULL) {
				pmsprintf(mess, sizeof(mess), "Duplicate value clause for instances of indom %s", pmInDomStr(lp->old_id));
				yyerror(mess);
			    }
			    if (lp->old_value != NULL &&
				strcmp(lp->old_value, $3) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Label for instances of instance domain %s: value: No change", pmInDomStr(lp->old_id));
				    yywarn(mess);
				}
				free($3);
			    }
			    else {
				lp->new_value = $3;
				lp->flags |= LABEL_CHANGE_VALUE;
			    }
			}
		    }
		;
		| newlabelspec
		    {
			new_indom_instance_label(current_label_id);
		    }
		;

%%
