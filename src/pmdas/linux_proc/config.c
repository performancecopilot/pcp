/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/stat.h>

#define _REGEX_RE_COMP
#include <sys/types.h>
#include <regex.h>

#include "pmapi.h"
#include "impl.h"

#include "gram_node.h"
#include "gram.tab.h"
#include "config.h"

char *conf_buffer;	/* contains config text */
char *pred_buffer;	/* contains parsed predicate */

static bool_node *the_tree;
static config_vars *the_vars;

/* internal functions */
static int eval_predicate(bool_node *);
static int eval_comparison(bool_node *);
static int eval_num_comp(N_tag, bool_node *, bool_node *);
static int eval_str_comp(N_tag, bool_node *, bool_node *);
static int eval_match_comp(N_tag, bool_node *, bool_node *);
static char* get_strvalue(bool_node *);
static double get_numvalue(bool_node *);
static void eval_error(char *);

extern int parse_predicate(bool_node **);
char *hotproc_configfile;
extern FILE *yyin;

void
set_conf_buffer(char *buf)
{
    if(conf_buffer != NULL)
        free(conf_buffer);
    conf_buffer = strdup(buf);
}

char *
get_conf_buffer(void)
{
    return pred_buffer;
}

FILE *
open_config(char configfile[])
{
    FILE *conf;
    int fd;
    struct stat sb;

    hotproc_configfile = strdup(configfile);

    if ((conf = fopen(hotproc_configfile, "r")) == NULL) {
	if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "%s: Cannot open configuration file \"%s\": %s\n",
		    pmProgname, hotproc_configfile, osstrerror());
	}
	return NULL;
    }

    fd = fileno(conf);
    if( fstat(fd, &sb) == -1 ) {
	fclose(conf);
        return NULL;
    }

    if( sb.st_mode & S_IWOTH){
        fprintf(stderr, "Hotproc config file : %s has global write permission, ignoring\n",
            hotproc_configfile);
	fclose(conf);
        return NULL;
    }

    return conf;
}

int
parse_config(bool_node **tree)
{
    /* Return 1 on success, 0 on empty config, sts on error (negative)*/
    int sts;
    FILE *file = NULL;
    char tmpname[] = "/var/tmp/pcp.XXXXXX";
    mode_t cur_umask;
    int fid = -1;
    struct stat stat_buf;
    long size;
    char *ptr = NULL;

    if ((sts = parse_predicate(tree)) != 0) {
	fprintf(stderr, "%s: Failed to parse configuration file\n", pmProgname);
	return -sts;
    }

    if( *tree == NULL ){
        /* Parsed as empty config, so we should disable */
        if (pred_buffer != NULL)
            free(pred_buffer);
        pred_buffer = NULL;
        return 0;
    }

    /* --- dump to tmp file & read to buffer --- */
    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    fid = mkstemp(tmpname);
    umask(cur_umask);
    if (fid == -1 ||
	(file = fdopen(fid, "w+")) == NULL) {
	sts = -oserror();
	fprintf(stderr, "%s: parse_config: failed to create \"%s\": %s\n",
	    pmProgname, tmpname, strerror(-sts));
	goto error;
    }
    if (unlink(tmpname) == -1) {
	sts = -oserror();
	fprintf(stderr, "%s: parse_config: failed to unlink \"%s\": %s\n",
	    pmProgname, tmpname, strerror(-sts));
	goto error;
    }
    dump_predicate(file, *tree);
    fflush(file);
    if (fstat(fileno(file), &stat_buf) < 0) {
	sts = -oserror();
	fprintf(stderr, "%s: parse_config: failed to stat \"%s\": %s\n",
	    pmProgname, tmpname, strerror(-sts));
	goto error;
    }
    size = (long)stat_buf.st_size;
    ptr = malloc(size+1);
    if (ptr == NULL) {
	sts = -oserror();
	fprintf(stderr, "%s: parse_config: failed to malloc: %s\n",
	    pmProgname, strerror(-sts));
	goto error;
    }
    rewind(file);
    if (fread(ptr, size, 1, file) != 1) {
	clearerr(file);
	fprintf(stderr, "%s: parse_config: failed to fread \"%s\"\n",
	    pmProgname, tmpname);
	sts = -1;
	goto error;
    }
    (void)fclose(file);

    if (pred_buffer != NULL)
	free(pred_buffer);
    pred_buffer = ptr; 
    pred_buffer[size] = '\0';
    return 1;

error:
    if (ptr)
	free(ptr);
    if (file)
	(void)fclose(file);
    return sts;
}

void
new_tree(bool_node *tree)
{
    /* free_tree will delete the tree we just constructed if NULL is passed in.  Not sure why */
    if (the_tree != NULL )
	free_tree(the_tree);

    the_tree = tree;
}

int
read_config(FILE *conf)
{
    struct stat stat_buf;
    long size;
    int sts;
    size_t nread;

    /* get length of file */
    sts = fstat(fileno(conf), &stat_buf);
    if (sts < 0) {
	fprintf(stderr, "%s: Failure to stat configuration file \"%s\": %s\n",
	    pmProgname, hotproc_configfile, osstrerror());
	return 0;
    }
    size = (long)stat_buf.st_size;

    /* create buffer */
    conf_buffer = (char*)malloc(size+1*sizeof(char));
    if (conf_buffer == NULL) {
	fprintf(stderr, "%s: Cannot create buffer configuration file \"%s\"\n",
	    pmProgname, hotproc_configfile);
	return 0;
    }

    /* read whole file into buffer */
    nread = fread(conf_buffer, sizeof(char), size, conf);
    if (nread != size) {
	fprintf(stderr, "%s: Failure to fread \"%s\" file into buffer\n",
	    pmProgname, hotproc_configfile);
	return 0;
    }
    conf_buffer[size] = '\0'; /* terminate the buffer */

    return parse_config(&the_tree);
}

void
dump_tree(FILE *f)
{
    dump_bool_tree(f, the_tree);
}

int
eval_tree(config_vars *vars)
{
    the_vars = vars;
    return eval_predicate(the_tree);
}

static void 
eval_error(char *msg)
{
   fprintf(stderr, "%s: Internal error : %s\n", pmProgname, msg?msg:""); 
   exit(1);
}

static int
eval_predicate(bool_node *pred)
{
    bool_node *lhs, *rhs;

    switch (pred->tag) {
	case N_and:	
	    lhs = pred->data.children.left;
	    rhs = pred->data.children.right;
	    return eval_predicate(lhs) && eval_predicate(rhs);	
	case N_or:	
	    lhs = pred->data.children.left;
	    rhs = pred->data.children.right;
	    return eval_predicate(lhs) || eval_predicate(rhs);	
	case N_not:	
	    lhs = pred->data.children.left;
	    return !eval_predicate(lhs);	
	case N_true:
	    return 1;
	case N_false:
	    return 0;
	default:
	    return eval_comparison(pred);
    }
}

static int
eval_comparison(bool_node *comp)
{
    bool_node *lhs = comp->data.children.left;
    bool_node *rhs = comp->data.children.right;

    switch (comp->tag) {
	case N_lt: case N_gt: case N_ge: case N_le:
        case N_eq: case N_neq:
	    return eval_num_comp(comp->tag, lhs, rhs); 
	case N_seq: case N_sneq:
	    return eval_str_comp(comp->tag, lhs, rhs);
	case N_match: case N_nmatch:
	    return eval_match_comp(comp->tag, lhs, rhs);
	default:
	    eval_error("comparison");
	    break;
    }
    return 0;
}

static int
eval_num_comp(N_tag tag, bool_node *lhs, bool_node *rhs)
{
    double x = get_numvalue(lhs);
    double y = get_numvalue(rhs);

    switch (tag) {
	case N_lt: return (x < y);
	case N_gt: return (x > y);
	case N_le: return (x <= y);
	case N_ge: return (x >= y);
	case N_eq: return (x == y);
	case N_neq: return (x != y);
	default:
	    eval_error("number comparison");
	    break;
    }
   return 0;
}

static double
get_numvalue(bool_node *n)
{
    switch(n->tag) {
	case N_number: return n->data.num_val;
	case N_cpuburn: return the_vars->cpuburn;
	/*case N_syscalls: return the_vars->preds.syscalls;*/
        case N_ctxswitch: return the_vars->preds.ctxswitch;
        case N_virtualsize: return the_vars->preds.virtualsize;
        case N_residentsize: return the_vars->preds.residentsize;
        case N_iodemand: return the_vars->preds.iodemand;
        case N_iowait: return the_vars->preds.iowait;
        case N_schedwait: return the_vars->preds.schedwait;
	case N_gid: return the_vars->gid;
	case N_uid: return the_vars->uid;
	default:
	    eval_error("number value");
	    break;
    }
    return 0;
}

static int
eval_str_comp(N_tag tag, bool_node *lhs, bool_node *rhs)
{
    char *x = get_strvalue(lhs);
    char *y = get_strvalue(rhs);

    switch (tag) {
	case N_seq: return (strcmp(x,y)==0?1:0);
	case N_sneq: return (strcmp(x,y)==0?0:1);
	default:
	    eval_error("string comparison");
	    break;
    }
    return 0;
}

static int
eval_match_comp(N_tag tag, bool_node *lhs, bool_node *rhs)
{
    int sts;
    char *res;
    char *str= get_strvalue(lhs);
    char *pat = get_strvalue(rhs);

    if (rhs->tag != N_pat) {
	eval_error("match");
    }

    res = re_comp(pat);
    if (res != NULL) {
	/* should have been checked at lex stage */
	/* => internal error */
	eval_error(res);
    }
    sts = re_exec(str);
    if (sts < 0) {
	eval_error("re_exec");
    }

    switch (tag) {
	case N_match: return sts;
	case N_nmatch: return !sts;
	default:
	    eval_error("match comparison");
	    break;
    }
    return 0;
}

static char *
get_strvalue(bool_node *n)
{
    switch (n->tag) {
	case N_str: 
	case N_pat:
		return n->data.str_val;
	case N_gname: 
		/*if (the_vars->gname != NULL)*/
		    return the_vars->gname;
		/*else
		    return get_gname_info(the_vars->gid);*/
	case N_uname: 
		/*if (the_vars->uname != NULL)*/
		    return the_vars->uname;
		/*else
		    return get_uname_info(the_vars->uid);*/
	case N_fname: return the_vars->fname; 
	case N_psargs: return the_vars->psargs; 
	default:
	    eval_error("string value");
	    break;
    }
    return 0;
}
