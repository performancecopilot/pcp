/*
 * Copyright (c) 1999-2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#define	SYSLOG_NAMES
#include "dstruct.h"
#include "eval.h"
#include "syntax.h"
#include "systemlog.h"
#include "pmapi.h"

#if defined(IS_SOLARIS) || defined(IS_AIX) || defined(IS_MINGW)
#include "logger.h"
#endif

/*
 * based on source for logger(1)
 */
static int
decode(char *name, CODE *codetab)
{
    CODE *c;

    if (isdigit((int)*name))
	return (atoi(name));

    for (c = codetab; c->c_name; c++)
	if (!strcasecmp(name, c->c_name))
	    return (c->c_val);

    return (-1);
}

/*
 * Decode a symbolic name to a numeric value
 * ... based on source for logger(1)
 */
static int
pencode(char *s)
{
    char *save;
    int fac, lev;

    save = s;
    while (*s && *s != '.') ++s;
    if (*s) {
	*s = '\0';
	fac = decode(save, facilitynames);
	if (fac < 0) {
	    synwarn();
	    fprintf(stderr, "Ignoring unknown facility (%s) for -p in syslog action\n", save);
	    fac = 0;
	}
	s++;
    }
    else {
	fac = 0;
	s = save;
    }
    lev = decode(s, prioritynames);
    if (lev < 0) {
	synwarn();
	fprintf(stderr, "Ignoring unknown priority (%s) for -p in syslog action\n", s);
	lev = LOG_NOTICE;
    }
    return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

/*
 * handle splitting of -t tag and -p prio across one or two
 * arguments to the syslog action in the rule
 */
static void
nextch(char **p, Expr **x)
{
    static char	end = '\0';
    (*p)++;
    while (**p == '\0') {
	if ((*x)->arg1 == NULL) {
	    *p = &end;
	    return;
	}
	*x = (*x)->arg1;
	*p = (*x)->ring;
    }
}

/*
 * post-process the expression tree for a syslog action to gather
 * any -t tag or -p pri options (as for logger(1)) and build the
 * encoded equivalent as a new expression accessed via arg2 from
 * the head of the arguments list
 */
void
do_syslog_args(Expr *act)
{
    int		pri = -1;
    char	*tag = NULL;
    char	*p;
    char	*q;
    Expr	*others;
    Expr	*tmp;
    Expr	*new;

    /*
     * scan for -p pri and -t tag
     */
    for (others = act->arg1; others != NULL; ) {
	if (others->ring == NULL) break;
	p = others->ring;
	if (*p != '-') break;
	nextch(&p, &others);
	if (*p == 'p' && pri == -1) {
	    nextch(&p, &others);
	    while (*p && isspace((int)*p)) nextch(&p, &others);
	    if (*p == '\0') {
		synwarn();
		fprintf(stderr, "Missing [facility.]priority after -p in syslog action\n");
	    }
	    else {
		q = p+1;
		while (*q && !isspace((int)*q)) q++;
		if (*q) {
		    synwarn();
		    fprintf(stderr, "Ignoring extra text (%s) after -p pri in syslog action\n", q);
		    *q = '\0';
		}
		pri = pencode(p);
	    }
	}
	else if (*p == 't' && tag == NULL) {
	    nextch(&p, &others);
	    while (*p && isspace((int)*p)) nextch(&p, &others);
	    if (*p == '\0') {
		synwarn();
		fprintf(stderr, "Missing tag after -t in syslog action\n");
	    }
	    else {
		q = p+1;
		while (*q && !isspace((int)*q)) q++;
		if (*q) {
		    synwarn();
		    fprintf(stderr, "Ignoring extra text (%s) after -t tag in syslog action\n", q);
		    *q = '\0';
		}
		tag = p;
	    }
	}
	else
	    break;
	others = others->arg1;
    }

    /* defaults if -t and/or -p not seen */
    if (pri < 0) pri = LOG_NOTICE;
    if (tag == NULL) tag = "pcp-pmie";

    /*
     * construct new arg2 argument node, with
     *	ring	-> pri (int) and tag (char *) concatenated
     */
    new = (Expr *) zalloc(sizeof(Expr));
    new->op = NOP;
    new->ring = (char *)alloc(sizeof(int)+strlen(tag)+1);
    *((int *)new->ring) = pri;
    strcpy(&((char *)new->ring)[sizeof(int)], tag);
    act->arg2 = new;
    new->parent = act;

    /* free old argument nodes used for -p and/or -t specifications */
    for (tmp = act->arg1; tmp != others; ) {
	if (tmp->ring) {
	    free(tmp->ring);
	}
	new = tmp->arg1;
	free(tmp);
	tmp = new;
    }

    /* re-link remaining argument nodes */
    if (others != act->arg1) {
	act->arg1 = others;
	if (others != NULL)
	    others->parent = act;
    }

}
