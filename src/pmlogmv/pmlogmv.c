/*
 * Copyright (c) 2020,2022 Ken McDonell.  All Rights Reserved.
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

/*
 * pmlogmv - move/rename PCP archives
 */

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "pmapi.h"
#include "libpcp.h"

static int myoverrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "checksum", 0, 'c', 0, "checksum all source and destintion files when copying" },
    { "force", 0, 'f', 0, "force changes, even if they look unsafe" },
    { "showme", 0, 'N', 0, "perform a dry run, showing what would be done" },
    { "verbose", 0, 'V', 0, "increase diagnostic verbosity" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "cD:fNV?",
    .long_options = longopts,
    .short_usage = "[options] oldname newname",
    .override = myoverrides
};

static int	showme = 0;
static int	verbose = 0;
static int	force = 0;
static int	checksum = 0;
static char	*oldname;		
static char	*newname;
/* need a sentinel that is < 0 and ! PM_LOG_VOL_TI amd ! PM_LOG_VOL_META */
#define PM_LOG_VOL_NONE -100
static int	lastvol = PM_LOG_VOL_NONE;
static __pmContext	*ctxp = NULL;
static char	**sufftab;

/*
 * sha256sum returns 64 hex digits
 */
#define MAX_CHECKSUM	64

static int
myoverrides(int opt, pmOptions *optsp)
{
    if (opt == 'N' || opt == 'V')
	/* -N and -V are for me, not pmGetOptions() */
	return 1;
    return 0;
}

static int
check_name(char *name)
{
    char	*meta = " $?*[(|;&<>";
    char	*p;

    for (p = meta; *p; p++) {
	if (index(name, *p) != NULL) {
	    fprintf(stderr, "pmlogmv: newname (%s) unsafe [shell metacharacter '%c']\n", name, *p);
	    return -1;
	}
    }

    return 0;
}

static int
setup_sufftab(void)
{
    char	**table;
    char	**table_tmp;
    char	*list;
    char	*p;
    int		n = 2;

    if ((list = strdup(pmGetAPIConfig("compress_suffixes"))) == NULL) {
	fprintf(stderr, "pmlogmv: cannot get archive suffix list\n");
	return -1;
    }

    if ((table = malloc(n * sizeof(char *))) == NULL) {
	fprintf(stderr, "pmlogmv: cannot malloc suffix table\n");
	free(list);
	return -1;
    }

    if ((table[0] = strdup("")) == NULL) {
	fprintf(stderr, "pmlogmv: cannot strdup for suffix table[0]\n");
	free(list);
	free(table);
	return -1;
    }

    p = strtok(list, " ");
    while (p) {
	if ((table_tmp = realloc(table, sizeof(char *) * ++n)) == NULL) {
	    fprintf(stderr, "pmlogmv: cannot realloc suffix table for %d entries\n", n);
	    free(list);
	    free(table);
	    return -1;
	}
	table = table_tmp;
	table[n-2] = p;
	p = strtok(NULL, " ");
    }

    /*
     * Note: table[i] -> &list[x], so do NOT free(list) on the success
     * path here.
     */
    table[n-1] = NULL;
    sufftab = table;
    return 0;
}

/*
 * Return checksum for one file in sum[] ... "" (empty string) if
 * no working checksum command available.
 *
 * Expect sum[] to be at least MAX_CHECKSUM+1 chars long
 */
void
do_checksum(const char *file, char *sum)
{
    char	cmd[MAXPATHLEN+40];
    static char	*executable = NULL;
    FILE	*fp;
    static int	trunc_warn = 0;

    if (executable == NULL) {
	/*
	 * one-trip to initialize the "checksum" command ...
	 * prefer md5sum, then sha256sum, then sha1sum, then sum,
	 * else do nothing
	 */
	snprintf(cmd, sizeof(cmd), "if which md5sum >/dev/null 2>&1; then exit 0; fi; exit 1");
	if (system(cmd) == 0)
	    executable = "md5sum";
	else {
	    snprintf(cmd, sizeof(cmd), "if which sha256sum >/dev/null 2>&1; then exit 0; fi; exit 1");
	    if (system(cmd) == 0)
		executable = "sha256sum";
	    else {
		snprintf(cmd, sizeof(cmd), "if which sha1sum >/dev/null 2>&1; then exit 0; fi; exit 1");
		if (system(cmd) == 0)
		    executable = "sha1sum";
		else {
		    snprintf(cmd, sizeof(cmd), "if which sum >/dev/null 2>&1; then exit 0; fi; exit 1");
		    if (system(cmd) == 0)
			executable = "sum";
		    else {
			executable = "none";
			fprintf(stderr, "pmlogmv: warning: no checksum command found, checksums skipped\n");
		    }
		}
	    }
	}
	if (verbose && strcmp(executable, "none") != 0)
	    printf("checksum cmd: %s\n", executable);
    }
    sum[0] = '\0';
    if (strcmp(executable, "none") == 0)
	return;
    snprintf(cmd, sizeof(cmd), "%s <%s", executable, file);
    if ((fp = popen(cmd, "r")) == NULL) {
	/*
	 * abandon checksuming ...
	 */
	fprintf(stderr, "pmlogmv: pipe(\"%s\") failed: %s\n", cmd, strerror(errno));
	executable = "none";
    }
    else {
	char	*p = sum;
	int	c;
	while ((c = fgetc(fp)) != EOF) {
	    if (c == ' ') {
		*p = '\0';
		break;
	    }
	    if (p >= &sum[MAX_CHECKSUM]) {
		/*
		 * avoid buffer overrun, report only once unless -V
		 */
		if (trunc_warn++ == 0 || verbose)
		    fprintf(stderr, "pmlogmv: warning: checksum truncated after %d characters\n", MAX_CHECKSUM);
		*p = '\0';
		break;
	    }
	    *p++ = c;
	}
	pclose(fp);
    }
}

/*
 * make link for one physical file
 * return codes:
 * 1: ok
 * 0: source file not found
 * -1: error
 */
static int
do_link(int vol)
{
    char	src[MAXPATHLEN];
    char	dst[MAXPATHLEN];
    char	**suff;

    for (suff = sufftab; *suff != NULL; suff++) {
	switch (vol) {
	    case PM_LOG_VOL_TI:
		    snprintf(src, sizeof(src), "%s.index%s", oldname, *suff);
		    break;
	    case PM_LOG_VOL_META:
		    snprintf(src, sizeof(src), "%s.meta%s", oldname, *suff);
		    break;
	    default:
		    snprintf(src, sizeof(src), "%s.%d%s", oldname, vol, *suff);
		    break;
	}
	if (access(src, F_OK) == 0) {
	    /* src exists ... off to the races */
	    switch (vol) {
		case PM_LOG_VOL_TI:
			snprintf(dst, sizeof(src), "%s.index%s", newname, *suff);
			break;
		case PM_LOG_VOL_META:
			snprintf(dst, sizeof(src), "%s.meta%s", newname, *suff);
			break;
		default:
			snprintf(dst, sizeof(src), "%s.%d%s", newname, vol, *suff);
			break;
	    }
	    if (access(dst, F_OK) == 0) {
		/* dst exists ... blah, no cigar */
		fprintf(stderr, "pmlogmv: %s: already exists\n", dst);
		return -1;
	    }
	    if (showme)
		printf("+ ln %s %s\n", src, dst);
	    else {
		if (link(src, dst) < 0) {
		    if (errno == EXDEV) {
			/* link() failed cross-device, need to copy ... */
			int		sts;
			char		cmd[2*MAXPATHLEN+4];
			char		sum_src[MAX_CHECKSUM+1];
			char		sum_dst[MAX_CHECKSUM+1];
			if (checksum) {
			    /*
			     * checksum src before cp ... kinder on the file
			     * system buffer cache
			     */
			    do_checksum(src, sum_src);
			    if (verbose && sum_src[0] != '\0')
				printf("source checksum: %s\n", sum_src);
			}

			snprintf(cmd, sizeof(cmd), "cp %s %s", src, dst);
			if ((sts = system(cmd)) != 0) {
			    fprintf(stderr, "pmlogmv: copy %s -> %s failed: %s\n", src, dst, strerror(errno));
			    return -1;
			}
			if (checksum) {
			    do_checksum(dst, sum_dst);
			    if (verbose && sum_dst[0] != '\0')
				printf("destination checksum: %s\n", sum_dst);
			    if (strcmp(sum_src, sum_dst) != 0) {
				/* different checksums! */
				fprintf(stderr, "pmlogmv: checksums %s ? %s differ\n", sum_src, sum_dst);
				if (unlink(dst) < 0)
				    fprintf(stderr, "pmlogmv: unlink %s failed: %s\n", dst, strerror(errno));
				else if (verbose)
				    printf("remove %s\n", dst);
				return -1;
			    }
			}
			if (verbose)
			    printf("copy %s -> %s\n", src, dst);
		    }
		    else {
			fprintf(stderr, "pmlogmv: link %s -> %s failed: %s\n", src, dst, strerror(errno));
			return -1;
		    }
		}
		else if (verbose)
		    printf("link %s -> %s\n", src, dst);
	    }
	    lastvol = vol;
	    return 1;
	}
    }

    switch (vol) {
	case PM_LOG_VOL_TI:
		if (verbose > 1)
		    fprintf(stderr, "pmlogmv: Warning: source file %s.index not found\n", oldname);
		break;
	case PM_LOG_VOL_META:
		if (verbose)
		    fprintf(stderr, "pmlogmv: Warning: source file %s.meta not found\n", oldname);
		break;
	default:
		if (verbose > 1)
		    fprintf(stderr, "pmlogmv: Warning: source file %s.%d not found\n", oldname, vol);
		break;
    }

    return 0;
}

/*
 * remove one physical file
 */
static void
do_unlink(int cleanup, char *name, int vol)
{
    char	src[MAXPATHLEN];
    char	**suff;

    for (suff = sufftab; *suff != NULL; suff++) {
	switch (vol) {
	    case PM_LOG_VOL_TI:
		    snprintf(src, sizeof(src), "%s.index%s", name, *suff);
		    break;
	    case PM_LOG_VOL_META:
		    snprintf(src, sizeof(src), "%s.meta%s", name, *suff);
		    break;
	    default:
		    snprintf(src, sizeof(src), "%s.%d%s", name, vol, *suff);
		    break;
	}
	if (access(src, F_OK) == 0) {
	    /* src exists ... offto the races */
	    if (showme)
		printf("+ rm %s\n", src);
	    else {
		if (verbose)
		    printf("%sremove %s\n", cleanup ? "cleanup: " : "", src);
		if (unlink(src) < 0) {
		    fprintf(stderr, "pmlogmv: unlink %s failed: %s\n", src, strerror(errno));
		}
	    }
	    break;
	}
    }
}

static void
cleanup(int sig)
{
    int		i;

    if (sig != 0) {
	fprintf(stderr, "Caught signal %d\n", sig);
	verbose = 1;
    }

    /* order here is the _reverse_ order of creation in main() */
    if (lastvol == PM_LOG_VOL_META) {
	/* newname.meta was created */
	do_unlink(1, newname, PM_LOG_VOL_META);
	lastvol = PM_LOG_VOL_TI;
    }
    if (lastvol == PM_LOG_VOL_TI) {
	/* newname.index was created */
	do_unlink(1, newname, PM_LOG_VOL_TI);
	if (ctxp == NULL)
	    lastvol = PM_LOG_VOL_NONE;
	else
	    lastvol = ctxp->c_archctl->ac_log->maxvol;
    }
    if (lastvol != PM_LOG_VOL_NONE) {
	/* newname vols were created */
	for (i = ctxp->c_archctl->ac_log->minvol; i <= lastvol; i++) {
	    do_unlink(1, newname, i);
	}
    }

    exit(1);
}

#ifdef HAVE_SA_SIGINFO
static void
trap(int sig, siginfo_t *sip, void *x)
{
    cleanup(sig);
}
#endif

int
main(int argc, char **argv)
{
    int		i;
    int		c;
    int		sts;
#ifdef HAVE_SA_SIGINFO
    static struct sigaction act;
#endif

    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* checksum if copying */
	    checksum = 1;
	    break;

	case 'f':	/* force, even if it looks unsafe */
	    force = 1;
	    break;

	case 'N':	/* dry-run, show-me */
	    showme = 1;
	    break;

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind != argc-2) {
	pmUsageMessage(&opts);
	exit(1);
    }

    oldname = strdup(argv[opts.optind]);
    if (oldname == NULL) {
	fprintf(stderr, "pmlogmv: malloc(oldname) failed!\n");
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, oldname)) < 0) {
	fprintf(stderr, "pmlogmv: Cannot open archive \"%s\": %s\n", oldname, pmErrStr(sts));
	exit(1);
    }
    if ((ctxp = __pmHandleToPtr(sts)) == NULL) {
	fprintf(stderr, "pmlogmv: botch: __pmHandleToPtr(%d) returns NULL!\n", sts);
	exit(1);
    }
    oldname = ctxp->c_archctl->ac_log->name;

    opts.optind++;
    newname = argv[opts.optind];

    if (!force && check_name(newname) < 0) {
	/* error reported in check_name() */
	exit(1);
    }

    if (setup_sufftab() < 0) {
	/* error reported in setup_sufftab() */
	exit(1);
    }

    /* install signal handler in case we're interrupted */
#ifdef HAVE_SA_SIGINFO
    act.sa_sigaction = trap;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
#else
    __pmSetSignalHandler(SIGINT, cleanup);
    __pmSetSignalHandler(SIGTERM, cleanup);
    __pmSetSignalHandler(SIGHUP, cleanup);
#endif


    for (i = ctxp->c_archctl->ac_log->minvol; i <= ctxp->c_archctl->ac_log->maxvol; i++) {
	if (do_link(i) < 0)
	    goto abandon;
    }
    if (do_link(PM_LOG_VOL_TI) < 0)
	goto abandon;
    if (do_link(PM_LOG_VOL_META) < 0)
	goto abandon;

    /* remove oldname files */
    for (i = ctxp->c_archctl->ac_log->minvol; i <= ctxp->c_archctl->ac_log->maxvol; i++) {
	do_unlink(0, oldname, i);
    }
    do_unlink(0, oldname, PM_LOG_VOL_TI);
    do_unlink(0, oldname, PM_LOG_VOL_META);
    return 0;

/* fatal error once we're started ... remove any newname files */
abandon:
    cleanup(0);
    /* NOTREACHED */
    return(1);		/* never reached, to keep some compilers happy, sigh */
}
