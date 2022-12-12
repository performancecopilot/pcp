/*
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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

#include "pmapi.h"
#include "pmda.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

extern __uint32_t	refreshtime;		/* seconds per refresh cycle */
extern char		*proc_prefix;		/* allow pathname prefix for QA */
extern long		hertz;			/* kernel clock rate for "ticks" */
extern __uint32_t	cycles;			/* count of refresh cycles */

/*
 * table of processes of interest ... updated in getinfo()
 */
typedef struct {
    pid_t	pid;
    int		state;
    char	*cmd;
    struct {			/* observed running total at last sample */
	long	utime;
	long	stime;
    } prior;
    struct {			/* fraction of CPU used over last sample */
	double	utime;
	double	stime;
    } burn;
} proctab_t;

/*
 * proctab[] state (bit flags) ...
 */
#define P_INIT	1	/* initialized, need second sample */
#define P_DATA	2	/* 2 or more samples, so have data */
#define P_SEEN	4	/* process seen in the current refresh() */
#define P_GONE	8	/* deleted slot */

/*
 * table of groups of interest
 */
typedef struct {
    int			id;
    char		*name;
    int			nproctab;	/* number of entries in proctab[] */
    int			nproc;		/* real entries in proctab[] */
    int			nproc_active;	/* active entries in proctab[] */
    int			indom_cycle;	/* refresh() cycle when indom last built */
    proctab_t		*proctab;
    char		*pattern;
    regex_t		regex;
} grouptab_t;
extern grouptab_t	*grouptab;
extern	int		ngroup;		/* number of grouptab[] entries */
