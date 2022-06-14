/*
 * Copyright (c) 2014-2016,2018,2022 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _LOGGER_H
#define _LOGGER_H

#include <pcp/pmapi.h>
#include <pcp/libpcp.h>
#include <pcp/archive.h>
#include <assert.h>

/*
 * Pass 0 and name cache helpers
 */
extern FILE *pass0(FILE *);
extern int cache_lookup(char *, pmID *, pmDesc *);
extern void cache_free(void);

/*
 * a task is a bundle of fetches to be done together - it
 * originally corresponded one-to-one with a configuration
 * file curly-brace-enclosed block, but no longer does.
 */
typedef struct task_s {
    struct task_s	*t_next;	/* linked list of all tasks */
    struct task_s	*t_alarmed;	/* linked list of alarmed tasks */
    struct timeval	t_delta;
    int			t_state;	/* logging state */
    int			t_numpmid;
    int			t_numvalid;
    pmID		*t_pmidlist;
    char		**t_namelist;
    pmDesc		*t_desclist;
    fetchctl_t		*t_fetch;
    int			t_afid;
    int			t_alarm;	/* set when log_callback() called for this task */
    int			t_size;		/* pdu size for -r flag reporting */
    int			t_dm;		/* 1 if derived metrics included */
} task_t;

extern task_t		*tasklist;	/* main list of tasks */
extern __pmLogCtl	logctl;		/* global log control */
extern __pmArchCtl	archctl;	/* global archive control */
extern int log_alarm;			/* set when log_callback() called for any task */

typedef struct {
    char		*name;
    int			state;
    int			control;
    struct timeval	delta;
} dynroot_t;

extern dynroot_t	*dyn_roots;	/* dynamic root list - never free'd */
extern int		n_dyn_roots;	/* and length of ~ */

/* config file parser states */
#define GLOBAL  0
#define INSPEC  1

/* generic error messages */
extern char	*chk_emess[];
extern void die(char *, int);

/*
 * hash control for per-metric (really per-metric per-log specification)
 * -- used to establish and maintain state for ControlLog operations
 */
extern __pmHashCtl	pm_hash;

/* another hash list used for maintaining information about all metrics and
 * instances that have EVER appeared in the log as opposed to just those
 * currently being logged.  It's a history list.
 */
extern __pmHashCtl	hist_hash;

typedef struct {
    int	ih_inst;
    int	ih_flags;
} insthist_t;

typedef struct {
    pmID	ph_pmid;
    pmInDom	ph_indom;
    int		ph_numinst;
    insthist_t	*ph_instlist;
} pmidhist_t;

/* access control goo */
#define PM_OP_LOG_ADV	0x1
#define PM_OP_LOG_MAND	0x2
#define PM_OP_LOG_ENQ	0x4

#define PM_OP_NONE	0x0
#define PM_OP_ALL	0x7

#define PMLC_SET_MAYBE(val, flag) \
	val = ((val) & ~0x10) | (((flag) & 0x1) << 4)
#define PMLC_GET_MAYBE(val) \
	(((val) & 0x10) >> 4)

/* volume switch types */
#define VOL_SW_SIGHUP  0
#define VOL_SW_PMLC    1
#define VOL_SW_COUNTER 2
#define VOL_SW_BYTES   3
#define VOL_SW_TIME    4
#define VOL_SW_MAX     5

/* initial time of day from remote PMCD */
extern __pmTimestamp	epoch;

/* offset to start of last written result */
extern int	last_log_offset;

/* yylex() gets input from here ... */
extern FILE		*fconfig;
extern FILE		*yyin;
extern char		*configfile;
extern int		lineno;

extern int myFetch(int, pmID *, __pmResult **);
extern void yyerror(char *);
extern void yywarn(char *);
extern void yylinemarker(char *);
extern int yylex(void);
extern int yyparse(void);
extern void yyend(void);
extern void buildinst(int *, int **, char ***, int, char *);
extern void freeinst(int *, int *, char **);
extern void linkback(task_t *);
extern optreq_t *findoptreq(pmID, int);
extern void log_callback(int, void *);
extern void do_work(task_t *);
extern int chk_one(task_t *, pmID, int);
extern int chk_all(task_t *, pmID);
extern int newvolume(int);
extern void validate_metrics(void);
extern void check_dynamic_metrics();
extern int do_control_req(__pmResult *, int, int, int, int);

extern void disconnect(int);
extern int reconnect(void);
extern int do_prologue(void);
extern int do_epilogue(void);
extern void run_done(int,char *);
extern int putmark(void);
extern void dumpit(void);

#include <sys/param.h>
extern char pmlc_host[];

#define LOG_DELTA_ONCE		-1
#define LOG_DELTA_DEFAULT	-2

/* command line parameters */
extern char	    	*archName;		/* real base name for log files */
extern char		*pmcd_host;		/* collecting from PMCD on this host */
extern char		*pmcd_host_conn;	/* ... and this is how we connected to it */
extern int		primary;		/* Non-zero for primary logger */
extern int		rflag;
extern struct timeval	delta;			/* default logging interval */
extern int		ctlport;		/* pmlogger control port number */
extern char		*note;			/* note for port map file */

/* signal handlers */
extern void init_signals(void);

/* pmlc support */
extern void init_ports(void);
extern int control_req(int ctlfd);
extern int client_req(void);
extern __pmHashCtl	hist_hash;
extern unsigned int	denyops;	/* for access control (ops not allowed) */
extern __pmTimestamp	last_stamp;
extern int		clientfd;
#define CFD_INET	0
#define CFD_IPV6	1
#define CFD_UNIX	2
#define CFD_NUM		3
extern int		ctlfds[CFD_NUM];
extern int		exit_samples;
extern int		vol_switch_samples;
extern __int64_t	vol_switch_bytes;
extern int		vol_switch_flag; /* logvol switch: set on SIGHUP */
extern int		log_switch_flag; /* archive switch: set on SIGUSR2 */
extern int		pmlogger_reexec;
extern int		vol_samples_counter;
extern int		archive_version; 
extern int		pmlc_ipc_version;
extern int		parse_done;
extern __int64_t	exit_bytes;
extern __int64_t	vol_bytes;
extern int		sig_code;

/* event record handling */
extern int do_events(pmValueSet *);

/* cleanup control fds and sockets etc prior to reexec or exit */
extern void cleanup(void);

/* QA testing and error injection support ... see do_request() */
extern int	qa_case;
#define QA_OFF		100
#define QA_SLEEPY	101

/* To expose selected function symbols for mybacktrace() */
#ifdef BACKTRACE_SYMBOLS
#define STATIC_FUNC
#else
#define STATIC_FUNC static
#endif

#endif /* _LOGGER_H */
