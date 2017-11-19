/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmapi.h"
#include "pmlc.h"

/* this pmResult is built after parsing the current statement.  The action
 * routines (Status, LogReq) send it to the logger as a request.
 */
pmResult	*logreq = NULL;
int		sz_logreq = 0;
int		n_logreq = 0;

int		n_metrics = 0;
int		sz_metrics = 0;
metric_t	*metric = NULL;

int		n_indoms = 0;
int		sz_indoms = 0;
indom_t		*indom = NULL;

void
freemetrics(void)
{
    int		i;
    metric_t	*mp;

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	free(mp->name);
	if (mp->status.has_insts && mp->inst != NULL)
	    free(mp->inst);
    }
    n_metrics = 0;
    /* keep the array around for re-use */
}

void
freeindoms(void)
{
    int		i;
    indom_t	*ip;

    for (i = 0, ip = indom; i < n_indoms; i++, ip++) {
	free(ip->inst);
	free(ip->name);
    }
    free(indom);
    sz_indoms = n_indoms = 0;
    indom = NULL;
}

/* Return a pointer to the specified instance domain, adding it to the list if
 * it is not already present.  Returns < 0 on error.
 */
int
addindom(pmInDom newindom, int *resptr)
{
    int		i;
    indom_t	*ip;

    for (i = 0; i < n_indoms; i++)
	if (newindom == indom[i].indom) {
	    *resptr = i;
	    return 0;
	}

    *resptr = -1;			/* in case of errors */
    if (n_indoms >= sz_indoms) {
	sz_indoms += 4;
	i = sz_indoms * sizeof(indom_t);
	if ((indom = (indom_t *)realloc(indom, i)) == NULL) {
	    pmNoMem("expanding instance domain array", i, PM_FATAL_ERR);
	}
    }
    ip = &indom[n_indoms];
    ip->inst = NULL;
    ip->name = NULL;

    if ((i = pmGetInDom(newindom, &ip->inst, &ip->name)) < 0) {
	*resptr = -1;
	return i;
    }
    ip->indom = newindom;
    ip->n_insts = i;
    *resptr = n_indoms;
    n_indoms++;
    return 0;
}

int		metric_cnt;		/* status of addmetric operation(s) */
static int	had_insts;		/* new metric(s) had instances specified */
static int	n_selected;		/* number of metrics selected */
static int	first_inst;		/* check consistency of new metrics' InDoms */

void
beginmetrics(void)
{
    metric_cnt = 0;
    fflush(stdout);
    fflush(stderr);
    freemetrics();
}

void
beginmetgrp(void)
{
    int		i;
    metric_t	*mp;

    had_insts = 0;
    n_selected = 0;
    first_inst = 1;
    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	mp->status.selected = 0;
	mp->status.new = 0;
    }
}

/* Perform any instance domain fixups and error checks required for the latest
 * metrics added.
 */
void
endmetgrp(void)
{
    int		i;
    metric_t	*mp;

    if (n_metrics <= 0) {
	if (metric_cnt >= 0)
	    metric_cnt = PM_ERR_PMID;
	return;
    }

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	if (!mp->status.selected)
	    continue;

	/* just added metric, no instances => use all instances */
	if (mp->status.new) {
	    if (mp->indom != -1 && !had_insts) {
		mp->n_insts = indom[mp->indom].n_insts;
		mp->inst = indom[mp->indom].inst;
		mp->status.has_insts = 0;
	    }
	}
	/* metric was there already */
	else
	    if (mp->indom == -1)
		fprintf(stderr, "Warning: %s has already been specified\n", mp->name);
	    else
		/* if metric had specific instances */
		if (mp->status.has_insts) {
		    if (!had_insts) {
			fprintf(stderr,
				"Warning: %s had instance(s) specified previously.\n"
				"         Using all instances since none specified this time\n",
				mp->name);
			if (mp->inst != NULL)
			    free(mp->inst);
			mp->n_insts = indom[mp->indom].n_insts;
			mp->inst = indom[mp->indom].inst;
			mp->status.has_insts = 0;
		    }
		}
		/* metric had "use all instances" */
		else
		    if (!had_insts)
			fprintf(stderr,
				"Warning: already using all instances for %s\n",
				mp->name);
		    else
			fprintf(stderr,
				"Warning: instance(s) specified for %s\n"
				"         (already using all instances)\n",
				mp->name);
    }
}

void
endmetrics(void)
{
    int			i, j, need;
    metric_t		*mp;
    pmValueSet		*vsp;

    if (metric_cnt < 0) {
	if (connected())
	    fputs("Logging statement ignored due to error(s)\n", stderr);
	goto done;
    }

    if (pmDebugOptions.appl0)
	dumpmetrics(stdout);

    /* free any old values in the reusable pmResult skeleton */
    if (n_logreq) {
	for (i = 0; i < n_logreq; i++)
	    free(logreq->vset[i]);
	n_logreq = 0;
    }

    /* build a result from the metrics and instances in the metric array */
    if (n_metrics > sz_logreq) {
	if (sz_logreq)
	    free(logreq);
	need = sizeof(pmResult) + (n_metrics - 1) * sizeof(pmValueSet *);
	/* - 1 because a pmResult already contains one pmValueSet ptr */
	if ((logreq = (pmResult *)malloc(need)) == NULL) {
	    pmNoMem("building result to send", need, PM_FATAL_ERR);
	}
	sz_logreq = n_metrics;
    }
    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	need = sizeof(pmValueSet);
	/* pmValueSet contains one pmValue, allow for more if > 1 inst */
	if (mp->status.has_insts && mp->n_insts > 1)
	    need += (mp->n_insts - 1) * sizeof(pmValue);
	if ((vsp = (pmValueSet *)malloc(need)) == NULL) {
	    pmNoMem("building result value set", need , PM_FATAL_ERR);
	}
	logreq->vset[i] = vsp;
	vsp->pmid = mp->pmid;
	if (mp->indom == -1)
	    vsp->numval = 1;
	else
	    vsp->numval = mp->status.has_insts ? mp->n_insts : 0;
	vsp->valfmt = PM_VAL_INSITU;
	if (mp->status.has_insts)
	    for (j = 0; j < vsp->numval; j++)
		vsp->vlist[j].inst = mp->inst[j];
	else
	    vsp->vlist[0].inst = PM_IN_NULL;
    }
    logreq->numpmid = n_metrics;
    n_logreq = n_metrics;

done:
    fflush(stderr);
    fflush(stdout);
}

/* Add a metric to the metric list. Sets metric_cnt to < 0 on fatal error. */

void
addmetric(const char *name)
{
    int		i;
    pmID	pmid;
    int		need;
    metric_t	*mp;
    pmDesc	desc;
    int		sts;

    if (metric_cnt < 0)
	return;

    /* Cast const away as pmLookUpName should not modify name */
    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	metric_cnt = sts;
	return;
    }

    for (i = 0, mp = metric; i < n_metrics; i++, mp++)
	if (pmid == mp->pmid)
	    break;

    if (i >= n_metrics) {
	/* expand metric array if necessary */
	if (n_metrics >= sz_metrics) {
	    sz_metrics += 4;
	    need = sz_metrics * sizeof(metric_t);
	    if ((metric = (metric_t *)realloc(metric, need)) == NULL) {
		pmNoMem("expanding metric array", need, PM_FATAL_ERR);
	    }
	}
	mp = &metric[i];
	mp->status.new = 1;
    }
    mp->status.selected = 1;
    n_selected++;
    if (i < n_metrics)			/* already have this metric */
	return;

    if ((sts = pmLookupDesc(pmid, &desc)) == PM_ERR_PMID
	|| desc.type == PM_TYPE_NOSUPPORT) {
	/*
	 * Name is bad or descriptor is not available ...
	 * this is not fatal, but need to back off a little
	 */
	n_selected--;
	return;
    }
    if (sts < 0) {
	/* other errors are more serious */
	metric_cnt = sts;
	return;
    }
    if (desc.indom == PM_INDOM_NULL)
	i = -1;
    else
	if ((metric_cnt = addindom(desc.indom, &i)) < 0)
	    return;

    mp->name = strdup(name);
    mp->pmid = pmid;
    mp->indom = i;
    mp->n_insts = 0;
    mp->inst = NULL;
    mp->status.has_insts = 0;
    metric_cnt = ++n_metrics;
    return;
}

/* Add an instance to the selected metric(s) on the metric list.
 * If name is NULL, use instance number in inst.
 * Check that
 *    - the last group of metrics added have the same pmInDom
 *    - the specified instance exists in the metrics' instance domain.
 */
void
addinst(char *name, int instid)
{
    metric_t	*mp, *first_mp;
    indom_t	*ip;
    int		m, i, j, need;
    int		inst;
    static int	done_inst_msg = 0;

    /* don't try to add instances if one more metrics were erroneous */
    if (metric_cnt < 0)
	return;

    had_insts = 1;

    /* Find the first selected metric */
    for (m = 0, mp = metric; m < n_metrics; m++, mp++)
	if (mp->status.selected)
	    break;
    if (m >= n_metrics) {
	fprintf(stderr, "Ark! No metrics selected for addinst(%s)\n", name);
	abort();
    }
    first_mp = mp;

    if (first_inst) {
	/* check that the first metric doesn't have PM_INDOM_NULL */
	if (mp->indom == -1) {
	    metric_cnt = PM_ERR_INDOM;
	    fprintf(stderr, "%s has no instance domain but an instance was specified\n",
		    mp->name);
	    return;
	}

	/* check that all of the metrics have the same instance domain */
	if (n_selected > 1) {
	    i = 0;
	    for (i++, mp++; m < n_metrics; m++, mp++) {
		if (!mp->status.selected)
		    continue;

		/* check pointers to indoms; PM_INDOM_NULL has -1 */
		if (first_mp->indom != mp->indom) {
		    if (i++ == 0) {
			fprintf(stderr,
				"The instance domains for the following metrics clash with %s:\n",
				first_mp->name);
			metric_cnt = PM_ERR_INDOM;
		    }
		    if (i < 5) {		/* elide any domain errors after this */
			fputs(mp->name, stderr);
			putc('\n', stderr);
		    }
		    else {
			fputs("(etc.)\n", stderr);
			break;
		    }
		}
	    }
	    if (i)
		return;
	}
	first_inst = 0;
    }

    mp = first_mp;			/* go back to first selected metric */
    ip = &indom[mp->indom];
    for (i = 0; i < ip->n_insts; i++)
	if (name != NULL) {
	    if (strcasecmp(name, ip->name[i]) == 0)
		break;
	}
	else
	    if (instid == ip->inst[i])
		break;

    for ( ; m < n_metrics; m++, mp++) {
	if (!mp->status.selected)
	    continue;

	/* check that metric with explicit instances has the specified inst.
	 * For those metrics that specify "all instances" an instance not there yet
	 * is OK (but generates a warning) since we don't need to give pmlogger any
	 * instance ids.
	 * For metrics with specific instances, an unknown instance is an error
	 * since the instance id can't be given to pmlogger.
	 */
	if (i >= ip->n_insts) {
	    if (mp->status.has_insts || mp->status.new) {
		metric_cnt = PM_ERR_INST;
		if (name != NULL)
		    fprintf(stderr, "%s currently has no instance named %s\n", mp->name, name);
		else
		    fprintf(stderr, "%s currently has no instance number %d\n", mp->name, instid);
		if (!done_inst_msg) {
		    fputs("    - you may only specify metric instances active now.  However if no\n"
			  "      instances are specified, pmlogger will use all of the instances\n"
			  "      available at the time the metric is logged\n",
			  stderr);
		    done_inst_msg = 1;
		}
	    }
	    /* for an old metric specifying all instances warn if the specified
	     * instance is not currently in the instance domain.
	     */	    
	    else {
		if (name != NULL)
		    fprintf(stderr,
			    "Warning: instance %s not currently in %s's instance domain\n",
			    name, mp->name);
		else
		    fprintf(stderr,
			    "Warning: instance %d not currently in %s's instance domain\n",
			    instid, mp->name);
		fputs("         (getting all instances anyway)\n", stderr);
	    }
	    continue;
	}
	inst = ip->inst[i];

	/* check that we don't already have the same instance */
	for (j = 0; j < mp->n_insts; j++)
	    if (inst == mp->inst[j])
		break;

	if (j < mp->n_insts) {		/* already have inst */
	    if (mp->status.has_insts) {
		if (name != NULL) {
		    fprintf(stderr,
			    "Warning: instance %s already specified for %s\n",
			    name, mp->name);
		}
		else {
		    fprintf(stderr,
			    "Warning: instance %d already specified for %s\n",
			    instid, mp->name);
		}
	    }

	    /* if the metric had no insts of its own, (it specifies all insts)
	     * just do nothing.  endmetgrp() will print a single message which
	     * is better than having, one printed for each instance specified.
	     */
	    continue;
	}

	else {				/* don't have this inst */
	    /* add inst for new metric or old metric with explicit insts */
	    if (mp->status.new || mp->status.has_insts) {
		j = mp->n_insts++;
		if (j == 0)
		    mp->status.has_insts = 1;
		need = mp->n_insts * sizeof(int);
		if ((mp->inst = (int *)realloc(mp->inst, need)) == NULL) {
		    if (name != NULL)
			fprintf(stderr, "%s inst %s: ", mp->name, name);
		    else
			fprintf(stderr, "%s inst %d: ", mp->name, instid);
		    pmNoMem("expanding instance array", need, PM_FATAL_ERR);
		}
		mp->inst[j] = inst;
	    }
	}
    }
}

void
dumpmetrics(FILE *f)
{
    int		i, j, k;
    metric_t	*mp;

    fprintf(f, "     Inst  Inst Name\n");
    fprintf(f, "    ====== =========\n");

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	fprintf(f, "%s\n", mp->name);
	if (mp->indom == -1)
	    fprintf(f, "           singular instance\n");
	else {
	    indom_t	*ip = &indom[mp->indom];		    
	    int		*inst = ip->inst;
	    char	**name = ip->name;
	    int		n_insts = ip->n_insts;

	    /* No instances specified, use them all */
	    if (mp->n_insts == 0)
		for (j = 0; j < n_insts; j++)
		    fprintf(f, "    %6d %s\n", inst[j], name[j]);
	    else
		for (j = 0; j < mp->n_insts; j++) {
		    int		m_inst = mp->inst[j];;

		    for (k = 0; k < n_insts; k++)
			if (m_inst == inst[k])
			    break;
		    if (k < n_insts)
			fprintf(f, "    %6d %s\n", inst[k], name[k]);
		    else
			fprintf(f, "    KAPOWIE! inst %d not found\n", m_inst);
		}
	}
	putc('\n', f);
    }
}
