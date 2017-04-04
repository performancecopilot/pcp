/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _PMLC_H
#define _PMLC_H

/* config file parser states (bit field values) */
#define GLOBAL  	0
#define INSPEC  	1
#define INSPECLIST	2

/* timezone to use when printing status */
#define TZ_LOCAL	0
#define TZ_LOGGER	1
#define TZ_OTHER	2

/* timezone variables */
extern char	*tz;			/* for -Z timezone */
extern int	tztype;			/* timezone for status cmd */

/* parse summary back from yacc to main */
extern char	*hostname;
extern int	state;
extern int	control;
extern int	mystate;
extern int	qa_case;

extern void yyerror(char *);
extern void yywarn(char *);
extern int yywrap(void);
extern int yylex(void);
extern void skipAhead (void);
extern int yyparse(void);
extern void beginmetrics(void);
extern void endmetrics(void);
extern void beginmetgrp(void);
extern void endmetgrp(void);
extern void addmetric(const char *);
extern void addinst(char *, int);
extern int connected(void);
extern int still_connected(int);
extern int metric_cnt;
#ifdef PCP_DEBUG
extern void dumpmetrics(FILE *);
#endif

/* connection routines */
extern int ConnectLogger(char *, int *, int *);
extern void DisconnectLogger(void);
extern int ConnectPMCD(void);

/* command routines */
extern int logfreq;
extern void ShowLoggers(char *);
extern void Query(void);
extern void LogCtl(int, int, int);
extern void Status(int, int);
extern void NewVolume(void);
extern void Sync(void);
extern void Qa(void);

/* information about an instance domain */
typedef struct {
    pmInDom	indom;
    int		n_insts;
    int		*inst;
    char	**name;
} indom_t;

/* a metric plus an optional list of instances */
typedef struct {
    char		*name;
    pmID		pmid;
    int			indom;		/* index of indom (or -1) */
    int			n_insts;	/* number of insts for this metric */
    int			*inst;		/* list of insts for this metric */
    struct {
	unsigned	selected : 1,	/* apply instances to metric? */
			new : 1,	/* new in current PMNS subtree */
			has_insts : 1;	/* free inst list? */
    } status;
} metric_t;

#endif /* _PMLC_H */
