/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

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
dos_native_path(char *path)
{
    char *p = path;

    if (path[0] == '/' && isalpha((int)path[1]) && path[2] == '/') {
	p[0] = tolower(p[1]);
	p[1] = ':';
	p += 2;
    }
    for (; *p; p++)
	if (*p == '/') *p = '\\';
    return path;
}

static int
dos_absolute_path(char *path)
{
    return (isalpha((int)path[0]) && path[1] == ':' && path[2] == '\\');
}

static char *
msys_native_path(char *path)
{
    char *p = path;

    /* Only single drive letters allowed (Wikipedia says so) */
    if (isalpha((int)path[0]) && path[1] == ':') {
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
dos_rewrite_path(char *var, char *val, int msys)
{
    char *p = (char *)rindex(var, '_');

    if (p && (strcmp(p, "_PATH") == 0 || strcmp(p, "_DIR") == 0)) {
	if (msys)
	    return msys_native_path(val);
	return dos_native_path(val);
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
static int posix_style(void)
{
    char	*s;
    int		sts;
    PM_LOCK(__pmLock_libpcp);
    s = getenv("SHELL");
    sts = (s && strncmp(s, "/bin/", 5) == 0);
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

static void
dos_formatter(char *var, char *prefix, char *val)
{
    char envbuf[MAXPATHLEN];
    int msys = posix_style();

    if (prefix && dos_rewrite_path(var, val, msys)) {
	char *p = msys ? msys_native_path(prefix) : prefix;
	snprintf(envbuf, sizeof(envbuf), "%s=%s%s", var, p, val);
    }
    else {
	snprintf(envbuf, sizeof(envbuf), "%s=%s", var, val);
    }
    PM_LOCK(__pmLock_libpcp);
    putenv(strdup(envbuf));
    PM_UNLOCK(__pmLock_libpcp);
}

INTERN const __pmConfigCallback __pmNativeConfig = dos_formatter;
char *__pmNativePath(char *path) { return dos_native_path(path); }
int __pmPathSeparator() { return posix_style() ? '/' : '\\'; }
int __pmAbsolutePath(char *path) { return posix_style() ? path[0] == '/' : dos_absolute_path(path); }
#else
char *__pmNativePath(char *path) { return path; }
int __pmAbsolutePath(char *path) { return path[0] == '/'; }
int __pmPathSeparator() { return '/'; }

static void
posix_formatter(char *var, char *prefix, char *val)
{
    /* +40 bytes for max PCP env variable name */
    char	envbuf[MAXPATHLEN+40];
    char	*vp;
    char	*vend;

    snprintf(envbuf, sizeof(envbuf), "%s=", var);
    vend = &val[strlen(val)-1];
    if (val[0] == *vend && (val[0] == '\'' || val[0] == '"')) {
	/*
	 * have quoted value like "gawk --posix" for $PCP_AWK_PROG ...
	 * strip quotes
	 */
	vp = &val[1];
	vend--;
    }
    else
	vp = val;
    strncat(envbuf, vp, vend-vp+1);
    envbuf[strlen(var)+1+vend-vp+1+1] = '\0';

    PM_LOCK(__pmLock_libpcp);
    putenv(strdup(envbuf));
    PM_UNLOCK(__pmLock_libpcp);
    (void)prefix;
}

INTERN const __pmConfigCallback __pmNativeConfig = posix_formatter;
#endif


static void
__pmconfig(__pmConfigCallback formatter, int fatal)
{
    /*
     * Scan ${PCP_CONF-$PCP_DIR/etc/pcp.conf} and put all PCP config
     * variables found therein into the environment.
     */
    FILE *fp;
    char confpath[32];
    char errmsg[PM_MAXERRMSGLEN];
    char dir[MAXPATHLEN];
    char var[MAXPATHLEN];
    char *prefix;
    char *conf;
    char *val;
    char *p;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    prefix = getenv("PCP_DIR");
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

    if ((fp = fopen(conf, "r")) == NULL) {
	PM_UNLOCK(__pmLock_libpcp);
	if (!fatal)
	    return;
	pmprintf(
	    "FATAL PCP ERROR: could not open config file \"%s\" : %s\n"
	    "You may need to set PCP_CONF or PCP_DIR in your environment.\n",
		conf, osstrerror_r(errmsg, sizeof(errmsg)));
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
	    fprintf(stderr, "pmgetconfig: (init) %s=%s\n", var, val);
    }
    fclose(fp);
    PM_UNLOCK(__pmLock_libpcp);
}

void
__pmConfig(__pmConfigCallback formatter)
{
    __pmconfig(formatter, PM_FATAL_ERR);
}

static char *
pmgetconfig(const char *name, int fatal)
{
    /*
     * state controls one-trip initialization, and recursion guard
     * for pathological failures in initialization
     */
    static int		state = 0;
    char		*val;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (state == 0) {
	state = 1;
	PM_UNLOCK(__pmLock_libpcp);
	__pmconfig(__pmNativeConfig, fatal);
	PM_LOCK(__pmLock_libpcp);
	state = 2;
    }
    else if (state == 1) {
	/* recursion from error in __pmConfig() ... no value is possible */
	PM_UNLOCK(__pmLock_libpcp);
	if (pmDebug & DBG_TRACE_CONFIG)
	    fprintf(stderr, "pmgetconfig: %s= ... recursion error\n", name);
	if (!fatal)
	    return NULL;
	val = "";
	return val;
    }

    if ((val = getenv(name)) == NULL) {
	if (!fatal)
	    return NULL;
	val = "";
    }

    if (pmDebug & DBG_TRACE_CONFIG)
	fprintf(stderr, "pmgetconfig: %s=%s\n", name, val);

    PM_UNLOCK(__pmLock_libpcp);
    return val;
}

char *
pmGetConfig(const char *name)
{
    return pmgetconfig(name, PM_FATAL_ERR);
}

char *
pmGetOptionalConfig(const char *name)
{
    return pmgetconfig(name, PM_RECOV_ERR);
}

/*
 * Details of runtime features available in the built libpcp
 */

static const char *enabled(void) { return "true"; }
static const char *disabled(void) { return "false"; }

#define STRINGIFY(s)		#s
#define TO_STRING(s)		STRINGIFY(s)
static const char *pmapi_version(void) { return TO_STRING(PMAPI_VERSION); }
static const char *pcp_version(void) { return PCP_VERSION; }
#if defined(HAVE_SECURE_SOCKETS)
#include "nss.h"
#include "nspr.h"
#include "sasl.h"
static const char *nspr_version(void) { return PR_VERSION; }
static const char *nss_version(void) { return NSS_VERSION; }
static const char *sasl_version_string(void)
{
    return TO_STRING(SASL_VERSION_MAJOR.SASL_VERSION_MINOR.SASL_VERSION_STEP);
}
#endif

static const char *
ipv6_enabled(void)
{
#if defined(IS_LINUX)
    return access("/proc/net/if_inet6", F_OK) == 0 ? enabled() : disabled();
#else
    return enabled();
#endif
}

#ifdef PM_MULTI_THREAD
#define MULTI_THREAD_ENABLED	enabled
#else
#define MULTI_THREAD_ENABLED	disabled
#endif
#ifdef PM_FAULT_INJECTION
#define FAULT_INJECTION_ENABLED	enabled
#else
#define FAULT_INJECTION_ENABLED	disabled
#endif
#if defined(HAVE_SECURE_SOCKETS)
#define SECURE_SOCKETS_ENABLED	enabled
#define AUTHENTICATION_ENABLED	enabled
#else
#define SECURE_SOCKETS_ENABLED	disabled
#define AUTHENTICATION_ENABLED	disabled
#endif
#if defined(HAVE_STRUCT_SOCKADDR_UN)
#define UNIX_DOMAIN_SOCKETS_ENABLED	enabled
#else
#define UNIX_DOMAIN_SOCKETS_ENABLED	disabled
#endif
#if defined(HAVE_STATIC_PROBES)
#define STATIC_PROBES_ENABLED	enabled
#else
#define STATIC_PROBES_ENABLED	disabled
#endif
#if defined(HAVE_SERVICE_DISCOVERY)
#define SERVICE_DISCOVERY_ENABLED	enabled
#else
#define SERVICE_DISCOVERY_ENABLED	disabled
#endif

typedef const char *(*feature_detector)(void);
static struct {
	const char 		*feature;
	feature_detector	detector;
} features[] = {
	{ "pcp_version",	pcp_version },
	{ "pmapi_version",	pmapi_version },
#if defined(HAVE_SECURE_SOCKETS)
	{ "nss_version",	nss_version },
	{ "nspr_version",	nspr_version },
	{ "sasl_version",	sasl_version_string },
#endif
	{ "multi_threaded",	MULTI_THREAD_ENABLED },
	{ "fault_injection",	FAULT_INJECTION_ENABLED },
	{ "secure_sockets",	SECURE_SOCKETS_ENABLED },	/* from pcp-3.7.x */
	{ "ipv6",		ipv6_enabled },
	{ "authentication",	AUTHENTICATION_ENABLED },	/* from pcp-3.8.x */
	{ "unix_domain_sockets",UNIX_DOMAIN_SOCKETS_ENABLED },	/* from pcp-3.8.2 */
	{ "static_probes",	STATIC_PROBES_ENABLED },	/* from pcp-3.8.3 */
	{ "service_discovery",	SERVICE_DISCOVERY_ENABLED },	/* from pcp-3.8.6 */
};

void
__pmAPIConfig(__pmAPIConfigCallback formatter)
{
    int i;

    for (i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
	const char *value = features[i].detector();
	if (pmDebug & DBG_TRACE_CONFIG)
	    fprintf(stderr, "__pmAPIConfig: %s=%s\n",
		  features[i].feature, value);
	formatter(features[i].feature, value);
    }
}

const char *
__pmGetAPIConfig(const char *name)
{
    int i;

    for (i = 0; i < sizeof(features)/sizeof(features[0]); i++)
        if (strcasecmp(name, features[i].feature) == 0)
	    return features[i].detector();
    return NULL;
}

/*
 * binary encoding of current PCP version
 */
int
pmGetVersion(void)
{
    return PM_VERSION_CURRENT;
}
