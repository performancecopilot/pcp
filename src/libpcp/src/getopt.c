/*
 * Common argument parsing for all PMAPI client tools.
 *
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (C) 1987-2014 Free Software Foundation, Inc.
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
#include "internal.h"
#include <ctype.h>

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

enum {
    REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER
};

/*
 * Using the current archive context, extract start and end
 * times and adjust the time window boundaries accordingly.
 */
static int
__pmUpdateBounds(pmOptions *opts, int index, struct timeval *begin, struct timeval *end)
{
    struct timeval logend;
    pmLogLabel label;
    int sts;

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	pmprintf("%s: Cannot get archive %s label record: %s\n",
		pmProgname, opts->archives[index], pmErrStr(sts));
	return sts;
    }
    if ((sts = pmGetArchiveEnd(&logend)) < 0) {
	logend.tv_sec = INT_MAX;
	logend.tv_usec = 0;
	fflush(stdout);
	fprintf(stderr, "%s: Cannot locate end of archive %s: %s\n",
		pmProgname, opts->archives[index], pmErrStr(sts));
	fprintf(stderr, "\nWARNING: "
    "This archive is sufficiently damaged that it may not be possible to\n");
	fprintf(stderr,   "         "
    "produce complete information.  Continuing and hoping for the best.\n\n");
	fflush(stderr);
    }

    if (index == 0) {
	/* the first archive in the set forms the initial boundaries */
	*begin = label.ll_start;
	*end = logend;
    } else {
	/* must now check if this archive pre- or post- dates others */
	if (__pmtimevalSub(begin, &label.ll_start) > 0.0)
	    *begin = label.ll_start;
	if (__pmtimevalSub(end, &logend) < 0.0)
	    *end = logend;
    }
    return 0;
}

/*
 * Calculate time window boundaries depending on context type.
 * In multi-archive context, this means opening all of them and
 * defining the boundary as being from the start of the earliest
 * through to the end of the last-written archive.
 *
 * Note - called with an active context via pmGetContextOptions.
 */
static int
__pmBoundaryOptions(pmOptions *opts, struct timeval *begin, struct timeval *end)
{
    int i, ctx, sts = 0;

    if (opts->context != PM_CONTEXT_ARCHIVE) {
	/* live/local context, open ended - start now, never end */
	__pmtimevalNow(begin);
	end->tv_sec = INT_MAX;
	end->tv_usec = 0;
    } else if (opts->narchives == 1) {
	/* singular archive context, make use of current context */
	sts = __pmUpdateBounds(opts, 0, begin, end);
    } else {
	/* multiple archives - figure out combined start and end */
	for (i = 0; i < opts->narchives; i++) {
	    sts = pmNewContext(PM_CONTEXT_ARCHIVE, opts->archives[i]);
	    if (sts < 0) {
		pmprintf("%s: Cannot open archive %s: %s\n",
			pmProgname, opts->archives[i], pmErrStr(sts));
		break;
	    }
	    ctx = sts;
	    sts = __pmUpdateBounds(opts, i, begin, end);
	    pmDestroyContext(ctx);
	    if (sts < 0)
		break;
	}
    }
    return sts;
}

/*
 * Final stages of argument parsing, anything that needs to wait
 * until after we have a context - e.g. timezones, time windows.
 */
int
pmGetContextOptions(int ctxid, pmOptions *opts)
{
    int window = (opts->start_optarg || opts->finish_optarg ||
		  opts->align_optarg || opts->origin_optarg) ||
		 (opts->flags & PM_OPTFLAG_BOUNDARIES);
    int tzh;

    /* timezone setup */
    if (opts->tzflag) {
	char hostname[MAXHOSTNAMELEN];

	pmGetContextHostName_r(ctxid, hostname, MAXHOSTNAMELEN);
	if ((tzh = pmNewContextZone()) < 0) {
	    pmprintf("%s: Cannot set context timezone: %s\n",
			pmProgname, pmErrStr(tzh));
	    opts->errors++;
	}
	else if (opts->flags & PM_OPTFLAG_STDOUT_TZ) {
	    printf("Note: timezone set to local timezone of host \"%s\"%s\n\n",
		    hostname,
		    opts->context != PM_CONTEXT_ARCHIVE ? "" : " from archive");
	}
    }
    else if (opts->timezone) {
	if ((tzh = pmNewZone(opts->timezone)) < 0) {
	    pmprintf("%s: Cannot set timezone to \"%s\": %s\n",
			pmProgname, opts->timezone, pmErrStr(tzh));
	    opts->errors++;
	}
	else if (opts->flags & PM_OPTFLAG_STDOUT_TZ) {
	    printf("Note: timezone set to \"TZ=%s\"\n\n", opts->timezone);
	}
    }

    /* time window setup */
    if (!opts->errors && window) {
	struct timeval first_boundary, last_boundary;
	char *msg;

	if (__pmBoundaryOptions(opts, &first_boundary, &last_boundary) < 0)
	    opts->errors++;
	else if (pmParseTimeWindow(
			opts->start_optarg, opts->finish_optarg,
			opts->align_optarg, opts->origin_optarg,
			&first_boundary, &last_boundary,
			&opts->start, &opts->finish, &opts->origin,
			&msg) < 0) {
	    pmprintf("%s: invalid time window: %s\n", pmProgname, msg);
	    opts->errors++;
	    free(msg);
	}
    }

    if (opts->errors) {
	if (!(opts->flags & PM_OPTFLAG_USAGE_ERR))
	    opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
	return PM_ERR_GENERIC;
    }

    return 0;
}

/*
 * All arguments have been parsed at this point (both internal and external).
 * We can now perform any final processing that could not be done earlier.
 *
 * Note that some end processing requires a context (in particular, the
 * "time window" processing, which may require timezone setup, and so on).
 * Such processing is deferred to pmGetContextOptions().
 */
void
__pmEndOptions(pmOptions *opts)
{
    if (opts->flags & PM_OPTFLAG_DONE)
	return;

    /* inform caller of the struct version used */
    if (opts->version != PMAPI_VERSION_2)
	opts->version = PMAPI_VERSION_2;

    if (!opts->context) {
	if (opts->Lflag)
	    opts->context = PM_CONTEXT_LOCAL;
	else if (opts->nhosts && !opts->narchives)
	    opts->context = PM_CONTEXT_HOST;
	else if (opts->narchives && !opts->nhosts)
	    opts->context = PM_CONTEXT_ARCHIVE;
    }

    if ((opts->start_optarg || opts->align_optarg || opts->origin_optarg) &&
	 opts->context != PM_CONTEXT_ARCHIVE) {
	pmprintf("%s: time window options are supported for archives only\n",
		 pmProgname);
	opts->errors++;
    }

    if (opts->tzflag && opts->context != PM_CONTEXT_ARCHIVE &&
	opts->context != PM_CONTEXT_HOST) {
	pmprintf("%s: use of timezone from metric source requires a source\n",
		 pmProgname);
	opts->errors++;
    }

    if (opts->errors && !(opts->flags & PM_OPTFLAG_RUNTIME_ERR))
	opts->flags |= PM_OPTFLAG_USAGE_ERR;
    opts->flags |= PM_OPTFLAG_DONE;
}

static void
__pmSetAlignment(pmOptions *opts, char *arg)
{
    opts->align_optarg = arg;
}

static void
__pmSetOrigin(pmOptions *opts, char *arg)
{
    opts->origin_optarg = arg;
}

static void
__pmSetStartTime(pmOptions *opts, char *arg)
{
    opts->start_optarg = arg;
}

static void
__pmSetDebugFlag(pmOptions *opts, char *arg)
{
    int sts;

    if ((sts = __pmParseDebug(arg)) < 0) {
	pmprintf("%s: unrecognized debug flag specification (%s)\n",
		pmProgname, arg);
	opts->errors++;
    }
    else {
	pmDebug |= sts;
    }
}

static void
__pmSetGuiModeFlag(pmOptions *opts)
{
    if (opts->guiport_optarg) {
	pmprintf("%s: at most one of -g and -p allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->guiflag = 1;
    }
}

static void
__pmSetGuiPort(pmOptions *opts, char *arg)
{
    char *endnum;

    if (opts->guiflag) {
	pmprintf("%s: at most one of -g and -p allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->guiport_optarg = arg;
	opts->guiport = (int)strtol(arg, &endnum, 10);
	if (*endnum != '\0' || opts->guiport < 0)
	    opts->guiport = 0;
    }
}

void
__pmAddOptArchive(pmOptions *opts, char *arg)
{
    char **archives = opts->archives;
    size_t size = sizeof(char *) * (opts->narchives + 1);

    if (opts->narchives && !(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: too many archives requested: %s\n", pmProgname, arg);
	opts->errors++;
    } else if (opts->nhosts && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one host or archive allowed\n", pmProgname);
	opts->errors++;
    } else if ((archives = realloc(archives, size)) != NULL) {
	archives[opts->narchives] = arg;
	opts->archives = archives;
	opts->narchives++;
    } else {
	__pmNoMem("pmGetOptions(archive)", size, PM_FATAL_ERR);
    }
}

static char *
comma_or_end(const char *start)
{
    char *end;

    if ((end = strchr(start, ',')) != NULL)
	return end;
    if (*start == '\0')
	return NULL;
    end = (char *)start + strlen(start);
    return end;
}

void
__pmAddOptArchiveList(pmOptions *opts, char *arg)
{
    char *start = arg, *end;

    if (!(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: too many archives requested: %s\n", pmProgname, arg);
	opts->errors++;
    } else if (opts->nhosts && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one of hosts or archives allowed\n", pmProgname);
	opts->errors++;
    } else {
	while ((end = comma_or_end(start)) != NULL) {
	    size_t size = sizeof(char *) * (opts->narchives + 1);
	    size_t length = end - start;
	    char **archives = opts->archives;
	    char *archive;

	    if (length == 0)
		goto next;

	    if ((archives = realloc(archives, size)) != NULL) {
		if ((archive = strndup(start, length)) != NULL) {
		    archives[opts->narchives] = archive;
		    opts->archives = archives;
		    opts->narchives++;
		} else {
		    __pmNoMem("pmGetOptions(archive)", length, PM_FATAL_ERR);
		}
	    } else {
		__pmNoMem("pmGetOptions(archives)", size, PM_FATAL_ERR);
	    }
	next:
	    start = (*end == '\0') ? end : end + 1;
	}
    }
}

void
__pmAddOptHost(pmOptions *opts, char *arg)
{
    char **hosts = opts->hosts;
    size_t size = sizeof(char *) * (opts->nhosts + 1);

    if (opts->nhosts && !(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: too many hosts requested: %s\n", pmProgname, arg);
	opts->errors++;
    } else if (opts->narchives && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one host or archive allowed\n", pmProgname);
	opts->errors++;
    } else if ((hosts = realloc(hosts, size)) != NULL) {
	hosts[opts->nhosts] = arg;
	opts->hosts = hosts;
	opts->nhosts++;
    } else {
	__pmNoMem("pmGetOptions(host)", size, PM_FATAL_ERR);
    }
}

static inline char *
skip_whitespace(char *p)
{
    while (*p && isspace((int)*p) && *p != '\n')
	p++;
    return p;
}

static inline char *
skip_nonwhitespace(char *p)
{
    while (*p && !isspace((int)*p))
	p++;
    return p;
}

void
__pmAddOptArchiveFolio(pmOptions *opts, char *arg)
{
    char buffer[MAXPATHLEN];
    FILE *fp;

#define FOLIO_MAGIC	"PCPFolio"
#define FOLIO_VERSION	"Version: 1"

    if (opts->nhosts && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one of hosts or archives allowed\n", pmProgname);
	opts->errors++;
    } else if (arg == NULL) {
	pmprintf("%s: cannot open empty archive folio name\n", pmProgname);
	opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
	opts->errors++;
    } else if ((fp = fopen(arg, "r")) == NULL) {
	pmprintf("%s: cannot open archive folio %s: %s\n", pmProgname,
		arg, pmErrStr_r(-oserror(), buffer, sizeof(buffer)));
	opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
	opts->errors++;
    } else {
	size_t length;
	char *p, *log, *dir;
	int line, sep = __pmPathSeparator();

	if (fgets(buffer, sizeof(buffer)-1, fp) == NULL) {
	    pmprintf("%s: archive folio %s has no header\n", pmProgname, arg);
	    goto badfolio;
	}
	if (strncmp(buffer, FOLIO_MAGIC, sizeof(FOLIO_MAGIC)-1) != 0) {
	    pmprintf("%s: archive folio %s has bad magic\n", pmProgname, arg);
	    goto badfolio;
	}
	if (fgets(buffer, sizeof(buffer)-1, fp) == NULL) {
	    pmprintf("%s: archive folio %s has no version\n", pmProgname, arg);
	    goto badfolio;
	}
	if (strncmp(buffer, FOLIO_VERSION, sizeof(FOLIO_VERSION)-1) != 0) {
	    pmprintf("%s: unknown version archive folio %s\n", pmProgname, arg);
	    goto badfolio;
	}

	line = 2;
	dir = dirname(arg);

	while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
	    line++;
	    p = buffer;

	    if (strncmp(p, "Archive:", sizeof("Archive:")-1) != 0)
		continue;
	    p = skip_nonwhitespace(p);
	    p = skip_whitespace(p);
	    if (*p == '\n') {
		pmprintf("%s: missing host on archive folio line %d\n",
			pmProgname, line);
		goto badfolio;
	    }
	    p = skip_nonwhitespace(p);
	    p = skip_whitespace(p);
	    if (*p == '\n') {
		pmprintf("%s: missing path on archive folio line %d\n",
			pmProgname, line);
		goto badfolio;
	    }

	    log = p;
	    p = skip_nonwhitespace(p);
	    *p = '\0';

	    length = strlen(dir) + 1 + strlen(log) + 1;
	    if ((p = (char *)malloc(length)) == NULL)
		__pmNoMem("pmGetOptions(archive)", length, PM_FATAL_ERR);
	    snprintf(p, length, "%s%c%s", dir, sep, log);
	    __pmAddOptArchive(opts, p);
	}

	fclose(fp);
    }
    return;

badfolio:
    fclose(fp);
    opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
    opts->errors++;
}

static void
__pmAddOptHostFile(pmOptions *opts, char *arg)
{
    if (!(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: too many hosts requested: %s\n", pmProgname, arg);
	opts->errors++;
    } else if (opts->narchives && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one of hosts or archives allowed\n", pmProgname);
	opts->errors++;
    } else {
	FILE *fp = fopen(arg, "r");

	if (fp) {
	    char buffer[MAXHOSTNAMELEN];

	    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
		size_t size = sizeof(char *) * (opts->nhosts + 1);
		char **hosts = opts->hosts;
		char *host, *p = buffer;
		size_t length;

		while (isspace((int)*p) && *p != '\n')
		    p++;
		if (*p == '\n' || *p == '#')
		    continue;
		host = p;
		length = 0;
		while (*p != '\n' && *p != '#' && !isspace((int)*p)) {
		    length++;
		    p++;
		}
		*p = '\0';
		if ((hosts = realloc(hosts, size)) != NULL) {
		    if ((host = strndup(host, length)) != NULL) {
			hosts[opts->nhosts] = host;
			opts->hosts = hosts;
			opts->nhosts++;
		    } else {
			__pmNoMem("pmGetOptions(host)", length, PM_FATAL_ERR);
		    }
		} else {
		    __pmNoMem("pmGetOptions(hosts)", size, PM_FATAL_ERR);
		}
	    }

	    fclose(fp);
	} else {
	    char errmsg[PM_MAXERRMSGLEN];

	    pmprintf("%s: cannot open hosts file %s: %s\n", pmProgname, arg,
		    osstrerror_r(errmsg, sizeof(errmsg)));
	    opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
	    opts->errors++;
	}
    }
}

void
__pmAddOptHostList(pmOptions *opts, char *arg)
{
    if (!(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: too many hosts requested: %s\n", pmProgname, arg);
	opts->errors++;
    } else if (opts->narchives && !(opts->flags & PM_OPTFLAG_MIXED)) {
	pmprintf("%s: only one of hosts or archives allowed\n", pmProgname);
	opts->errors++;
    } else {
	char *start = arg, *end;

	while ((end = comma_or_end(start)) != NULL) {
	    size_t size = sizeof(char *) * (opts->nhosts + 1);
	    size_t length = end - start;
	    char **hosts = opts->hosts;
	    char *host;

	    if (length == 0)
		goto next;

	    if ((hosts = realloc(hosts, size)) != NULL) {
		if ((host = strndup(start, length)) != NULL) {
		    hosts[opts->nhosts] = host;
		    opts->hosts = hosts;
		    opts->nhosts++;
		} else {
		    __pmNoMem("pmGetOptions(host)", length, PM_FATAL_ERR);
		}
	    } else {
		__pmNoMem("pmGetOptions(hosts)", size, PM_FATAL_ERR);
	    }
	next:
	    start = (*end == '\0') ? end : end + 1;
	}
    }
}

static void
__pmAddOptContainer(pmOptions *opts, char *arg)
{
    char buffer[MAXPATHLEN+16];

    (void)opts;
    snprintf(buffer, sizeof(buffer), "%s=%s", "PCP_CONTAINER", arg? arg : "");
    putenv(buffer);
}

static void
__pmSetLocalContextTable(pmOptions *opts, char *arg)
{
    char *errmsg;

    if ((errmsg = __pmSpecLocalPMDA(arg)) != NULL) {
	pmprintf("%s: __pmSpecLocalPMDA failed: %s\n", pmProgname, errmsg);
	opts->errors++;
    }
}

static void
__pmSetLocalContextFlag(pmOptions *opts)
{
    if (opts->context && !(opts->flags & PM_OPTFLAG_MULTI)) {
	pmprintf("%s: at most one of -a, -h and -L allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->Lflag = 1;
    }
}

static void
__pmSetNameSpace(pmOptions *opts, char *arg, int dupok)
{
    int sts;

    if ((sts = pmLoadASCIINameSpace(arg, dupok)) < 0) {
	pmprintf("%s: Cannot load namespace from \"%s\": %s\n",
		pmProgname, arg, pmErrStr(sts));
	opts->flags |= PM_OPTFLAG_RUNTIME_ERR;
	opts->errors++;
    } else {
	opts->nsflag = 1;
    }
}

static void
__pmSetSampleCount(pmOptions *opts, char *arg)
{
    char *endnum;

    if (opts->finish_optarg) {
	pmprintf("%s: at most one of -T and -s allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->samples = (int)strtol(arg, &endnum, 10);
	if (*endnum != '\0' || opts->samples < 0) {
	    pmprintf("%s: -s requires numeric argument\n", pmProgname);
	    opts->errors++;
	}
    }
}

static void
__pmSetFinishTime(pmOptions *opts, char *arg)
{
    if (opts->samples) {
	pmprintf("%s: at most one of -T and -s allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->finish_optarg = arg;
    }
}

static void
__pmSetSampleInterval(pmOptions *opts, char *arg)
{
    char *endnum;

    if (pmParseInterval(arg, &opts->interval, &endnum) < 0) {
	pmprintf("%s: -t argument not in pmParseInterval(3) format:\n",
		pmProgname);
	pmprintf("%s\n", endnum);
	opts->errors++;
	free(endnum);
    }
}

static void
__pmSetTimeZone(pmOptions *opts, char *arg)
{
    if (opts->tzflag) {
	pmprintf("%s: at most one of -Z and -z allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->timezone = arg;
    }
}

static void
__pmSetHostZone(pmOptions *opts)
{
    if (opts->timezone) {
	pmprintf("%s: at most one of -Z and -z allowed\n", pmProgname);
	opts->errors++;
    } else {
	opts->tzflag = 1;
    }
}

/*
 * Called once at the start of option processing, before any getopt calls.
 * For our needs, we can set default values at this point based on values
 * we find set in the processes environment.
 */
void
__pmStartOptions(pmOptions *opts)
{
    extern char **_environ;
    char **p, *s, *value = NULL;

    if (opts->flags & PM_OPTFLAG_INIT)
	return;

    for (p = _environ; *p != NULL; p++) {
	s = *p;
	if (strncmp(s, "PCP_", 4) != 0)
	    continue;	/* short circuit if not PCP-prefixed */
	s += 4;
	if ((value = strchr(s, '=')) != NULL) {
	    *value = '\0';
	    value++;	/* skip over the equals sign */
	}

	if (strcmp(s, "ALIGN_TIME") == 0)
	    __pmSetAlignment(opts, value);
	else if (strcmp(s, "ARCHIVE") == 0)
	    __pmAddOptArchive(opts, value);
	else if (strcmp(s, "ARCHIVE_LIST") == 0)
	    __pmAddOptArchiveList(opts, value);
	else if (strcmp(s, "DEBUG") == 0)
	    __pmSetDebugFlag(opts, value);
	else if (strcmp(s, "FOLIO") == 0)
	    __pmAddOptArchiveFolio(opts, value);
	else if (strcmp(s, "GUIMODE") == 0)
	    __pmSetGuiModeFlag(opts);
	else if (strcmp(s, "HOST") == 0)
	    __pmAddOptHost(opts, value);
	else if (strcmp(s, "HOST_LIST") == 0)
	    __pmAddOptHostList(opts, value);
	else if (strcmp(s, "LOCALMODE") == 0)
	    __pmSetLocalContextFlag(opts);
	else if (strcmp(s, "NAMESPACE") == 0)
	    __pmSetNameSpace(opts, value, 1);
	else if (strcmp(s, "UNIQNAMES") == 0)
	    __pmSetNameSpace(opts, value, 0);
	else if (strcmp(s, "ORIGIN") == 0 ||
		 strcmp(s, "ORIGIN_TIME") == 0)
	    __pmSetOrigin(opts, value);
	else if (strcmp(s, "GUIPORT") == 0)
	    __pmSetGuiPort(opts, value);
	else if (strcmp(s, "START_TIME") == 0)
	    __pmSetStartTime(opts, value);
	else if (strcmp(s, "SAMPLES") == 0)
	    __pmSetSampleCount(opts, value);
	else if (strcmp(s, "FINISH_TIME") == 0)
	    __pmSetFinishTime(opts, value);
	else if (strcmp(s, "INTERVAL") == 0)
	    __pmSetSampleInterval(opts, value);
	else if (strcmp(s, "TIMEZONE") == 0)
	    __pmSetTimeZone(opts, value);
	else if (strcmp(s, "HOSTZONE") == 0)
	    __pmSetHostZone(opts);

	if (value)		/* reset the environment */
	    *(value-1) = '=';
    }

    opts->flags |= PM_OPTFLAG_INIT;
}

int
pmGetOptions(int argc, char *const *argv, pmOptions *opts)
{
    pmLongOptions *opt;
    int flag = 0;
    int c = EOF;

    if (!(opts->flags & PM_OPTFLAG_INIT)) {
	__pmSetProgname(argv[0]);
	opts->__initialized = 1;
	__pmStartOptions(opts);
    }

    /* environment has been checked at this stage, leave opt* well alone */
    if (opts->flags & PM_OPTFLAG_ENV_ONLY) {
	__pmEndOptions(opts);
	return EOF;
    }

    while (!flag) {
	c = pmgetopt_r(argc, argv, opts);

	/* provide opportunity for overriding the general set of options */
	if (c != EOF && opts->override && opts->override(c, opts))
	    break;

	switch (c) {
	case 'A':
	    __pmSetAlignment(opts, opts->optarg);
	    break;
	case 'a':
	    __pmAddOptArchive(opts, opts->optarg);
	    break;
	case 'D':
	    __pmSetDebugFlag(opts, opts->optarg);
	    break;
	case 'g':
	    __pmSetGuiModeFlag(opts);
	    break;
	case 'H':
	    __pmAddOptHostFile(opts, opts->optarg);
	    break;
	case 'h':
	    __pmAddOptHost(opts, opts->optarg);
	    break;
	case 'K':
	    __pmSetLocalContextTable(opts, opts->optarg);
	    break;
	case 'L':
	    __pmSetLocalContextFlag(opts);
	    break;
	case 'N':
	    __pmSetNameSpace(opts, opts->optarg, 0);
	    break;
	case 'n':
	    __pmSetNameSpace(opts, opts->optarg, 1);
	    break;
	case 'O':
	    __pmSetOrigin(opts, opts->optarg);
	    break;
	case 'p':
	    __pmSetGuiPort(opts, opts->optarg);
	    break;
	case 'S':
	    __pmSetStartTime(opts, opts->optarg);
	    break;
	case 's':
	    __pmSetSampleCount(opts, opts->optarg);
	    break;
	case 'T':
	    __pmSetFinishTime(opts, opts->optarg);
	    break;
	case 't':
	    __pmSetSampleInterval(opts, opts->optarg);
	    break;
	case 'V':
	    opts->flags |= PM_OPTFLAG_EXIT;
	    pmprintf("%s version %s\n", pmProgname, PCP_VERSION);
	    break;
	case 'Z':
	    __pmSetTimeZone(opts, opts->optarg);
	    break;
	case 'z':
	    __pmSetHostZone(opts);
	    break;
	case '?':
	    opts->errors++;
	    break;
	case 0:
	    /* long-option-only standard argument handling */
	    opt = &opts->long_options[opts->index];
	    if (strcmp(opt->long_opt, PMLONGOPT_HOST_LIST) == 0)
		__pmAddOptHostList(opts, opts->optarg);
	    else if (strcmp(opt->long_opt, PMLONGOPT_ARCHIVE_LIST) == 0)
		__pmAddOptArchiveList(opts, opts->optarg);
	    else if (strcmp(opt->long_opt, PMLONGOPT_ARCHIVE_FOLIO) == 0)
		__pmAddOptArchiveFolio(opts, opts->optarg);
	    else if (strcmp(opt->long_opt, PMLONGOPT_CONTAINER) == 0)
		__pmAddOptContainer(opts, opts->optarg);
	    else
		flag = 1;
	    break;
	default:	/* pass back out to caller */
	    flag = 1;
	}
    }

    /* end of arguments - process everything we can now */
    if (c == EOF)
	__pmEndOptions(opts);
    return c;
}

void
pmFreeOptions(pmOptions *opts)
{
    if (opts->narchives)
	free(opts->archives);
    if (opts->nhosts)
	free(opts->hosts);
}

void
pmUsageMessage(pmOptions *opts)
{
    pmLongOptions *option;
    const char *message;
    int bytes;

    if (opts->flags & (PM_OPTFLAG_RUNTIME_ERR|PM_OPTFLAG_EXIT))
	goto flush;

    message = opts->short_usage ? opts->short_usage : "[options]";
    pmprintf("Usage: %s %s\n", pmProgname, message);

    for (option = opts->long_options; option; option++) {
	if (!option->long_opt)	/* sentinel */
	    break;
	if (!option->message)	/* undocumented option */
	    continue;
	if (option->short_opt == '-') {	/* section header */
	    pmprintf("\n%s:\n", option->message);
	    continue;
        }
	if (option->short_opt == '|') {	/* descriptive text */
	    pmprintf("%s\n", option->message);
	    continue;
        }

	message = option->argname ? option->argname : "?";
	if (option->long_opt && option->long_opt[0] != '\0') {
	    if (option->short_opt && option->has_arg)
		bytes = pmprintf("  -%c %s, --%s=%s", option->short_opt,
				message, option->long_opt, message);
	    else if (option->short_opt)
		bytes = pmprintf("  -%c, --%s", option->short_opt,
				option->long_opt);
	    else if (option->has_arg)
		bytes = pmprintf("  --%s=%s", option->long_opt, message);
	    else
		bytes = pmprintf("  --%s", option->long_opt);
	} else {	/* short option with no long option */
	    if (option->has_arg)
		bytes = pmprintf("  -%c %s", option->short_opt, message);
	    else
		bytes = pmprintf("  -%c", option->short_opt);
	}

	if (bytes < 24)		/* message will fit here */
	    pmprintf("%*s%s\n", 24 - bytes, "", option->message);
	else			/* message on next line */
	    pmprintf("\n%24s%s\n", "", option->message);
    }
flush:
    if (!(opts->flags & PM_OPTFLAG_NOFLUSH))
	pmflush();
}

/*
 * Exchange two adjacent subsequences of ARGV.
 * One subsequence is elements [first_nonopt,last_nonopt)
 * which contains all the non-options that have been skipped so far.
 * The other is elements [last_nonopt,optind), which contains all
 * the options processed since those non-options were skipped.

 * `first_nonopt' and `last_nonopt' are relocated so that they describe
 * the new indices of the non-options in ARGV after they are moved.
 */
static void
__pmgetopt_exchange(char **argv, pmOptions *d)
{
    int bottom = d->__first_nonopt;
    int middle = d->__last_nonopt;
    int top = d->optind;
    char *tem;

    /*
     * Exchange the shorter segment with the far end of the longer segment.
     * That puts the shorter segment into the right place.
     * It leaves the longer segment in the right place overall,
     * but it consists of two parts that need to be swapped next.
     */
    while (top > middle && middle > bottom) {
	if (top - middle > middle - bottom) {
	    /* Bottom segment is the short one. */
	    int len = middle - bottom;
	    int i;

	    /* Swap it with the top part of the top segment. */
	    for (i = 0; i < len; i++) {
		tem = argv[bottom + i];
		argv[bottom + i] = argv[top - (middle - bottom) + i];
		argv[top - (middle - bottom) + i] = tem;
	    }
	    /* Exclude the moved bottom segment from further swapping. */
	    top -= len;
	}
	else {
	    /* Top segment is the short one.  */
	    int len = top - middle;
	    int i;

	    /* Swap it with the bottom part of the bottom segment. */
	    for (i = 0; i < len; i++) {
		tem = argv[bottom + i];
		argv[bottom + i] = argv[middle + i];
		argv[middle + i] = tem;
	    }
	    /* Exclude the moved top segment from further swapping. */
	    bottom += len;
	}
    }

    /* Update records for the slots the non-options now occupy. */
    d->__first_nonopt += (d->optind - d->__last_nonopt);
    d->__last_nonopt = d->optind;
}

/*
 * Initialize the internal data when the first getopt call is made.
 */
static const char *
__pmgetopt_initialize(int argc, char *const *argv, pmOptions *d)
{
    const char *optstring = d->short_options;
    int posixly_correct = !!(d->flags & PM_OPTFLAG_POSIX);

    /* Start processing options with ARGV-element 1 (since ARGV-element 0
     * is the program name); the sequence of previously skipped
     * non-option ARGV-elements is empty.
     */
    d->__first_nonopt = d->__last_nonopt = d->optind;
    d->__nextchar = NULL;
    d->__posixly_correct = posixly_correct | !!getenv ("POSIXLY_CORRECT");

    /* Determine how to handle the ordering of options and nonoptions. */
    if (optstring[0] == '-') {
	d->__ordering = RETURN_IN_ORDER;
	++optstring;
    }
    else if (optstring[0] == '+') {
	d->__ordering = REQUIRE_ORDER;
	++optstring;
    }
    else if (d->__posixly_correct)
	d->__ordering = REQUIRE_ORDER;
    else
	d->__ordering = PERMUTE;

  return optstring;
}

/*
 * Scan elements of ARGV (whose length is ARGC) for option characters
 * given in OPTSTRING.
 *
 * If an element of ARGV starts with '-', and is not exactly "-" or "--",
 * then it is an option element.  The characters of this element
 * (aside from the initial '-') are option characters.  If `getopt'
 * is called repeatedly, it returns successively each of the option characters
 * from each of the option elements.
 *
 * If `getopt' finds another option character, it returns that character,
 * updating `optind' and `nextchar' so that the next call to `getopt' can
 * resume the scan with the following option character or ARGV-element.
 *
 * If there are no more option characters, `getopt' returns -1.
 * Then `optind' is the index in ARGV of the first ARGV-element
 * that is not an option.  (The ARGV-elements have been permuted
 * so that those that are not options now come last.)
 *
 * OPTSTRING is a string containing the legitimate option characters.
 * If an option character is seen that is not listed in OPTSTRING,
 * return '?' after printing an error message.  If you set `opterr' to
 * zero, the error message is suppressed but we still return '?'.
 *
 * If a char in OPTSTRING is followed by a colon, that means it wants an arg,
 * so the following text in the same ARGV-element, or the text of the following
 * ARGV-element, is returned in `optarg'.  Two colons mean an option that
 * wants an optional arg; if there is text in the current ARGV-element,
 * it is returned in `optarg', otherwise `optarg' is set to zero.
 *
 * If OPTSTRING starts with `-' or `+', it requests different methods of
 * handling the non-option ARGV-elements.
 * See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.
 *
 * Long-named options begin with `--' instead of `-'.
 * Their names may be abbreviated as long as the abbreviation is unique
 * or is an exact match for some defined option.  If they have an
 * argument, it follows the option name in the same ARGV-element, separated
 * from the option name by a `=', or else the in next ARGV-element.
 * When `getopt' finds a long-named option, it returns 0 if that option's
 * `flag' field is nonzero, the value of the option's `val' field
 * if the `flag' field is zero.
 *
 * The elements of ARGV aren't really const, because we permute them.
 * But we pretend they're const in the prototype to be compatible
 * with other systems.
 *
 * LONGOPTS is a vector of `pmOptions' terminated by an
 * element containing a name which is zero.
 *
 * LONGIND returns the index in LONGOPT of the long-named option found.
 * It is only valid when a long-named option has been found by the most
 * recent call.
 *
 * If LONG_ONLY is nonzero, '-' as well as '--' can introduce
 * long-named options.
 */

typedef struct pmOptList {
    pmLongOptions *	p;
    struct pmOptList *	next;
} pmOptionsList;

int
pmgetopt_r(int argc, char *const *argv, pmOptions *d)
{
    const char *optstring = d->short_options;
    pmLongOptions *longopts = d->long_options;
    int *longind = &d->index;
    int long_only = (d->flags & PM_OPTFLAG_LONG_ONLY);
    int quiet = (d->flags & PM_OPTFLAG_QUIET);
    int print_errors = d->opterr || !quiet;

    if (argc < 1 || !optstring)
	return -1;

    d->optarg = NULL;

    if (d->optind == 0 || d->__initialized <= 1) {
	if (d->optind == 0)
	    d->optind = 1;	/* Don't scan ARGV[0], the program name.  */
	if (!d->__initialized)
	    __pmSetProgname(argv[0]);
	optstring = __pmgetopt_initialize(argc, argv, d);
	d->__initialized = 2;
    }
    else if (optstring[0] == '-' || optstring[0] == '+')
	optstring++;
    if (optstring[0] == ':')
	print_errors = 0;

    /* Test whether ARGV[optind] points to a non-option argument.
     * Either it does not have option syntax, or there is an environment flag
     * from the shell indicating it is not an option.  The later information
     * is only used when the used in the GNU libc.
     */

    if (d->__nextchar == NULL || *d->__nextchar == '\0') {
	/* Advance to the next ARGV-element.  */

	/* Give FIRST_NONOPT & LAST_NONOPT rational values if OPTIND has been
	 * moved back by the user (who may also have changed the arguments).
	 */
	if (d->__last_nonopt > d->optind)
	    d->__last_nonopt = d->optind;
	if (d->__first_nonopt > d->optind)
	    d->__first_nonopt = d->optind;

	if (d->__ordering == PERMUTE) {
	    /* If we have just processed options following some non-options,
	     * exchange them so that the options come first.
	     */
	    if (d->__first_nonopt != d->__last_nonopt
		&& d->__last_nonopt != d->optind)
		__pmgetopt_exchange((char **) argv, d);
	    else if (d->__last_nonopt != d->optind)
		d->__first_nonopt = d->optind;

	    /* Skip any additional non-options and
	     * extend the range of non-options previously skipped.
	     */
	    while (d->optind < argc && \
		   (argv[d->optind][0] != '-' || argv[d->optind][1] == '\0'))
		d->optind++;
	    d->__last_nonopt = d->optind;
	}

	/*
	 * The special ARGV-element `--' means premature end of options.
	 * Skip it like a null option,
	 * then exchange with previous non-options as if it were an option,
	 * then skip everything else like a non-option.
	 */
	if (d->optind != argc && !strcmp(argv[d->optind], "--")) {
	    d->optind++;

	    if (d->__first_nonopt != d->__last_nonopt
		&& d->__last_nonopt != d->optind)
		__pmgetopt_exchange((char **) argv, d);
	    else if (d->__first_nonopt == d->__last_nonopt)
		d->__first_nonopt = d->optind;
	    d->__last_nonopt = argc;
	    d->optind = argc;
	}

	/* If we have done all the ARGV-elements, stop the scan
	 * and back over any non-options that we skipped and permuted.
	 */
	if (d->optind == argc) {
	    /* Set the next-arg-index to point at the non-options
	     * that we previously skipped, so the caller will digest them.
	     */
	    if (d->__first_nonopt != d->__last_nonopt)
		d->optind = d->__first_nonopt;
	    return -1;
	}

	/* If we have come to a non-option and did not permute it,
	 * either stop the scan or describe it to the caller and pass it by.o
	 */
	if (argv[d->optind][0] != '-' || argv[d->optind][1] == '\0') {
	    if (d->__ordering == REQUIRE_ORDER)
		return -1;
	    d->optarg = argv[d->optind++];
	    return 1;
	}

	/* We have found another option-ARGV-element.
	 * Skip the initial punctuation.
	 */
	d->__nextchar = (argv[d->optind] + 1
		  + (longopts != NULL && argv[d->optind][1] == '-'));
    }

    /* Decode the current option-ARGV-element.  */

    /*
     * Check whether the ARGV-element is a long option.
     *
     * If long_only and the ARGV-element has the form "-f", where f is
     * a valid short option, don't consider it an abbreviated form of
     * a long option that starts with f.  Otherwise there would be no
     * way to give the -f short option.
     *
     * On the other hand, if there's a long option "fubar" and
     * the ARGV-element is "-fu", do consider that an abbreviation of
     * the long option, just like "--fu", and not "-f" with arg "u".
     *
     * This distinction seems to be the most useful approach.
     */
    if (longopts != NULL
	&& (argv[d->optind][1] == '-'
	    || (long_only && (argv[d->optind][2]
		  || !strchr(optstring, argv[d->optind][1]))))) {
	char *nameend;
	unsigned int namelen;
	pmLongOptions *p;
	pmLongOptions *pfound = NULL;
	pmOptionsList *ambig_list = NULL;
	int exact = 0;
	int indfound = -1;
	int option_index;

	for (nameend = d->__nextchar; *nameend && *nameend != '='; nameend++)
	    /* Do nothing.  */ ;
	namelen = nameend - d->__nextchar;

	/* Test all long options for either exact match
	 * or abbreviated matches.
	 */
	for (p = longopts, option_index = 0; p->long_opt; p++, option_index++) {
	    if (!strncmp(p->long_opt, d->__nextchar, namelen)) {
		if (namelen == (unsigned int) strlen(p->long_opt)) {
		    /* Exact match found.  */
		    pfound = p;
		    indfound = option_index;
		    exact = 1;
		    break;
		}
		else if (pfound == NULL) {
		    /* First nonexact match found.  */
		    pfound = p;
		    indfound = option_index;
		}
		else if (long_only
			|| pfound->has_arg != p->has_arg
			|| pfound->short_opt != p->short_opt) {
		    /* Second or later nonexact match found.  */
		    pmOptionsList *newp = malloc(sizeof(*newp));
		    newp->p = p;
		    newp->next = ambig_list;
		    ambig_list = newp;
		}
	    }
	}

	if (ambig_list != NULL && !exact) {
	    if (print_errors) {
		pmOptionsList first;
		first.p = pfound;
		first.next = ambig_list;
		ambig_list = &first;

		pmprintf("%s: option '%s' is ambiguous; possibilities:",
			pmProgname, argv[d->optind]);
		do {
		    pmprintf(" '--%s'", ambig_list->p->long_opt);
		    ambig_list = ambig_list->next;
		} while (ambig_list != NULL);
		pmprintf("\n");
	    }
	    d->__nextchar += strlen(d->__nextchar);
	    d->optind++;
	    d->optopt = 0;
	    free(ambig_list);
	    return '?';
	}
	else if (ambig_list != NULL) {
	    free(ambig_list);
	}

	if (pfound != NULL) {
	    option_index = indfound;
	    d->optind++;
	    if (*nameend) {
		if (pfound->has_arg) {
		    d->optarg = nameend + 1;
		} else {
		    if (print_errors) {
			if (argv[d->optind - 1][1] == '-') {
			    /* --option */
			    pmprintf("%s: option '--%s' doesn't allow an argument\n",
				     pmProgname, pfound->long_opt);
			} else {
			    /* +option or -option */
			    pmprintf("%s: option '%c%s' doesn't allow an argument\n",
				     pmProgname, argv[d->optind - 1][0],
				     pfound->long_opt);
			}
		    }
		    d->__nextchar += strlen(d->__nextchar);
		    d->optopt = pfound->short_opt;
		    return '?';
		}
	    }
	    else if (pfound->has_arg == 1) {
		if (d->optind < argc) {
		    d->optarg = argv[d->optind++];
		} else {
		    if (print_errors) {
			pmprintf("%s: option '--%s' requires an argument\n",
				pmProgname, pfound->long_opt);
		    }
		    d->__nextchar += strlen(d->__nextchar);
		    d->optopt = pfound->short_opt;
		    return optstring[0] == ':' ? ':' : '?';
		}
	    }
	    d->__nextchar += strlen(d->__nextchar);
	    if (longind != NULL)
		*longind = option_index;
	    return pfound->short_opt;
	}

	/* Can't find it as a long option.  If this is not a long-only form,
	 * or the option starts with '--', or is not a valid short option,
	 * then it's an error.
	 * Otherwise interpret it as a short option.
	 */
	if (!long_only || argv[d->optind][1] == '-'
	    || strchr (optstring, *d->__nextchar) == NULL) {
	    if (print_errors) {
		if (argv[d->optind][1] == '-') {
		    /* --option */
		    pmprintf("%s: unrecognized option '--%s'\n",
			    pmProgname, d->__nextchar);
		} else {
		    /* +option or -option */
		    pmprintf("%s: unrecognized option '%c%s'\n",
			    pmProgname, argv[d->optind][0], d->__nextchar);
		}
	    }
	    d->__nextchar = (char *) "";
	    d->optind++;
	    d->optopt = 0;
	    return '?';
	}
    }

    /* Look at and handle the next short option-character.  */

    {
	char c = *d->__nextchar++;
	char *temp = strchr(optstring, c);

	/* Increment `optind' when we start to process its last character.  */
	if (*d->__nextchar == '\0')
	    ++d->optind;

	if (temp == NULL || c == ':' || c == ';') {
	    if (print_errors) {
		pmprintf("%s: invalid option -- '%c'\n", pmProgname, c);
	    }
	    d->optopt = c;
	    return '?';
	}
	/* Convenience. Treat POSIX -W foo same as long option --foo */
	if (temp[0] == 'W' && temp[1] == ';') {
	    if (longopts == NULL)
		goto no_longs;

	char *nameend;
	pmLongOptions *p;
	pmLongOptions *pfound = NULL;
	int exact = 0;
	int ambig = 0;
	int indfound = 0;
	int option_index;

	/* This is an option that requires an argument. */
	if (*d->__nextchar != '\0') {
	    d->optarg = d->__nextchar;
	    /* If we end this ARGV-element by taking the rest as an arg,
	     * we must advance to the next element now.
	     */
	    d->optind++;
	}
	else if (d->optind == argc) {
	    if (print_errors) {
		pmprintf("%s: option requires an argument -- '%c'\n",
			 pmProgname, c);
	    }
	    d->optopt = c;
	    if (optstring[0] == ':')
		c = ':';
	    else
		c = '?';
	    return c;
	}
	else {
	    /* We already incremented `d->optind' once;
	     * increment it again when taking next ARGV-elt as argument.
	     */
	    d->optarg = argv[d->optind++];
	}

	/* optarg is now the argument, see if it's in the table of longopts. */

	for (d->__nextchar = nameend = d->optarg; *nameend && *nameend != '=';
	     nameend++)
	    /* Do nothing */ ;

	/* Test all long options for either exact or abbreviated matches. */
	for (p = longopts, option_index = 0; p->long_opt; p++, option_index++)
	    if (!strncmp(p->long_opt, d->__nextchar, nameend - d->__nextchar)) {
		if ((unsigned int)(nameend - d->__nextchar) == strlen(p->long_opt)) {
		    /* Exact match found.  */
		    pfound = p;
		    indfound = option_index;
		    exact = 1;
		    break;
		}
		else if (pfound == NULL) {
		    /* First nonexact match found.  */
		    pfound = p;
		    indfound = option_index;
		}
		else if (long_only
			|| pfound->has_arg != p->has_arg
			|| pfound->short_opt != p->short_opt) {
		    /* Second or later nonexact match found. */
		    ambig = 1;
		}
	    }
	    if (ambig && !exact) {
		if (print_errors) {
		    pmprintf("%s: option '-W %s' is ambiguous\n",
			     pmProgname, d->optarg);
		}
		d->__nextchar += strlen(d->__nextchar);
		d->optind++;
		return '?';
	    }
	    if (pfound != NULL) {
		option_index = indfound;
		if (*nameend) {
		    if (pfound->has_arg) {
			d->optarg = nameend + 1;
		    } else {
			if (print_errors) {
			    pmprintf("%s: option '-W %s' doesn't allow an argument\n",
				     pmProgname, pfound->long_opt);
			}
			d->__nextchar += strlen(d->__nextchar);
			return '?';
		    }
		}
		else if (pfound->has_arg == 1) {
		    if (d->optind < argc) {
			d->optarg = argv[d->optind++];
		    } else {
			if (print_errors) {
			    pmprintf("%s: option '-W %s' requires an argument\n",
				     pmProgname, pfound->long_opt);
			}
			d->__nextchar += strlen(d->__nextchar);
			return optstring[0] == ':' ? ':' : '?';
		    }
		}
		else {
		    d->optarg = NULL;
		}
		d->__nextchar += strlen(d->__nextchar);
		if (longind != NULL)
		    *longind = option_index;
		return pfound->short_opt;
	    }

        no_longs:
	    d->__nextchar = NULL;
	    return 'W';	/* Let the application handle it. */
	}
	if (temp[1] == ':') {
	    if (temp[2] == ':') {
		/* This is an option that accepts an argument optionally. */
		if (*d->__nextchar != '\0') {
		    d->optarg = d->__nextchar;
		    d->optind++;
		} else {
		    d->optarg = NULL;
		}
		d->__nextchar = NULL;
	    }
	    else {
		/* This is an option that requires an argument. */
		if (*d->__nextchar != '\0') {
		    d->optarg = d->__nextchar;
		    /* If we end this ARGV-element by taking the rest as an arg,
		     * we must advance to the next element now.
		     */
		    d->optind++;
		}
		else if (d->optind == argc) {
		    if (print_errors) {
			pmprintf("%s: option requires an argument -- '%c'\n",
				 pmProgname, c);
		    }
		    d->optopt = c;
		    if (optstring[0] == ':')
			c = ':';
		    else
			c = '?';
		}
		else {
		    /* We already incremented `optind' once;
		     * increment it again when taking next ARGV-elt as argument.
		     */
		    d->optarg = argv[d->optind++];
		}
	        d->__nextchar = NULL;
	    }
	}
	return c;
    }
}
