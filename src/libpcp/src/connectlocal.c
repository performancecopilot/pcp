/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 *
 * Thread-safe notes
 *
 * atexit_installed is protected by the __pmLock_libpcp mutex.
 *
 * __pmSpecLocalPMDA() uses buffer[], but this routine is only called
 * from main() in single-threaded apps like pminfo, pmprobe, pmval
 * and pmevent ... so we can ignore any multi-threading issues,
 * especially as buffer[] is only used on an error handling code path.
 *
 * dsotab[] and numdso are obviously of interest via calls to
 * __pmLookupDSO(), EndLocalContext(), __pmConnectLocal() or
 * __pmLocalPMDA().
 *
 * Within libpcp, __pmLookupDSO() is called _only_ for PM_CONTEXT_LOCAL
 * and it is not called from outside libpcp.  Local contexts are only
 * supported for single-threaded applications in the scope
 * PM_SCOPE_DSO_PMDA that is enforced in pmNewContext.  Multi-threaded
 * applications are not supported for local contexts, so we do not need
 * additional concurrency control for __pmLookupDSO().
 *
 * The same arguments apply to EndLocalContext() and __pmConnectLocal().
 *
 * __pmLocalPMDA() is a mixed bag, sharing some of the justification from
 * __pmSpecLocalPMDA() and some from __pmConnectLocal().
 *
 * Because __pmConnectLocal() is not going to be used in a multi-threaded
 * environment, the call to the thread-unsafe dlerror() is OK.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <ctype.h>
#include <sys/stat.h>

static __pmDSO *dsotab;
static int	numdso = -1;

static int
build_dsotab(void)
{
    /*
     * parse pmcd's config file extracting details from dso lines
     *
     * very little syntactic checking here ... pmcd(1) does that job
     * nicely and even if we get confused, the worst thing that happens
     * is we don't include one or more of the DSO PMDAs in dsotab[]
     *
     * lines for DSO PMDAs generally look like this ...
     * Name	Domain	Type	Init Routine	Path
     * mmv	70	dso	mmv_init	/var/lib/pcp/pmdas/mmv/pmda_mmv.so 
     *
     */
    char	configFileName[MAXPATHLEN];
    char	pathbuf[MAXPATHLEN];
    FILE	*configFile;
    char	*config;
    char	*pmdas;
    char	*p;
    char	*q;
    struct stat	sbuf;
    int		lineno = 1;
    int		domain;
    char	*init;
    char	*name;
    char	peekc;

    numdso = 0;
    dsotab = NULL;

    if ((pmdas = pmGetOptionalConfig("PCP_PMDAS_DIR")) == NULL)
	return PM_ERR_GENERIC;
    if ((config = pmGetOptionalConfig("PCP_PMCDCONF_PATH")) == NULL)
	return PM_ERR_GENERIC;
    strncpy(configFileName, config, sizeof(configFileName));
    configFileName[sizeof(configFileName) - 1] = '\0';
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "build_dsotab: parsing %s\n", configFileName);
    }
#endif
    if (stat(configFileName, &sbuf) < 0) {
	return -oserror();
    }
    configFile = fopen(configFileName, "r");
    if (configFile == NULL) {
	return -oserror();
    }
    if ((config = malloc(sbuf.st_size+1)) == NULL) {
	__pmNoMem("build_dsotbl:", sbuf.st_size+1, PM_RECOV_ERR);
	fclose(configFile);
	return -oserror();
    }
    if (fread(config, 1, sbuf.st_size, configFile) != sbuf.st_size) {
	fclose(configFile);
	free(config);
	return -oserror();
    }
    config[sbuf.st_size] = '\0';

    p = config;
    while (*p != '\0') {
	/* each time through here we're at the start of a new line */
	if (*p == '#')
	    goto eatline;
	if (strncmp(p, "pmcd", 4) == 0) {
	    /*
	     * the pmcd PMDA is an exception ... it makes reference to
	     * symbols in pmcd, and only makes sense when attached to the
	     * pmcd process, so we skip this one
	     */
	    goto eatline;
	}
	if (strncmp(p, "linux", 5) == 0) {
	    /*
	     * the Linux PMDA is an exception now too ... we run it as root
	     * (daemon) but we still want to make the DSO available for any
	     * local context users.  We add this explicitly below.
	     */
	    domain = 60;
	    init = "linux_init";
	    snprintf(pathbuf, sizeof(pathbuf), "%s/linux/pmda_linux.so", pmdas);
	    name = pathbuf;
	    peekc = *p;
	    goto dsoload;
	}
	/* skip the PMDA's name */
	while (*p != '\0' && *p != '\n' && !isspace((int)*p))
	    p++;
	while (*p != '\0' && *p != '\n' && isspace((int)*p))
	    p++;
	/* extract domain number */
	domain = (int)strtol(p, &q, 10);
	p = q;
	while (*p != '\0' && *p != '\n' && isspace((int)*p))
	    p++;
	/* only interested if the type is "dso" */
	if (strncmp(p, "dso", 3) != 0)
	    goto eatline;
	p += 3;
	while (*p != '\0' && *p != '\n' && isspace((int)*p))
	    p++;
	/* up to the init routine name */
	init = p;
	while (*p != '\0' && *p != '\n' && !isspace((int)*p))
	    p++;
	*p = '\0';
	p++;
	while (*p != '\0' && *p != '\n' && isspace((int)*p))
	    p++;
	/* up to the dso pathname */
	name = p;
	while (*p != '\0' && *p != '\n' && !isspace((int)*p))
	    p++;
	peekc = *p;
	*p = '\0';
dsoload:
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "[%d] domain=%d, name=%s, init=%s\n", lineno, domain, name, init);
	}
#endif
	/*
	 * a little bit recursive if we got here via __pmLocalPMDA(),
	 * but numdso has been set correctly, so this is OK
	 */
	__pmLocalPMDA(PM_LOCAL_ADD, domain, name, init);
	*p = peekc;

eatline:
	while (*p != '\0' && *p != '\n')
	    p++;
	if (*p == '\n') {
	    lineno++;
	    p++;
	}
    }

    fclose(configFile);
    free(config);
    return 0;
}

static int
build_dsoattrs(pmdaInterface *dispatch, __pmHashCtl *attrs)
{
    __pmHashNode *node;
    char name[32];
    char *namep;
    int sts = 0;

#ifdef HAVE_GETUID
    snprintf(name, sizeof(name), "%u", getuid());
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
	__pmHashAdd(PCP_ATTR_USERID, namep, attrs);
#endif

#ifdef HAVE_GETGID
    snprintf(name, sizeof(name), "%u", getgid());
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
	__pmHashAdd(PCP_ATTR_GROUPID, namep, attrs);
#endif

    snprintf(name, sizeof(name), "%u", getpid());
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
	__pmHashAdd(PCP_ATTR_PROCESSID, namep, attrs);

    if (dispatch->version.six.attribute != NULL) {
	for (node = __pmHashWalk(attrs, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(attrs, PM_HASH_WALK_NEXT)) {
	    if ((sts = dispatch->version.six.attribute(
				0, node->key, node->data,
				node->data ? strlen(node->data)+1 : 0,
				dispatch->version.six.ext)) < 0)
		break;
	}
    }
    return sts;
}

#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

/*
 * As of PCP version 2.1, we're no longer searching for DSO's;
 * pmcd's config file should have full paths to each of 'em.
 */
const char *
__pmFindPMDA(const char *name)
{
    return (access(name, F_OK) == 0) ? name : NULL;
}

__pmDSO *
__pmLookupDSO(int domain)
{
    int		i;
    for (i = 0; i < numdso; i++) {
	if (dsotab[i].domain == domain && dsotab[i].handle != NULL)
	    return &dsotab[i];
    }
    return NULL;
}

static void
EndLocalContext(void)
{
    int		i;
    __pmDSO	*dp;
    int		ctx = pmWhichContext();

    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	/*
	 * Local context requires single-threaded applications
	 * ... should not really get here, so do nothing!
	 */
	return;

    for (i = 0; i < numdso; i++) {
	dp = &dsotab[i];
	if (dp->domain != -1 &&
	    dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5 &&
	    dp->dispatch.version.four.ext->e_endCallBack != NULL) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		fprintf(stderr, "NotifyEndLocalContext: DSO PMDA %s (%d) notified of context %d close\n", 
		    dp->name, dp->domain, ctx);
	    }
#endif
	    (*(dp->dispatch.version.four.ext->e_endCallBack))(ctx);
	}
    }
}

/*
 * Note order is significant here.  Also EndLocalContext can be
 * called from atexit handler (if previously registered), but if
 * caller invokes shutdown beforehand, thats OK; EndLocalContext
 * will safely do nothing on the second call.
 */
int
__pmShutdownLocal(void)
{
    /* Call through to any PMDA termination callbacks */
    EndLocalContext();

    /* dso close and free up local memory allocations */
    return __pmLocalPMDA(PM_LOCAL_CLEAR, 0, NULL, NULL);
}

int
__pmConnectLocal(__pmHashCtl *attrs)
{
    int			i;
    __pmDSO		*dp;
    char		pathbuf[MAXPATHLEN];
    char		*pmdas;
    const char		*path;
#if defined(HAVE_DLOPEN)
    unsigned int	challenge;
    void		(*initp)(pmdaInterface *);
#ifdef HAVE_ATEXIT
    static int		atexit_installed = 0;
#endif
#endif

    if ((pmdas = pmGetOptionalConfig("PCP_PMDAS_DIR")) == NULL)
	return PM_ERR_GENERIC;

    if (numdso == -1) {
	int	sts;
	sts = build_dsotab();
	if (sts < 0) return sts;
    }

    for (i = 0; i < numdso; i++) {
	dp = &dsotab[i];
	if (dp->domain == -1 || dp->handle != NULL)
	    continue;
	/*
	 * __pmLocalPMDA() means the path to the DSO may be something
	 * other than relative to $PCP_PMDAS_DIR ... need to try both
	 * options and also with and without DSO_SUFFIX (so, dll, etc)
	 */
	snprintf(pathbuf, sizeof(pathbuf), "%s%c%s",
		 pmdas, __pmPathSeparator(), dp->name);
	if ((path = __pmFindPMDA(pathbuf)) == NULL) {
	    snprintf(pathbuf, sizeof(pathbuf), "%s%c%s.%s",
		 pmdas, __pmPathSeparator(), dp->name, DSO_SUFFIX);
	    if ((path = __pmFindPMDA(pathbuf)) == NULL) {
		if ((path = __pmFindPMDA(dp->name)) == NULL) {
		    snprintf(pathbuf, sizeof(pathbuf), "%s.%s", dp->name, DSO_SUFFIX);
		    if ((path = __pmFindPMDA(pathbuf)) == NULL) {
			pmprintf("__pmConnectLocal: Warning: cannot find DSO at \"%s\" or \"%s\"\n", 
			     pathbuf, dp->name);
			pmflush();
			dp->domain = -1;
			dp->handle = NULL;
			continue;
		    }
		}
	    }
	}
#if defined(HAVE_DLOPEN)
	dp->handle = dlopen(path, RTLD_NOW);
	if (dp->handle == NULL) {
	    pmprintf("__pmConnectLocal: Warning: error attaching DSO "
		     "\"%s\"\n%s\n\n", path, dlerror());
	    pmflush();
	    dp->domain = -1;
	}
#else	/* ! HAVE_DLOPEN */
	dp->handle = NULL;
	pmprintf("__pmConnectLocal: Warning: error attaching DSO \"%s\"\n",
		 path);
	pmprintf("No dynamic DSO/DLL support on this platform\n\n");
	pmflush();
	dp->domain = -1;
#endif

	if (dp->handle == NULL)
	    continue;

#if defined(HAVE_DLOPEN)
	/*
	 * rest of this only makes sense if the dlopen() worked
	 */
	if (dp->init == NULL)
	    initp = NULL;
	else
	    initp = (void (*)(pmdaInterface *))dlsym(dp->handle, dp->init);
	if (initp == NULL) {
	    pmprintf("__pmConnectLocal: Warning: couldn't find init function "
		     "\"%s\" in DSO \"%s\"\n", dp->init, path);
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	    continue;
	}

	/*
	 * Pass in the expected domain id.
	 * The PMDA initialization routine can (a) ignore it, (b) check it
	 * is the expected value, or (c) self-adapt.
	 */
	dp->dispatch.domain = dp->domain;

	/*
	 * the PMDA interface / PMAPI version discovery as a "challenge" ...
	 * for pmda_interface it is all the bits being set,
	 * for pmapi_version it is the complement of the one you are using now
	 */
	challenge = 0xff;
	dp->dispatch.comm.pmda_interface = challenge;
	dp->dispatch.comm.pmapi_version = ~PMAPI_VERSION;
	dp->dispatch.comm.flags = 0;
	dp->dispatch.status = 0;

	(*initp)(&dp->dispatch);

	if (dp->dispatch.status != 0) {
	    /* initialization failed for some reason */
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("__pmConnectLocal: Warning: initialization "
		     "routine \"%s\" failed in DSO \"%s\": %s\n", 
		     dp->init, path, pmErrStr_r(dp->dispatch.status, errmsg, sizeof(errmsg)));
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	}
	else {
	    if (dp->dispatch.comm.pmda_interface < PMDA_INTERFACE_2 ||
		dp->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
		pmprintf("__pmConnectLocal: Error: Unknown PMDA interface "
			 "version %d in \"%s\" DSO\n", 
			 dp->dispatch.comm.pmda_interface, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }
	    else if (dp->dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
		pmprintf("__pmConnectLocal: Error: Unknown PMAPI version %d "
			 "in \"%s\" DSO\n",
			 dp->dispatch.comm.pmapi_version, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }
	    else if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_6 &&
		    (dp->dispatch.comm.flags & PDU_FLAG_AUTH) != 0) {
		/* Agent wants to know about connection attributes */
		build_dsoattrs(&dp->dispatch, attrs);
	    }
	}
#ifdef HAVE_ATEXIT
	PM_INIT_LOCKS();
	PM_LOCK(__pmLock_libpcp);
	if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5 &&
	    atexit_installed == 0) {
	    /* install end of local context handler */
	    atexit(EndLocalContext);
	    atexit_installed = 1;
	}
	PM_UNLOCK(__pmLock_libpcp);
#endif
#endif	/* HAVE_DLOPEN */
    }

    return 0;
}

int
__pmLocalPMDA(int op, int domain, const char *name, const char *init)
{
    int		sts = 0;
    int		i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmLocalPMDA(op=");
	if (op == PM_LOCAL_ADD) fprintf(stderr, "ADD");
	else if (op == PM_LOCAL_DEL) fprintf(stderr, "DEL");
	else if (op == PM_LOCAL_CLEAR) fprintf(stderr, "CLEAR");
	else fprintf(stderr, "%d ???", op);
	fprintf(stderr, ", domain=%d, name=%s, init=%s)\n", domain, name, init);
    }
#endif

    if (numdso == -1) {
	if (op != PM_LOCAL_CLEAR)
	    if ((sts = build_dsotab()) < 0)
		return sts;
    }

    switch (op) {
	case PM_LOCAL_ADD:
	    if ((dsotab = (__pmDSO *)realloc(dsotab, (numdso+1)*sizeof(__pmDSO))) == NULL) {
		__pmNoMem("__pmLocalPMDA realloc", (numdso+1)*sizeof(__pmDSO), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    dsotab[numdso].domain = domain;
	    if (name == NULL) {
		/* odd, will fail later at dlopen */
		dsotab[numdso].name = NULL;
	    }
	    else {
		if ((dsotab[numdso].name = strdup(name)) == NULL) {
		    sts = -oserror();
		    __pmNoMem("__pmLocalPMDA name", strlen(name)+1, PM_RECOV_ERR);
		    return sts;
		}
	    }
	    if (init == NULL) {
		/* odd, will fail later at initialization call */
		dsotab[numdso].init = NULL;
	    }
	    else {
		if ((dsotab[numdso].init = strdup(init)) == NULL) {
		    sts = -oserror();
		    __pmNoMem("__pmLocalPMDA init", strlen(init)+1, PM_RECOV_ERR);
		    return sts;
		}
	    }
	    dsotab[numdso].handle = NULL;
	    numdso++;
	    break;

	case PM_LOCAL_DEL:
	    sts = PM_ERR_INDOM;
	    for (i = 0; i < numdso; i++) {
		if ((domain != -1 && dsotab[i].domain == domain) ||
		    (name != NULL && strcmp(dsotab[i].name, name) == 0)) {
		    if (dsotab[i].handle) {
			dlclose(dsotab[i].handle);
			dsotab[i].handle = NULL;
		    }
		    dsotab[i].domain = -1;
		    sts = 0;
		}
	    }
	    break;

	case PM_LOCAL_CLEAR:
	    for (i = 0; i < numdso; i++) {
		free(dsotab[i].name);
	    	free(dsotab[i].init);
		if (dsotab[i].handle)
		    dlclose(dsotab[i].handle);
	    }
	    free(dsotab);
	    dsotab = NULL;
	    numdso = 0;
	    break;

	default:
	    sts = PM_ERR_CONV;
	    break;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	if (sts != 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLocalPMDA -> %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
	fprintf(stderr, "Local Context PMDA Table");
	if (numdso == 0)
	    fprintf(stderr, " ... empty");
	fputc('\n', stderr);
	for (i = 0; i < numdso; i++) {
	    fprintf(stderr, PRINTF_P_PFX "%p [%d] domain=%d name=%s init=%s handle=" PRINTF_P_PFX "%p\n",
		&dsotab[i], i, dsotab[i].domain, dsotab[i].name, dsotab[i].init, dsotab[i].handle);
	}
    }
#endif

    return sts;
}

/*
 * Parse a command line string that encodes arguments to __pmLocalPMDA(),
 * then call __pmLocalPMDA().
 *
 * The syntax for the string is 1 to 4 fields separated by colons:
 * 	- op ("add" for add, "del" for delete, "clear" for clear)
 *	- domain (PMDA's PMD)
 *	- path (path to DSO PMDA)
 *	- init (name of DSO's initialization routine)
 */
char *
__pmSpecLocalPMDA(const char *spec)
{
    int		op;
    int		domain = -1;
    char	*name = NULL;
    char	*init = NULL;
    int		sts;
    char	*arg;
    char	*sbuf;
    char	*ap;

    if ((arg = sbuf = strdup(spec)) == NULL) {
	sts = -oserror();
	__pmNoMem("__pmSpecLocalPMDA dup spec", strlen(spec)+1, PM_RECOV_ERR);
	return "strdup failed";
    }
    if (strncmp(arg, "add", 3) == 0) {
	op = PM_LOCAL_ADD;
	ap = &arg[3];
    }
    else if (strncmp(arg, "del", 3) == 0) {
	op = PM_LOCAL_DEL;
	ap = &arg[3];
    }
    else if (strncmp(arg, "clear", 5) == 0) {
	op = PM_LOCAL_CLEAR;
	ap = &arg[5];
	if (*ap == '\0')
	    goto doit;
	else {
	    free(sbuf);
	    return "unexpected text after clear op in spec";
	}
    }
    else {
	free(sbuf);
	return "bad op in spec";
    }

    if (*ap != ',') {
	free(sbuf);
	return "expected , after op in spec";
    }
    /* ap-> , after add or del */
    arg = ++ap;
    if (*ap == '\0') {
	free(sbuf);
	return "missing domain in spec";
    }
    else if (*ap != ',') {
	/* ap-> domain */
	domain = (int)strtol(arg, &ap, 10);
	if ((*ap != ',' && *ap != '\0') || domain < 0 || domain > 510) {
	    free(sbuf);
	    return "bad domain in spec";
	}
	if (*ap != '\0')
	    /* skip , after domain */
	    ap++;
    }
    else {
	if (op != PM_LOCAL_DEL) {
	    /* found ,, where ,domain, expected */
	    free(sbuf);
	    return "missing domain in spec";
	}
	ap++;
    }
    /* ap -> char after , following domain */
    if (*ap == ',') {
	/* no path, could have init (not useful but possible!) */
	ap++;
	if (*ap != '\0')
	    init = ap;
    }
    else if (*ap != '\0') {
	/* have path and possibly init */
	name = ap;
	while (*ap != ',' && *ap != '\0')
	    ap++;
	if (*ap == ',') {
	    *ap++ = '\0';
	    if (*ap != '\0')
		init = ap;
	    else {
		if (op != PM_LOCAL_DEL) {
		    /* found end of string where init-routine expected */
		    free(sbuf);
		    return "missing init-routine in spec";
		}
	    }
	}
	else {
	    if (op != PM_LOCAL_DEL) {
		/* found end of string where init-routine expected */
		free(sbuf);
		return "missing init-routine in spec";
	    }
	}
    }
    else {
	if (op != PM_LOCAL_DEL) {
	    /* found end of string where path expected */
	    free(sbuf);
	    return "missing dso-path in spec";
	}
    }

    if (domain == -1 && name == NULL) {
	free(sbuf);
	return "missing domain and dso-path in spec";
    }

doit:
    sts = __pmLocalPMDA(op, domain, name, init);
    if (sts < 0) {
	/* see thread-safe note at the head of this file */
	static char buffer[256];
	char	errmsg[PM_MAXERRMSGLEN];
	snprintf(buffer, sizeof(buffer), "__pmLocalPMDA: %s", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	free(sbuf);
	return buffer;
    }

    free(sbuf);
    return NULL;
}
