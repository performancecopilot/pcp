/*
 * Copyright (c) 1999 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef STATS_H
#define STATS_H

#include <sys/types.h>
#include <sys/param.h>

/* subdir nested under PCP_TMP_DIR */
#define PMIE_SUBDIR	"pmie"

/* pmie performance instrumentation */
typedef struct {
    char		config[MAXPATHLEN+1];
    char		logfile[MAXPATHLEN+1];
    char		defaultfqdn[MAXHOSTNAMELEN+1];
    float		eval_expected;		/* pmcd.pmie.eval.expected */
    unsigned int	numrules;		/* pmcd.pmie.numrules      */
    unsigned int	actions;		/* pmcd.pmie.actions       */
    unsigned int	eval_true;		/* pmcd.pmie.eval.true     */
    unsigned int	eval_false;		/* pmcd.pmie.eval.false    */
    unsigned int	eval_unknown;		/* pmcd.pmie.eval.unknown  */
    unsigned int	eval_actual;		/* pmcd.pmie.eval.actual   */
    unsigned int	version;
} pmiestats_t;

#endif /* STATS_H */
