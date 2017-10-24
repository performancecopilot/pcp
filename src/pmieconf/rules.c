/*
 * rules.c - rule description parsing routines (rules & pmie config)
 * 
 * Copyright (c) 1998-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"
#include "rules.h"
#include "stats.h" 

#define SEP __pmPathSeparator()


#define PMIE_FILE	"pmieconf-pmie"
#define	PMIE_VERSION	"1"	/* local configurations file format version */
#define RULES_FILE	"pmieconf-rules"
#define	RULES_VERSION	"1"	/* rule description file format version */

#define START_STRING	\
	"// --- START GENERATED SECTION (do not change this section) ---\n"
#define END_STRING	\
	"// --- END GENERATED SECTION (changes below will be preserved) ---\n"
#define TOKEN_LENGTH	2048	/* max length of input token, incl string */
#define LINE_LENGTH	4096

#if !defined(sgi)
#define PROC_DIR	"/proc"
#else
#define PROC_DIR	"/proc/pinfo"
#endif

char	errmsg[512];		/* error message buffer */
char	rulepath[MAXPATHLEN+1];	/* root of rules files  */
char	pmiefile[MAXPATHLEN+1];	/* pmie configuration file */
char	token[TOKEN_LENGTH+1];

rule_t		*rulelist;	/* global list of rules */
unsigned int	rulecount;	/* # rule list elements */
rule_t		*globals;	/* list of atoms with global scope */

#define GLOBAL_LEN	7
static char	global_name[] = "global";	/* GLOBAL_LEN chars long */
static char	global_data[] = "generic variables applied to all rules";
static char	global_help[] = \
    "The global variables are used by all rules, but their values can be\n"
    "overridden at the level of an individual rule or group of rules.";
static char	yes[] = "yes";
static char	no[]  = "no";

static char		*filename;	/* file currently being parsed */
static unsigned int	linenum;	/* input line number */

symbol_t types[] = {
    { TYPE_STRING,	"string" },     /* predicate data types */
    { TYPE_DOUBLE,	"double" },
    { TYPE_INTEGER,	"integer" },
    { TYPE_UNSIGNED,	"unsigned" },
    { TYPE_PERCENT,	"percent" },
    { TYPE_HOSTLIST,	"hostlist" },
    { TYPE_INSTLIST,	"instlist" },
    { TYPE_PRINT,	"print" },      /* action types */
    { TYPE_SHELL,	"shell" },
    { TYPE_ALARM,	"alarm" },
    { TYPE_SYSLOG,	"syslog" },
    { TYPE_RULE,	"rule" },       /* fundamental type */
};
int	numtypes = (sizeof(types)/sizeof(types[0]));

symbol_t attribs[] = {
    { ATTRIB_HELP,	"help" },
    { ATTRIB_MODIFY,	"modify" },
    { ATTRIB_ENABLED,	"enabled" },
    { ATTRIB_DISPLAY,	"display" },
    { ATTRIB_DEFAULT,	"default" },
    { ATTRIB_DEFAULT,	"summary" },	/*  alias for "default"  */
    { ATTRIB_VERSION,	"version" },	/* applies to rules only */
    { ATTRIB_PREDICATE,	"predicate" },	/* applies to rules only */
    { ATTRIB_ENUMERATE,	"enumerate" },	/* applies to rules only */
};
int	numattribs = (sizeof(attribs)/sizeof(attribs[0]));

/* pmiefile variables */
static int	gotpath;	/* state flag - has realpath been run */
static char	*save_area;	/* holds text to restore on write */
static int	sa_size;	/* current size of save area */
static int	sa_mark = 1;	/* number used chars in save area, 1 for \0 */
static dep_t	*dlist;		/* list of depreciated rules */
static int	dcount;		/* number of entries in dlist */
static char	drulestring[] =	"rule definition no longer exists";
static char	dverstring[] =	"rule version no longer supported";

/* io-related stuff */
extern int resized(void);

char	*get_pmiefile(void) { return &pmiefile[0]; }
char	*get_rules(void) { return &rulepath[0]; }

char *
get_aname(rule_t *r, atom_t *a)
{
    if (r == globals)
	return &a->name[GLOBAL_LEN];	/* lose "globals." at the start */
    return a->name;
}


/*
 *   ####  error reporting routines  ###
 */

static void
alloc_error(size_t request)
{
    if (linenum == 0)	/* parsing user input, not a file */
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for requested operation.\n"
		    "    requested: %u bytes", (unsigned int)request);
    else
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for parsing file.\n"
		    "    requested: %u bytes", (unsigned int)request);
}

static void
parse_error(char *expected, char *found)
{
    if (linenum == 0)	/* parsing user input, not a file */
	pmsprintf(errmsg, sizeof(errmsg), "input is invalid - expected %.60s, got \"%.60s\"",
		expected, found);
    else
	pmsprintf(errmsg, sizeof(errmsg), "file parsing error.\n"
		"    line number: %u (\"%s\")\n"
		"    expected: %.60s\n"
		"    found: %.60s", linenum, filename, expected, found);
}

/* report attribute format error */
static void
type_error(char *attrib, char *expected)
{
    pmsprintf(errmsg, sizeof(errmsg), "%s's value is invalid.\n"
		    "    It should %s.", attrib, expected);
}


/*
 *   ####  search routines  ###
 */

char *
find_rule(char *name, rule_t **rule)
{
    int	i;

    for (i = 0; i < rulecount; i++) {
	if (strcmp(rulelist[i].self.name, name) == 0) {
	    *rule = &rulelist[i];
	    return NULL;
	}
    }
    pmsprintf(errmsg, sizeof(errmsg), "rule named \"%s\" does not exist", name);
    return errmsg;
}

/* is global attribute 'atom' overridden by a local in 'rule's atom list */
int
is_overridden(rule_t *rule, atom_t *atom)
{
    atom_t	*aptr;

    for (aptr = rule->self.next; aptr != NULL; aptr = aptr->next)
	if (strcmp(get_aname(globals, atom), get_aname(rule, aptr)) == 0)
	    return 1;
    return 0;
}

/* tests whether a rule is in the fullname group, if so returns 0 */
int
rule_match(char *fullname, char *rulename)
{
    char	*s;

    /* if fullname == rulename, then obvious match */
    if (strcmp(fullname, rulename) == 0)
	return 1;
    /* fullname may be a group, so match against rulename's groups */
    s = strcpy(token, rulename);	/* reuse the token buffer */
    while ((s = strrchr(s, '.')) != NULL) {
	s[0] = '\0';
	if (strcmp(token, fullname) == 0)
	    return 1;
    }
    return 0;
}

/* find rule or set of rules in given rule or group name */
char *
lookup_rules(char *name, rule_t ***rlist, unsigned int *count, int all)
{
    size_t		size;
    rule_t		**rptr = NULL;
    unsigned int	i;
    unsigned int	matches = 0;

    /* search through the rulelist and build up rlist & count */
    for (i = 0; i < rulecount; i++) {
	/* don't match globals if we've been asked for "all" */
	if ((all && i > 0) || rule_match(name, rulelist[i].self.name)) {
	    size = (1 + matches) * sizeof(rule_t *);
	    if ((rptr = (rule_t **)realloc(rptr, size)) == NULL) {
		pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for rule search"
			" (needed %u bytes)\n", (unsigned int)size);
		return errmsg;
	    }
	    rptr[matches] = &rulelist[i];
	    matches++;
	}
    }
    if (matches == 0) {
	pmsprintf(errmsg, sizeof(errmsg), "no group or rule names match \"%s\"", name);
	return errmsg;
    }
    *rlist = rptr;	/* rlist must be freed by caller */
    *count = matches;
    return NULL;
}


/*
 *   ####  memory management routines  ###
 */

static char *
alloc_string(size_t size)
{
    char	*p;

    if ((p = (char *)malloc(size)) == NULL)
	alloc_error(size);
    return p;
}

atom_t *
alloc_atom(rule_t *r, atom_t atom, int global)
{
    atom_t	*aptr;
    atom_t	*tmp;

    /* create some space and copy in the atom data we have already */
    if ((aptr = (atom_t *)malloc(sizeof(atom_t))) == NULL) {
	alloc_error(sizeof(atom_t));
	return NULL;
    }
    *aptr = atom;
    aptr->next = NULL;	/* want contents of this atom, but not rest of list */
    if (global) {	/* applies to all rules */
	r = rulelist;
	aptr->global = 1;
    }

    aptr->next = NULL;	/* want contents of this atom, but not rest of list */

    /* stick into the list of atoms associated with this rule */
    if (r->self.next == NULL)
	r->self.next = aptr;	/* insert at head of list */
    else {
	for (tmp = r->self.next; tmp->next != NULL; tmp = tmp->next);
	tmp->next = aptr;		/* append at tail of list */
    }

    return aptr;
}

rule_t *
alloc_rule(rule_t rule)
{
    size_t	size;
    rule_t	*rptr;

    /* first check that name is unique */
    if (find_rule(rule.self.name, &rptr) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "rule name \"%s\" has already been used, duplicate name"
	    " found in:\n\t\"%.60s\", line %u.", rule.self.name, filename, linenum);
	return NULL;
    }
    size = (rulecount+1) * sizeof(rule_t);
    if ((rulelist = globals = (rule_t *)realloc(rulelist, size)) == NULL) {
	alloc_error(size);
	return NULL;
    }
    rptr = &rulelist[rulecount];
    *rptr = rule;
    rulecount++;
    return rptr;
}


/*
 *   ####  misc parsing routines  ###
 */

/* given string contains no isgraph chars? */
int
empty_string(char *s)
{
    char	*str = s;
    while (*str != '\0') {
	if (isgraph((int)*str))
	    return 0;
	str++;
    }
    return 1;
}


/* lookup keyword, returns symbol identifier or -1 if not there */
int
map_symbol(symbol_t *table, int tsize, char *symbol)
{
    int	i;

    for (i = 0; i < tsize; i++) {
	if (strcmp(symbol, table[i].symbol) == 0)
	    return table[i].symbol_id;
    }
    return -1;
}

/* lookup symbol identifier, returns keyword or NULL if not there */
char *
map_identifier(symbol_t *table, int tsize, int symbol_id)
{
    int	i;

    for (i = 0; i < tsize; i++) {
	if (symbol_id == table[i].symbol_id)
	    return table[i].symbol;
    }
    return NULL;
}


/* parse yes/no attribute value; returns 0 no, 1 yes, -1 error */
int
map_boolean(char *token)
{
    if (token[0] == 'y')
	return 1;
    if (token[0] == 'n')
	return 0;
    parse_error("yes or no", token);
    return -1;
}


/* scan token from string, return 1 ok, 0 no more, -1 error */
int
string_token(char **scan, char *token)
{
    char	*s = *scan;
    char	*t = token;

    while (! isgraph((int)*s) || *s == ',') {
	if (*s == '\0')
	    return 0;
	s++;
    }

    if (*s == '\'') {   /* quoted token */
	*t++ = *s++;
	while (*s != '\'') {
	    if (*s == '\\')
		s++;
	    if (*s == '\0')
		return -1;
	    *t++ = *s++;
	}
	*t++ = *s++;
    }
    else {      /* ordinary token */
	while (isgraph((int)*s) && *s != ',')
	    *t++ = *s++;
    }

    *t = '\0';
    *scan = s;
    return 1;
}


/* check proposed value for type, returns NULL/failure message */
char *
validate(int type, char *name, char *value)
{
    int	    		x;
    char    		*s;
    double  		d;
    /*
     * Below we don't care about the value from strtol() and strtoul()
     * we're interested in updating the pointer "s".  The messiness is
     * thanks to gcc and glibc ... strtol() amd strtoul() are marked
     * __attribute__((warn_unused_result)) ... to avoid warnings on all
     * platforms, assign to dummy variables that are explicitly marked
     * unused.
     */
    long    		l __attribute__((unused));
    unsigned long	ul __attribute__((unused));

    switch (type) {
    case TYPE_RULE:
    case TYPE_STRING:
	break;
    case TYPE_SHELL:
    case TYPE_PRINT:
    case TYPE_ALARM:
    case TYPE_SYSLOG:
	if (map_boolean(value) < 0)
	    return errmsg;
	break;
    case TYPE_DOUBLE:
	d = strtod(value, &s);
	if (*s != '\0') {
	    type_error(name, "be a real number");
	    return errmsg;
	}
	break;
    case TYPE_INTEGER:
	l = strtol(value, &s, 10);
	if (*s != '\0') {
	    type_error(name, "be an integer number");
	    return errmsg;
	}
	break;
    case TYPE_UNSIGNED:
	ul = strtoul(value, &s, 10);
	if (*s != '\0') {
	    type_error(name, "be a positive integer number");
	    return errmsg;
	}
	break;
    case TYPE_PERCENT:
	if ((s = strrchr(value, '%')) != NULL)	/* % as final char is OK */
	    *s = '\0';
	d = strtod(value, &s);
	if (*s != '\0' || d < 0.0 || d > 100.0) {
	    type_error(name, "be a percentage between 0.0 and 100.0");
	    return errmsg;
	}
	break;
    case TYPE_HOSTLIST:
    case TYPE_INSTLIST:
	if ((s = alloc_string(strlen(value)+1)) == NULL)
	    return errmsg;
	while ((x = string_token(&value, s)) > 0)
	    ;
	if (x < 0) {
	    type_error(name, "include a closing single quote");
	    return errmsg;
	}
	free(s);
	break;
    }
    return NULL;
}


/*
 * printable string form of atoms value, returns NULL terminated string
 * pp (pretty print) argument valued 1 means use format appropriate for
 * a user interface
 */

char *
value_string(atom_t *atom, int pp)
{
    int		key;
    int		i = 0;
    int		start = 1;
    int		quoted = 0;
    char	*s;

    switch (atom->type) {
    case TYPE_RULE:
    case TYPE_STRING:
	if (pp) {
	    pmsprintf(token, sizeof(token), "\"%s\"", atom->data);
	    return token;
	}
	return atom->data;
    case TYPE_PRINT:
    case TYPE_SHELL:
    case TYPE_ALARM:
    case TYPE_SYSLOG:
	return atom->enabled? yes : no;
    case TYPE_HOSTLIST:
    case TYPE_INSTLIST:
	if (pp) token[i++] = '"';
	if (atom->type == TYPE_HOSTLIST) key = ':';
	else key = '#';
	for (s = atom->data; *s != '\0'; s++) {
	    if (!isspace((int)*s)) {
		if (start && !pp) {
		    token[i++] = key;
		    token[i++] = '\'';
		    if (*s != '\'')
			quoted = 0;
		    else if (!quoted) {
			quoted = 1;
			start = 0;
			continue;
		    }
		}
		else if (*s == '\'' && !start && !pp)
		    quoted = 0;
		start = 0;
	    }
	    else if (!pp && !quoted) {
		quoted = 0;
		if (i > 0 && token[i-1] != '\'')
		    token[i++] = '\'';
		start = 1;
	    }
	    token[i++] = *s;
	}
	if (!pp && i > 0 && token[i-1] != '\'')
	    token[i++] = '\'';
	else if (pp) token[i++] = '"';
	token[i++] = '\0';
	return token;
    case TYPE_DOUBLE:
	pmsprintf(token, sizeof(token), "%g", strtod(atom->data, &s));
	return token;
    case TYPE_INTEGER:
	pmsprintf(token, sizeof(token), "%ld", strtol(atom->data, &s, 10));
	return token;
    case TYPE_UNSIGNED:
	pmsprintf(token, sizeof(token), "%lu", strtoul(atom->data, &s, 10));
	return token;
    case TYPE_PERCENT:
	pmsprintf(token, sizeof(token), "%g%c", strtod(atom->data, &s), pp? '%':'\0');
	return token;
    }
    return NULL;
}


/*  ####  rules file parsing routines  ####  */


/* returns attrib number or -1 if not an attribute */
int
is_attribute(char *aname)
{
    return map_symbol(attribs, numattribs, aname);
}

/* returns attrib value as a string, or NULL on error */
char *
get_attribute(char *attrib, atom_t *atom)
{
    char	*value = NULL;

    switch (map_symbol(attribs, numattribs, attrib)) {
    case ATTRIB_HELP:
	value = atom->help;
	break;
    case ATTRIB_MODIFY:
	if (atom->modify) value = yes;
	else value = no;
	break;
    case ATTRIB_ENABLED:
	if (atom->enabled) value = yes;
	else value = no;
	break;
    case ATTRIB_DISPLAY:
	if (atom->display) value = yes;
	else value = no;
	break;
    case ATTRIB_DEFAULT:
	if (IS_ACTION(atom->type)) {
	    if (atom->enabled) value = yes;
	    else value = no;
	}
	else
	    value = atom->data;
	break;
    }
    return value;
}


/*
 *   ####  sorting routines  ###
 */

static int
compare_rules(const void *a, const void *b)
{
    rule_t	*ra = (rule_t *)a;
    rule_t	*rb = (rule_t *)b;
    return strcmp(ra->self.name, rb->self.name);
}

void
sort_rules(void)
{
    /* start at second array element so that 'globals' is skipped */
    qsort(&rulelist[1], rulecount-1, sizeof(rule_t), compare_rules);
}


/* revert to default rules file values for a single atom (enabled/data/both) */
static char *
atom_defaults(atom_t *a, atom_t *p, char *param)
{
    int		sts = map_symbol(attribs, numattribs, param);

    if (sts != -1) {	/* an attribute - is it valid? */
	if (sts == ATTRIB_ENABLED) {
	    if (a->global) {	/* this was a global atom promoted to local */
		if (p) p->next = a->next;
		free(a->name);
		free(a);
		a = NULL;
	    }
	    else {
		a->enabled = a->denabled;	/* reset enabled flag */
		a->changed = 0;
	    }
	    return NULL;
	}
	pmsprintf(errmsg, sizeof(errmsg), "variable \"%s\" is inappropriate for this "
			"operation", param);
	return errmsg;
    }
    else {
	if (a->global) {	/* this was a global atom promoted to local */
	    if (p) p->next = a->next;
	    free(a->name);
	    free(a);
	    a = NULL;
	}
	else {
	    if (strcmp(a->data, a->ddata) != 0) {	/* need to alloc mem? */
		free(a->data);
		if ((a->data = strdup(a->ddata)) == NULL) {
		    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to set defaults");
		    return errmsg;
		}
            }
	    a->enabled = a->denabled;
	    a->changed = 0;
	}
	return NULL;
    }
}

/* revert to default rules values for a rule or attribute (enabled/data/both) */
char *
rule_defaults(rule_t *rule, char *param)
{
    atom_t      *aptr;
    atom_t      *prev = NULL;

    if (param == NULL) {	/* for this rule, reset all attributes */
	for (aptr = &rule->self; aptr != NULL; aptr = aptr->next) {
	    atom_defaults(aptr, prev, aptr->name);
	    prev = aptr;
	}
    }
    else {	/* find the associated atom, and just reset that */
	if (map_symbol(attribs, numattribs, param) != -1) {
	    rule->self.enabled = rule->self.denabled;	/* reset enabled flag */
	    rule->self.changed = 0;
	    return NULL;
	}
	for (aptr = &rule->self; aptr != NULL; aptr = aptr->next) {
	    if (strcmp(get_aname(rule, aptr), param) == 0)
		return atom_defaults(aptr, prev, param);
	    prev = aptr;
	}
    }
    return NULL;
}

/* set an attribute field in an atom; returns NULL/failure message */
static char *
set_attribute(rule_t *r, atom_t *atom, int attrib, char *value, int changed)
{
    char	*s;
    int		sts;

    switch(attrib) {
    case ATTRIB_HELP:
	if (empty_string(value)) {
	    parse_error("non-empty string for help", value);
	    return errmsg;
	}
	if ((s = alloc_string(strlen(value)+1)) == NULL)
	    return errmsg;
	atom->help = strcpy(s, value);
	break;
    case ATTRIB_MODIFY:
	if ((sts = map_boolean(value)) < 0)
	    return errmsg;
	atom->modify = sts;
	break;
    case ATTRIB_ENABLED:
	if ((sts = map_boolean(value)) < 0)
	    return errmsg;
	if (!changed)	/* initially, set enabled to default */
	    atom->denabled = sts;
	atom->enabled = sts;
	break;
    case ATTRIB_DISPLAY:
	if ((sts = map_boolean(value)) < 0)
	    return errmsg;
	atom->display = sts;
	break;
    case ATTRIB_DEFAULT:
	if (IS_ACTION(atom->type) && changed) {
	    if ((sts = map_boolean(value)) < 0)
		return errmsg;
	    atom->enabled = sts;
	}
	else {	/* actions from rules file (string) handled here too... */
	    if (!IS_ACTION(atom->type) &&
		(validate(atom->type, get_aname(r, atom), value) != NULL))
		return errmsg;
	    sts = strlen(value)+1;
	    if ((s = alloc_string(sts)) == NULL)
		return errmsg;
	    atom->data = strcpy(s, value);
	    if (!changed) {	/* initially, set the default as well */
		if ((s = alloc_string(sts)) == NULL) {
		    free(atom->data);
		    atom->data = NULL;
		    return errmsg;
		}
		atom->ddata = strcpy(s, value);
	    }
	}
	break;
    }
    if (changed)
	atom->changed = 1;
    return NULL;
}

/* set a parameter field in a rule; returns NULL/failure message */
char *
value_change(rule_t *rule, char *param, char *value)
{
    int         sts;
    atom_t      *aptr;

    if ((sts = map_symbol(attribs, numattribs, param)) != -1)
	return set_attribute(rule, &rule->self, sts, value, 1);
    else {
	for (aptr = rule->self.next; aptr != NULL; aptr = aptr->next) {
	    if (strcmp(get_aname(rule, aptr), param) == 0)
		return set_attribute(rule, aptr, ATTRIB_DEFAULT, value, 1);
	}
	/* if found in globals, promote the global to customised local.. */
	for (aptr = globals->self.next; aptr != NULL; aptr = aptr->next) {
	    if (strcmp(get_aname(globals, aptr), param) == 0) {
		if ((aptr = alloc_atom(rule, *aptr, 0)) == NULL)
		    return errmsg;
		if ((aptr->name = strdup(get_aname(globals, aptr))) == NULL) {
		    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to change value");
		    return errmsg;
		}
		return set_attribute(globals, aptr, ATTRIB_DEFAULT, value, 1);
	    }
	}
    }
    pmsprintf(errmsg, sizeof(errmsg), "variable \"%s\" is undefined for rule %s",
		    param, rule->self.name);
    return errmsg;
}

static char *
append_string(char *s, char *append, int len)
{
    size_t	size = (strlen(s) + len + 1);

#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - \"%s\" + (%d)\"%s\" = (%d chars)\n",
		s, len, append, size);
#endif
    if ((s = (char *)realloc(s, size)) == NULL)
	return NULL;
    strncat(s, append, len);
    s[size-1] = '\0';
    return s;
}

/* fix up value strings by doing variable expansion */
char *
dollar_expand(rule_t *rule, char *string, int pp)
{
    atom_t	*aptr;
    char	*tmp, *r;
    char	*sptr;
    char	*s;
    char	*mark = NULL;
    char	localbuf[TOKEN_LENGTH];

#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - dollar_expand %s in %s\n", string, rule->self.name);
#endif
    if ((s = (char *)malloc(sizeof(char))) == NULL)
	return NULL;
    *s = '\0';

    for (sptr = string; *sptr != '\0'; sptr++) {
	if (*sptr == '\\' && *(sptr+1) == '$') {	/* skip escaped $ */
	    if ((s = append_string(s, sptr+1, 1)) == NULL)
		return NULL;
	    sptr++;	/* move passed the escaped char */
	    continue;
	}
	if (*sptr == '$') {
	    if (mark == NULL)	/* start of an expansion section */
		mark = sptr+1;
	    else {		/* end of an expansion section */
		/* look through atom list & if not there search globally */
		strncpy(localbuf, mark, sptr - mark);
		localbuf[sptr - mark] = '\0';
		mark = NULL;
#ifdef PMIECONF_DEBUG
		fprintf(stderr, "debug - expand localbuf: %s\n", localbuf);
#endif
		if ((tmp = get_attribute(localbuf, &rule->self)) == NULL) {
		    for (aptr = &rule->self; tmp == NULL && aptr != NULL; aptr = aptr->next)
			if (strcmp(get_aname(rule, aptr), localbuf) == 0)
			    tmp = value_string(aptr, pp);
		    for (aptr = globals->self.next; tmp == NULL && aptr != NULL; aptr = aptr->next)
			if (strcmp(get_aname(globals, aptr), localbuf) == 0)
			    tmp = value_string(aptr, pp);
#ifdef PMIECONF_DEBUG
		    fprintf(stderr, "debug - expanded localbuf? %s\n", tmp);
#endif
		    if (tmp == NULL) {
			pmsprintf(errmsg, sizeof(errmsg), "variable \"$%s$\" in %s is undefined",
				localbuf, rule->self.name);
			free(s);
			return NULL;
		    }
		}
		if (tmp != NULL) {
		    if ((r = dollar_expand(rule, tmp, pp)) == NULL) {
			free(s);
			return NULL;
		    }
		    if ((s = append_string(s, r, strlen(r))) == NULL) {
			free(r);
			return NULL;
		    }
		    free(r);
		}
	    }
	}
	else if (mark == NULL) {	/* need memory to hold this character */
	    if ((s = append_string(s, sptr, 1)) == NULL)
		return NULL;
	}
    }
    if (mark != NULL)	/* no terminating '$' */
	s = append_string(s, mark, strlen(mark));
#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - expanded '%s' to '%s'\n", string, s);
#endif
    return s;
}


/*
 *   ####  main parsing routines  ###
 */

/* need a SIGWINCH-aware read routine for interactive pmieconf */
static int
mygetc(FILE *f)
{
    int	c;

    for (;;) {
	setoserror(0);
	c = getc(f);
	/* did we get told to resize the window during read? */
	if (c == -1 && oserror() == EINTR && resized() == 1)
	    continue;	/* signal handled, try reading again */
	break;
    }
    return c;
}

/*
 * skip leading white space and comments, return first character in next token
 * or zero on end of file
 */
static int
prime_next_pread(FILE *f, int end)
{
    int	c;

    do {
	c = mygetc(f);
	if (c == '#')
	    do {
		c = mygetc(f);
	    } while (c != '\n' && c != end && c != EOF);
	if (c == end)
	    return 0;
	else if (c == EOF)
	    return -2;
	if (c == '\n' && end != '\n')
	    linenum++;
    } while (!isgraph(c));
    return c;
}

/*
 * read next input token; returns 1 ok, 0 end, -1 error, -2 EOF (if end!=EOF)
 * nb: `end' can be either EOL or EOF, depending on use of this routine
 */
int
read_token(FILE *f, char *token, int token_length, int end)
{
    int	c;
    int	n = 0;

    switch (c = prime_next_pread(f, end)) {
    case 0:	/* end */
    case -2:	/* EOF */
	return c;
    case '"':			/* scan string */
	c = mygetc(f);
	while (c != '"') {
	    if (c == '\\')
		c = mygetc(f);
	    if (c == end || c == EOF || n == token_length) {
		token[n] = '\0';
		parse_error("end-of-string", token);
		return -1;
	    }
	    if (c == '\n' && end != '\n')
		linenum++;
	    token[n++] = c;
	    c = mygetc(f);
	}
	break;
    case ';':
    case '=':
	token[n++] = c;			/* single char token */
	break;
    default:				/* some other token */
	while (isgraph(c)) {
	    if (c == '=' || c == ';') {
		ungetc(c, f);
		break;
	    }
	    if (n == token_length) {
		token[n] = '\0';
		parse_error("end-of-token", token);
		return -1;
	    }
	    token[n++] = c;
	    c = mygetc(f);
	    if (c == end || c == EOF)
		ungetc(c, f);
	}
	if (c == '\n' && end != '\n')
	    linenum++;
	break;
    }

    token[n] = '\0';
    return 1;
}

/*
 * get attribute list part of an atom; returns -1 on error, 0 on reaching
 * the end of the attribute list, and 1 at end of each attribute.
 */
static int
read_next_attribute(FILE *f, char **attr, char **value)
{
    int		sts;

    if ((sts = read_token(f, token, TOKEN_LENGTH, EOF)) <= 0) {
	if (sts == 0)
	    parse_error("attribute or ';'", "end-of-file");
	return -1;
    }
    if (token[0] == ';')
	return 0;
    if (map_symbol(attribs, numattribs, token) < 0) {
	parse_error("attribute keyword", token);
	return -1;
    }
    if ((*attr = alloc_string(strlen(token)+1)) == NULL)
	return -1;
    strcpy(*attr, token);
    if ((sts = read_token(f, token, TOKEN_LENGTH, EOF)) <= 0
		|| token[0] != '=') {
	if (sts == 0)
	    parse_error("=", "end-of-file");
	else
	    parse_error("=", token);
	free(*attr);
	return -1;
    }
    if ((sts = read_token(f, token, TOKEN_LENGTH, EOF)) <= 0) {
	if (sts == 0)
	    parse_error("attribute value", "end-of-file");
	else
	    parse_error("attribute value", token);
	free(*attr);
	return -1;
    }
    if ((*value = alloc_string(strlen(token)+1)) == NULL) {
	free(*attr);
	return -1;
    }
    strcpy(*value, token);
    return 1;
}


/* parse an atom, return NULL/failure message */
static char *
read_atom(FILE *f, rule_t *r, char *name, int type, int global)
{
    int		sts;
    int		attrib;
    char	*attr;
    char	*value;
    atom_t	atom;

    memset(&atom, 0, sizeof(atom_t));
    atom.name = name;
    atom.type = type;
    atom.enabled = atom.display = atom.modify = 1;	/* defaults */
    for (;;) {
	if ((sts = read_next_attribute(f, &attr, &value)) < 0)
	    return errmsg;
	else if (sts == 0) {	/* end of parameter list */
	    if (alloc_atom(r, atom, global) == NULL)
		return errmsg;
	    break;
	}
	else {
	    if ((attrib = map_symbol(attribs, numattribs, attr)) < 0) {
		parse_error("attribute keyword", attr);
		goto fail;
	    }
	    if (set_attribute(r, &atom, attrib, value, 0) != NULL)
		goto fail;
	    free(attr);
	    free(value);
	}
    }
    return NULL;

fail:
    free(attr);
    free(value);
    return errmsg;
}


/* parse type-identifier pair, return NULL/failure message */
static char *
read_type(FILE *f, rule_t *r, int *type, int *global, char **name)
{
    int	sts;

    /* read type - rule, percent, double, unsigned, string... */
    if ((sts = read_token(f, token, TOKEN_LENGTH, EOF)) < 0)
	return errmsg;
    else if (sts == 0)
	return NULL;
    if ((*type = map_symbol(types, numtypes, token)) < 0) {
	parse_error("type keyword", token);
	return errmsg;
    }

    /* read name identifying this rule/atom of type '*type' */
    if ((sts = read_token(f, token, TOKEN_LENGTH, EOF)) <= 0)
	return errmsg;
    if ((*name = alloc_string(strlen(token)+1)) == NULL)
	return errmsg;
    strcpy(*name, token);
    *global = (strncmp(*name, "global.", GLOBAL_LEN) == 0)? 1 : 0;

    /* do some simple validity checks */
    if (IS_RULE(*type) && strncmp(*name, global_name, GLOBAL_LEN-1) == 0) {
	pmsprintf(errmsg, sizeof(errmsg), "rule name may not be \"%s\" - this is reserved",
		global_name);
	free(*name);
	return errmsg;
    }
    if (r == NULL) {	/* any rule defined yet? - simple validity checks */
	if (*global && IS_RULE(*type)) {
	    pmsprintf(errmsg, sizeof(errmsg), "rules not allowed in global group: \"%s\"", *name);
	    free(*name);
	    return errmsg;
	}
	else if (!*global && !IS_RULE(*type)) {	/* not global, and no rule */
	    pmsprintf(errmsg, sizeof(errmsg), "no rule defined, cannot make sense of %s \"%s\""
			    " without one\n    line number: %u (\"%s\")\n",
			    types[*type].symbol, *name, linenum, filename);
	    free(*name);
	    return errmsg;
	}
    }
    return NULL;
}

/* set an attribute field in an atom; returns NULL/failure message */
static char *
set_rule_attribute(rule_t *rule, int attrib, char *value)
{
    char	*s;

    if (attrib == ATTRIB_PREDICATE) {
	if ((s = alloc_string(strlen(value)+1)) == NULL)
	    return errmsg;
	rule->predicate = strcpy(s, value);
	return NULL;
    }
    if (attrib == ATTRIB_ENUMERATE) {
	if ((s = alloc_string(strlen(value)+1)) == NULL)
	    return errmsg;
	rule->enumerate = strcpy(s, value);
	return NULL;
    }
    else if (attrib == ATTRIB_VERSION) {
	rule->version = strtoul(value, &s, 10);
	if (*s != '\0') {
	    parse_error("version number", "be a positive integer number");
	    return errmsg;
	}
	return NULL;
    }
    /* else */
    return set_attribute(rule, &rule->self, attrib, value, 0);
}


/* parse a single "rule" expression, return NULL/failure message */
static char *
read_rule(FILE *f, rule_t **r, char *name)
{
    int		sts;
    int		attrib;
    char	*attr;
    char	*value;
    rule_t	rule;

    memset(&rule, 0, sizeof(rule_t));
    rule.self.name = name;
    rule.self.type = TYPE_RULE;
    rule.version = rule.self.enabled = rule.self.display = 1;	/* defaults */
    for (;;) {
	if ((sts = read_next_attribute(f, &attr, &value)) < 0)
	    return errmsg;
	else if (sts == 0) {	/* end of attribute list */
	    if ((*r = alloc_rule(rule)) == NULL)
		return errmsg;
	    break;
	}
	else {
	    if ((attrib = map_symbol(attribs, numattribs, attr)) < 0) {
		parse_error("rule attribute keyword", attr);
		goto fail;
	    }
	    if (set_rule_attribute(&rule, attrib, value) != NULL)
		goto fail;
	    free(attr);
	    free(value);
	}
    }
    return NULL;

fail:
    free(attr);
    free(value);
    return errmsg;
}


/* parse rule description file; returns NULL/failure message */
static char *
read_all_rules(FILE *f)
{
    rule_t	*rule = NULL;	/* current rule */
    char	*name = NULL;
    int		type = 0;
    int		global = 0;

    /* rule files have quite a simple grammar, along these lines:
	    TYPE identifier [ ATTRIB '=' value ]* ';'
    */
    for (;;) {
	if (read_type(f, rule, &type, &global, &name) != NULL)
	    return errmsg;
	if (feof(f))	/* end of file reached without error */
	    break;
	if (type == TYPE_RULE) {
	    if (read_rule(f, &rule, name) != NULL)
		return errmsg;
	}
	else {
	    if (read_atom(f, rule, name, type, global) != NULL)
		return errmsg;
	}
    }

    return NULL;
}


/*
 * validate header of rule description file, return NULL/failure message
 */
static char *
read_pheader(FILE *f)
{
    int	c;

    c = getc(f);
    if (c != '#' || read_token(f, token, TOKEN_LENGTH, EOF) != 1 ||
		strcmp(token, RULES_FILE) != 0 ||
		read_token(f, token, TOKEN_LENGTH, EOF) != 1) {
	pmsprintf(errmsg, sizeof(errmsg), "%s is not a rule description file (bad header)\n"
			"found \"%s\", expected \"%s\"", filename,
			token, RULES_FILE);
	return errmsg;
    }
    else if (strcmp(token, RULES_VERSION) != 0) {	/* one version only */
	pmsprintf(errmsg, sizeof(errmsg), "unknown version number in %s: \"%s\" (expected %s)",
			filename, token, RULES_VERSION);
	return errmsg;
    }
    return NULL;
}


/*
 * builds up rule data structures for all rule files in given directory
 * and all its subdirectories, returns NULL/failure message
 */
char *
read_rule_subdir(char *subdir)
{
    struct stat		sbuf;
    struct dirent	*dp;
    FILE		*fp;
    DIR			*dirp;
    char		fullpath[MAXPATHLEN+1];

    if (stat(subdir, &sbuf) < 0) {
	pmsprintf(errmsg, sizeof(errmsg), "cannot stat %s: %s",
		subdir, osstrerror());
	return errmsg;
    }
    if (!S_ISDIR(sbuf.st_mode)) {
	if ((fp = fopen(subdir, "r")) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "cannot open %s: %s",
		    subdir, osstrerror());
	    return errmsg;
	}
	linenum = 1;
	filename = subdir;
	if (read_pheader(fp) == NULL) {
	    if (read_all_rules(fp) != NULL) {
		fclose(fp);
		return errmsg;
	    }
	}
#ifdef PMIECONF_DEBUG
	else {
	    fprintf(stderr, "debug - %s isn't a pmie rule file: %s\n",
			    filename, errmsg);
	}
#endif
	fclose(fp);
    }
    else {
	/* iterate through the rules directory and for each subdirectory  */
	/* fetch all the rules along with associated parameters & values  */

	if ((dirp = opendir(subdir)) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "cannot opendir %s: %s", subdir, osstrerror());
	    return errmsg;
	}
	while ((dp = readdir(dirp)) != NULL) {	  /* groups */
	    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
		continue;
	    pmsprintf(fullpath, sizeof(fullpath), "%s%c%s", subdir, SEP, dp->d_name);
	    if (read_rule_subdir(fullpath) != NULL) {	/* recurse */
		closedir(dirp);
		return errmsg;
	    }
	}
	closedir(dirp);
    }
    return NULL;
}


/*  #####  pmiefile parsing routines  ####  */


/* returns NULL on successfully adding rule to list, else failure message */
char *
deprecate_rule(char *name, unsigned int version, int type)
{
    int	index;

    /* first check to see if this rule is deprecated already */
    for (index = 0; index < dcount; index++) {
	if (strcmp(dlist[index].name, name) == 0
		&& dlist[index].version == version)
	    return NULL;
    }

    /* get the memory we need & then keep a copy of deprecated rule info */
    if ((dlist = (dep_t *)realloc(dlist, (dcount+1)*sizeof(dep_t))) == NULL
		|| (dlist[dcount].name = strdup(name)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to deprecate rule %s", name);
	return errmsg;
    }
    dlist[dcount].type = type;
    dlist[dcount].version = version;
    if (type == DEPRECATE_NORULE)
	dlist[dcount].reason = drulestring;
    else
	dlist[dcount].reason = dverstring;
    dcount++;
    return NULL;
}

/*
 * makes list of deprecated rules available to caller (for warning message)
 * nb: called following a pmiefile write to see what was deprecated during
 *     that write - subsequent writing of this pmiefile should not deprecate
 *     anything (deprecations done once & not again). caller must free list.
 */
int
fetch_deprecated(dep_t **list)
{
    int	sts;

    *list = dlist;
    sts = dcount;
    dcount = 0;
    return sts;
}

/* merges local customisations back into the rules atom list */
static char *
merge_local(unsigned int version, char *name, char *attrib, char *value)
{
    atom_t	*aptr;
    rule_t	*rule;
    int		a;

    /*
	first find the rule to which this local belongs, then figure
	out what sort of attribute this really is, and finally merge
	the customisation back into the values in the rules attribs.
    */

    if (find_rule(name, &rule) != NULL)		/* in pmiefile but not rules */
	return NULL;	/* this will be deprecated later */
    else if (rule->version != version)		/* no rule for this version */
	return NULL;	/* this will be deprecated later */

    if ((a = is_attribute(attrib)) != -1)
	return set_attribute(rule, &rule->self, a, value, 1);
    else {	/* search through this rules list of atoms */
	for (aptr = rule->self.next; aptr != NULL; aptr = aptr->next) {
	    if (strcmp(get_aname(rule, aptr), attrib) == 0)
		return set_attribute(rule, aptr, ATTRIB_DEFAULT, value, 1);
	}
	for (aptr = globals->self.next; aptr != NULL; aptr = aptr->next) {
	    if (strcmp(get_aname(globals, aptr), attrib) == 0) {
		/* promote global to become a local */
		if ((aptr = alloc_atom(rule, *aptr, 0)) == NULL)
		    return errmsg;
		if ((aptr->name = strdup(get_aname(globals, aptr))) == NULL) {
		    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to change value");
		    return errmsg;
		}
		return set_attribute(globals, aptr, ATTRIB_DEFAULT, value, 1);
	    }
	}
    }
    pmsprintf(errmsg, sizeof(errmsg), "variable \"%s\" is undefined for rule %s",
		    attrib, name);
    return errmsg;
}


/*
 *   ####  produce pmie configuration file from rules  ###
 */

char *
action_string(int type)
{
    switch (type) {
	case TYPE_PRINT:	return "print";
	case TYPE_SHELL:	return "shell";
	case TYPE_ALARM:	return "alarm";
	case TYPE_SYSLOG:	return "syslog";
    }
    return NULL;
}

static int
expand_action(FILE *f, int count, rule_t *rule, atom_t *atom)
{
    char	*p;
    char	*str;

    if (IS_ACTION(atom->type) && atom->enabled) {
	if ((str = dollar_expand(rule, " $holdoff$ ", 0)) == NULL)
	    return count;
	if (count == 0)
	    fprintf(f, " -> ");
	else
	    fprintf(f, "\n  & ");
	fprintf(f, "%s", action_string(atom->type));
	fprintf(f, "%s", str);
	free(str);
	if ((str = dollar_expand(rule, atom->data, 0)) == NULL) {
	    fprintf(stderr, "Warning - failed to expand action for rule %s\n"
			    "  string: \"%s\"\n", rule->self.name, atom->data);
	    /* keep going - too late to bail out without file corruption */
	}
#ifdef PMIECONF_DEBUG
	else {
	    fprintf(stderr, "expanded action= \"%s\"\n", str);
	}
#endif
	fputc('"', f);
	for (p = str; p != NULL && *p; p++) {	/* expand the '^' character */
	    if (*p == '/' && *(p+1) == '^') {
		fputc(*p, f); p++;
		fputc(*p, f); p++;
	    }
	    else if (*p == '^')
		fputs("\" \"", f);
	    else fputc(*p, f);
	}
	fputc('"', f);
	if (str != NULL)
	    free(str);
	return count + 1;
    }
    return count;
}


/*
 * this struct and the enumerate function are used only in generate_rules()
 * and the enumerate() routines, for enumeration of either a hostlist or an
 * instlist
 */

typedef struct {
    atom_t	*atom;
    int		nvalues;
    char	**valuelist;
    char	*restore;
} enumlist_t;

static enumlist_t	*list;
static int		nlistitems;
static int		writecount;

/*
 * expands and writes out a single rule, and optionally the delta
 * note: in the single rule case (not enumerated), we absolutely
 * must write out the delta every time
 */
static char *
write_rule(FILE *f, rule_t *rule)
{
    atom_t	*aptr;
    char	*dgen = NULL;	/* holds generated "delta" */
    char	*pgen;		/* holds generated "predicate" */
    int		actions = 0;

#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - writing rule %s\n", rule->self.name);
#endif

    if (writecount == 0 && (dgen = dollar_expand(rule, "$delta$", 0)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "\"$delta$\" variable expansion failed for rule %s",
		rule->self.name);
	return errmsg;
    }
    if ((pgen = dollar_expand(rule, rule->predicate, 0)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "\"$predicate$\" variable expansion failed "
		"for rule %s", rule->self.name);
	return errmsg;
    }
    if (writecount == 0) {
	fprintf(f, "// %u %s\ndelta = %s;\n%s = \n", rule->version,
		rule->self.name, dgen, rule->self.name);
	free(dgen);
    }
    else	/* we're enumerating, need to differentiate rule names */
	fprintf(f, "%s%u = \n", rule->self.name, writecount);
    fputs(pgen, f);
    free(pgen);
    for (aptr = rule->self.next; aptr != NULL; aptr = aptr->next)
	actions = expand_action(f, actions, rule, aptr);
    for (aptr = globals->self.next; aptr != NULL; aptr = aptr->next)
	actions = expand_action(f, actions, rule, aptr);
    fprintf(f, ";\n\n");

    writecount++;
    return NULL;
}

/* parses the "enumerate" value string passed in thru the rules file */
char *
parse_enumerate(rule_t *rule)
{
    atom_t	*ap;
    char	*p = rule->enumerate;
    int		needsave = 0;	/* should we save this variable name yet? */
    int		i = 0;

#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - parse_enumerate called for %s\n", rule->self.name);
#endif

    nlistitems = 0;
    list = NULL;
    while (*p != '\0') {
	if (!isspace((int)*p)) {
	    needsave = 1;
	    token[i++] = *p;
	}
	p++;
	if ((isspace((int)*p) && needsave) || *p == '\0') {
	    token[i] = '\0';
	    i = 0;
	    if (map_symbol(attribs, numattribs, token) != -1) {
		pmsprintf(errmsg, sizeof(errmsg), "cannot enumerate rule %s using attribute"
			" \"%s\"", rule->self.name, token);
		return errmsg;
	    }
	    else {
		for (ap = rule->self.next; ap != NULL; ap = ap->next)
		    if (strcmp(get_aname(rule, ap), token) == 0)
			goto foundname;
		for (ap = globals->self.next; ap != NULL; ap = ap->next)
		    if (strcmp(get_aname(globals, ap), token) == 0)
			goto foundname;
		pmsprintf(errmsg, sizeof(errmsg), "variable \"%s\" undefined for enumerated"
				" rule %s", token, rule->self.name);
		return errmsg;
	    }
foundname:
	    if (ap->type != TYPE_HOSTLIST && ap->type != TYPE_INSTLIST) {
		pmsprintf(errmsg, sizeof(errmsg), "rules file error - \"$%s$\" in \"enumerate\" "
			"clause of rule %s is not of type hostlist or instlist",
			token, rule->self.name);
		return errmsg;
	    }
	    /* increase size of list & keep a copy of the variable name */
	    if ((list = realloc(list, (nlistitems+1)*sizeof(enumlist_t))) == NULL) {
		pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to write rules");
		return errmsg;
	    }
	    list[nlistitems].atom = ap;
	    list[nlistitems].nvalues = 0;
	    list[nlistitems].valuelist = NULL;
	    list[nlistitems].restore = NULL;
#ifdef PMIECONF_DEBUG
	    fprintf(stderr, "debug - variable %s added to enum list (#%d)\n",
			    list[nlistitems].atom->name, nlistitems);
#endif
	    nlistitems++;
	    needsave = 0;
	}
    }
    return NULL;
}

/*
 * converts a host/inst list into individual elements, overwrites liststr
 * (turns all spaces to NULLs to mark string ends - reduces mallocing)
 */
char **
get_listitems(char *liststr, int *count)
{
    char	**result = NULL;
    char	*p = liststr;
    int		keepwhite = 0;
    int		startagain = 0;	/* set to signify new list item has started */
    int		ptrcount = 0;

    if ((result = realloc(result, (ptrcount+1) * sizeof(char *))) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to get list elements");
	return NULL;
    }
    result[ptrcount++] = p;
    while (*p != '\0') {
	if (!isspace((int)*p) || keepwhite) {
	    if (startagain) {
		result = realloc(result, (ptrcount+1) * sizeof(char *));
		if (result == NULL) {
		    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to get list elements");
		    return NULL;
		}
		result[ptrcount++] = p;
		startagain = 0;
	    }
	    if (*p == '\\')
		p++;
	    else if (*p == '\'')
		keepwhite = !keepwhite;
	}
	else {
	    *p = '\0';
	    startagain = 1;
	}
	p++;
    }
#ifdef PMIECONF_DEBUG
    fputs("debug - instances are:", stderr);
    for (keepwhite = 0; keepwhite < ptrcount; keepwhite++)
	fprintf(stderr, " %s", result[keepwhite]);
    fputs("\n", stderr);
#endif
    *count = ptrcount;
    return result;
}

/* expands variables from the "enumerate" string in the rules file */
char *
expand_enumerate(rule_t *rule)
{
    int		i, j;
    char	*p;

#ifdef PMIECONF_DEBUG
    fprintf(stderr, "debug - expanding enum variables for rule %s\n",
	    rule->self.name);
#endif

    for (i = 0; i < nlistitems; i++) {
	if ((p = dollar_expand(rule, list[i].atom->data, 0)) == NULL)
	    return errmsg;
	if ((list[i].valuelist = realloc(list[i].valuelist,
		sizeof(char *) * (list[i].nvalues + 1))) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for rule enumeration");
	    free(p);
	    return errmsg;
	}
	if ((list[i].valuelist = get_listitems(p, &j)) == NULL) {
	    free(p);
	    return errmsg;
	}
	list[i].nvalues = j;
#ifdef PMIECONF_DEBUG
	fprintf(stderr, "debug - %s value list:", list[i].atom->name);
	for (j = 0; j < list[i].nvalues; j++)
	    fprintf(stderr, " %s", list[i].valuelist[j]);
	fprintf(stderr, "\n");
#endif
    }
    return NULL;
}

void
enumerate(FILE *f, rule_t *r, enumlist_t *listoffset, int wssize, char **wset)
{
    int	i;

    if (wssize < nlistitems) {
	for (i = 0; i < listoffset->nvalues; i++) {
	    /* add current word to word set, and move down a level */
	    wset[wssize] = listoffset->valuelist[i];
	    enumerate(f, r, &list[wssize+1], wssize+1, wset);
	}
    }
    else {      /* have a full set, generate rule */
#ifdef PMIECONF_DEBUG
	for (i = 0; i < wssize; i++)
	    printf("%s=%s ", list[i].atom->name, wset[i]);
	printf("\n");
#endif
	for (i = 0; i < wssize; i++) {
	    list[i].restore = list[i].atom->data;
	    list[i].atom->data = wset[i];
	}

	write_rule(f, r);

	for (i = 0; i < wssize; i++)
	    list[i].atom->data = list[i].restore;
    }
}

/* generate pmie rules for rule, returns rule string/NULL */
static char *
generate_rules(FILE *f, rule_t *rule)
{
    int	i;

    if (rule->self.enabled == 0)
	return NULL;
    if (rule->enumerate == NULL) {
	writecount = 0;
	write_rule(f, rule);
    }
    else {
	char		**workingset;	/* holds current variable values set */

#ifdef PMIECONF_DEBUG
	fprintf(stderr, "debug - generating enumerated rule %s\n",
		rule->self.name);
#endif

	/* "enumerate" attrib is a space-separated list of variables */
	/* 1.create a list of variable info structs (name->valuelist) */
	/* 2.recurse thru lists, when each set built, write out rule */

	if ((parse_enumerate(rule)) != NULL)
	    return errmsg;
	if ((expand_enumerate(rule)) != NULL)
	    return errmsg;
	if ((workingset = malloc(nlistitems * sizeof(char*))) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to generate rules");
	    return errmsg;
	}
	writecount = 0;
	enumerate(f, rule, list, 0, workingset);
	free(workingset);
	for (i = 0; i < nlistitems; i++)
	    free(list[i].valuelist);	/* alloc'd by dollar_expand */
	free(list);
    }

    return NULL;
}

/* generate local configuration changes, returns rule string/NULL */
static char *
generate_pmiefile(FILE *f, rule_t *rule)
{
    atom_t	*aptr;

    for (aptr = &rule->self; aptr != NULL; aptr = aptr->next) {
	if (aptr->changed == 0)
	    continue;
	if (IS_RULE(aptr->type)) {
	    fprintf(f, "// %u %s %s = %s\n", rule->version,
		    get_aname(rule, aptr), "enabled",
		    rule->self.enabled? "yes" : "no");
	}
	else {
	    fprintf(f, "// %u %s %s = %s\n", rule->version, rule->self.name,
		    get_aname(rule, aptr), value_string(aptr, 1));
	}
    }
    return NULL;
}

/* generate pmie rules and write to file, returns NULL/failure message */
char *
write_pmiefile(char *program, int autocreate)
{
    time_t	now = time(NULL);
    char	*p, *msg = NULL;
    char	buf[MAXPATHLEN+10];
    char	*fname = get_pmiefile();
    FILE	*fp;
    int		i;

    /* create full path to file if it doesn't exist */
    if ((p = strrchr(fname, '/')) != NULL) {
	struct stat	sbuf;

	*p = '\0';	/* p is the dirname of fname */
	if (stat(fname, &sbuf) < 0) {
	    pmsprintf(buf, sizeof(buf), "/bin/mkdir -p %s", fname);
	    if (system(buf) < 0) {
		pmsprintf(errmsg, sizeof(errmsg), "failed to create directory \"%s\"", p);
		return errmsg;
	    }
	}
	else if (!S_ISDIR(sbuf.st_mode)) {
	    pmsprintf(errmsg, sizeof(errmsg), "\"%s\" exists and is not a directory", p);
	    return errmsg;
	}
	fname[strlen(fname)] = '/';	/* stitch together */
    }

    if ((fp = fopen(fname, "w")) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "cannot write file %s: %s", fname, osstrerror());
	return errmsg;
    }
    else if (!gotpath) {
	strcpy(token, fname);
	if (realpath(token, pmiefile) == NULL) {
	    fclose(fp);
	    pmsprintf(errmsg, sizeof(errmsg), "failed to resolve %s realpath: %s", token, osstrerror());
	    return errmsg;
	}
	gotpath = 1;
    }

    fprintf(fp, "// %s %s %s\n", PMIE_FILE, PMIE_VERSION, get_rules());
    for (i = 0; i < rulecount; ++i)
	if ((msg = generate_pmiefile(fp, &rulelist[i])) != NULL)
	    goto imouttahere;
    fputs("// end\n//\n", fp);

    fprintf(fp, "%s// %sgenerated by %s on:  %s//\n\n",
	    START_STRING, autocreate ? "Auto-" : "    ", program, ctime(&now));
    for (i = 1; i < rulecount; ++i)	/* 1: start _after_ globals */
	if ((msg = generate_rules(fp, &rulelist[i])) != NULL)
	    goto imouttahere;

    /* write user-modifications area */
    fprintf(fp, END_STRING);
    /* finally any other local changes */
    if (save_area != NULL)
	fputs(save_area, fp);

imouttahere:
    fclose(fp);
    return msg;
}


/*
 *   ####  pmiefile manipulation routines ###
 */

/*
 * skip leading white space and comments, return first character in next token
 * or zero on end of file
 */
static int
prime_next_lread(FILE *f)
{
    int	c;

    do {
	c = getc(f);
	if (c == EOF)
	    return 0;
	if (c == '\n') {
	    linenum++;
	    if (getc(f) != '/') return 0;
	    if (getc(f) != '/') return 0;
	}
    } while (! isgraph(c));
    return c;
}

/* read next input token; returns 1 ok, 0 eof, -1 error */
static int
read_ltoken(FILE *f)
{
    int	    c;
    int	    n = 0;

    switch (c = prime_next_lread(f)) {
    case 0:	/* EOF */
	return 0;
    case '"':			/* scan string */
	c = getc(f);
	while (c != '"') {
	    if (c == '\\')
		c = getc(f);
	    if (c == EOF || n == TOKEN_LENGTH) {
		token[n] = '\0';
		parse_error("end-of-string", token);
		return -1;
	    }
	    if (c == '\n') {
		token[n] = '\0';
		parse_error("end-of-string", "end-of-line");
		return -1;
	    }
	    token[n++] = c;
	    c = getc(f);
	}
	break;
    case '=':
	token[n++] = c;			/* single char token */
	break;
    default:				/* some other token */
	while (isgraph(c)) {
	    if (c == '=') {
		ungetc(c, f);
		break;
	    }
	    if (n == TOKEN_LENGTH) {
		token[n] = '\0';
		parse_error("end-of-token", token);
		return -1;
	    }
	    token[n++] = c;
	    c = getc(f);
	}
	if (c == '\n') {
	    linenum++;
	    if (strncmp(token, "end", 3) == 0) break;
	    if (getc(f) != '/') return 0;
	    if (getc(f) != '/') return 0;
	}
	break;
    }

    token[n] = '\0';
    return 1;
}


/* allocates memory & appends a string to the save area */
char *
save_area_append(char *str)
{
    int		size = strlen(str);

    while ( (size+1) >= (sa_size-sa_mark) ) {
	sa_size += 256;		/* increase area by 256 bytes at a time */
	if ((save_area = (char *)realloc(save_area, sa_size)) == NULL)
	    return NULL;
    }
    if (sa_mark == 1)
	save_area = strcpy(save_area, str);
    else
	save_area = strcat(save_area, str);
    sa_mark += size;
    return save_area;
}

/* read and save text which is to be restored on pmiefile write */
static char *
read_restore(FILE *f)
{
    unsigned int	version;
    rule_t		*rule;
    char		buf[LINE_LENGTH];
    int			saverule = 0;
    int			saveall = 0;

    do {
	if (fgets(buf, LINE_LENGTH, f) == NULL)
	    break;
	if (!saveall) {	/* not yet at start of explicit "save" position */
	    if (strcmp(buf, END_STRING) == 0)
		saveall = 1;
	    else if (sscanf(buf, "// %u %s\n", &version, token) == 2) {
		/*
		 * where the rule has disappeared or its version does not match
		 * the one in the pmiefile, add the rule name & version to list
		 * of rules to be deprecated (i.e. moved to the "save area")
		*/
		/* check that we still have this rule definition */
		if (find_rule(token, &rule) != NULL) {	/* not found! */
		    pmsprintf(buf, sizeof(buf), "// %u %s (deprecated, %s)\n",
					version, token, drulestring);
		    deprecate_rule(token, version, DEPRECATE_NORULE);
		    saverule = 1;
		}
		else if (rule->version != version) {	/* not supported! */
		    pmsprintf(buf, sizeof(buf), "// %u %s (deprecated, %s)\n",
					version, token, dverstring);
		    deprecate_rule(token, version, DEPRECATE_VERSION);
		    saverule = 1;
		}
		else
		    saverule = 0;
	    }
	    if (!saveall && saverule) {
		if (save_area_append("// ") == NULL ||
			save_area_append(buf) == NULL) {
		    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to deprecate a rule");
		    return errmsg;
		}
	    }
	}
	else if (save_area_append(buf) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory to preserve save area");
	    return errmsg;
	}
    } while (!feof(f));
    return NULL;
}


/* read custom values, return NULL/failure message */
static char *
read_locals(FILE *f)
{
    int			sts;
    char		*rule;
    char		*attrib;
    char		*value;
    unsigned int	version;

    /* general pmiefile format:   version rulename attribute = value */

    for (;;) {
	if ((sts = read_ltoken(f)) < 0)
	    return errmsg;
	else if (sts == 0) {
	    parse_error("rule identifier or \"end\" symbol", "end-of-file");
	    return errmsg;
	}
	if (strcmp("end", token) == 0)
	    break;

	/* read the version number for this rule */
	version = strtoul(token, &value, 10);
	if (*value != '\0') {
	    parse_error("version number", token);
	    return errmsg;
	}

	/* read the name of the rule */
	if ((sts = read_ltoken(f)) != 1 ||
		(rule = alloc_string(strlen(token)+1)) == NULL) {
	    if (sts == 0)
		parse_error("rule name", token);
	    return errmsg;
	}
	strcpy(rule, token);

	/* read the rule attribute component */
	if ((sts = read_ltoken(f)) != 1 ||
		(attrib = alloc_string(strlen(token)+1)) == NULL) {
	    free(rule);
	    if (sts == 0)
		parse_error("rule attribute", token);
	    return errmsg;
	}
	strcpy(attrib, token);

	if ((sts = read_ltoken(f)) != 1 || strcmp("=", token) != 0) {
	    free(rule); free(attrib);
	    if (sts == 0)
		parse_error("'=' symbol", "end-of-file");
	    return errmsg;
	}

	/* read the modified value of this attribute */
	if ((sts = read_ltoken(f)) != 1 ||
		(value = alloc_string(strlen(token)+1)) == NULL) {
	    free(rule); free(attrib);
	    if (sts == 0)
		parse_error("rule attribute value", "end-of-file");
	    return errmsg;
	}
	strcpy(value, token);

	if (merge_local(version, rule, attrib, value) != NULL) {
	    free(rule); free(attrib); free(value);
	    return errmsg;
	}
	free(rule); free(attrib); free(value);	/* no longer need these */
    }
    return NULL;
}

/* validate header of rule customizations file, return NULL/failure message */
static char *
read_lheader(FILE *f, char **proot)
{
    if (read_ltoken(f) != 1 || strcmp(token, "//") || read_ltoken(f) != 1
		|| strcmp(token, PMIE_FILE) || read_ltoken(f) != 1) {
	pmsprintf(errmsg, sizeof(errmsg), "%s is not a rule customization file (bad header)",
		filename);
	return errmsg;
    }
    else if (strcmp(token, PMIE_VERSION) != 0) {	/* one version only */
	pmsprintf(errmsg, sizeof(errmsg), "unknown version number in %s: \"%s\" (expected %s)",
			filename, token, PMIE_VERSION);
	return errmsg;
    }
    else if (read_ltoken(f) != 1) {
	pmsprintf(errmsg, sizeof(errmsg), "no rules path specified in %s after version number",
		filename);
	return errmsg;
    }
    *proot = token;
    return NULL;
}


/*
 * read the pmiefile format into global data structures
 */
char *
read_pmiefile(char *warning, size_t warnlen)
{
    char	*tmp = NULL;
    char	*p, *home;
    FILE	*f;
    struct stat	sbuf;
    char	*rule_path_sep;

    if ((f = fopen(get_pmiefile(), "r")) == NULL) {
	if (oserror() == ENOENT)
	    return NULL;
	pmsprintf(errmsg, sizeof(errmsg), "cannot open %s: %s",
		get_pmiefile(), osstrerror());
	return errmsg;
    }

    linenum = 1;
    filename = get_pmiefile();
    if (fstat(fileno(f), &sbuf) == 0 && sbuf.st_size == 0) {
	/* short-circuit empty file special case */
	fclose(f);
	return NULL;
    }

    if (read_lheader(f, &tmp) != NULL) {
	fclose(f);
	return errmsg;
    }

    /* check that we have access to all components of the path */
    if ((home = strdup(tmp)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for pmie file parsing");
	return errmsg;
    }
#ifdef IS_MINGW
    rule_path_sep = ";";
#else
    rule_path_sep = ":";
#endif
    p = strtok(home, rule_path_sep);
    while (p != NULL) {
	if (access(p, F_OK) < 0) {
	    free(home);
	    pmsprintf(errmsg, sizeof(errmsg), "cannot access rules path component: \"%s\"", p);
	    return errmsg;
	}
	p = strtok(NULL, rule_path_sep);
    }
    free(home);

    if (strcmp(get_rules(), tmp) != 0)
	pmsprintf(warning, warnlen, "warning - pmie configuration file \"%s\"\n"
		" may not have been built using rules path:\n\t\"%s\"\n"
		" (originally built using \"%s\")", filename, get_rules(), tmp);

    tmp = NULL;
    if (read_locals(f) != NULL || read_restore(f) != NULL)
	tmp = errmsg;
    fclose(f);
    return tmp;
}


/*  ####  setup global data structures; return NULL/failure message  ####  */
char *
initialise(char *in_rules, char *in_pmie, char *warning, size_t warnlen)
{
    char	*p;
    char	*home;
    rule_t	global;
    char	*rule_path_sep;

    /* setup pointers to the configuration files */
#ifdef IS_MINGW
    if ((home = getenv("USERPROFILE")) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "USERPROFILE undefined in environment");
	return errmsg;
    }
    if (in_pmie == NULL)
	pmsprintf(pmiefile, sizeof(pmiefile), "%s\\%s", home, DEFAULT_USER_PMIE);
    else
	strcpy(pmiefile, in_pmie);
    rule_path_sep = ";";
#else
    if (getuid() == 0) {
	if (in_pmie == NULL)
	    pmsprintf(pmiefile, sizeof(pmiefile), "%s%c%s", pmGetConfig("PCP_SYSCONF_DIR"), SEP, DEFAULT_ROOT_PMIE);
	else if (realpath(in_pmie, pmiefile) == NULL && oserror() != ENOENT) {
	    pmsprintf(errmsg, sizeof(errmsg), "failed to resolve realpath for %s: %s",
		    in_pmie, osstrerror());
	    return errmsg;
	}
	else if (oserror() != ENOENT)
	    gotpath = 1;
    }
    else {
	if ((home = getenv("HOME")) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "$HOME undefined in environment");
	    return errmsg;
	}
	if (in_pmie == NULL)
	    pmsprintf(pmiefile, sizeof(pmiefile), "%s%c%s", home, SEP, DEFAULT_USER_PMIE);
	else
	    strcpy(pmiefile, in_pmie);
    }
    rule_path_sep = ":";
#endif

    if (in_rules == NULL) {
	if ((p = getenv("PMIECONF_PATH")) == NULL)
	    pmsprintf(rulepath, sizeof(rulepath), "%s%c%s", pmGetConfig("PCP_VAR_DIR"), SEP, DEFAULT_RULES);
	else
	    strcpy(rulepath, p);
    }
    else
	pmsprintf(rulepath, sizeof(rulepath), "%s", in_rules);

    memset(&global, 0, sizeof(rule_t));
    global.self.name = global_name;
    global.self.data = global_data;
    global.self.help = global_help;
    global.self.global = 1;
    if (alloc_rule(global) == NULL) {	/* 1st rule holds global (fake rule) */
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for global parameters");
	return errmsg;
    }

    if ((home = strdup(rulepath)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for rules path parsing");
	return errmsg;
    }
    p = strtok(home, rule_path_sep);
    while (p != NULL) {
	if (read_rule_subdir(p) != NULL) {
	    free(home);
	    return errmsg;
	}
	p = strtok(NULL, rule_path_sep);
    }
    free(home);

    if (read_pmiefile(warning, warnlen) != NULL)
	return errmsg;
    linenum = 0;	/* finished all parsing */
    return NULL;
}


/* iterate through the pmie status directory and find running pmies */
char *
lookup_processes(int *count, char ***processes)
{
    int			fd;
    int			running = 0;
    DIR			*dirp;
    void		*ptr;
    char		proc[MAXPATHLEN+1];
    char		**proc_list = NULL;
    size_t		size;
    pmiestats_t		*stats;
    struct dirent	*dp;
    struct stat		statbuf;
    int			sep = __pmPathSeparator();

    pmsprintf(proc, sizeof(proc), "%s%c%s",
	     pmGetConfig("PCP_TMP_DIR"), sep, PMIE_SUBDIR);
    if ((dirp = opendir(proc)) == NULL) {
	pmsprintf(errmsg, sizeof(errmsg), "cannot opendir %s: %s",
		 proc, osstrerror());
	return NULL;
    }
    while ((dp = readdir(dirp)) != NULL) {
	/* bunch of checks to find valid pmie data files... */
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;
	pmsprintf(proc, sizeof(proc), "%s%c%s",
		 PROC_DIR, sep, dp->d_name);	/* check /proc */
	if (access(proc, F_OK) < 0)
	    continue;	/* process has exited */
	pmsprintf(proc, sizeof(proc), "%s%c%s%c%s",
		 pmGetConfig("PCP_TMP_DIR"), sep, PMIE_SUBDIR, sep, dp->d_name);
	if (stat(proc, &statbuf) < 0)
	    continue;
	if (statbuf.st_size != sizeof(pmiestats_t))
	    continue;
	if ((fd = open(proc, O_RDONLY)) < 0)
	    continue;
	ptr = __pmMemoryMap(fd, statbuf.st_size, 0);
	close(fd);
	if (ptr == NULL)
	    continue;
	stats = (pmiestats_t *)ptr;
	if (strcmp(stats->config, get_pmiefile()) != 0)
	    continue;

	size = (1 + running) * sizeof(char *);
	if ((proc_list = (char **)realloc(proc_list, size)) == NULL
		|| (proc_list[running] = strdup(dp->d_name)) == NULL) {
	    pmsprintf(errmsg, sizeof(errmsg), "insufficient memory for process search");
	    if (proc_list) free(proc_list);
	    closedir(dirp);
	    close(fd);
	    return errmsg;
	}
	running++;
    }
    closedir(dirp);
    *count = running;
    *processes = proc_list;
    return NULL;
}
