/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995-2001,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#include "stats.h"
#include "pmcd/src/pmcd.h"
#include "pmcd/src/client.h"
#include <sys/stat.h>
#if defined(IS_SOLARIS)
#include <sys/systeminfo.h>
#endif

/*
 * Note: strange numbering for pmcd.pdu_{in,out}.total for
 * compatibility with earlier PCP versions ... this is the "item"
 * field of the PMID
 */
#define _TOTAL			16

/*
 * all metrics supported in this PMD - one table entry for each
 */
static pmDesc	desctab[] = {
/* control.debug */
    { PMDA_PMID(0,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* datasize */
    { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* numagents */
    { PMDA_PMID(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* numclients */
    { PMDA_PMID(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.timeout */
    { PMDA_PMID(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* timezone -- local $TZ -- for pmlogger */
    { PMDA_PMID(0,5), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* simabi -- Subprogram Interface Model, ABI version of this pmcd (normally PM_TYPE_STRING) */
    { PMDA_PMID(0,6), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* version -- pcp version */
    { PMDA_PMID(0,7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.register -- bulletin board */
    { PMDA_PMID(0,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.traceconn -- trace connections */
    { PMDA_PMID(0,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.tracepdu -- trace PDU traffic */
    { PMDA_PMID(0,10), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.tracebufs -- number of trace buffers */
    { PMDA_PMID(0,11), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.dumptrace -- push-button, pmStore to dump trace */
    { PMDA_PMID(0,12), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.dumpconn -- push-button, pmStore to dump connections */
    { PMDA_PMID(0,13), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.tracenobuf -- unbuffered tracing  */
    { PMDA_PMID(0,14), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* control.sighup -- push-button, pmStore to SIGHUP pmcd */
    { PMDA_PMID(0,15), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* services -- locally running PCP services */
    { PMDA_PMID(0,16), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* openfds -- number of open file descriptors */
    { PMDA_PMID(0,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* buf.alloc */
    { PMDA_PMID(0,18), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* buf.free */
    { PMDA_PMID(0,19), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* build -- pcp build number */
    { PMDA_PMID(0,20), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* hostname -- local hostname -- for pmlogger */
    { PMDA_PMID(0,21), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },

/* pdu_in.error */
    { PMDA_PMID(1,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.result */
    { PMDA_PMID(1,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.profile */
    { PMDA_PMID(1,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.fetch */
    { PMDA_PMID(1,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.desc_req */
    { PMDA_PMID(1,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.desc */
    { PMDA_PMID(1,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.instance_req */
    { PMDA_PMID(1,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.instance */
    { PMDA_PMID(1,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.text_req */
    { PMDA_PMID(1,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.text */
    { PMDA_PMID(1,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.control_req */
    { PMDA_PMID(1,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.creds */
    { PMDA_PMID(1,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.pmns_ids */
    { PMDA_PMID(1,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.pmns_names */
    { PMDA_PMID(1,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.pmns_child */
    { PMDA_PMID(1,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.total */
    { PMDA_PMID(1,_TOTAL), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.pmns_traverse */
    { PMDA_PMID(1,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_in.auth */
    { PMDA_PMID(1,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },

/* pdu_out.error */
    { PMDA_PMID(2,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.result */
    { PMDA_PMID(2,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.profile */
    { PMDA_PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.fetch */
    { PMDA_PMID(2,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.desc_req */
    { PMDA_PMID(2,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.desc */
    { PMDA_PMID(2,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.instance_req */
    { PMDA_PMID(2,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.instance */
    { PMDA_PMID(2,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.text_req */
    { PMDA_PMID(2,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.text */
    { PMDA_PMID(2,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.control_req */
    { PMDA_PMID(2,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.creds */
    { PMDA_PMID(2,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.pmns_ids */
    { PMDA_PMID(2,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.pmns_names */
    { PMDA_PMID(2,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.pmns_child */
    { PMDA_PMID(2,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.total */
    { PMDA_PMID(2,_TOTAL), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.pmns_traverse */
    { PMDA_PMID(2,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pdu_out.auth */
    { PMDA_PMID(2,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },

/* pmlogger.port */
    { PMDA_PMID(3,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmlogger.pmcd_host */
    { PMDA_PMID(3,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmlogger.archive */
    { PMDA_PMID(3,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmlogger.host */
    { PMDA_PMID(3,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },

/* agent.type */
    { PMDA_PMID(4,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* agent.status */
    { PMDA_PMID(4,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },

/* pmie.configfile */
    { PMDA_PMID(5,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmie.logfile */
    { PMDA_PMID(5,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmie.pmcd_host */
    { PMDA_PMID(5,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmie.numrules */
    { PMDA_PMID(5,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmie.actions */
    { PMDA_PMID(5,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pmie.eval.true */
    { PMDA_PMID(5,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pmie.eval.false */
    { PMDA_PMID(5,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pmie.eval.unknown */
    { PMDA_PMID(5,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* pmie.eval.expected */
    { PMDA_PMID(5,8), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) },
/* pmie.eval.actual */
    { PMDA_PMID(5,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },

/* client.whoami */
    { PMDA_PMID(6,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* client.start_date */
    { PMDA_PMID(6,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },

/* pmcd.cputime.total */
    { PMDA_PMID(7,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) },
/* pmcd.cputime.per_pdu_in */
    { PMDA_PMID(7,1), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,1,-1,0,PM_TIME_USEC,PM_COUNT_ONE) },

/* pmcd.feature.secure */
    { PMDA_PMID(8,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.compress */
    { PMDA_PMID(8,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.ipv6 */
    { PMDA_PMID(8,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.authentication */
    { PMDA_PMID(8,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.creds_required */
    { PMDA_PMID(8,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.unix_domain_sockets */
    { PMDA_PMID(8,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.service_discovery */
    { PMDA_PMID(8,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pmcd.feature.containers */
    { PMDA_PMID(8,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },

/* End-of-List */
    { PM_ID_NULL, 0, 0, 0, PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }
};
static int		ndesc = sizeof(desctab)/sizeof(desctab[0]);

static __pmProfile	*_profile;	/* last received profile */

/* there are four instance domains: pmlogger, register, PMDA, and pmie */
#define INDOM_PMLOGGERS	1
static pmInDom		logindom;
#define INDOM_REGISTER	2
static pmInDom		regindom;
#define INDOM_PMDAS	3
static pmInDom		pmdaindom;
#define INDOM_PMIES	4
static pmInDom		pmieindom;
#define INDOM_POOL	5
static pmInDom		bufindom;
#define INDOM_CLIENT	6
static pmInDom		clientindom;

#define NUMREG 16
static int		reg[NUMREG];

typedef struct {
    pid_t	pid;
    int		size;
    char	*name;
    void	*mmap;
} pmie_t;
static pmie_t		*pmies;
static unsigned int	npmies;

static struct {
    int		inst;
    char	*iname;
} bufinst[] = {
    {	  12,	"0012" },
    {	  20,	"0020" },
    {	1024,	"1024" },
    {	2048,	"2048" },
    {	4196,	"4196" },
    {	8192,	"8192" },
    {   8193,	"8192+" },
};
static const int	nbufsz = sizeof(bufinst) / sizeof(bufinst[0]);

typedef struct {
    int		id;		/* index into client[] */
    int		seq;
    char	*value;
} whoami_t;
static whoami_t		*whoamis;
static unsigned int	nwhoamis;

typedef struct {
    int		pid;
    int		length;
    char	*name;
} pmcd_container_t;

typedef struct {
    int			state;
    pmcd_container_t	container;
    double		last_cputime;
    __uint64_t		last_pdu_in;
} perctx_t;

/* utilization values for per context state */
#define CTX_INACTIVE    0
#define CTX_ACTIVE      1

static perctx_t *ctxtab;
static int      num_ctx;
static int      rootfd = -1;

/*
 * expand and initialize the per client context table
 */
static void
grow_ctxtab(int ctx)
{
    ctxtab = (perctx_t *)realloc(ctxtab, (ctx+1)*sizeof(ctxtab[0]));
    if (ctxtab == NULL) {
        __pmNoMem("grow_ctxtab", (ctx+1)*sizeof(ctxtab[0]), PM_FATAL_ERR);
        /*NOTREACHED*/
    }
    while (num_ctx <= ctx) {
        ctxtab[num_ctx].state = CTX_INACTIVE;
	ctxtab[num_ctx].container.name = NULL;
        num_ctx++;
    }
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].container.name = NULL;
}

/*
 * this routine is called at initialization to patch up any parts of the
 * desctab that cannot be statically initialized, and to optionally
 * modify our Performance Metrics Domain Id (dom)
 */
static void
init_tables(int dom)
{
    int			i;
    __pmID_int		*pmidp;
    __pmInDom_int	*indomp;

    /* set domain in instance domain correctly */
    indomp = (__pmInDom_int *)&logindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMLOGGERS;
    indomp = (__pmInDom_int *)&regindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_REGISTER;
    indomp = (__pmInDom_int *)&pmdaindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMDAS;
    indomp = (__pmInDom_int *)&pmieindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMIES;
    indomp = (__pmInDom_int *)&bufindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_POOL;
    indomp = (__pmInDom_int *)&clientindom;
    indomp->flag = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_CLIENT;

    /* merge performance domain ID part into PMIDs in pmDesc table */
    for (i = 0; desctab[i].pmid != PM_ID_NULL; i++) {
	pmidp = (__pmID_int *)&desctab[i].pmid;
	pmidp->domain = dom;
	if (pmidp->cluster == 0 && pmidp->item == 8)
	    desctab[i].indom = regindom;
	else if (pmidp->cluster == 0 && (pmidp->item == 18 || pmidp->item == 19))
	    desctab[i].indom = bufindom;
	else if (pmidp->cluster == 3)
	    desctab[i].indom = logindom;
	else if (pmidp->cluster == 4)
	    desctab[i].indom = pmdaindom;
	else if (pmidp->cluster == 5)
	    desctab[i].indom = pmieindom;
	else if (pmidp->cluster == 6)
	    desctab[i].indom = clientindom;
    }
    ndesc--;
}

static int
pmcd_profile(__pmProfile *prof, pmdaExt *pmda)
{
    _profile = prof;	
    return 0;
}

static void
remove_pmie_indom(void)
{
    int n;

    for (n = 0; n < npmies; n++) {
	free(pmies[n].name);
	__pmMemoryUnmap(pmies[n].mmap, pmies[n].size);
    }
    free(pmies);
    pmies = NULL;
    npmies = 0;
}

static int
stat_time_differs(struct stat *statbuf, struct stat *lastsbuf)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    if (statbuf->st_mtime != lastsbuf->st_mtime)
	return 1;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    if ((statbuf->st_mtimespec.tv_sec != lastsbuf->st_mtimespec.tv_sec) ||
	(statbuf->st_mtimespec.tv_nsec != lastsbuf->st_mtimespec.tv_nsec))
	return 1;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
    if ((statbuf->st_mtim.tv_sec != lastsbuf->st_mtim.tv_sec) ||
	(statbuf->st_mtim.tv_nsec != lastsbuf->st_mtim.tv_nsec))
	return 1;
#else
!bozo!
#endif
    return 0;
}

/* use a static timestamp, stat PMIE_SUBDIR, if changed update "pmies" */
static unsigned int
refresh_pmie_indom(void)
{
    static struct stat	lastsbuf;
    pid_t		pmiepid;
    struct dirent	*dp;
    struct stat		statbuf;
    size_t		size;
    char		*endp;
    char		fullpath[MAXPATHLEN];
    void		*ptr;
    DIR			*pmiedir;
    int			fd;
    int			sep = __pmPathSeparator();

    snprintf(fullpath, sizeof(fullpath), "%s%c%s",
	     pmGetConfig("PCP_TMP_DIR"), sep, PMIE_SUBDIR);
    if (stat(fullpath, &statbuf) == 0) {
	if (stat_time_differs(&statbuf, &lastsbuf)) {

	    lastsbuf = statbuf;

	    /* tear down the old instance domain */
	    if (pmies)
		remove_pmie_indom();

	    /* open the directory iterate through mmaping as we go */
	    if ((pmiedir = opendir(fullpath)) == NULL) {
		__pmNotifyErr(LOG_ERR, "pmcd pmda cannot open %s: %s",
				fullpath, osstrerror());
		return 0;
	    }
	    /* NOTE:  all valid files are already mmapped by pmie */
	    while ((dp = readdir(pmiedir)) != NULL) {
		size = (npmies+1) * sizeof(pmie_t);
		pmiepid = (pid_t)strtoul(dp->d_name, &endp, 10);
		if (*endp != '\0')	/* skips over "." and ".." here */
		    continue;
		if (!__pmProcessExists(pmiepid))
		    continue;
		snprintf(fullpath, sizeof(fullpath), "%s%c%s%c%s",
			 pmGetConfig("PCP_TMP_DIR"), sep, PMIE_SUBDIR, sep,
			 dp->d_name);
		if (stat(fullpath, &statbuf) < 0) {
		    __pmNotifyErr(LOG_WARNING, "pmcd pmda cannot stat %s: %s",
				fullpath, osstrerror());
		    continue;
		}
		if (statbuf.st_size != sizeof(pmiestats_t))
		    continue;
		if  ((endp = strdup(dp->d_name)) == NULL) {
		    __pmNoMem("pmie iname", strlen(dp->d_name), PM_RECOV_ERR);
		    continue;
		}
		if ((pmies = (pmie_t *)realloc(pmies, size)) == NULL) {
		    __pmNoMem("pmie instlist", size, PM_RECOV_ERR);
		    free(endp);
		    continue;
		}
		if ((fd = open(fullpath, O_RDONLY)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "pmcd pmda cannot open %s: %s",
				fullpath, osstrerror());
		    free(endp);
		    continue;
		}
		ptr = __pmMemoryMap(fd, statbuf.st_size, 0);
		close(fd);
		if (ptr == NULL) {
		    __pmNotifyErr(LOG_ERR, "pmcd pmda memmap of %s failed: %s",
				fullpath, osstrerror());
		    free(endp);
		    continue;
		}
		else if (((pmiestats_t *)ptr)->version != 1) {
		    __pmNotifyErr(LOG_WARNING, "incompatible pmie version: %s",
				fullpath);
		    __pmMemoryUnmap(ptr, statbuf.st_size);
		    free(endp);
		    continue;
		}
		pmies[npmies].pid = pmiepid;
		pmies[npmies].name = endp;
		pmies[npmies].size = statbuf.st_size;
		pmies[npmies].mmap = ptr;
		npmies++;
	    }
	    closedir(pmiedir);
	}
    }
    else {
	remove_pmie_indom();
    }
    setoserror(0);
    return npmies;
}

static int
pmcd_instance_reg(int inst, char *name, __pmInResult **result)
{
    __pmInResult	*res;
    int		i;
    char	idx[3];		/* ok for NUMREG <= 99 */

    res = (__pmInResult *)malloc(sizeof(__pmInResult));
    if (res == NULL)
        return -oserror();

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = NUMREG;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -oserror();
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < res->numinst; i++) {
	    res->instlist[i] = i;
	    snprintf(idx, sizeof(idx), "%d", i);
	    if ((res->namelist[i] = strdup(idx)) == NULL) {
		__pmFreeInResult(res);
		return -oserror();
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	if (0 <= inst && inst < NUMREG) {
	    snprintf(idx, sizeof(idx), "%d", inst);
	    if ((res->namelist[0] = strdup(idx)) == NULL) {
		__pmFreeInResult(res);
		return -oserror();
	    }
	}
	else {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	char	*endp;
	i = (int)strtol(name, &endp, 10);
	if (*endp == '\0' && 0 <= i && i < NUMREG)
	    res->instlist[0] = i;
	else {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }

    *result = res;
    return 0;
}

static int
pmcd_instance_pool(int inst, char *name, __pmInResult **result)
{
    __pmInResult	*res;
    int		i;

    res = (__pmInResult *)malloc(sizeof(__pmInResult));
    if (res == NULL)
        return -oserror();

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = nbufsz;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -oserror();
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < nbufsz; i++) {
	    res->instlist[i] = bufinst[i].inst;
	    if ((res->namelist[i] = strdup(bufinst[i].iname)) == NULL) {
		__pmFreeInResult(res);
		return -oserror();
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	for (i = 0; i < nbufsz; i++) {
	    if (inst == bufinst[i].inst) {
		if ((res->namelist[0] = strdup(bufinst[i].iname)) == NULL) {
		    __pmFreeInResult(res);
		    return -oserror();
		}
		break;
	    }
	}
	if (i == nbufsz) {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	for (i = 0; i < nbufsz; i++) {
	    if (strcmp(name, bufinst[i].iname) == 0) {
		res->instlist[0] = bufinst[i].inst;
		break;
	    }
	}
	if (i == nbufsz) {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }

    *result = res;
    return 0;
}

static int
pmcd_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    int			sts = 0;
    __pmInResult	*res;
    int			getall = 0;
    int			getname = 0;	/* initialize to pander to gcc */
    int			nports = 0;	/* initialize to pander to gcc */
    __pmLogPort		*ports;
    unsigned int	pmiecount = 0;	/* initialize to pander to gcc */
    int			i;

    if (indom == regindom)
	return pmcd_instance_reg(inst, name, result);
    else if (indom == bufindom)
	return pmcd_instance_pool(inst, name, result);
    else if (indom == logindom || indom == pmdaindom || indom == pmieindom || indom == clientindom) {
	res = (__pmInResult *)malloc(sizeof(__pmInResult));
	if (res == NULL)
	    return -oserror();
	res->instlist = NULL;
	res->namelist = NULL;

	if (indom == logindom) {
	    /* use the wildcard behaviour of __pmLogFindPort to find
	     * all pmlogger ports on localhost.  Note that
	     * __pmLogFindPort will not attempt to contact pmcd if
	     * localhost is specified---this means we don't get a
	     * recursive call to pmcd which would hang!
	     */
	    if ((nports = __pmLogFindPort("localhost", PM_LOG_ALL_PIDS, &ports)) < 0) {
		free(res);
		return nports;
	    }
	}
	else if (indom == pmieindom)
	    pmiecount = refresh_pmie_indom();

	if (name == NULL && inst == PM_IN_NULL) {
	    getall = 1;

	    if (indom == logindom)
		res->numinst = nports;
	    else if (indom == pmdaindom)
		res->numinst = nAgents;
	    else if (indom == pmieindom)
		res->numinst = pmiecount;
	    else if (indom == clientindom) {
		res->numinst = 0;
		for (i = 0; i < nClients; i++) {
		    if (client[i].status.connected) res->numinst++;
		}
	    }
	}
	else {
	    getname = name == NULL;
	    res->numinst = 1;
	}

	if (getall || !getname) {
	    if ((res->instlist = (int *)malloc(res->numinst * sizeof(int))) == NULL) {
		sts = -oserror();
		__pmNoMem("pmcd_instance instlist", res->numinst * sizeof(int), PM_RECOV_ERR);
		__pmFreeInResult(res);
		return sts;
	    }
	}
	if (getall || getname) {
	    if ((res->namelist = (char **)malloc(res->numinst * sizeof(char *))) == NULL) {
		sts = -oserror();
		__pmNoMem("pmcd_instance namelist", res->numinst * sizeof(char *), PM_RECOV_ERR);
		free(res->instlist);
		__pmFreeInResult(res);
		return sts;
	    }
	}
    }
    else
	return PM_ERR_INDOM;

    if (indom == logindom) {
	res->indom = logindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < nports; i++) {
		res->instlist[i] = ports[i].pid;
		res->namelist[i] = strdup(ports[i].name);
		if (res->namelist[i] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmGetInDom",
			     strlen(ports[i].name), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < nports; i++) {
		if (inst == ports[i].pid)
		    break;
	    }
	    if (i == nports) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(ports[i].name);
		if (res->namelist[0] == NULL) {
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(ports[i].name), PM_RECOV_ERR);
		    sts = -oserror();
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < nports; i++) {
		if (strcmp(name, ports[i].name) == 0)
		    break;
	    }
	    if (i == nports)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = ports[i].pid;
	}
    }
    else if (indom == pmieindom) {
	res->indom = pmieindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < pmiecount; i++) {
		res->instlist[i] = pmies[i].pid;
		res->namelist[i] = strdup(pmies[i].name);
		if (res->namelist[i] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmie_instance pmGetInDom",
			     strlen(pmies[i].name), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < pmiecount; i++) {
		if (inst == pmies[i].pid)
		    break;
	    }
	    if (i == pmiecount) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(pmies[i].name);
		if (res->namelist[0] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(pmies[i].name), PM_RECOV_ERR);
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < pmiecount; i++) {
		if (strcmp(name, pmies[i].name) == 0)
		    break;
	    }
	    if (i == pmiecount)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = pmies[i].pid;
	}
    }
    else if (indom == pmdaindom) {
	res->indom = pmdaindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < nAgents; i++) {
		res->instlist[i] = agent[i].pmDomainId;
		res->namelist[i] = strdup(agent[i].pmDomainLabel);
		if (res->namelist[i] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmGetInDom",
			     strlen(agent[i].pmDomainLabel), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < nAgents; i++) {
		if (inst == agent[i].pmDomainId)
		    break;
	    }
	    if (i == nAgents) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(agent[i].pmDomainLabel);
		if (res->namelist[0] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(agent[i].pmDomainLabel), PM_RECOV_ERR);
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < nAgents; i++) {
		if (strcmp(name, agent[i].pmDomainLabel) == 0)
		    break;
	    }
	    if (i == nAgents)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = agent[i].pmDomainId;
	}
    }
    else if (indom == clientindom) {
	res->indom = clientindom;

	if (getall) {		/* get instance ids and names */
	    int		k = 0;
	    for (i = 0; i < nClients; i++) {
		char	buf[11];	/* enough for 32-bit client seq number */
		if (!client[i].status.connected)
		    continue;
		res->instlist[k] = client[i].seq;
		snprintf(buf, sizeof(buf), "%u", client[i].seq);
		res->namelist[k] = strdup(buf);
		if (res->namelist[k] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmGetInDom",
			     strlen(buf), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
		k++;
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < nClients; i++) {
		if (client[i].status.connected && inst == client[i].seq)
		    break;
	    }
	    if (i == nClients) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		char	buf[11];	/* enough for 32-bit client seq number */
		snprintf(buf, sizeof(buf), "%u", (unsigned int)inst);
		res->namelist[0] = strdup(buf);
		if (res->namelist[0] == NULL) {
		    sts = -oserror();
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(buf), PM_RECOV_ERR);
		}
	    }
	}
	else {			/* given name, get id */
	    char	buf[11];	/* enough for 32-bit client seq number */
	    for (i = 0; i < nClients; i++) {
		if (!client[i].status.connected)
		    continue;
		snprintf(buf, sizeof(buf), "%u", client[i].seq);
		if (strcmp(name, buf) == 0)
		    break;
	    }
	    if (i == nClients)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = client[i].seq;
	}
    }

    if (sts < 0) {
	__pmFreeInResult(res);
	return sts;
    }

    *result = res;
    return 0;
}

/*
 * numval != 1, so re-do vset[i] allocation
 */
static int
vset_resize(pmResult *rp, int i, int onumval, int numval)
{
    int		expect = numval;

    if (rp->vset[i] != NULL) {
	free(rp->vset[i]);
    }

    if (numval < 0)
	expect = 0;

    rp->vset[i] = (pmValueSet *)malloc(sizeof(pmValueSet) + (expect-1)*sizeof(pmValue));

    if (rp->vset[i] == NULL) {
	if (i) {
	    /* we're doomed ... reclaim pmValues 0, 1, ... i-1 */
	    rp->numpmid = i;
	    __pmFreeResultValues(rp);
	}
	return -1;
    }

    rp->vset[i]->numval = numval;
    return 0;
}

static char *
simabi()
{
#if defined(__linux__) || defined(IS_GNU)
# if defined(__i386__)
    return "ia32";
# elif defined(__ia64__) || defined(__ia64)
    return "ia64";
# else
    return SIM_ABI;	/* SIM_ABI is defined in the linux Makefile */
# endif /* __linux__ */
#elif defined(IS_SOLARIS)
    static char abi[32];
    if (sysinfo(SI_ARCHITECTURE_NATIVE, abi, sizeof(abi)) < 0) {
	return "unknown";
    } else {
	return abi;
    }
#elif defined(IS_FREEBSD) || defined(IS_NETBSD)
    return "elf";
#elif defined(IS_DARWIN)
    return "Mach-O " SIM_ABI;
#elif defined(IS_MINGW)
    return "x86_64";
#elif defined(IS_AIX)
    return "powerpc";
#else
    !!! bozo : dont know which executable format pmcd should be!!!
#endif
}

static char *
tzinfo(void)
{
    /*
     * __pmTimezone() caches its result in $TZ - pmcd is long running,
     * however, and we *really* want to see changes in the timezone or
     * daylight savings state via pmcd.timezone, so we clear TZ first.
     */
#ifdef HAVE_UNSETENV
    unsetenv("TZ");
#else	/* MINGW */
    putenv("TZ=");
#endif
    return __pmTimezone();
}

static int
extract_service(const char *path, char *name, pid_t *pid)
{
    int		length, sep = __pmPathSeparator();
    char	fullpath[MAXPATHLEN];
    char	buffer[64];
    FILE	*fp;

    /* check basename has a ".pid" suffix */
    if ((length = strlen(name)) < 5)
	return 0;
    length -= 4;
    if (strcmp(&name[length], ".pid") != 0)
	return 0;

    /* extract PID lurking within the file */
    snprintf(fullpath, sizeof(fullpath), "%s%c%s", path, sep, name);
    if ((fp = fopen(fullpath, "r")) == NULL)
	return 0;
    sep = fscanf(fp, "%63s", buffer);
    fclose(fp);
    if (sep != 1)
	return 0;
    *pid = atoi(buffer);
    if (!__pmProcessExists(*pid))
	return 0;

    /* finally setup service name to return */
    name[length] = '\0';
    return length;
}

char *
services(void)
{
    static char		servicelist[128];
    static struct stat	lastsbuf;
    pid_t		pid;
    struct dirent	*dp;
    struct stat		statbuf;
    char		*path;
    DIR			*rundir;
    int			length, offset;

    path = pmGetConfig("PCP_RUN_DIR");
    if (stat(path, &statbuf) == 0) {
	if (stat_time_differs(&statbuf, &lastsbuf)) {
	    lastsbuf = statbuf;

	    /* by definition, pmcd is currently running */
	    strcpy(servicelist, PM_SERVER_SERVICE_SPEC);
	    offset = sizeof(PM_SERVER_SERVICE_SPEC) - 1;

	    /* iterate through directory, building up services string */
	    if ((rundir = opendir(path)) == NULL) {
		__pmNotifyErr(LOG_ERR, "pmcd pmda cannot open %s: %s",
				path, osstrerror());
		return servicelist;
	    }
	    while ((dp = readdir(rundir)) != NULL) {
		if (dp->d_name[0] == '.')
		    continue;
		length = sizeof(PM_SERVER_SERVICE_SPEC) - 1;
		if (strncmp(dp->d_name, PM_SERVER_SERVICE_SPEC, length) == 0)
		    continue;
		if ((length = extract_service(path, dp->d_name, &pid)) <= 0)
		    continue;
		if (!__pmProcessExists(pid))
		    continue;
		if (offset + 1 + length + 1 > sizeof(servicelist))
		    continue;
		servicelist[offset++] = ' ';
		strcpy(&servicelist[offset], dp->d_name);
		offset += length;
	    }
	    closedir(rundir);
	}
    } else {
	strcpy(servicelist, PM_SERVER_SERVICE_SPEC);
    }
    return servicelist;
}

static char *
hostnameinfo(void)
{
    static char	host[MAXHOSTNAMELEN];
    char	*name;

    (void)gethostname(host, MAXHOSTNAMELEN);
    name = host;

    return name;
}

static int
fetch_feature(int item, pmAtomValue *avp)
{
    if (item < 0 || item >= PM_SERVER_FEATURES)
	return PM_ERR_PMID;
    avp->ul = __pmServerHasFeature((__pmServerFeature)item);
    return 0;
}

static pmcd_container_t *
ctx_container(int ctx)
{
    if (ctx < num_ctx && ctx >= 0 && ctxtab[ctx].container.name)
	return &ctxtab[ctx].container;
    return NULL;
}

static char *
fetch_hostname(int ctx, pmAtomValue *avp, char *hostname)
{
    static char		host[MAXHOSTNAMELEN];
    pmcd_container_t	*container;
    int			sts;

    if (hostname) {	/* ensure we only ever refresh once-per-fetch */
	avp->cp = hostname;
	return hostname;
    }

    /* see if we're dealing with a request within a container */
    if ((container = ctx_container(ctx)) != NULL &&
	((sts = pmdaRootContainerHostName(rootfd,
					container->name,
					container->length,
					host, sizeof(host)) >= 0))) {
	avp->cp = hostname = host;
	return hostname;
    }

    if (_pmcd_hostname) {
	avp->cp = hostname = _pmcd_hostname;
    } else {
	if (!hostname)
	    hostname = hostnameinfo();
	avp->cp = hostname;
    }
    return hostname;
}

static int
fetch_cputime(int item, int ctx, pmAtomValue *avp)
{
    double	usr, sys;
    double	cputime;

    if (item < 0 || item > 1) {
	return PM_ERR_PMID;
    }
    if (ctx < 0) {
	/* should not happen */
	return PM_ERR_NOTCONN;
    }
    __pmProcessRunTimes(&usr, &sys);
    cputime = (usr+sys)*1000;
    if (ctx >= num_ctx)
	grow_ctxtab(ctx);
    if (item == 0) {	/* pmcd.cputime.total */
	avp->ull = (__uint64_t)cputime;
    }
    else if (item == 1) {	/* pmcd.cputime.per_pdu_in */
	int	j;
	int	pdu_in;
	for (pdu_in = j = 0; j <= PDU_MAX; j++)
	    pdu_in += __pmPDUCntIn[j];
	if (ctxtab[ctx].state == CTX_INACTIVE) {
	    /* first call for this context */
	    ctxtab[ctx].state = CTX_ACTIVE;
	    avp->d = cputime*1000/pdu_in;
	}
	else {
	    if (pdu_in > ctxtab[ctx].last_pdu_in)
		avp->d = 1000*(cputime-ctxtab[ctx].last_cputime)/(pdu_in-ctxtab[ctx].last_pdu_in);
	    else {
		/* should not happen, as you need another pdu to get here */
		avp->d = 0;
	    }
	}
	ctxtab[ctx].last_cputime = cputime;
	ctxtab[ctx].last_pdu_in = pdu_in;
    }
    return 0;
}

static void
end_context(int ctx)
{
    if (ctx >= 0 && ctx < num_ctx) {
	if (ctxtab[ctx].state == CTX_ACTIVE)
	    ctxtab[ctx].state = CTX_INACTIVE;
	if (ctxtab[ctx].container.name)
	    free(ctxtab[ctx].container.name);
	ctxtab[ctx].container.name = NULL;
    }
}

static int
pmcd_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int			i;		/* over pmidlist[] */
    int			j;
    int			sts, nports;
    int			need;
    int			numval;
    int			valfmt;
    unsigned long	datasize;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    char		*host = NULL;	/* refresh max once per fetch */
    pmiestats_t		*pmie;
    pmValueSet		*vset;
    pmDesc		*dp = NULL;	/* initialize to pander to gcc */
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    __pmLogPort		*lpp;

    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -ENOMEM;
	maxnpmids = numpmid;
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    for (i = 0; i < numpmid; i++) {
	/* Allocate a pmValueSet with room for just one value.  Even for the
	 * pmlogger port metric which has an instance domain, most of the time
	 * there will only be one logger running (i.e. one instance).  For the
	 * infrequent cases resize the value set later.
	 */
	res->vset[i] = NULL;
	if (vset_resize(res, i, 0, 1) == -1)
		return -ENOMEM;
	vset = res->vset[i];
	vset->pmid = pmidlist[i];
	vset->vlist[0].inst = PM_IN_NULL;

	for (j = 0; j < ndesc; j++) {
	    if (desctab[j].pmid == pmidlist[i]) {
		dp = &desctab[j];
		break;
	    }
	}
	if (j == ndesc) {
	    /* Error, need a smaller vset */
	    if (vset_resize(res, i, 1, PM_ERR_PMID) == -1)
		return -ENOMEM;
	    res->vset[i]->pmid = pmidlist[i];
	    continue;
	}

	valfmt = -1;
	sts = 0;
	
	pmidp = (__pmID_int *)&pmidlist[i];
	switch (pmidp->cluster) {

	    case 0:	/* global metrics */
		    switch (pmidp->item) {
			case 0:		/* control.debug */
				atom.l = pmDebug;
				break;
			case 1:		/* datasize */
				__pmProcessDataSize(&datasize);
				atom.ul = datasize;
				break;
			case 2:		/* numagents */
				atom.ul = 0;
				for (j = 0; j < nAgents; j++)
				    if (agent[j].status.connected)
					atom.ul++;
				break;
			case 3:		/* numclients */
				atom.ul = 0;
				for (j = 0; j < nClients; j++)
				    if (client[j].status.connected)
					atom.ul++;
				break;
			case 4:		/* control.timeout */
				atom.ul = _pmcd_timeout;
				break;
			case 5:		/* timezone $TZ */
				atom.cp = tzinfo();
				break;
			case 6:		/* simabi (pmcd calling convention) */
				atom.cp = simabi();
				break;
			case 7:		/* version */
				atom.cp = PCP_VERSION;
				break;
			case 8:		/* register */
				for (j = numval = 0; j < NUMREG; j++) {
				    if (__pmInProfile(regindom, _profile, j))
					numval++;
				}
				if (numval != 1) {
				    /* need a different vset size */
				    if (vset_resize(res, i, 1, numval) == -1)
					return -ENOMEM;
				    vset = res->vset[i];
				    vset->pmid = pmidlist[i];
				}
				for (j = numval = 0; j < NUMREG; j++) {
				    if (!__pmInProfile(regindom, _profile, j))
					continue;
				    vset->vlist[numval].inst = j;
				    atom.l = reg[j];
				    sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
				    if (sts < 0)
					break;
				    valfmt = sts;
				    numval++;
				}
				break;
			case 9:		/* traceconn */
				atom.l = (_pmcd_trace_mask & TR_MASK_CONN) ? 1 : 0;
				break;
			case 10:	/* tracepdu */
				atom.l = (_pmcd_trace_mask & TR_MASK_PDU) ? 1 : 0;
				break;
			case 11:	/* tracebufs */
				atom.l = _pmcd_trace_nbufs;
				break;
			case 12:	/* dumptrace ... always 0 */
				atom.l = 0;
				break;
			case 13:	/* dumpconn ... always 0 */
				atom.l = 0;
				break;
			case 14:	/* tracenobuf */
				atom.l = (_pmcd_trace_mask & TR_MASK_NOBUF) ? 1 : 0;
				break;
			case 15:	/* sighup ... always 0 */
				atom.l = 0;
				break;
			case 16:	/* services */
				atom.cp = services();
				break;
			case 17:	/* openfds */
				atom.ul = (unsigned int)pmcd_hi_openfds;
				break;
			case 18:	/* buf.alloc */
			case 19:	/* buf.free */
				for (j = numval = 0; j < nbufsz; j++) {
				    if (__pmInProfile(bufindom, _profile, bufinst[j].inst))
					numval++;
				}
				if (numval != 1) {
				    /* need a different vset size */
				    if (vset_resize(res, i, 1, numval) == -1)
					return -ENOMEM;
				    vset = res->vset[i];
				    vset->pmid = pmidlist[i];
				}
				for (j = numval = 0; j < nbufsz; j++) {
				    int		alloced;
				    int		free;
				    int		xtra_alloced;
				    int		xtra_free;
				    if (!__pmInProfile(bufindom, _profile, bufinst[j].inst))
					continue;
				    vset->vlist[numval].inst = bufinst[j].inst;
				    /* PDUBuf pool */
				    __pmCountPDUBuf(bufinst[j].inst, &alloced, &free);
				    /*
				     * the 2K buffer count also includes
				     * the 3K, 4K, ... buffers, so sub
				     * these ... which are reported as
				     * the 3K buffer count
				     */
				    __pmCountPDUBuf(bufinst[j].inst + 1024, &xtra_alloced, &xtra_free);
				    alloced -= xtra_alloced;
				    free -= xtra_free;
				    if (pmidp->item == 18)
					atom.l = alloced;
				    else
					atom.l = free;
				    sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
				    if (sts < 0)
					break;
				    valfmt = sts;
				    numval++;
				}
				break;

			case 20:	/* build */
				atom.cp = BUILD;
				break;

			case 21:	/* hostname */
				need = pmda->e_context;	/* client context ID */
				host = fetch_hostname(need, &atom, host);
				break;
			default:
				sts = atom.l = PM_ERR_PMID;
				break;
		    }
		    break;

	    case 1:	/* PDUs received */
		    if (pmidp->item == _TOTAL) {
			/* total */
			atom.ul = 0;
			for (j = 0; j <= PDU_MAX; j++)
			    atom.ul += __pmPDUCntIn[j];
		    }
		    else if (pmidp->item > PDU_MAX+1)
			sts = atom.l = PM_ERR_PMID;
		    else if (pmidp->item < _TOTAL)
			atom.ul = __pmPDUCntIn[pmidp->item];
		    else
			atom.ul = __pmPDUCntIn[pmidp->item-1];
		    break;

	    case 2:	/* PDUs sent */
		    if (pmidp->item == _TOTAL) {
			/* total */
			atom.ul = 0;
			for (j = 0; j <= PDU_MAX; j++)
			    atom.ul += __pmPDUCntOut[j];
		    }
		    else if (pmidp->item > PDU_MAX+1)
			sts = atom.l = PM_ERR_PMID;
		    else if (pmidp->item < _TOTAL)
			atom.ul = __pmPDUCntOut[pmidp->item];
		    else
			atom.ul = __pmPDUCntOut[pmidp->item-1];
		    break;

	    case 3:	/* pmlogger control port, pmcd_host, archive and host */
		    /* find all ports.  localhost => no recursive pmcd access */
		    nports = __pmLogFindPort("localhost", PM_LOG_ALL_PIDS, &lpp);
		    if (nports < 0) {
			sts = nports;
			break;
		    }
		    for (j = numval = 0; j < nports; j++) {
			if (__pmInProfile(logindom, _profile, lpp[j].pid))
			    numval++;
		    }
		    if (numval != 1) {
			/* need a different vset size */
			if (vset_resize(res, i, 1, numval) == -1)
			    return -ENOMEM;
			vset = res->vset[i];
			vset->pmid = pmidlist[i];
		    }
		    for (j = numval = 0; j < nports; j++) {
			if (!__pmInProfile(logindom, _profile, lpp[j].pid))
			    continue;
			vset->vlist[numval].inst = lpp[j].pid;
			switch (pmidp->item) {
			    case 0:		/* pmlogger.port */
				atom.ul = lpp[j].port;
				break;
			    case 1:		/* pmlogger.pmcd_host */
				atom.cp = lpp[j].pmcd_host ?
						lpp[j].pmcd_host : "";
				break;
			    case 2:		/* pmlogger.archive */
				atom.cp = lpp[j].archive ? lpp[j].archive : "";
				break;
			    case 3:		/* pmlogger.host */
                                if (!host)
				    host = hostnameinfo();
                                atom.cp = host;
				break;
			    default:
				sts = atom.l = PM_ERR_PMID;
				break;
			}
			if (sts >= 0)
			    sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
			if (sts < 0)
			    break;
			valfmt = sts;
			numval++;
		    }
		    break;

	    case 4:	/* PMDA metrics */
		for (j = numval = 0; j < nAgents; j++) {
		    if (__pmInProfile(pmdaindom, _profile, agent[j].pmDomainId))
			numval++;
		}
		if (numval != 1) {
		    /* need a different vset size */
		    if (vset_resize(res, i, 1, numval) == -1)
			return -ENOMEM;
		    vset = res->vset[i];
		    vset->pmid = pmidlist[i];
		}
		for (j = numval = 0; j < nAgents; j++) {
		    if (!__pmInProfile(pmdaindom, _profile, agent[j].pmDomainId))
			continue;
		    vset->vlist[numval].inst = agent[j].pmDomainId;
		    switch (pmidp->item) {
			case 0:		/* agent.type */
			    atom.ul = agent[j].ipcType << 1;
			    break;
			case 1:		/* agent.status */
			    if (agent[j].status.notReady)
				atom.l = 1;
			    else if (agent[j].status.connected)
				atom.l = 0;
			    else
				atom.l = agent[j].reason;
			    break;
			default:
			    sts = atom.l = PM_ERR_PMID;
			    break;
		    }
		    if (sts >= 0)
			sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
		    if (sts < 0)
			break;
		    valfmt = sts;
		    numval++;
		}
		if (numval > 0) {
		    pmResult	sortme;
		    sortme.numpmid = 1;
		    sortme.vset[0] = vset;
		    pmSortInstances(&sortme);
		}
		break;


	    case 5:	/* pmie metrics */
		refresh_pmie_indom();
		for (j = numval = 0; j < npmies; j++) {
		    if (__pmInProfile(pmieindom, _profile, pmies[j].pid))
			numval++;
		}
		if (numval != 1) {
		    /* need a different vset size */
		    if (vset_resize(res, i, 1, numval) == -1)
			return -ENOMEM;
		    vset = res->vset[i];
		    vset->pmid = pmidlist[i];
		}
		for (j = numval = 0; j < npmies; ++j) {
		    if (!__pmInProfile(pmieindom, _profile, pmies[j].pid))
			continue;
		    vset->vlist[numval].inst = pmies[j].pid;
		    pmie = (pmiestats_t *)pmies[j].mmap;
		    switch (pmidp->item) {
			case 0:		/* pmie.configfile */
			    atom.cp = pmie->config;
			    break;
			case 1:		/* pmie.logfile */
			    atom.cp = pmie->logfile;
			    break;
			case 2:		/* pmie.pmcd_host */
			    atom.cp = pmie->defaultfqdn;
			    break;
			case 3:		/* pmie.numrules */
			    atom.ul = pmie->numrules;
			    break;
			case 4:		/* pmie.actions */
			    atom.ul = pmie->actions;
			    break;
			case 5:		/* pmie.eval.true */
			    atom.ul = pmie->eval_true;
			    break;
			case 6:		/* pmie.eval.false */
			    atom.ul = pmie->eval_false;
			    break;
			case 7:		/* pmie.eval.unknown */
			    atom.ul = pmie->eval_unknown;
			    break;
			case 8:		/* pmie.eval.expected */
			    atom.f = pmie->eval_expected;
			    break;
			case 9:		/* pmie.eval.actual */
			    atom.ul = pmie->eval_actual;
			    break;
			default:
			    sts = atom.l = PM_ERR_PMID;
			    break;
		    }
		    if (sts >= 0)
			sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
		    if (sts < 0)
			break;
		    valfmt = sts;
		    numval++;
		}
		if (numval > 0) {
		    pmResult	sortme;
		    sortme.numpmid = 1;
		    sortme.vset[0] = vset;
		    pmSortInstances(&sortme);
		}
		break;

	    case 6:	/* client metrics */
		for (j = numval = 0; j < nClients; j++) {
		    if (!client[j].status.connected)
			continue;
		    if (__pmInProfile(clientindom, _profile, client[j].seq))
			numval++;
		}
		if (numval != 1) {
		    /* need a different vset size */
		    if (vset_resize(res, i, 1, numval) == -1)
			return -ENOMEM;
		    vset = res->vset[i];
		    vset->pmid = pmidlist[i];
		}
		for (j = numval = 0; j < nClients; ++j) {
		    int		k;
		    char	ctim[sizeof("Thu Nov 24 18:22:48 1986\n")];
		    if (!client[j].status.connected)
			continue;
		    if (!__pmInProfile(clientindom, _profile, client[j].seq))
			continue;
		    vset->vlist[numval].inst = client[j].seq;
		    switch (pmidp->item) {
			case 0:		/* client.whoami */
			    for (k = 0; k < nwhoamis; k++) {
				if (whoamis[k].seq == client[j].seq) {
				    atom.cp = whoamis[k].value;
				    break;
				}
			    }
			    if (k == nwhoamis)
				/* no id registered, so no value */
				atom.cp = "";
			    break;
			case 1:		/* client.start_date */
			    atom.cp = strcpy(ctim, ctime(&client[j].start));
			    /* trim trailing \n */
			    k = strlen(atom.cp);
			    atom.cp[k-1] = '\0';
			    break;
			default:
			    sts = atom.l = PM_ERR_PMID;
			    break;
		    }
		    if (sts >= 0)
			sts = __pmStuffValue(&atom, &vset->vlist[numval], dp->type);
		    if (sts < 0)
			break;
		    valfmt = sts;
		    numval++;
		}
		if (numval > 0) {
		    pmResult	sortme;
		    sortme.numpmid = 1;
		    sortme.vset[0] = vset;
		    pmSortInstances(&sortme);
		}
		break;

	    case 7:	/* cputime metrics */
		sts = fetch_cputime(pmidp->item, pmda->e_context, &atom);
		break;

	    case 8:	/* feature metrics */
		sts = fetch_feature(pmidp->item, &atom);
		break;
	}

	if (sts == 0 && valfmt == -1 && vset->numval == 1)
	    sts = valfmt = __pmStuffValue(&atom, &vset->vlist[0], dp->type);

	if (sts < 0) {
	    /* failure, encode status in numval, need a different vset size */
	    if (vset_resize(res, i, vset->numval, sts) == -1)
		return -ENOMEM;
	}
	else
	    vset->valfmt = valfmt;
    }
    *resp = res;

    return 0;
}

static int
pmcd_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int		i;

    for (i = 0; i < ndesc; i++) {
	if (desctab[i].pmid == pmid) {
	    *desc = desctab[i];
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

static int
pmcd_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    pmValueSet	*vsp;
    int		sts = 0;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;
	if (pmidp->cluster == 0) {
	    if (pmidp->item == 0) {	/* pmcd.control.debug */
		pmDebug = vsp->vlist[0].value.lval;
	    }
	    else if (pmidp->item == 4) { /* pmcd.control.timeout */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    break;
		}
		if (val != _pmcd_timeout) {
		    _pmcd_timeout = val;
		}
	    }
	    else if (pmidp->item == 8) { /* pmcd.control.register */
		int	j;
		for (j = 0; j < vsp->numval; j++) {
		    if (0 <= vsp->vlist[j].inst && vsp->vlist[j].inst < NUMREG)
			reg[vsp->vlist[j].inst] = vsp->vlist[j].value.lval;
		    else {
			sts = PM_ERR_INST;
			break;
		    }
		}
	    }
	    else if (pmidp->item == 9) { /* pmcd.control.traceconn */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_CONN);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_CONN;
		else {
		    sts = PM_ERR_BADSTORE;
		    break;
		}
	    }
	    else if (pmidp->item == 10) { /* pmcd.control.tracepdu */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_PDU);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_PDU;
		else {
		    sts = PM_ERR_BADSTORE;
		    break;
		}
	    }
	    else if (pmidp->item == 11) { /* pmcd.control.tracebufs */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    break;
		}
		pmcd_init_trace(val);
	    }
	    else if (pmidp->item == 12) { /* pmcd.control.dumptrace */
		pmcd_dump_trace(stderr);
	    }
	    else if (pmidp->item == 13) { /* pmcd.control.dumpconn */
		time_t	now;
		time(&now);
		fprintf(stderr, "\n->Current PMCD clients at %s", ctime(&now));
		ShowClients(stderr);
	    }
	    else if (pmidp->item == 14) { /* pmcd.control.tracenobuf */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_NOBUF);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_NOBUF;
		else {
		    sts = PM_ERR_BADSTORE;
		    break;
		}
	    }
	    else if (pmidp->item == 15) { /* pmcd.control.sighup */
#ifdef HAVE_SIGHUP
		/*
		 * send myself SIGHUP
		 */
		__pmNotifyErr(LOG_INFO, "pmcd reset via pmcd.control.sighup");
		raise(SIGHUP);
#endif
	    }
	    else {
		sts = PM_ERR_PMID;
		break;
	    }
	}
	else if (pmidp->cluster == 6) {
	    if (pmidp->item == 0) {	/* pmcd.client.whoami */
		/*
		 * Expect one value for one instance (PM_IN_NULL)
		 *
		 * Use the value from the pmResult to change the value
		 * for the client[] that matches the current pmcd client.
		 */
		char	*cp = vsp->vlist[0].value.pval->vbuf;
		int	j;
		int	last_free = -1;

		if (vsp->numval != 1 || vsp->vlist[0].inst != PM_IN_NULL) {
		    return PM_ERR_INST;
		}
		for (j = 0; j < nwhoamis; j++) {
		    if (whoamis[j].id == -1) {
			/* slot in whoamis[] not in use */
			last_free = j;
			continue;
		    }
		    if (whoamis[j].id == this_client_id &&
		        whoamis[j].seq == client[this_client_id].seq) {
			/* found the one to replace */
			free(whoamis[j].value);
			break;
		    }
		    if (!client[whoamis[j].id].status.connected ||
		        client[whoamis[j].id].seq != whoamis[j].seq) {
			/* old whoamis[] entry, mark as available for reuse */
			free(whoamis[j].value);
			whoamis[j].id = -1;
			last_free = j;
		    }
		}
		if (j == nwhoamis) {
		    if (last_free != -1) {
			j = last_free;
		    }
		    else {
			nwhoamis++;
			if ((whoamis = (whoami_t *)realloc(whoamis, nwhoamis*sizeof(whoamis[0]))) == NULL) {
			    __pmNoMem("pmstore whoami", nwhoamis*sizeof(whoamis[0]), PM_RECOV_ERR);
			    nwhoamis = 0;
			    return -ENOMEM;
			}
		    }
		    whoamis[j].id = this_client_id;
		    whoamis[j].seq = client[this_client_id].seq;
		}
		whoamis[j].value = strdup(cp);
	    }
	    else {
		sts = PM_ERR_PMID;
		break;
	    }
	}
	else {
	    /* not one of the metrics we are willing to change */
	    sts = PM_ERR_PMID;
	    break;
	}
    }

    return sts;
}

static int
pmcd_attribute(int ctx, int attr, const char *value, int len, pmdaExt *pmda)
{
    if (ctx >= num_ctx)
	grow_ctxtab(ctx);
    if (attr == PCP_ATTR_CONTAINER) {
	if (ctxtab[ctx].container.name)
	    free(ctxtab[ctx].container.name);
	if ((ctxtab[ctx].container.name = strdup(value)) == NULL)
	    return -ENOMEM;
	ctxtab[ctx].container.length = len;
	ctxtab[ctx].container.pid = 0;
    }
    return pmdaAttribute(ctx, attr, value, len, pmda);
}

void
__PMDA_INIT_CALL
pmcd_init(pmdaInterface *dp)
{
    char helppath[MAXPATHLEN];
    int sep = __pmPathSeparator();
 
    snprintf(helppath, sizeof(helppath), "%s%c" "pmcd" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDSO(dp, PMDA_INTERFACE_6, "pmcd", helppath);
    dp->comm.flags |= (PDU_FLAG_AUTH|PDU_FLAG_CONTAINER);

    dp->version.six.profile = pmcd_profile;
    dp->version.six.fetch = pmcd_fetch;
    dp->version.six.desc = pmcd_desc;
    dp->version.six.instance = pmcd_instance;
    dp->version.six.store = pmcd_store;
    dp->version.six.attribute = pmcd_attribute;
    dp->version.six.ext->e_endCallBack = end_context;

    init_tables(dp->domain);
    rootfd = pmdaRootConnect(NULL);
    pmdaInit(dp, NULL, 0, NULL, 0);
}
