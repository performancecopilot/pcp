/*
 * Utiility routines for pmlogrewrite
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "logger.h"
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

void
yywarn(char *s)
{
    fprintf(stderr, "Warning [%s, line %d]\n%s\n", configfile, lineno, s);
}

void
yyerror(char *s)
{
    fprintf(stderr, "Specification error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "[line %d] %s\n", lineno, s);
    exit(1);
}

void
yysemantic(char *s)
{
    fprintf(stderr, "Semantic error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "%s\n", s);
    exit(1);
}

/*
 * instance name matching ... return
 *  0 for no match
 *  1 for match to first space
 *  2 for complete match
 * -1 if either name is empty or NULL
 */
int
inst_name_eq(const char *p, const char *q)
{
    if (p == NULL || *p == '\0')
	return -1;
    if (q == NULL || *q == '\0')
	return -1;

    for ( ; ; p++, q++) {
	if (*p == '\0' && *q == '\0')
	    return 2;
	if (*p == '\0' || *p == ' ') {
	    if (*q == '\0' || *q == ' ')
		return 1;
	    break;
	}
	if (*q == '\0' || *q == ' ') {
	    if (*p == '\0' || *p == ' ')
		return 1;
	    break;
	}
	if (*p != *q)
	    break;
    }
    return 0;
}

/*
 * Rename all the physical archive files with basename of old to
 * a basename of new.
 *
 * If _any_ error occurs, don't make any changes.
 *
 * Note: does not handle compressed versions of files.
 *
 * TODO - need global locking for PCP 3.6 version if this is promoted
 *        to libpcp
 */
int
_pmLogRename(const char *old, const char *new)
{
    int			sts;
    int			nfound = 0;
    char		**found = NULL;
    char		*dname;
    char		*obase;
    char		path[MAXPATHLEN+1];
    char		opath[MAXPATHLEN+1];
    char		npath[MAXPATHLEN+1];
    DIR			*dirp;
    struct dirent	*dp;

    strncpy(path, old, sizeof(path));
    path[sizeof(path)-1] = '\0';
    dname = dirname(path);

    if ((dirp = opendir(dname)) == NULL)
	return -oserror();

    strncpy(path, old, sizeof(path));
    path[sizeof(path)-1] = '\0';
    obase = basename(path);

    for ( ; ; ) {
	setoserror(0);
	if ((dp = readdir(dirp)) == NULL)
	    break;
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;
	if (strncmp(obase, dp->d_name, strlen(obase)) == 0) {
	    /*
	     * match the base part of the old archive name, now check
	     * for meta or index or a valid volume number
	     */
	    char	*p = &dp->d_name[strlen(obase)];
	    int		want = 0;
	    if (strcmp(p, ".meta") == 0)
		want = 1;
	    else if (strcmp(p, ".index") == 0)
		want = 1;
	    else if (*p == '.' && isdigit((int)p[1])) {
		char	*endp;
		long	vol;
		vol = strtol(&p[1], &endp, 10);
		if (vol >= 0 && *endp == '\0')
		    want = 1;
	    }
	    if (want) {
		struct stat	stbuf;
		snprintf(opath, sizeof(opath), "%s%s", old, p);
		snprintf(npath, sizeof(npath), "%s%s", new, p);
		if (stat(npath, &stbuf) == 0) {
		    fprintf(stderr, "__pmLogRename: destination file %s already exists\n", npath);
		    goto revert;
		}
		if (rename(opath, npath) == -1) {
		    fprintf(stderr, "__pmLogRename: rename %s -> %s failed: %s\n", opath, npath, pmErrStr(-oserror()));
		    goto revert;
		}
		nfound++;
		found = (char **)realloc(found, nfound*sizeof(found[0]));
		if (found == NULL) {
		    __pmNoMem("__pmLogRename: realloc", nfound*sizeof(found[0]), PM_RECOV_ERR);
		    abandon();
		    /*NOTREACHED*/
		}
		if ((found[nfound-1] = strdup(p)) == NULL) {
		    __pmNoMem("__pmLogRename: strdup", strlen(p)+1, PM_RECOV_ERR);
		    abandon();
		    /*NOTREACHED*/
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "__pmLogRename: %s -> %s\n", opath, npath);
#endif
	    }
	}
    }

    if ((sts = oserror()) != 0) {
	fprintf(stderr, "__pmLogRename: readdir for %s failed: %s\n", dname, pmErrStr(-sts));
	goto revert;
    }

    sts = 0;
    goto cleanup;

revert:
    while (nfound > 0) {
	snprintf(opath, sizeof(opath), "%s%s", old, found[nfound-1]);
	snprintf(npath, sizeof(npath), "%s%s", new, found[nfound-1]);
	if (rename(npath, opath) == -1) {
	    fprintf(stderr, "__pmLogRename: arrgh trying to revert rename %s -> %s failed: %s\n", npath, opath, pmErrStr(-oserror()));
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "__pmLogRename: revert %s <- %s\n", opath, npath);
#endif
	nfound--;
    }
    sts = PM_ERR_GENERIC;

cleanup:
    closedir(dirp);
    while (nfound > 0) {
	free(found[nfound-1]);
	nfound--;
    }
    if (found != NULL)
	free(found);

    return sts;
}

/*
 * Remove all the physical archive files with basename of base.
 *
 * Note: does not handle compressed versions of files.
 *
 * TODO - need global locking for PCP 3.6 version if this is promoted
 *        to libpcp
 */
int
_pmLogRemove(const char *name)
{
    int			sts;
    int			nfound = 0;
    char		*dname;
    char		*base;
    char		path[MAXPATHLEN+1];
    DIR			*dirp;
    struct dirent	*dp;

    strncpy(path, name, sizeof(path));
    path[sizeof(path)-1] = '\0';
    dname = strdup(dirname(path));
    if (dname == NULL) {
	__pmNoMem("__pmLogRemove: dirname strdup", strlen(dirname(path))+1, PM_RECOV_ERR);
	abandon();
	/*NOTREACHED*/
    }

    if ((dirp = opendir(dname)) == NULL) {
	free(dname);
	return -oserror();
    }

    strncpy(path, name, sizeof(path));
    path[sizeof(path)-1] = '\0';
    base = strdup(basename(path));
    if (base == NULL) {
	__pmNoMem("__pmLogRemove: basename strdup", strlen(basename(path))+1, PM_RECOV_ERR);
	abandon();
	/*NOTREACHED*/
    }

    for ( ; ; ) {
	setoserror(0);
	if ((dp = readdir(dirp)) == NULL)
	    break;
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;
	if (strncmp(base, dp->d_name, strlen(base)) == 0) {
	    /*
	     * match the base part of the old archive name, now check
	     * for meta or index or a valid volume number
	     */
	    char	*p = &dp->d_name[strlen(base)];
	    int		want = 0;
	    if (strcmp(p, ".meta") == 0)
		want = 1;
	    else if (strcmp(p, ".index") == 0)
		want = 1;
	    else if (*p == '.' && isdigit((int)p[1])) {
		char	*endp;
		long	vol;
		vol = strtol(&p[1], &endp, 10);
		if (vol >= 0 && *endp == '\0')
		    want = 1;
	    }
	    if (want) {
		snprintf(path, sizeof(path), "%s%s", name, p);
		unlink(path);
		nfound++;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "__pmLogRemove: %s\n", path);
#endif
	    }
	}
    }

    if ((sts = oserror()) != 0) {
	fprintf(stderr, "__pmLogRemove: readdir for %s failed: %s\n", dname, pmErrStr(-sts));
	sts = -sts;
    }
    else
	sts = nfound;

    closedir(dirp);
    free(dname);
    free(base);

    return sts;
}
