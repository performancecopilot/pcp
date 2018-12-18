/*
 * Copyright (c) 2012-2018 Red Hat.
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
#include "libpcp.h"
#include "internal.h"
#include "pmda.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	config_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*config_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == config_lock
 */
int
__pmIsConfigLock(void *lock)
{
    return lock == (void *)&config_lock;
}
#endif

#ifdef IS_OLD_MINGW
/*
 * It is not clear what's the right thing to do on Windows ...
 * pro tem, disable all of this path rewriting until we have a
 * clearer understanding.  Note IS_OLD_MINGW that needs to be
 * removed/changed.  See also NQR note below.  TODO.
 *
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

    PM_LOCK(__pmLock_extcall);
    s = getenv("SHELL");		/* THREADSAFE */
    /*
     * TODO - this is NQR
     * with the current git-sdk based mingw build ecosystem, SHELL
     * from the env here is C:\git-sdk-64\usr\bin\bash.exe and
     * $EXEPATH=C:\git-sdk-64
     */
    sts = (s && (strncmp(s, "/bin/", 5) == 0 || strncmp(s, "/usr/bin/", 9) == 0));
    PM_UNLOCK(__pmLock_extcall);
    return sts;
}

/*
 * Called with __pmLock_extcall held, so setenv() is thread-safe.
 */
static void
dos_formatter(char *var, char *prefix, char *val)
{
    int msys = posix_style();

    if (prefix && dos_rewrite_path(var, val, msys)) {
	char *p = msys ? msys_native_path(prefix) : prefix;
	char envbuf[MAXPATHLEN];

	pmsprintf(envbuf, sizeof(envbuf), "%s%s", p, val);
	setenv(var, envbuf, 1);	/* THREADSAFE */
    }
    else {
	setenv(var, val, 1);	/* THREADSAFE */
    }
}

PCP_DATA const __pmConfigCallback __pmNativeConfig = dos_formatter;
char *__pmNativePath(char *path) { return dos_native_path(path); }
int pmPathSeparator() { return posix_style() ? '/' : '\\'; }
int __pmAbsolutePath(char *path) { return posix_style() ? path[0] == '/' : dos_absolute_path(path); }
#else
int __pmAbsolutePath(char *path) { return path[0] == '/'; }
int pmPathSeparator() { return '/'; }

/*
 * path argument MUST be malloc'd ... could be free'd here and a
 * new path allocated
 *
 * Handle path rewriting for non-*nix platforms
 */
char *__pmNativePath(char *path)
{
#ifdef IS_MINGW
    /*
     * if path[0] is not '/', do nothing
     * map /c/mingw... $PCP_DIR/mingw...
     * map /anythinglese to $PCP_DIR/anythingelse
     */
    char	*p;
    char	*new_path;
    char	*start;
    static char *pcp_dir;
    static int	init = 1;

    if (path[0] != '/') {
	/* relative pathname, nothing to do */
	return path;
    }

    if (init) {
	/* one-trip initialization */
	pcp_dir = getenv("PCP_DIR");		/* THREADSAFE */
	init = 0;
    }

    if (pcp_dir == NULL)
	return path;

    /*
     * check for / drive-digit / mingw...
     */
    if (strlen(path) >= 8 &&
	path[2] == '/' &&
	strncmp(&path[3], "mingw", 5) == 0) {
	start = &path[2];
    }
    else
	start = path;

    new_path = (char *)malloc(strlen(pcp_dir) + strlen(start) + 1);
    if (new_path == NULL) {
	pmNoMem("__pmNativePath", strlen(pcp_dir) + strlen(start) + 1, PM_FATAL_ERR);
	/* NOTREACHED */
    }
    strncpy(new_path, pcp_dir, strlen(pcp_dir) + 1);
    for (p = new_path; *p; p++) {
	if (*p == '\\') *p = '/';
    }
    strncat(new_path, start, strlen(start) + 1);
    if (pmDebugOptions.config && pmDebugOptions.desperate)
	fprintf(stderr, "__pmNativePath: \"%s\" start @ [%d] -> \"%s\"\n", path, (int)(start - path), new_path);

    free(path);
    return(new_path);
#else
    return path;
#endif
}

/*
 * Called with __pmLock_extcall held, so setenv() is thread-safe.
 */
static void
posix_formatter(char *var, char *prefix, char *val)
{
    char	envbuf[MAXPATHLEN];
    char	*vp;
    char	*vend;
    unsigned	length;

    (void)prefix;
    vend = &val[strlen(val)-1];
    if (val[0] == *vend && (val[0] == '\'' || val[0] == '"')) {
	/*
	 * have quoted value like "gawk --posix" for $PCP_AWK_PROG ...
	 * strip quotes
	 */
	vp = &val[1];
	vend--;
    } else {
	vp = val;
    }
    if ((length = vend - vp + 1) > sizeof(envbuf))
	length = sizeof(envbuf);
    pmsprintf(envbuf, sizeof(envbuf), "%.*s", length, vp);
    setenv(var, envbuf, 1);		/* THREADSAFE */
}

PCP_DATA const __pmConfigCallback __pmNativeConfig = posix_formatter;
#endif

/*
 * Search order for pcp.conf file is:
 * - $PCP_CONF if set (handled already)
 * - $PCP_DIR/etc/pcp.conf if $PCP_DIR is set
 * - /usr/local/etc/pcp.conf if it exists and /etc/pcp.conf does NOT exist
 * - /etc/pcp.conf otherwise
 */
static char *
__pmconfigpath(const char *pcp_dir, const char *pcp_conf)
{
    char	path[MAXPATHLEN];

    if (pcp_dir != NULL) {
	pmsprintf(path, sizeof(path), "%s/etc/pcp.conf", pcp_dir);
	return strdup(path);
    }

    if (access("/etc/pcp.conf", R_OK) == -1) {
	/* may still be the Mac OS X case with HomeBrew, for example */
	if (access("/usr/local/etc/pcp.conf", R_OK) == 0)
	    return strdup("/usr/local/etc/pcp.conf");
    }
    return strdup("/etc/pcp.conf");
}

/*
 * Scan pcp.conf and put all PCP config variables found therein
 * into the environment.
 */
static void
__pmconfig(__pmConfigCallback formatter, int fatal)
{
    FILE	*fp;
    char	*pcp_conf, *pcp_dir, *val, *p;
    char	errmsg[PM_MAXERRMSGLEN];
    char	var[MAXPATHLEN];

    PM_LOCK(__pmLock_extcall);
    pcp_dir = getenv("PCP_DIR");	/* THREADSAFE */
    if (pcp_dir != NULL)
	pcp_dir = strdup(pcp_dir);
    pcp_conf = getenv("PCP_CONF");	/* THREADSAFE */
    if (pcp_conf != NULL)
	pcp_conf = strdup(pcp_conf);
    else
	pcp_conf = __pmconfigpath(pcp_dir, pcp_conf);
    PM_UNLOCK(__pmLock_extcall);

    if (pcp_conf == NULL) {
	if (!fatal)
	    goto out;
	/* see note below about fprintf use - applies equally here */
	fprintf(stderr,
		"FATAL PCP ERROR: could not allocate %u bytes for %s\n",
		(unsigned int) strlen("/etc/pcp.conf") + 1, "/etc/pcp.conf");
	goto failure;
    }

    /* THREADSAFE - no locks acquired in __pmNativePath() */
    pcp_conf = __pmNativePath(pcp_conf);

    if ((fp = fopen(pcp_conf, "r")) == NULL) {
	if (!fatal)
	    goto out;
	/*
	 * we used to pmprintf() here to be sure the message
	 * would be seen, given the seriousness of the situation
	 * ... but that introduces recursion back into
	 * pmGetOptionalConfig() to get the PCP settings that
	 * control what how to dispose of output from pmprintf()
	 * ... and kaboom.
	 */
	fprintf(stderr,
	    "FATAL PCP ERROR: could not open config file \"%s\" : %s\n"
	    "You may need to set PCP_CONF or PCP_DIR in your environment.\n",
		pcp_conf, osstrerror_r(errmsg, sizeof(errmsg)));
	goto failure;
    }

    while (fgets(var, sizeof(var), fp) != NULL) {
	if (var[0] == '#' || (p = strchr(var, '=')) == NULL)
	    continue;
	*p = '\0';
	val = p+1;
	if ((p = strrchr(val, '\n')) != NULL)
	    *p = '\0';
	PM_LOCK(__pmLock_extcall);
	p = getenv(var);		/* THREADSAFE */
	if (p != NULL)
	    val = p;
	else {
	    /*
	     * THREADSAFE - no locks acquired in formatter() which is
	     * really dos_formatter() or posix_formatter()
	     */
	    formatter(var, pcp_dir, val);
	}
	if (pmDebugOptions.config)
	    fprintf(stderr, "pmgetconfig: (init) %s=%s\n", var, val);
	PM_UNLOCK(__pmLock_extcall);
    }
    fclose(fp);
out:
    if (pcp_dir != NULL)
	free(pcp_dir);
    free(pcp_conf);
    return;

failure:
    if (pcp_dir != NULL)
	free(pcp_dir);
    free(pcp_conf);
    exit(1);
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
     * state controls one-trip initialization
     */
    static int		state = 0;
    char		*val;

    PM_LOCK(config_lock);
    if (state == 0) {
	state = 1;
	__pmconfig(__pmNativeConfig, fatal);
    }
    PM_UNLOCK(config_lock);

    /*
     * THREADSAFE TODO ... this is bad (and documented), returning a
     * direct pointer into the env ... should strdup() here and fix all
     * callers to free() as needed later
     */
    val = getenv(name);		/* THREAD-UNSAFE! */
    if (val == NULL) {
	if (pmDebugOptions.config) {
	    fprintf(stderr, "pmgetconfig: getenv(%s) -> NULL\n", name);
	}
	if (!fatal)
	    return NULL;
	val = "";
    }

    if (pmDebugOptions.config)
	fprintf(stderr, "pmgetconfig: %s=%s\n", name, val);

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

int
pmGetUsername(char **username)
{
    char *user = pmGetOptionalConfig("PCP_USER");
    if (user && user[0] != '\0') {
	*username = user;
	return 1;
    }
    *username = "pcp";
    return 0;
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
    int c;
    FILE *fp = fopen("/proc/sys/net/ipv6/conf/all/disable_ipv6", "r");
    if (fp == NULL)
	return access("/proc/net/if_inet6", F_OK) == 0 ? enabled() : disabled();
    c = fgetc(fp);
    fclose(fp);
    if (c == '1')
	return disabled();
    return enabled();
#else
    return enabled();
#endif
}

extern const char *compress_suffix_list(void);

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
#if defined(BUILD_WITH_LOCK_ASSERTS)
#define LOCK_ASSERTS_ENABLED	enabled
#else
#define LOCK_ASSERTS_ENABLED	disabled
#endif
#if defined(PM_MULTI_THREAD_DEBUG)
#define LOCK_DEBUG_ENABLED	enabled
#else
#define LOCK_DEBUG_ENABLED	disabled
#endif
#if defined(HAVE_LZMA_DECOMPRESSION)
#define LZMA_DECOMPRESS		enabled
#else
#define LZMA_DECOMPRESS		disabled
#endif
#if defined(HAVE_TRANSPARENT_DECOMPRESSION)
#define TRANSPARENT_DECOMPRESS	enabled
#else
#define TRANSPARENT_DECOMPRESS	disabled
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
	{ "multi_archive_contexts", enabled },			/* from pcp-3.11.1 */
	{ "lock_asserts",	LOCK_ASSERTS_ENABLED },		/* from pcp-3.11.10 */
	{ "lock_debug",		LOCK_DEBUG_ENABLED },		/* from pcp-3.11.10 */
	{ "lzma_decompress",	LZMA_DECOMPRESS },		/* from pcp-4.0.0 */
	{ "transparent_decompress", TRANSPARENT_DECOMPRESS },	/* from pcp-4.0.0 */
	{ "compress_suffixes",	compress_suffix_list },		/* from pcp-4.0.1 */
};

void
__pmAPIConfig(__pmAPIConfigCallback formatter)
{
    int i;

    for (i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
	const char *value = features[i].detector();
	if (pmDebugOptions.config)
	    fprintf(stderr, "__pmAPIConfig: %s=%s\n",
		  features[i].feature, value);
	formatter(features[i].feature, value);
    }
}

const char *
pmGetAPIConfig(const char *name)
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
