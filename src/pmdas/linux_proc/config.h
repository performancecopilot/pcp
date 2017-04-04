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

#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	/*double  syscalls;*/
	double  ctxswitch;
	double  virtualsize;
	double  residentsize;
	double  iodemand;
	double  iowait;
	double  schedwait;
} derived_pred_t;

typedef struct {
        uid_t   uid;         /* real user id */
        gid_t   gid;         /* real group id */
	char	uname[64];
	char	gname[64];
        char    fname[256];     /* basename of exec()'d pathname */
        char    psargs[256];     /* initial chars of arg list */
	double  cpuburn;
        derived_pred_t preds;
} config_vars;

#include "gram_node.h"

extern void set_conf_buffer(char *);
extern char *get_conf_buffer(void);
extern FILE *open_config(char []);
extern int read_config(FILE *);
extern int parse_config(bool_node **tree);
extern void new_tree(bool_node *tree);
extern int eval_tree(config_vars *);
extern void dump_tree(FILE *);
extern void do_pred_testing(void);

#endif
