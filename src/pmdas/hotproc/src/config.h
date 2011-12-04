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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <sys/procfs.h>

typedef struct {
	double  syscalls;
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
	char	*uname;
	char	*gname;
        char    fname[PRCOMSIZ];     /* basename of exec()'d pathname */
        char    psargs[PRARGSZ];     /* initial chars of arg list */
	double  cpuburn;

        derived_pred_t preds;

	/* --- ioctl buffer fields for testing purposes only --- */

        /* prpsinfo_t fields */
	ulong_t pr_size;
	ulong_t pr_rssize;

	/* prusage_t fields */
	ulong_t pu_sysc;
	ulong_t pu_ictx;
	ulong_t pu_vctx;
	ulong_t pu_gbread;
	ulong_t pu_bread;
	ulong_t pu_gbwrit;
	ulong_t pu_bwrit;

	/* accounting fields */
	accum_t ac_bwtime;
	accum_t ac_rwtime;
	accum_t ac_qwtime;

} config_vars;

#include "gram_node.h"

FILE *open_config(void);
void read_config(FILE *);
int parse_config(bool_node **tree);
void new_tree(bool_node *tree);
int eval_tree(config_vars *);
void dump_tree(FILE *);
void do_pred_testing(void);
int read_test_values(FILE *, config_vars *);

#endif
