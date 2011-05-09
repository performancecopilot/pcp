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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "./gram_node.h"

/* functions */
static void dump_comparison(FILE *, bool_node *);
static void dump_var(FILE *, bool_node *);

static bool_node *node_list = NULL;

void start_tree(void)
{
    node_list = NULL;
}

void free_tree(bool_node *root)
{
    bool_node *n, *next;

    if (root == NULL)
	root = node_list; /* use last tree */

    /* free all nodes in list */
    for (n = root; n != NULL; ) {
	next = n->next;
	if (n->tag == N_pat || n->tag == N_str)
	    free(n->data.str_val);
	free(n);
        n = next; 
    }    

    if (root == node_list)
	node_list = NULL;
}

bool_node *
create_tag_node(N_tag tag)
{
    bool_node *new_node;

    new_node = (bool_node*)malloc(sizeof(bool_node));
    if (new_node == NULL) {
	fprintf(stderr, "hotproc: malloc failed in config: %s", osstrerror());
	exit(1);
    }
    new_node->tag = tag;

    /* add to front of node-list */
    new_node->next = node_list;
    node_list = new_node;

    return new_node;
}

bool_node *
create_tnode(N_tag tag, bool_node *lnode, bool_node *rnode)
{
    bool_node *n = create_tag_node(tag);
    n->data.children.left = lnode;
    n->data.children.right = rnode;
    return n;
}

bool_node *
create_number_node(double x)
{
    bool_node *n = create_tag_node(N_number);
    n->data.num_val = x;
    return n;
}


bool_node *create_str_node(char *str)
{
    bool_node *n = create_tag_node(N_str);
    n->data.str_val = str;
    return n;
}

bool_node *create_pat_node(char *str)
{
    bool_node *n = create_tag_node(N_pat);
    n->data.str_val = str;
    return n;
}

void
dump_bool_tree(FILE *f, bool_node *tree)
{
    (void)fprintf(f, "--- bool tree ---\n");
    dump_predicate(f, tree);
    (void)fprintf(f, "\n--- end bool tree ---\n");
}

void
dump_predicate(FILE *f, bool_node *pred)
{
    bool_node *lhs, *rhs;

    switch(pred->tag) {
	case N_and:	
	    lhs = pred->data.children.left;
	    rhs = pred->data.children.right;
	    (void)fprintf(f, "(");
	    dump_predicate(f, lhs);
	    (void)fprintf(f, " && ");
	    dump_predicate(f, rhs);
	    (void)fprintf(f, ")");
	    break;
	case N_or:	
	    lhs = pred->data.children.left;
	    rhs = pred->data.children.right;
	    (void)fprintf(f, "(");
	    dump_predicate(f, lhs);
	    (void)fprintf(f, " || ");
	    dump_predicate(f, rhs);
	    (void)fprintf(f, ")");
	    break;
	case N_not:	
	    lhs = pred->data.children.left;
	    (void)fprintf(f, "(! ");
	    dump_predicate(f, lhs);
	    (void)fprintf(f, ")");
	    break;
	case N_true:
	    (void)fprintf(f, "(true)");
	    break;
	case N_false:
	    (void)fprintf(f, "(false)");
	    break;
	default:
	    dump_comparison(f, pred);
    }/*switch*/
}

static void
dump_comparison(FILE *f, bool_node *comp)
{
    bool_node *lhs = comp->data.children.left;
    bool_node *rhs = comp->data.children.right;

    (void)fprintf(f, "(");
    dump_var(f, lhs);
    switch(comp->tag) {
	case N_lt: (void)fprintf(f, " < "); break;
	case N_gt: (void)fprintf(f, " > "); break;
	case N_le: (void)fprintf(f, " <= "); break;
	case N_ge: (void)fprintf(f, " >= "); break;
	case N_eq: (void)fprintf(f, " == "); break;
	case N_seq: (void)fprintf(f, " == "); break;
	case N_sneq: (void)fprintf(f, " != "); break;
	case N_neq: (void)fprintf(f, " != "); break;
	case N_match: (void)fprintf(f, " ~ "); break;
	case N_nmatch: (void)fprintf(f, " !~ "); break;
	default: (void)fprintf(f, "<ERROR>"); break;
    }/*switch*/
    dump_var(f, rhs);
    (void)fprintf(f, ")");
}

static void
dump_var(FILE *f, bool_node *var)
{
    switch(var->tag) {
	case N_str: (void)fprintf(f, "\"%s\"", var->data.str_val); break;
	case N_pat: (void)fprintf(f, "\"%s\"", var->data.str_val); break;
	case N_number: (void)fprintf(f, "%f", var->data.num_val); break;
	case N_uid: (void)fprintf(f, "uid"); break;
	case N_gid: (void)fprintf(f, "gid"); break;
	case N_uname: (void)fprintf(f, "uname"); break;
	case N_gname: (void)fprintf(f, "gname"); break;
	case N_fname: (void)fprintf(f, "fname"); break;
	case N_psargs: (void)fprintf(f, "psargs"); break;
	case N_cpuburn: (void)fprintf(f, "cpuburn"); break;
	case N_syscalls: (void)fprintf(f, "syscalls"); break;
	case N_ctxswitch: (void)fprintf(f, "ctxswitch"); break;
	case N_virtualsize: (void)fprintf(f, "virtualsize"); break;
	case N_residentsize: (void)fprintf(f, "residentsize"); break;
	case N_iodemand: (void)fprintf(f, "iodemand"); break;
	case N_iowait: (void)fprintf(f, "iowait"); break;
	case N_schedwait: (void)fprintf(f, "schedwait"); break;
	default: (void)fprintf(f, "<ERROR>"); break;
    }/*switch*/
}
