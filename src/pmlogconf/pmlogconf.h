/*
 * Copyright (c) 2020 Red Hat.  All Rights Reserved.
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
#ifndef PMLOGCONF_H
#define PMLOGCONF_H

#include "util.h"

typedef struct group {
    char		*tag;		/* path component identifier */
    char		*ident;		/* description (ident strings) */
    char		*delta;		/* optional logging interval */
    char		*metric;	/* metric name to probe, if probing */
    unsigned int	valid: 1;	/* was group successfully parsed? */
    unsigned int	success: 1;	/* was group successfully probed? */
    unsigned int	logging: 1;	/* log group mandatory or advisory */
    unsigned int	pmlogger: 1;	/* was group in pmlogger config? */
    unsigned int	pmlogconf: 1;	/* was group in pmlogconf configs? */
    unsigned int	pmrep: 1;	/* was group in pmrep config? */
    pmID		pmid;		/* identifier for probed metric */
    char		*force;		/* force expression, if forcing */
    char		*probe;		/* probe expression, if probing */
    char		*value;		/* probe expression condition value */
    unsigned int	probe_style;	/* parse state */
    unsigned int	true_state;	/* parse state */
    unsigned int	false_state;	/* parse state */
    unsigned int	nmetrics;	/* number of elements in metric array */
    char		**metrics;	/* array of metrics for this group */
    char		*saved_delta;	/* previously configured log interval */
    unsigned int	saved_state;	/* previously configured enable state */
    unsigned int	probe_state;	/* result of evaluating probe */
} group_t;

extern unsigned	ngroups;
extern group_t	*groups;

extern int	autocreate;
extern int	prompt;
extern int	deltas;
extern int	quick;
extern int	reprobe;
extern int	rewrite;
extern int	verbose;
extern int	existing;
extern char	groupdir[];
extern char	*tmpconfig;
extern char	*trailer;
extern char	*header;
extern char	*config;
extern char	*host;
extern char	*setupfile;
extern char	*finaltag;

extern int pmlogconf(int argc, char **argv);
extern int pmrepconf(int argc, char **argv);
extern void pmapi_setup(pmOptions *opts);
extern void group_dircheck(const char *path);
extern int resize_groups(unsigned int extras);
extern int parse_groupfile(FILE *file, const char *tag);
extern int parse_group(group_t *group);
extern int parse_groups(const char *path, const char *subdir);
extern int evaluate_group(group_t *group);
extern void group_setup(void);
extern void setup_groups(void);
extern int fetch_groups(void);
extern char *update_groups(FILE *tempfile, const char *pattern);
extern unsigned int evaluate_state(group_t *group);
extern group_t *group_free(group_t *group);
extern group_t *group_finish(group_t *group, unsigned int *hint);
extern void group_ident(group_t *group, const char *bytes);
extern void diff_tempfile(FILE *tempfile);
extern void create_tempfile(FILE *file, FILE **tempfile, struct stat *stat);

#endif
