/*
 * rules.h - rule description data structures and parsing
 * 
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
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

#ifndef RULES_H
#define RULES_H

/* defaults relative to $PCP_VAR_DIR */
#ifdef IS_MINGW
#define DEFAULT_RULES          "config\\pmieconf"
#define DEFAULT_ROOT_PMIE      "pmie\\config.default"
#else 
#define DEFAULT_RULES		"config/pmieconf"
#define DEFAULT_ROOT_PMIE	"pmie/config.default"
#endif

/* default relative to $HOME */
#ifdef IS_MINGW
#define DEFAULT_USER_PMIE      ".pcp\\pmie\\config.pmie"
#else
#define DEFAULT_USER_PMIE	".pcp/pmie/config.pmie"
#endif

#define TYPE_STRING		0	/* arbitrary string (default) */
#define TYPE_DOUBLE		1	/* real number */
#define TYPE_INTEGER		2	/* integer number */
#define TYPE_UNSIGNED		3	/* cardinal number */
#define TYPE_PERCENT		4	/* percentage 0..100 */
#define TYPE_HOSTLIST		5	/* list of host names */
#define TYPE_INSTLIST		6	/* list of metric instances */
#define TYPE_PRINT		7	/* print action */
#define TYPE_SHELL		8	/* shell action */
#define TYPE_ALARM		9	/* alarm window action */
#define TYPE_SYSLOG		10	/* syslog action */
#define TYPE_RULE		11	/* rule definition */

#define ATTRIB_HELP		0	/* help= text */
#define ATTRIB_MODIFY		1	/* modify= text */
#define ATTRIB_ENABLED		2	/* enabled= y/n */
#define ATTRIB_DISPLAY		3	/* display= y/n */
#define ATTRIB_DEFAULT		4	/* default= value */
#define ATTRIB_VERSION		5	/* version= int */
#define ATTRIB_PREDICATE	6	/* predicate= text */
#define ATTRIB_ENUMERATE	7	/* enumerate= text */

#define IS_ACTION(t)	\
	(t==TYPE_PRINT || t==TYPE_SHELL || t==TYPE_ALARM || t==TYPE_SYSLOG)
#define IS_RULE(t)	(t == TYPE_RULE)

/* generic data block definition */
struct atom_type {
    char		*name;		/* atom identifier */
    char		*data;		/* value string */
    char		*ddata;		/* default value */
    char		*help;		/* help text */
    unsigned int	type    : 16;	/* data type */
    unsigned int	modify  :  1;	/* advice for editor */	
    unsigned int	display :  1;	/* advice for editor */
    unsigned int	enabled :  1;	/* switched on/off */
    unsigned int	denabled:  1;	/* default on/off */
    unsigned int	changed :  1;	/* has value been changed? */
    unsigned int	global  :  1;	/* global scope */
    unsigned int	padding : 10;	/* unused */
    struct atom_type	*next;
};
typedef struct atom_type atom_t;

typedef struct {
    unsigned int	version;
    atom_t		self;
    char		*predicate;
    char		*enumerate;	/* list of hostlist/instlist params */
} rule_t;


extern rule_t		*rulelist;
extern unsigned int	rulecount;
extern rule_t		*globals;

extern char		errmsg[];	/* error message buffer */


/*
 * routines below returning char*, on success return NULL else failure message
 */

char *initialise(char *, char *, char *, size_t);    /* setup global data */

char *get_pmiefile(void);
char *get_rules(void);
char *get_aname(rule_t *, atom_t *);

void sort_rules(void);
char *find_rule(char *, rule_t **);
char *lookup_rules(char *, rule_t ***, unsigned int *, int);

char *value_string(atom_t *, int);   /* printable string form of atoms value */
char *value_change(rule_t *, char *, char *); /* change rule parameter value */
char *validate(int, char *, char *);  /* check proposed value for named type */

char *write_pmiefile(char *, int);
char *lookup_processes(int *, char ***);

int is_attribute(char *);
char *get_attribute(char *, atom_t *);
char *rule_defaults(rule_t *, char *);

int is_overridden(rule_t *, atom_t *);
int read_token(FILE *, char *, int, int);
char *dollar_expand(rule_t *, char *, int);


/* deprecated rules stuff */
#define DEPRECATE_NORULE	0
#define DEPRECATE_VERSION	1

typedef struct {
    unsigned int	version;	/* version not matching/in rules */
    char		*name;		/* full name of the offending rule */
    char		*reason;	/* ptr to deprecation description */
    int			type;		/* reason for deprecating this rule */
} dep_t;
int fetch_deprecated(dep_t **list);


/* generic symbol table definition */
typedef struct {
    int		symbol_id;
    char	*symbol;
} symbol_t;

/* lookup keyword, returns symbol identifier or -1 if not there */
int map_symbol(symbol_t *, int, char *);

/* lookup symbol identifier, returns keyword or NULL if not there */
char *map_identifier(symbol_t *, int, int);

/* parse yes/no attribute value; returns 0 no, 1 yes, -1 error */
int map_boolean(char *);

#endif
