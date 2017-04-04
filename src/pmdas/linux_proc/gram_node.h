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

#ifndef GRAM_NODE_H
#define GRAM_NODE_H

/* --- types --- */
typedef enum
{
    N_and, N_or, N_not,
    N_lt, N_le, N_gt, N_ge, 
    N_eq, N_neq, N_seq, N_sneq,
    N_match, N_nmatch,
    N_str, N_pat, N_number,
    N_uid, N_gid, N_uname, N_gname, 
    N_fname, N_psargs, N_cpuburn,
    N_true, N_false,
    N_syscalls, N_ctxswitch, 
    N_virtualsize, N_residentsize,
    N_iodemand, N_iowait, N_schedwait
} N_tag;

typedef struct
{
    struct bool_node *left;
    struct bool_node *right;
} bool_children;

typedef struct bool_node
{
    N_tag tag;
    struct bool_node *next;
    union {
	bool_children children;
	char *str_val;
	double num_val;
    }
    data;
} bool_node;

/* --- functions --- */

void free_tree(bool_node *);
void start_tree(void);

bool_node *create_tnode(N_tag, bool_node *, bool_node *);
bool_node *create_tag_node(N_tag);
bool_node *create_number_node(double);
bool_node *create_str_node(char *);
bool_node *create_pat_node(char *);
void dump_bool_tree(FILE *, bool_node *);
void dump_predicate(FILE *, bool_node *);

#endif
