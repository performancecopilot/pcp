/*
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
 *  pmlogextract parser
 */
#include "pmapi.h"
#include "impl.h"
#include "logger.h"

int	i;
int	found;
int	argcount;		/* number of arguments in config file */
char	*arglist[24];		/* arguments from config file */
char	emess[240];

static char	*name;
static int	sts;
static int	numinst;	/* num instances (per metric) in config file */
static int	*intlist;	/* instance id's  (internal list) */
static char	**extlist;	/* instance names (external list) */
static int	warn = 1;

extern int	lineno;

static void buildinst(int *, int **, char ***, int , char *);
static void freeinst(int *, int *, char **);

%}
%union {
	long lval;
	char * str;
}

%token	LSQB
	RSQB
	COMMA

%token<str>		NAME STRING
%token<lval>	NUMBER 
%%

config		: somemetrics
    		;

somemetrics	: metriclist
		| /* nothing */
		;

metriclist	: metricspec
		| metriclist metricspec
		| metriclist COMMA metricspec
		;

metricspec	: NAME { name = strdup($1); numinst = 0; } optinst
		    {
			if (name == NULL) {
			    snprintf(emess, sizeof(emess), "malloc failed: %s", osstrerror());
			    yyerror(emess);
			}
			found = 0;
			for (i=0; i<inarchnum; i++) {
			    if ((sts = pmUseContext(inarch[i].ctx)) < 0) {
				fprintf(stderr, 
				    "%s: Error: cannot use context (%d) "
				    "from archive \"%s\"\n", 
				    pmProgname, inarch[i].ctx, inarch[i].name);
				exit(1);
			    }

			    if ((sts = pmTraversePMNS (name, dometric)) >= 0) {
				found = 1;
				break;
			    }
			}

			if (!found) {
			    snprintf(emess, sizeof(emess), 
				"Problem with lookup for metric \"%s\" ... "
				"metric ignored", name);
			    yywarn(emess);
			    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
			}

			free(name);
			freeinst(&numinst, intlist, extlist);
		    }
		;

optinst		: LSQB instancelist RSQB
		| /* nothing */
		;

instancelist	: instance
		| instance instancelist
		| instance COMMA instancelist
		;

instance	: NAME	{ buildinst(&numinst, &intlist, &extlist, -1, $1); }
		| NUMBER{ buildinst(&numinst, &intlist, &extlist, $1, NULL);}
		| STRING{ buildinst(&numinst, &intlist, &extlist, -1, $1);}
		;

%%

void
dometric(const char *name)
{
    int		i;
    int		j;
    int		inst;
    int		skip;
    int		sts;
    pmID	pmid;
    pmDesc	*dp;

    if ((dp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL) {
	goto nomem;
    }

    /* Cast away const, pmLookUpName should not modify name
     */
    if ((sts = pmLookupName(1,(char **)&name,&pmid)) < 0 || pmid == PM_ID_NULL){
	snprintf(emess, sizeof(emess), "Metric \"%s\" is unknown ... not logged", name);
	goto defer;
    }

    if ((sts = pmLookupDesc(pmid, dp)) < 0) {
	snprintf(emess, sizeof(emess),
	    "Description unavailable for metric \"%s\" ... not logged", name);
	goto defer;
    }


    ml_numpmid++;
    if (ml_size < ml_numpmid) {
	ml_size = ml_numpmid;
	ml = (mlist_t *) realloc(ml, ml_size * sizeof(mlist_t));
	if (ml == NULL) {
            goto nomem;
	}
    }

    ml[ml_numpmid-1].name = NULL;
    ml[ml_numpmid-1].idesc = NULL;
    ml[ml_numpmid-1].odesc = NULL;
    ml[ml_numpmid-1].numinst = 0;
    ml[ml_numpmid-1].instlist = NULL;


    /*
     * ml_nmpmid-1 == index of latest addition to the list
     */

    ml[ml_numpmid-1].name = strdup(name);
    if (ml[ml_numpmid-1].name == NULL) {
	goto nomem;
    }

    /* input descriptor (idesc) and output descriptor (odesc) are initially
     * pointed at the same descriptor
     */
    ml[ml_numpmid-1].idesc = dp;
    ml[ml_numpmid-1].odesc = dp;
    ml[ml_numpmid-1].numinst = numinst;

    skip = 0;
    if (numinst == 0) {
	intlist = NULL;
	extlist = NULL;
	/* user hasn't specified any instances
	 *	- if there is NO instance domain, then allocate at least one
	 *	- if there is an instance domain, set to -1 and searchmlist
         *        will grab them all
	 */
	if (dp->indom == PM_INDOM_NULL) {
	    ml[ml_numpmid-1].numinst = 1;
	}
	else {
	    ml[ml_numpmid-1].numinst = -1;
	    ml[ml_numpmid-1].instlist = NULL;
	}

	if (ml[ml_numpmid-1].numinst >= 1) {
	    /*
	     * malloc here, and keep ... gets buried
	     */
	    ml[ml_numpmid-1].instlist = (int *)malloc(ml[ml_numpmid-1].numinst * sizeof(int));
	    if (ml[ml_numpmid-1].instlist == NULL) {
		goto nomem;
	    }

	    for (i=0; i<ml[ml_numpmid-1].numinst; i++) {
		if (intlist == NULL) {
		    /* PM_INDOM_NULL case */
		    ml[ml_numpmid-1].instlist[i] = -1;
		}
		else
		    ml[ml_numpmid-1].instlist[i] = intlist[i];

	    } /*for(i)*/
	}

	if (intlist != NULL)
	    free(intlist);
	if (extlist != NULL)
	    free(extlist);

	intlist = NULL;
	extlist = NULL;
    }
    else if (numinst) {
	/*
	 * malloc here, and keep ... gets buried
	 */
	ml[ml_numpmid-1].instlist = (int *)malloc(numinst * sizeof(int));
	if (ml[ml_numpmid-1].instlist == NULL) {
	    goto nomem;
	}

	j = 0;
	for (i=0; i<numinst; i++) {
	    inst = -1;
	    if (extlist[i] != NULL) {
		if ((sts = pmLookupInDomArchive(dp->indom, extlist[i])) < 0) {
		    snprintf(emess, sizeof(emess),
			"Instance \"%s\" is not defined for the metric \"%s\"",
			    extlist[i], name);
		    yywarn(emess);
		    ml[ml_numpmid-1].numinst--;
		    continue;
		}
		inst = sts;
	    }
	    else {
		char *p;
		if ((sts = pmNameInDomArchive(dp->indom, intlist[i], &p)) < 0) {
		    snprintf(emess, sizeof(emess),
			"Instance \"%d\" is not defined for the metric \"%s\"",
			    intlist[i], name);
		    yywarn(emess);
		    ml[ml_numpmid-1].numinst--;
		    continue;
		}
		else {
		    inst = intlist[i];
		}
		free(p);
	    }

	    /* if inst is > -1 then this instance exists, and its id is `inst'
	     */
	    if (inst > -1) {
		ml[ml_numpmid-1].instlist[j] = inst;
		++j;
	    }
	} /* for(i) */

	if (ml[ml_numpmid-1].numinst == 0)
	    skip = 1;

    }


    /* if skip has been set, then this metric has no instances
     * (probably because all instances specified by user are invalid)
     * then we don't want this metric, so ...
     *	- free dp (the descriptor)
     *	- free the instance list
     *	- adjust ml_numpmid ... do not free the space ... it isn't much
     *		and we may need it (if there is another metric)
     */
    if (skip) {
	free(dp);
	free(ml[ml_numpmid-1].instlist);
	--ml_numpmid;
    }
    else {
	/* EXCEPTION PCP 2.1.1 - may want to check instances here
	 */
    }
    return;

defer:
    /* EXCEPTION PCP 2.1.1 - defer this one until sometime later ... */
    if (warn) {
	yywarn(emess);
	fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    }
    free(dp);
    return;

nomem:
    snprintf(emess, sizeof(emess), "malloc failed: %s", osstrerror());
    yyerror(emess);
}


static void
buildinst(int *numinst, int **intlist, char ***extlist, int intid, char *extid)
{
    char        **el;
    int         *il;
    int         num = *numinst;

    if (num == 0) {
	il = NULL;
	el = NULL;
    }
    else {
	il = *intlist;
	el = *extlist;
    }

    el = (char **)realloc(el, (num+1)*sizeof(el[0]));
    il = (int *)realloc(il, (num+1)*sizeof(il[0]));

    il[num] = intid;

    if (extid == NULL)
	el[num] = NULL;
    else {
	if (*extid == '"') {
	    char        *p;
	    p = ++extid;
	    while (*p && *p != '"') p++;
	    *p = '\0';
	}
	el[num] = strdup(extid);
    }

    *numinst = ++num;
    *intlist = il;
    *extlist = el;
}


static void
freeinst(int *numinst, int *intlist, char **extlist)
{
    int         i;

    if (*numinst) {
	free(intlist);
	for (i = 0; i < *numinst; i++)
	    free(extlist[i]);
	free(extlist);

	intlist = NULL;
	extlist = NULL;
	*numinst = 0;
    }
}

