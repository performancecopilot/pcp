/*
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#include "dsotbl.h"
#include <ctype.h>

static __pmDSO *dsotab = dsotab_i;

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

int
__pmConnectLocal(void)
{
    int			i;
    __pmDSO		*dp;
    char		pathbuf[MAXPATHLEN];
    const char		*path;
#if defined(HAVE_DLOPEN)
    unsigned int	challenge;
    void		(*initp)(pmdaInterface *);
#endif

    for (i = 0; i < numdso; i++) {
	dp = &dsotab[i];
	if (dp->domain == -1 || dp->handle != NULL)
	    continue;
	if (dp->domain == SAMPLE_DSO) {
	    /*
	     * only attach sample pmda dso if env var PCP_LITE_SAMPLE
	     * or PMDA_LOCAL_SAMPLE is set
	     */
	    if (getenv("PCP_LITE_SAMPLE") == NULL &&
		getenv("PMDA_LOCAL_SAMPLE") == NULL) {
		/* no sample pmda */
		dp->domain = -1;
		continue;
	    }
	}
#if defined(PROC_DSO)
	/*
	 * For Linux (and perhaps anything other than IRIX), the proc
	 * PMDA is part of the OS PMDA, so this one cannot be optional
	 * ... the makefile will ensure dsotbl.h is set up correctly
	 * and PROC_DSO will or will not be defined as required
	 */
	if (dp->domain == PROC_DSO) {
	    /*
	     * only attach proc pmda dso if env var PMDA_LOCAL_PROC
	     * is set
	     */
	    if (getenv("PMDA_LOCAL_PROC") == NULL) {
		/* no proc pmda */
		dp->domain = -1;
		continue;
	    }
	}
#endif

	/*
	 * __pmLocalPMDA() means the path to the DSO may be something
	 * other than relative to $PCP_PMDAS_DIR ... need to try both
	 * options and also with and without DSO_SUFFIX (so, dll, etc)
	 */
	snprintf(pathbuf, sizeof(pathbuf), "%s%c%s",
		 pmGetConfig("PCP_PMDAS_DIR"), __pmPathSeparator(), dp->name);
	if ((path = __pmFindPMDA(pathbuf)) == NULL) {
	    snprintf(pathbuf, sizeof(pathbuf), "%s%c%s.%s",
		 pmGetConfig("PCP_PMDAS_DIR"), __pmPathSeparator(), dp->name, DSO_SUFFIX);
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
	    pmprintf("__pmConnectLocal: Warning: initialization "
		     "routine \"%s\" failed in DSO \"%s\": %s\n", 
		     dp->init, path, pmErrStr(dp->dispatch.status));
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	}
	else {
	    if (dp->dispatch.comm.pmda_interface == challenge) {
		/*
		 * DSO did not change pmda_interface, assume PMAPI version 1
		 * from PCP 1.x and PMDA_INTERFACE_1
		 */
		dp->dispatch.comm.pmda_interface = PMDA_INTERFACE_1;
		dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
	    }
	    else {
		/*
		 * gets a bit tricky ...
		 * interface_version (8-bits) used to be version (4-bits),
		 * so it is possible that only the bottom 4 bits were
		 * changed and in this case the PMAPI version is 1 for
		 * PCP 1.x
		 */
		if ((dp->dispatch.comm.pmda_interface & 0xf0) == (challenge & 0xf0)) {
		    dp->dispatch.comm.pmda_interface &= 0x0f;
		    dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
		}
	    }

	    if (dp->dispatch.comm.pmda_interface < PMDA_INTERFACE_1 ||
		dp->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
		pmprintf("__pmConnectLocal: Error: Unknown PMDA interface "
			 "version %d in \"%s\" DSO\n", 
			 dp->dispatch.comm.pmda_interface, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }

	    if (dp->dispatch.comm.pmapi_version != PMAPI_VERSION_1 &&
		dp->dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
		pmprintf("__pmConnectLocal: Error: Unknown PMAPI version %d "
			 "in \"%s\" DSO\n",
			 dp->dispatch.comm.pmapi_version, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }
	}
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

    if (dsotab == dsotab_i) {
	/*
	 * first call, so promote dsotab[] to be a malloc'd copy of
	 * dsotab_i[] so we can realloc from here on ...
	 */
	if ((dsotab = (__pmDSO *)malloc(numdso*sizeof(__pmDSO))) == NULL) {
	    sts = -errno;
	    __pmNoMem("__pmLocalPMDA malloc", numdso*sizeof(__pmDSO), PM_RECOV_ERR);
	    dsotab = dsotab_i;
	    return sts;
	}
	memcpy((void *)dsotab, (void *)dsotab_i, numdso*sizeof(__pmDSO));
	/*
	 * need to strdup name and init so PM_LOCAL_CLEAR works for
	 * all entries
	 */
	for (i = 0; i < numdso; i++) {
	    if ((dsotab[i].name = strdup(dsotab_i[i].name)) == NULL) {
		sts = -errno;
		__pmNoMem("__pmLocalPMDA init name", strlen(dsotab_i[i].name)+1, PM_RECOV_ERR);
		i--;
		while (i >= 0) {
		    free(dsotab[i].name);
		    free(dsotab[i].init);
		    i--;
		}
		free(dsotab);
		dsotab = dsotab_i;
		return sts;
	    }
	    if ((dsotab[i].init = strdup(dsotab_i[i].init)) == NULL) {
		sts = -errno;
		__pmNoMem("__pmLocalPMDA init", strlen(dsotab_i[i].init)+1, PM_RECOV_ERR);
		free(dsotab[i].name);
		i--;
		while (i >= 0) {
		    free(dsotab[i].name);
		    free(dsotab[i].init);
		    i--;
		}
		free(dsotab);
		dsotab = dsotab_i;
		return sts;
	    }
	}
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
		    sts = -errno;
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
		    sts = -errno;
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
	if (sts != 0)
	    fprintf(stderr, "__pmLocalPMDA -> %s\n", pmErrStr(sts));
	fprintf(stderr, "Local Context PMDA Table");
	if (numdso == 0)
	    fprintf(stderr, " ... empty");
	fputc('\n', stderr);
	for (i = 0; i < numdso; i++) {
	    fprintf(stderr, "%p [%d] domain=%d name=%s init=%s handle=%p\n",
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
	sts = -errno;
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
    }
    else {
	free(sbuf);
	return "bad op in spec";
    }
    if (op == PM_LOCAL_CLEAR && *ap == '\0')
	goto doit;

    if (*ap != ',') {
	free(sbuf);
	return "bad spec";
    }
    arg = ++ap;
    if (*ap != ',' && *ap != '\0') {
	domain = (int)strtol(arg, &ap, 10);
	if ((*ap != ',' && *ap != '\0') || domain < 0 || domain > 510) {
	    free(sbuf);
	    return "bad domain in spec";
	}
    }
    if (*ap == ',') {
	ap++;
	if (*ap == ',') {
	    /* no name, could have init (not useful but possible!) */
	    ap++;
	    if (*ap != '\0')
		init = ap;
	}
	else if (*ap != '\0') {
	    /* have name and possibly init */
	    name = ap;
	    while (*ap != ',' && *ap != '\0')
		ap++;
	    if (*ap == ',') {
		*ap++ = '\0';
		if (*ap != '\0')
		    init = ap;
	    }
	}
    }

doit:
    sts = __pmLocalPMDA(op, domain, name, init);
    if (sts < 0) {
	static char buffer[256];
	snprintf(buffer, sizeof(buffer), "__pmLocalPMDA: %s", pmErrStr(sts));
	free(sbuf);
	return buffer;
    }

    free(sbuf);
    return NULL;
}
