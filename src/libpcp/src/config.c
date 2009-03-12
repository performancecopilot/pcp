/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#ifdef IS_MINGW
/*
 * Fix up the Windows path separator quirkiness - PCP code deals
 * typically with forward-slash separators (i.e. if not passed in
 * on the command line, but hard-coded), but only very little now.
 * In addition, we also need to cater for native Windows programs
 * versus the MinimalSYStem POSIX-alike shell (which translates a
 * drive letter into a root filesystem entry for us).  Yee-hah!
 * NB: Only single drive letters allowed (Wikipedia says so)
 */
char *
mingw_native_path(char *path)
{
    char *p = path;

    if (path[0] == '/' && path[1] != '\0' && path[2] == '/') {
	p[0] = tolower(p[1]);
	p[1] = ':';
	p += 2;
    }
    for (; *p; p++)
	if (*p == '/') *p = '\\';
    return path;
}

static char *
msys_native_path(char *path)
{
    char *p = path;

    /* Only single drive letters allowed (Wikipedia says so) */
    if (path[0] != '\0' && path[1] == ':') {
	p[1] = tolower(p[0]);
	p[0] = '/';
	p += 2;
    }
    for (; *p; p++) {
	if (*p == '\\') *p = '/';
	else *p = tolower(*p);
    }
    return path;
}

static char *
mingw_rewrite_path(char *var, char *val, int msys)
{
    char *p = (char *)rindex(var, '_');

    if (p && (strcmp(p, "_PATH") == 0 ||
	      strcmp(p, "_DIR") == 0 ||
	      strcmp(p, "_PROG") == 0)) {
	if (msys)
	    return msys_native_path(val);
	return mingw_native_path(val);
    }
    return NULL;
}

/*
 * For native Win32 console tools, we need to translate the paths
 * used in scripts to native paths with PCP_DIR prefix prepended.
 *
 * For Win32 MSYS shell usage, we need to translate the paths
 * used in scripts to paths with PCP_DIR prefix prepended AND
 * drive letter path mapping done AND posix-style separators.
 *
 * Choose which way to go based on our environment (SHELL).
 */
static void
mingw_formatter(char *var, char *prefix, char *val)
{
    char envbuf[MAXPATHLEN];
    char *p, *s = getenv("SHELL");
    int msys = (s && strcmp(s, "/bin/sh") == 0);

    if (prefix && mingw_rewrite_path(var, val, msys)) {
	p = msys ? msys_native_path(prefix) : prefix;
	snprintf(envbuf, sizeof(envbuf), "%s=%s%s", var, p, val);
    }
    else {
	snprintf(envbuf, sizeof(envbuf), "%s=%s", var, val);
    }
    putenv(strdup(envbuf));
}

INTERN __pmConfigCallback __pmNativeConfig = mingw_formatter;
char *__pmNativePath(char *path) { return mingw_native_path(path); }
int __pmPathSeparator() { return '\\'; }
#else
char *__pmNativePath(char *path) { return path; }
int __pmPathSeparator() { return '/'; }

static void
posix_formatter(char *var, char *prefix, char *val)
{
    char envbuf[MAXPATHLEN];

    snprintf(envbuf, sizeof(envbuf), "%s=%s", var, val);
    putenv(strdup(envbuf));
    (void)prefix;
}

INTERN __pmConfigCallback __pmNativeConfig = posix_formatter;
#endif

void
__pmConfig(const char *name, __pmConfigCallback formatter)
{
    /*
     * Scan ${PCP_CONF-$PCP_DIR/etc/pcp.conf} and put all PCP config
     * variables found therein into the environment.
     */
    FILE *fp;
    char confpath[32];
    char dir[MAXPATHLEN];
    char var[MAXPATHLEN];
    char *prefix = getenv("PCP_DIR");
    char *conf;
    char *val;
    char *p;

    if ((conf = getenv("PCP_CONF")) == NULL) {
	strncpy(confpath, "/etc/pcp.conf", sizeof(confpath));
	if (prefix == NULL)
	    conf = __pmNativePath(confpath);
	else {
	    snprintf(dir, sizeof(dir),
			 "%s%s", prefix, __pmNativePath(confpath));
	    conf = dir;
	}
    }

    if (access((const char *)conf, R_OK) < 0 ||
	(fp = fopen(conf, "r")) == (FILE *)NULL) {
	pmprintf("FATAL PCP ERROR: could not open config file \"%s\" : %s\n",
		conf, strerror(errno));
	pmprintf("You may need to set PCP_CONF or PCP_DIR in your environment.\n");
	pmflush();
	exit(1);
    }

    while (fgets(var, sizeof(var), fp) != NULL) {
	if (var[0] == '#' || (p = strchr(var, '=')) == NULL)
	    continue;
	*p = '\0';
	val = p+1;
	if ((p = strrchr(val, '\n')) != NULL)
	    *p = '\0';
	if ((p = getenv(var)) != NULL)
	    val = p;
	else
	    formatter(var, prefix, val);

	if (pmDebug & DBG_TRACE_CONFIG)
	    fprintf(stderr, "pmGetConfig: (init) %s=%s\n", var, val);
    }
    fclose(fp);
}

char *
pmGetConfig(const char *name)
{
    static char *empty = "";
    static int first = 1;
    char *val;

    if (first) {
	__pmConfig(name, __pmNativeConfig);
	first = 0;
    }

    if ((val = getenv(name)) == NULL) {
	pmprintf("Error: \"%s\" is not set in the environment\n", name);
	val = empty;
    }

    if (pmDebug & DBG_TRACE_CONFIG)
	fprintf(stderr, "pmGetConfig: %s=%s\n", name, val);

    return val;
}
