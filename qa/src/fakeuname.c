/*
 * fakeuname.c
 * - set one or more of the FAKEUNAME_FOO environment variables,
 *   e.g. export FAKEUNAME_MACHINE=aarch64
 * - run apps with LD_PRELOAD=$here/src/fakeuname.$(DSOSUFFIX)
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <pcp/pmapi.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>

int
uname(struct utsname *buf)
{
    int		(*real_uname)(struct utsname *) = dlsym(RTLD_NEXT, "uname");
    int		sts = real_uname(buf);
    const char	*p;
    if (sts == 0) {
	if ((p = getenv("FAKEUNAME_SYSNAME")) != NULL) {
	    strncpy(buf->sysname, p, sizeof(buf->sysname) - 1);
	    buf->sysname[sizeof(buf->sysname) - 1] = '\0';
	}
	if ((p = getenv("FAKEUNAME_NODENAME")) != NULL) {
	    strncpy(buf->nodename, p, sizeof(buf->nodename) - 1);
	    buf->nodename[sizeof(buf->nodename) - 1] = '\0';
	}
	if ((p = getenv("FAKEUNAME_RELEASE")) != NULL) {
	    strncpy(buf->release, p, sizeof(buf->release) - 1);
	    buf->release[sizeof(buf->release) - 1] = '\0';
	}
	if ((p = getenv("FAKEUNAME_VERSION")) != NULL) {
	    strncpy(buf->version, p, sizeof(buf->version) - 1);
	    buf->version[sizeof(buf->version) - 1] = '\0';
	}
	if ((p = getenv("FAKEUNAME_MACHINE")) != NULL) {
	    strncpy(buf->machine, p, sizeof(buf->machine) - 1);
	    buf->machine[sizeof(buf->machine) - 1] = '\0';
	}
#ifdef _GNU_SOURCE
	if ((p = getenv("FAKEUNAME_DOMAINNAME")) != NULL) {
	    strncpy(buf->domainname, p, sizeof(buf->domainname) - 1);
	    buf->domainname[sizeof(buf->domainname) - 1] = '\0';
	}
#endif
    }
    return sts;
}
#else
/* nothing we can do ... */
int
uname(struct utsname *buf)
{
    fprintf(stderr, "fake uname() botch: no <dlfcn.h>\n");
    return -1;
}
#endif
