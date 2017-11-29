/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"
/* need pmda.h and libpcp_pmda for the pmdaCache* routines */
#include "pmda.h"

#include <ctype.h>

/*
 * Fault Injection - run-time control structure
 */
typedef struct {
    int		ntrip;
    int		op;
    int		thres;
    int		nfault;
} control_t;

#define PM_FAULT_LT	0
#define PM_FAULT_LE	1
#define PM_FAULT_EQ	2
#define PM_FAULT_GE	3
#define PM_FAULT_GT	4
#define PM_FAULT_NE	5
#define PM_FAULT_MOD	6

#ifdef PM_FAULT_INJECTION

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	fault_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*fault_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == fault_lock
 */
int
__pmIsFaultLock(void *lock)
{
    return lock == (void *)&fault_lock;
}
#endif

int	__pmFault_arm;

#define FAULT_INDOM pmInDom_build(DYNAMIC_PMID, 1024)

static void
__pmFaultAtExit(void)
{
    __pmFaultSummary(stderr);
}

void
__pmFaultInject(const char *ident, int class)
{
    static int first = 1;
    int		sts;
    control_t	*cp;

    PM_LOCK(fault_lock);
    if (first) {
	char	*fname;
	first = 0;
	PM_LOCK(__pmLock_extcall);
	fname = getenv("PM_FAULT_CONTROL");		/* THREADSAFE */
	if (fname != NULL)
	    fname = strdup(fname);
	PM_UNLOCK(__pmLock_extcall);
	if (fname != NULL) {
	    FILE	*f;
	    if ((f = fopen(fname, "r")) == NULL) {
		char	msgbuf[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmFaultInject: cannot open \"%s\": %s\n", fname, pmErrStr_r(-errno, msgbuf, sizeof(msgbuf)));
	    }
	    else {
		char	line[128];
		int	lineno = 0;
		/*
		 * control line format
		 * ident	- start of line to first white space
		 * guard	- optional, consists of <op> and threshold
		 * 		  <op> is one of <, <=, ==, >=, >=, != or %
		 *		  threshold is an integer value ...
		 *		  fault will be injected when
		 *		  tripcount <op> threshold == 1
		 * default guard is ">0", i.e. fault on every trip
		 * leading # => comment
		 */
		PM_LOCK(__pmLock_extcall);
		pmdaCacheOp(FAULT_INDOM, PMDA_CACHE_CULL);	/* THREADSAFE */
		PM_UNLOCK(__pmLock_extcall);
		while (fgets(line, sizeof(line), f) != NULL) {
		    char	*lp = line;
		    char	*sp;
		    char	*ep;
		    int		op;
		    int		thres;
		    lineno++;
		    while (*lp) {
			if (*lp == '\n') {
			    *lp = '\0';
			    break;
			}
			lp++;
		    }
		    lp = line;
		    while (*lp && isspace((int)*lp)) lp++;
		    /* comment? */
		    if (*lp == '#')
			continue;
		    sp = lp;
		    while (*lp && !isspace((int)*lp)) lp++;
		    /* empty line? */
		    if (lp == sp)
			continue;
		    ep = lp;
		    while (*lp && isspace((int)*lp)) lp++;
		    if (*lp == '\0') {
			op = PM_FAULT_GT;
			thres = 0;
		    }
		    else {
			if (strncmp(lp, "<=", 2) == 0) {
			    op = PM_FAULT_LE;
			    lp +=2;
			}
			else if (strncmp(lp, ">=", 2) == 0) {
			    op = PM_FAULT_GE;
			    lp +=2;
			}
			else if (strncmp(lp, "!=", 2) == 0) {
			    op = PM_FAULT_NE;
			    lp +=2;
			}
			else if (strncmp(lp, "==", 2) == 0) {
			    op = PM_FAULT_EQ;
			    lp +=2;
			}
			else if (*lp == '<') {
			    op = PM_FAULT_LT;
			    lp++;
			}
			else if (*lp == '>') {
			    op = PM_FAULT_GT;
			    lp++;
			}
			else if (*lp == '%') {
			    op = PM_FAULT_MOD;
			    lp++;
			}
			else {
			    fprintf(stderr, "Ignoring: %s[%d]: illegal operator: %s\n", fname, lineno, line);
			    continue;
			}
		    }
		    while (*lp && isspace((int)*lp)) lp++;
		    thres = (int)strtol(lp, &lp, 10);
		    while (*lp && isspace((int)*lp)) lp++;
		    if (*lp != '\0') {
			fprintf(stderr, "Ignoring: %s[%d]: non-numeric threshold: %s\n", fname, lineno, line);
			continue;
		    }
		    cp = (control_t *)malloc(sizeof(control_t));
		    if (cp == NULL) {
			char	errmsg[PM_MAXERRMSGLEN];
			fprintf(stderr, "__pmFaultInject: malloc failed: %s\n", pmErrStr_r(-errno, errmsg, sizeof(errmsg)));
			break;
		    }
		    *ep = '\0';
		    cp->ntrip = cp->nfault = 0;
		    cp->op = op;
		    cp->thres = thres;
		    PM_LOCK(__pmLock_extcall);
		    sts = pmdaCacheStore(FAULT_INDOM, PMDA_CACHE_ADD, sp, cp);	/* THREADSAFE */
		    PM_UNLOCK(__pmLock_extcall);
		    if (sts < 0) {
			char	errmsg[PM_MAXERRMSGLEN];
			fprintf(stderr, "%s[%d]: %s\n", fname, lineno, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		    }
		}
		fclose(f);
	    }
	    free(fname);
	}
#ifdef HAVE_ATEXIT
	if (pmDebugOptions.fault)
	    atexit(__pmFaultAtExit);
#endif
    }
    PM_UNLOCK(fault_lock);

    PM_LOCK(__pmLock_extcall);
    sts = pmdaCacheLookupName(FAULT_INDOM, ident, NULL, (void **)&cp);	/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);
    if (sts == PMDA_CACHE_ACTIVE) {
	cp->ntrip++;
	__pmFault_arm = 0;
	switch (cp->op) {
	    case PM_FAULT_LT:
	    	__pmFault_arm = (cp->ntrip < cp->thres) ? class : 0;
		break;
	    case PM_FAULT_LE:
	    	__pmFault_arm = (cp->ntrip <= cp->thres) ? class : 0;
		break;
	    case PM_FAULT_EQ:
	    	__pmFault_arm = (cp->ntrip == cp->thres) ? class : 0;
		break;
	    case PM_FAULT_GE:
	    	__pmFault_arm = (cp->ntrip >= cp->thres) ? class : 0;
		break;
	    case PM_FAULT_GT:
	    	__pmFault_arm = (cp->ntrip > cp->thres) ? class : 0;
		break;
	    case PM_FAULT_NE:
	    	__pmFault_arm = (cp->ntrip != cp->thres) ? class : 0;
		break;
	    case PM_FAULT_MOD:
	    	__pmFault_arm = ((cp->ntrip % cp->thres) == 1) ? class : 0;
		break;
	}
	if (__pmFault_arm != 0)
	    cp->nfault++;
	if (pmDebugOptions.fault)
	    fprintf(stderr, "__pmFaultInject(%s) ntrip=%d %s\n", ident, cp->ntrip, __pmFault_arm == 0 ? "SKIP" : "INJECT");
    }
    else if (sts == PM_ERR_INST) {
	/*
	 * expected for injection points that are compiled in the code
	 * but not registered via the control file
	 */
	if (pmDebugOptions.fault)
	    fprintf(stderr, "__pmFaultInject(%s) not registered\n", ident);
	 ;
    }
    else {
	/* oops, this is serious */
	char	errmsg[PM_MAXERRMSGLEN];
	fprintf(stderr, "__pmFaultInject(%s): %s\n", ident, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    }

}

void
__pmFaultSummary(FILE *f)
{
    int		inst;
    char	*ident;
    control_t	*cp;
    int		sts;
    static char	*opstr[] = { "<", "<=", "==", ">=", ">", "!=", "%" };

    fprintf(f, "=== Fault Injection Summary Report ===\n");

    PM_LOCK(__pmLock_extcall);
    pmdaCacheOp(FAULT_INDOM, PMDA_CACHE_WALK_REWIND);		/* THREADSAFE */
    while ((inst = pmdaCacheOp(FAULT_INDOM, PMDA_CACHE_WALK_NEXT)) != -1) {	/* THREADSAFE */
	sts = pmdaCacheLookup(FAULT_INDOM, inst, &ident, (void **)&cp);	/* THREADSAFE */
	if (sts < 0) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(f, "pmdaCacheLookup(%s, %d, %s, ..): %s\n", pmInDomStr_r(FAULT_INDOM, strbuf, sizeof(strbuf)), inst, ident, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
	else
	    fprintf(f, "%s: guard trip%s%d, %d trips, %d faults\n", ident, opstr[cp->op], cp->thres, cp->ntrip, cp->nfault);

    }
    PM_UNLOCK(__pmLock_extcall);
}

void
*__pmFault_malloc(size_t size)
{
    if (__pmFault_arm == PM_FAULT_ALLOC) {
	__pmFault_arm = 0;
	errno = ENOMEM;
	return NULL;
    }
    else 
#undef malloc
	return malloc(size);
}

void
*__pmFault_calloc(size_t nmemb, size_t size)
{
    if (__pmFault_arm == PM_FAULT_ALLOC) {
	__pmFault_arm = 0;
	errno = ENOMEM;
	return NULL;
    }
    else 
#undef calloc
	return calloc(nmemb, size);
}

void
*__pmFault_realloc(void *ptr, size_t size)
{
    if (__pmFault_arm == PM_FAULT_ALLOC) {
	__pmFault_arm = 0;
	errno = ENOMEM;
	return NULL;
    }
    else 
#undef realloc
	return realloc(ptr, size);
}

char *
__pmFault_strdup(const char *s)
{
    if (__pmFault_arm == PM_FAULT_ALLOC) {
	__pmFault_arm = 0;
	errno = ENOMEM;
	return NULL;
    }
    else
#undef strdup
	return strdup(s);
}

#else
void
__pmFaultInject(const char *ident, int class)
{
    fprintf(stderr, "__pmFaultInject() called but library not compiled with -DPM_FAULT_INJECTION\n");
    exit(1);
}

void
__pmFaultSummary(FILE *f)
{
    fprintf(f, "__pmFaultSummary() called but library not compiled with -DPM_FAULT_INJECTION\n");
    exit(1);

}
#endif
