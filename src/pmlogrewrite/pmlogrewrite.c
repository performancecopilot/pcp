/*
 * pmlogrewrite - config-driven stream editor for PCP archives
 *
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>
#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <assert.h>

global_t	global;
indomspec_t	*indom_root;
metricspec_t	*metric_root;
int		lineno;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "config", 1, 'c', "PATH", "file or directory to load rules from" },
    { "check", 0, 'C', 0, "parse config file(s) and quit (verbose warnings also)" },
    { "desperate", 0, 'd', 0, "desperate, save output archive even after error" },
    { "", 0, 'i', 0, "rewrite in place, input-archive will be over-written" },
    { "quick", 0, 'q', 0, "quick mode, no output if no change" },
    { "scale", 0, 's', 0, "do scale conversion" },
    { "verbose", 0, 'v', 0, "increased diagnostic verbosity" },
    { "warnings", 0, 'w', 0, "emit warnings [default is silence]" },
    PMAPI_OPTIONS_TEXT(""),
    PMAPI_OPTIONS_TEXT("output-archive is required unless -i is specified"),
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:CdD:iqsvw?",
    .long_options = longopts,
    .short_usage = "[options] input-archive [output-archive]",
};

/*
 *  Global variables
 */
static int	first_datarec = 1;	/* first record flag */
static char	bak_base[MAXPATHLEN+1];	/* basename for backup with -i */

off_t		new_log_offset;		/* new log offset */
off_t		new_meta_offset;	/* new meta offset */


/* archive control stuff */
inarch_t		inarch;		/* input archive control */
outarch_t		outarch;	/* output archive control */

/* command line args */
int	nconf;				/* number of config files */
char	**conf;				/* list of config files */
char	*configfile;			/* current config file */
int	Cflag;				/* -C parse config and quit */
int	dflag;				/* -d desperate */
int	iflag;				/* -i in-place */
int	qflag;				/* -q quick or quiet */
int	sflag;				/* -s scale values */
int	vflag;				/* -v verbosity */
int	wflag;				/* -w emit warnings */

/*
 *  report that archive is corrupted
 */
static void
_report(FILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = lseek(fileno(fp), 0L, SEEK_CUR);
    fprintf(stderr, "%s: Error occurred at byte offset %ld into a file of",
	    pmProgname, (long)here);
    if (fstat(fileno(fp), &sbuf) < 0)
	fprintf(stderr, ": stat: %s\n", osstrerror());
    else
	fprintf(stderr, " %ld bytes.\n", (long)sbuf.st_size);
    if (dflag)
	fprintf(stderr, "The last record, and the remainder of this file will not be processed.\n");
    abandon();
    /*NOTREACHED*/
}

/*
 *  switch output volumes
 */
void
newvolume(int vol)
{
    FILE		*newfp;

    if ((newfp = __pmLogNewFile(outarch.name, vol)) != NULL) {
	fclose(outarch.logctl.l_mfp);
	outarch.logctl.l_mfp = newfp;
	outarch.logctl.l_label.ill_vol = outarch.logctl.l_curvol = vol;
	__pmLogWriteLabel(outarch.logctl.l_mfp, &outarch.logctl.l_label);
	fflush(outarch.logctl.l_mfp);
    }
    else {
	fprintf(stderr, "%s: __pmLogNewFile(%s,%d) Error: %s\n",
		pmProgname, outarch.name, vol, pmErrStr(-oserror()));
	abandon();
	/*NOTREACHED*/
    }
}

/* construct new archive label */
static void
newlabel(void)
{
    __pmLogLabel	*lp = &outarch.logctl.l_label;

    /* copy magic number, pid, host and timezone */
    lp->ill_magic = inarch.label.ll_magic;
    lp->ill_pid = inarch.label.ll_pid;
    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
	strncpy(lp->ill_hostname, global.hostname, PM_LOG_MAXHOSTLEN);
    else
	strncpy(lp->ill_hostname, inarch.label.ll_hostname, PM_LOG_MAXHOSTLEN);
    lp->ill_hostname[PM_LOG_MAXHOSTLEN-1] = '\0';
    if (global.flags & GLOBAL_CHANGE_TZ)
	strncpy(lp->ill_tz, global.tz, PM_TZ_MAXLEN);
    else
	strncpy(lp->ill_tz, inarch.label.ll_tz, PM_TZ_MAXLEN);
    lp->ill_tz[PM_TZ_MAXLEN-1] = '\0';
}

/*
 * write label records at the start of each physical file
 */
void
writelabel(int do_rewind)
{
    off_t	old_offset;

    if (do_rewind) {
	old_offset = ftell(outarch.logctl.l_tifp);
	assert(old_offset >= 0);
	rewind(outarch.logctl.l_tifp);
    }
    outarch.logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(outarch.logctl.l_tifp, &outarch.logctl.l_label);
    if (do_rewind)
	fseek(outarch.logctl.l_tifp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = ftell(outarch.logctl.l_mdfp);
	assert(old_offset >= 0);
	rewind(outarch.logctl.l_mdfp);
    }
    outarch.logctl.l_label.ill_vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(outarch.logctl.l_mdfp, &outarch.logctl.l_label);
    if (do_rewind)
	fseek(outarch.logctl.l_mdfp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = ftell(outarch.logctl.l_mfp);
	assert(old_offset >= 0);
	rewind(outarch.logctl.l_mfp);
    }
    outarch.logctl.l_label.ill_vol = 0;
    __pmLogWriteLabel(outarch.logctl.l_mfp, &outarch.logctl.l_label);
    if (do_rewind)
	fseek(outarch.logctl.l_mfp, (long)old_offset, SEEK_SET);
}

/*
 * read next metadata record 
 */
static int
nextmeta()
{
    int			sts;
    __pmLogCtl		*lcp;

    lcp = inarch.ctxp->c_archctl->ac_log;
    if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &inarch.metarec)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mdfp);
	}
	return -1;
    }

    return ntohl(inarch.metarec[1]);
}


/*
 * read next log record
 *
 * return status is
 * 0		ok
 * 1		ok, but volume switched
 * PM_ERR_EOL	end of file
 * -1		fatal error
 */
static int
nextlog(void)
{
    int			sts;
    __pmLogCtl		*lcp;
    int			old_vol;


    lcp = inarch.ctxp->c_archctl->ac_log;
    old_vol = inarch.ctxp->c_archctl->ac_log->l_curvol;

    if ((sts = __pmLogRead(lcp, PM_MODE_FORW, NULL, &inarch.rp, PMLOGREAD_NEXT)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mfp);
	}
	return -1;
    }

    return old_vol == inarch.ctxp->c_archctl->ac_log->l_curvol ? 0 : 1;
}

#ifdef IS_MINGW
#define S_ISLINK(mode) 0	/* no symlink support */
#else
#ifndef S_ISLINK
#define S_ISLINK(mode) ((mode & S_IFMT) == S_IFLNK)
#endif
#endif

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			sep = __pmPathSeparator();
    struct stat		sbuf;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* config file */
	    if (stat(opts.optarg, &sbuf) < 0) {
		pmprintf("%s: stat(%s) failed: %s\n",
			pmProgname, opts.optarg, osstrerror());
		opts.errors++;
		break;
	    }
	    if (S_ISREG(sbuf.st_mode) || S_ISLINK(sbuf.st_mode)) {
		nconf++;
		if ((conf = (char **)realloc(conf, nconf*sizeof(conf[0]))) != NULL)
		    conf[nconf-1] = opts.optarg;
	    }
	    else if (S_ISDIR(sbuf.st_mode)) {
		DIR		*dirp;
		struct dirent	*dp;
		char		path[MAXPATHLEN+1];

		if ((dirp = opendir(opts.optarg)) == NULL) {
		    pmprintf("%s: opendir(%s) failed: %s\n", pmProgname, opts.optarg, osstrerror());
		    opts.errors++;
		}
		else while ((dp = readdir(dirp)) != NULL) {
		    /* skip ., .. and "hidden" files */
		    if (dp->d_name[0] == '.') continue;
		    snprintf(path, sizeof(path), "%s%c%s", opts.optarg, sep, dp->d_name);
		    if (stat(path, &sbuf) < 0) {
			pmprintf("%s: %s: %s\n", pmProgname, path, osstrerror());
			opts.errors++;
		    }
		    else if (S_ISREG(sbuf.st_mode) || S_ISLINK(sbuf.st_mode)) {
			nconf++;
			if ((conf = (char **)realloc(conf, nconf*sizeof(conf[0]))) == NULL)
			    break;
			if ((conf[nconf-1] = strdup(path)) == NULL) {
			    fprintf(stderr, "conf[%d] strdup(%s) failed: %s\n", nconf-1, path, strerror(errno));
			    abandon();
			    /*NOTREACHED*/
			}
		    }
		}
		if (dirp != NULL)
		    closedir(dirp);
	    }
	    else {
		pmprintf("%s: Error: -c config %s is not a file or directory\n", pmProgname, opts.optarg);
		opts.errors++;
	    }
	    if (nconf > 0 && conf == NULL) {
		fprintf(stderr, "%s: Error: conf[%d] realloc(%d) failed: %s\n", pmProgname, nconf, (int)(nconf*sizeof(conf[0])), strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }
	    break;

	case 'C':	/* parse configs and quit */
	    Cflag = 1;
	    vflag = 1;
	    wflag = 1;
	    break;

	case 'd':	/* desperate */
	    dflag = 1;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'i':	/* in-place, over-write input archive */
	    iflag = 1;
	    break;

	case 'q':	/* quick or quiet */
	    qflag = 1;
	    break;

	case 's':	/* do scale conversions */
	    sflag = 1;
	    break;

	case 'v':	/* verbosity */
	    vflag++;
	    break;

	case 'w':	/* print warnings */
	    wflag = 1;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors == 0) {
	if ((iflag == 0 && opts.optind != argc-2) ||
	    (iflag == 1 && opts.optind != argc-1))
	    opts.errors++;
    }

    return -opts.errors;
}

static void
parseconfig(char *file)
{
    configfile = file;
    if ((yyin = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmProgname, configfile, osstrerror());
	exit(1);
    }
    if (vflag > 1)
	fprintf(stderr, "Start configfile: %s\n", file);
    lineno = 1;

    if (yyparse() != 0)
	exit(1);

    fclose(yyin);
    yyin = NULL;

    return;
}

char *
SemStr(int sem)
{
    static char	buf[20];

    if (sem == PM_SEM_COUNTER) snprintf(buf, sizeof(buf), "counter");
    else if (sem == PM_SEM_INSTANT) snprintf(buf, sizeof(buf), "instant");
    else if (sem == PM_SEM_DISCRETE) snprintf(buf, sizeof(buf), "discrete");
    else snprintf(buf, sizeof(buf), "bad sem? %d", sem);

    return buf;
}

static void
reportconfig(void)
{
    indomspec_t		*ip;
    metricspec_t	*mp;
    int			i;
    int			change = 0;

    printf("PCP Archive Log Rewrite Specifications Summary\n");
    change |= (global.flags != 0);
    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
	printf("Hostname:\t%s -> %s\n", inarch.label.ll_hostname, global.hostname);
    if (global.flags & GLOBAL_CHANGE_TZ)
	printf("Timezone:\t%s -> %s\n", inarch.label.ll_tz, global.tz);
    if (global.flags & GLOBAL_CHANGE_TIME) {
	static struct tm	*tmp;
	char			*sign = "";
	time_t			time;
	if (global.time.tv_sec < 0) {
	    time = (time_t)(-global.time.tv_sec);
	    sign = "-";
	}
	else
	    time = (time_t)global.time.tv_sec;
	tmp = gmtime(&time);
	tmp->tm_hour += 24 * tmp->tm_yday;
	if (tmp->tm_hour < 10)
	    printf("Delta:\t\t-> %s%02d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
	else
	    printf("Delta:\t\t-> %s%d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
    }
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	int		hdr_done = 0;
	if (ip->new_indom != ip->old_indom) {
	    printf("\nInstance Domain: %s\n", pmInDomStr(ip->old_indom));
	    hdr_done = 1;
	    printf("pmInDom:\t-> %s\n", pmInDomStr(ip->new_indom));
	    change |= 1;
	}
	for (i = 0; i < ip->numinst; i++) {
	    change |= (ip->inst_flags[i] != 0);
	    if (ip->inst_flags[i]) {
		if (hdr_done == 0) {
		    printf("\nInstance Domain: %s\n", pmInDomStr(ip->old_indom));
		    hdr_done = 1;
		}
		printf("Instance:\t\[%d] \"%s\" -> ", ip->old_inst[i], ip->old_iname[i]);
		if (ip->inst_flags[i] & INST_DELETE)
		    printf("DELETE\n");
		else {
		    if (ip->inst_flags[i] & INST_CHANGE_INST)
			printf("[%d] ", ip->new_inst[i]);
		    else
			printf("[%d] ", ip->old_inst[i]);
		    if (ip->inst_flags[i] & INST_CHANGE_INAME)
			printf("\"%s\"\n", ip->new_iname[i]);
		    else
			printf("\"%s\"\n", ip->old_iname[i]);
		}
	    }
	}
    }
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->flags != 0 || mp->ip != NULL) {
	    change |= 1;
	    printf("\nMetric: %s (%s)\n", mp->old_name, pmIDStr(mp->old_desc.pmid));
	}
	if (mp->flags & METRIC_CHANGE_PMID) {
	    printf("pmID:\t\t%s ->", pmIDStr(mp->old_desc.pmid));
	    printf(" %s\n", pmIDStr(mp->new_desc.pmid));
	}
	if (mp->flags & METRIC_CHANGE_NAME)
	    printf("Name:\t\t%s -> %s\n", mp->old_name, mp->new_name);
	if (mp->flags & METRIC_CHANGE_TYPE) {
	    printf("Type:\t\t%s ->", pmTypeStr(mp->old_desc.type));
	    printf(" %s\n", pmTypeStr(mp->new_desc.type));
	}
	if (mp->flags & METRIC_CHANGE_INDOM) {
	    printf("InDom:\t\t%s ->", pmInDomStr(mp->old_desc.indom));
	    printf(" %s\n", pmInDomStr(mp->new_desc.indom));
	    if (mp->output != OUTPUT_ALL) {
		printf("Output:\t\t");
		switch (mp->output) {
		    case OUTPUT_ONE:
			if (mp->old_desc.indom != PM_INDOM_NULL) {
			    printf("value for instance");
			    if (mp->one_inst != PM_IN_NULL)
				printf(" %d", mp->one_inst);
			    if (mp->one_name != NULL)
				printf(" \"%s\"", mp->one_name);
			    putchar('\n');
			}
			else
			    printf("the only value (output instance %d)\n", mp->one_inst);
			break;
		    case OUTPUT_FIRST:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("first value\n");
			else {
			    if (mp->one_inst != PM_IN_NULL)
				printf("first and only value (output instance %d)\n", mp->one_inst);
			    else
				printf("first and only value (output instance \"%s\")\n", mp->one_name);
			}
			break;
		    case OUTPUT_LAST:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("last value\n");
			else
			    printf("last and only value (output instance %d)\n", mp->one_inst);
			break;
		    case OUTPUT_MIN:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("smallest value\n");
			else
			    printf("smallest and only value (output instance %d)\n", mp->one_inst);
			break;
		    case OUTPUT_MAX:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("largest value\n");
			else
			    printf("largest and only value (output instance %d)\n", mp->one_inst);
			break;
		    case OUTPUT_SUM:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("sum value (output instance %d)\n", mp->one_inst);
			else
			    printf("sum and only value (output instance %d)\n", mp->one_inst);
			break;
		    case OUTPUT_AVG:
			if (mp->old_desc.indom != PM_INDOM_NULL)
			    printf("average value (output instance %d)\n", mp->one_inst);
			else
			    printf("average and only value (output instance %d)\n", mp->one_inst);
			break;
		}
	    }
	}
	if (mp->ip != NULL)
	    printf("Inst Changes:\t<- InDom %s\n", pmInDomStr(mp->ip->old_indom));
	if (mp->flags & METRIC_CHANGE_SEM) {
	    printf("Semantics:\t%s ->", SemStr(mp->old_desc.sem));
	    printf(" %s\n", SemStr(mp->new_desc.sem));
	}
	if (mp->flags & METRIC_CHANGE_UNITS) {
	    printf("Units:\t\t%s ->", pmUnitsStr(&mp->old_desc.units));
	    printf(" %s", pmUnitsStr(&mp->new_desc.units));
	    if (mp->flags & METRIC_RESCALE)
		printf(" (rescale)");
	    putchar('\n');
	}
	if (mp->flags & METRIC_DELETE)
	    printf("DELETE\n");
    }
    if (change == 0)
	printf("No changes\n");
}

static int
anychange(void)
{
    indomspec_t		*ip;
    metricspec_t	*mp;
    int			i;

    if (global.flags != 0)
	return 1;
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (ip->new_indom != ip->old_indom)
	    return 1;
	for (i = 0; i < ip->numinst; i++) {
	    if (ip->inst_flags[i])
		return 1;
	}
    }
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->flags != 0 || mp->ip != NULL)
	    return 1;
    }
    
    return 0;
}

static int
fixstamp(struct timeval *tvp)
{
    if (global.flags & GLOBAL_CHANGE_TIME) {
	if (global.time.tv_sec > 0) {
	    tvp->tv_sec += global.time.tv_sec;
	    tvp->tv_usec += global.time.tv_usec;
	    if (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	    }
	    return 1;
	}
	else if (global.time.tv_sec < 0) {
	    /* parser makes tv_sec < 0 and tv_usec >= 0 */
	    tvp->tv_sec += global.time.tv_sec;
	    tvp->tv_usec -= global.time.tv_usec;
	    if (tvp->tv_usec < 0) {
		tvp->tv_sec--;
		tvp->tv_usec += 1000000;
	    }
	    return 1;
	}
    }
    return 0;
}

/*
 * Link metricspec_t entries to corresponding indom_t entry if there
 * are changes to instance identifiers or instance names (includes
 * instance deletion)
 */
static void
link_entries(void)
{
    indomspec_t		*ip;
    metricspec_t	*mp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    int			i;
    int			change;

    hcp = &inarch.ctxp->c_archctl->ac_log->l_hashpmid;
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	change = 0;
	for (i = 0; i < ip->numinst; i++)
	    change |= (ip->inst_flags[i] != 0);
	if (change == 0 && ip->new_indom == ip->old_indom)
	    continue;

	for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	    mp = start_metric((pmID)(node->key));
	    if (mp->old_desc.indom == ip->old_indom) {
		if (change)
		    mp->ip = ip;
		if (ip->new_indom != ip->old_indom) {
		    if (mp->flags & METRIC_CHANGE_INDOM) {
			/* indom already changed via metric clause */
			if (mp->new_desc.indom != ip->new_indom) {
			    char	strbuf[80];
			    snprintf(strbuf, sizeof(strbuf), "%s", pmInDomStr(mp->new_desc.indom));
			    snprintf(mess, sizeof(mess), "Conflicting indom change for metric %s (%s from metric clause, %s from indom clause)", mp->old_name, strbuf, pmInDomStr(ip->new_indom));
			    yysemantic(mess);
			}
		    }
		    else {
			mp->flags |= METRIC_CHANGE_INDOM;
			mp->new_desc.indom = ip->new_indom;
		    }
		}
	    }
	}
    }
}

static void
check_indoms()
{
    /*
     * For each metric, make sure the output instance domain will be in
     * the output archive.
     * Called after link_entries(), so if an input metric is associated
     * with an instance domain that has any instance rewriting, we're OK.
     * The case to be checked here is a rewritten metric with an indom
     * clause and no associated indomspec_t (so no instance domain changes,
     * but the new indom may not match any indom in the archive.
     */
    metricspec_t	*mp;
    indomspec_t		*ip;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;


    hcp = &inarch.ctxp->c_archctl->ac_log->l_hashindom;

    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->ip != NULL)
	    /* associated indom has instance changes, we're OK */
	    continue;
	if ((mp->flags & METRIC_CHANGE_INDOM) && mp->new_desc.indom != PM_INDOM_NULL) {
	    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
		 node != NULL;
		 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
		/*
		 * if this indom has an indomspec_t, check that, else
		 * this indom will go to the archive without change
		 */
		for (ip = indom_root; ip != NULL; ip = ip->i_next) {
		    if (ip->old_indom == mp->old_desc.indom)
			break;
		}
		if (ip == NULL) {
		    if ((pmInDom)(node->key) == mp->new_desc.indom)
			/* we're OK */
			break;
		}
		else {
		    if (ip->new_indom != ip->old_indom &&
		        ip->new_indom == mp->new_desc.indom)
			/* we're OK */
			break;
		}
	    }
	    if (node == NULL) {
		snprintf(mess, sizeof(mess), "New indom (%s) for metric %s is not in the output archive", pmInDomStr(mp->new_desc.indom), mp->old_name);
		yysemantic(mess);
	    }
	}
    }

    /*
     * For each modified instance domain, make sure instances are
     * still unique and instance names are unique to the first
     * space.
     */
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	int	i;
	for (i = 0; i < ip->numinst; i++) {
	    int		insti;
	    char	*namei;
	    int		j;
	    if (ip->inst_flags[i] & INST_CHANGE_INST)
		insti = ip->new_inst[i];
	    else
		insti = ip->old_inst[i];
	    if (ip->inst_flags[i] & INST_CHANGE_INAME)
		namei = ip->new_iname[i];
	    else
		namei = ip->old_iname[i];
	    for (j = 0; j < ip->numinst; j++) {
		int	instj;
		char	*namej;
		if (i == j)
		    continue;
		if (ip->inst_flags[j] & INST_CHANGE_INST)
		    instj = ip->new_inst[j];
		else
		    instj = ip->old_inst[j];
		if (ip->inst_flags[j] & INST_CHANGE_INAME)
		    namej = ip->new_iname[j];
		else
		    namej = ip->old_iname[j];
		if (insti == instj) {
		    snprintf(mess, sizeof(mess), "Duplicate instance id %d (\"%s\" and \"%s\") for indom %s", insti, namei, namej, pmInDomStr(ip->old_indom));
		    yysemantic(mess);
		}
		if (inst_name_eq(namei, namej) > 0) {
		    snprintf(mess, sizeof(mess), "Duplicate instance name \"%s\" (%d) and \"%s\" (%d) for indom %s", namei, insti, namej, instj, pmInDomStr(ip->old_indom));
		    yysemantic(mess);
		}
	    }
	}
    }
}

static void
check_output()
{
    /*
     * For each metric, if there is an INDOM clause, perform some
     * additional semantic checks and perhaps a name -> instance id
     * mapping.
     *
     * Note instance renumbering happens _after_ value selction from
     * 		the INDOM -> ,,,, OUTPUT clause, so all references to
     * 		instance names and instance ids are relative to the
     * 		"old" set.
     */
    metricspec_t	*mp;
    indomspec_t		*ip;

    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if ((mp->flags & METRIC_CHANGE_INDOM)) {
	    if (mp->output == OUTPUT_ONE || mp->output == OUTPUT_FIRST) {
		/*
		 * cases here are
		 * INAME "name"
		 * 	=> one_name == "name" and one_inst == PM_IN_NULL
		 * INST id
		 * 	=> one_name == NULL and one_inst = id
		 */
		if (mp->old_desc.indom != PM_INDOM_NULL && mp->output == OUTPUT_ONE) {
		    /*
		     * old metric is not singular, so one_name and one_inst
		     * are used to pick the value
		     * also map one_name -> one_inst 
		     */
		    int		i;
		    ip = start_indom(mp->old_desc.indom);
		    for (i = 0; i < ip->numinst; i++) {
			if (mp->one_name != NULL) {
			    if (inst_name_eq(ip->old_iname[i], mp->one_name) > 0) {
				mp->one_name = NULL;
				mp->one_inst = ip->old_inst[i];
				break;
			    }
			}
			else if (ip->old_inst[i] == mp->one_inst)
			    break;
		    }
		    if (i == ip->numinst) {
			if (wflag) {
			    if (mp->one_name != NULL)
				snprintf(mess, sizeof(mess), "Instance \"%s\" from OUTPUT clause not found in old indom %s", mp->one_name, pmInDomStr(mp->old_desc.indom));
			    else
				snprintf(mess, sizeof(mess), "Instance %d from OUTPUT clause not found in old indom %s", mp->one_inst, pmInDomStr(mp->old_desc.indom));
			    yywarn(mess);
			}
		    }
		}
		if (mp->new_desc.indom != PM_INDOM_NULL) {
		    /*
		     * new metric is not singular, so one_inst should be
		     * found in the new instance domain ... ignore one_name
		     * other than to map one_name -> one_inst if one_inst
		     * is not already known
		     */
		    int		i;
		    ip = start_indom(mp->new_desc.indom);
		    for (i = 0; i < ip->numinst; i++) {
			if (mp->one_name != NULL) {
			    if (inst_name_eq(ip->old_iname[i], mp->one_name) > 0) {
				mp->one_name = NULL;
				mp->one_inst = ip->old_inst[i];
				break;
			    }
			}
			else if (ip->old_inst[i] == mp->one_inst)
			    break;
		    }
		    if (i == ip->numinst) {
			if (wflag) {
			    if (mp->one_name != NULL)
				snprintf(mess, sizeof(mess), "Instance \"%s\" from OUTPUT clause not found in new indom %s", mp->one_name, pmInDomStr(mp->new_desc.indom));
			    else
				snprintf(mess, sizeof(mess), "Instance %d from OUTPUT clause not found in new indom %s", mp->one_inst, pmInDomStr(mp->new_desc.indom));
			    yywarn(mess);
			}
		    }
		    /*
		     * use default rule (id 0) if INAME not found and
		     * and instance id is needed for output value
		     */
		    if (mp->old_desc.indom == PM_INDOM_NULL && mp->one_inst == PM_IN_NULL)
			mp->one_inst = 0;
		}
	    }
	}
    }
}

int
main(int argc, char **argv)
{
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta = 0;		/* sts from nextmeta() */
    int		i;
    int		ti_idx;			/* next slot for input temporal index */
    int		dir_fd = -1;		/* poinless initialization to humour gcc */
    int		needti = 0;
    int		doneti = 0;
    __pmTimeval	tstamp = { 0 };		/* for last log record */
    off_t	old_log_offset = 0;	/* log offset before last log record */
    off_t	old_meta_offset;

    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* input archive */
    if (iflag == 0)
	inarch.name = argv[argc-2];
    else
	inarch.name = argv[argc-1];
    inarch.logrec = inarch.metarec = NULL;
    inarch.mark = 0;
    inarch.rp = NULL;

    if ((inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE, inarch.name)) < 0) {
	fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		pmProgname, inarch.name, pmErrStr(inarch.ctx));
	exit(1);
    }
    inarch.ctxp = __pmHandleToPtr(inarch.ctx);
    assert(inarch.ctxp != NULL);

    if ((sts = pmGetArchiveLabel(&inarch.label)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n",
		pmProgname, inarch.name, pmErrStr(sts));
	exit(1);
    }

    if ((inarch.label.ll_magic & 0xff) != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmProgname, inarch.label.ll_magic & 0xff, inarch.name);
	exit(1);
    }

    /* output archive */
    if (iflag && Cflag == 0) {
	/*
	 * -i (in place) method outline
	 *
	 * + create one temporary base filename in the same directory is
	 *   the input archive, keep a copy of this name this accessed
	 *   via outarch.name
	 * + create a second (and different) temporary base file name
	 *   in the same directory, keep this name in bak_base[]
	 * + close the temporary file descriptors and unlink the basename
	 *   files
	 * + create the output as per normal in outarch.name
	 * + fsync() all the output files and the container directory
	 * + rename the _input_ archive files using the _second_ temporary
	 *   basename
	 * + rename the output archive files to the basename of the input
	 *   archive ... if this step fails for any reason, restore the
	 *   original input files
	 * + unlink all the (old) input archive files
	 */
	char	path[MAXPATHLEN+1];
	char	dname[MAXPATHLEN+1];
	mode_t	cur_umask;
	int	tmp_f1;			/* fd for first temp basename */
	int	tmp_f2;			/* fd for second temp basename */

#if HAVE_MKSTEMP
	strncpy(path, argv[argc-1], sizeof(path));
	path[sizeof(path)-1] = '\0';
	strncpy(dname, dirname(path), sizeof(dname));
	dname[sizeof(dname)-1] = '\0';
	if ((dir_fd = open(dname, O_RDONLY)) < 0) {
	    fprintf(stderr, "%s: Error: cannot open directory \"%s\" for reading: %s\n", pmProgname, dname, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	sprintf(path, "%s%cXXXXXX", dname, __pmPathSeparator());
	cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	tmp_f1 = mkstemp(path);
	umask(cur_umask);
	outarch.name = strdup(path);
	if (outarch.name == NULL) {
	    fprintf(stderr, "%s: Error: temp file strdup(%s) failed: %s\n", pmProgname, path, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	sprintf(bak_base, "%s%cXXXXXX", dname, __pmPathSeparator());
	cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	tmp_f2 = mkstemp(bak_base);
	umask(cur_umask);
#else
	char	fname[MAXPATHLEN+1];
	char	*s;

	strncpy(path, argv[argc-1], sizeof(path));
	path[sizeof(path)-1] = '\0';
	strncpy(fname, basename(path), sizeof(fname));
	fname[sizeof(fname)-1] = '\0';
	strncpy(dname, dirname(path), sizeof(dname));
	dname[sizeof(dname)-1] = '\0';

	if ((s = tempnam(dname, fname)) == NULL) {
	    fprintf(stderr, "%s: Error: first tempnam() failed: %s\n", pmProgname, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	else {
	    outarch.name = strdup(s);
	    if (outarch.name == NULL) {
		fprintf(stderr, "%s: Error: temp file strdup(%s) failed: %s\n", pmProgname, s, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }
	    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	    tmp_f1 = open(outarch.name, O_WRONLY|O_CREAT|O_EXCL, 0600);
	    umask(cur_umask);
	}
	if ((s = tempnam(dname, fname)) == NULL) {
	    fprintf(stderr, "%s: Error: second tempnam() failed: %s\n", pmProgname, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	else {
	    strcpy(bak_base, s);
	    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	    tmp_f2 = open(bak_base, O_WRONLY|O_CREAT|O_EXCL, 0600);
	    umask(cur_umask);
	}
#endif
	if (tmp_f1 < 0) {
	    fprintf(stderr, "%s: Error: create first temp (%s) failed: %s\n", pmProgname, outarch.name, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	if (tmp_f2 < 0) {
	    fprintf(stderr, "%s: Error: create second temp (%s) failed: %s\n", pmProgname, bak_base, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	close(tmp_f1);
	close(tmp_f2);
	unlink(outarch.name);
	unlink(bak_base);
    }
    else
	outarch.name = argv[argc-1];

    /*
     * process config file(s)
     */
    for (i = 0; i < nconf; i++) {
	parseconfig(conf[i]);
    }

    /*
     * cross-specification dependencies and semantic checks once all
     * config files have been processed
     */
    link_entries();
    check_indoms();
    check_output();

    if (vflag)
	reportconfig();

    if (Cflag)
	exit(0);

    if (qflag && anychange() == 0)
	exit(0);

    /* create output log - must be done before writing label */
    if ((sts = __pmLogCreate("", outarch.name, PM_LOG_VERS02, &outarch.logctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate(%s): %s\n",
		pmProgname, outarch.name, pmErrStr(sts));
	abandon();
	/*NOTREACHED*/
    }

    /* initialize and write label records */
    newlabel();
    outarch.logctl.l_state = PM_LOG_STATE_INIT;
    writelabel(0);

    first_datarec = 1;
    ti_idx = 0;

    /*
     * loop
     *	- get next log record
     *	- write out new/changed meta data required by this log record
     *	- write out log
     *	- do ti update if necessary
     */
    while (1) {
	static long	in_offset;		/* for -Dappl0 */

	fflush(outarch.logctl.l_mdfp);
	old_meta_offset = ftell(outarch.logctl.l_mdfp);
	assert(old_meta_offset >= 0);

	in_offset = ftell(inarch.ctxp->c_archctl->ac_log->l_mfp);
	stslog = nextlog();
	if (stslog < 0) {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "Log: read EOF @ offset=%ld\n", in_offset);
#endif
	    break;
	}
	if (stslog == 1) {
	    /* volume change */
	    if (inarch.ctxp->c_archctl->ac_log->l_curvol >= outarch.logctl.l_curvol+1)
		/* track input volume numbering */
		newvolume(inarch.ctxp->c_archctl->ac_log->l_curvol);
	    else
		/*
		 * output archive volume number is ahead, probably because
		 * rewriting has forced an earlier volume change
		 */
		newvolume(outarch.logctl.l_curvol+1);
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    struct timeval	stamp;
	    fprintf(stderr, "Log: read ");
	    stamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    stamp.tv_usec = inarch.rp->timestamp.tv_usec;
	    __pmPrintStamp(stderr, &stamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, in_offset);
	}
#endif

	if (ti_idx < inarch.ctxp->c_archctl->ac_log->l_numti) {
	    __pmLogTI	*tip = &inarch.ctxp->c_archctl->ac_log->l_ti[ti_idx];
	    if (tip->ti_stamp.tv_sec == inarch.rp->timestamp.tv_sec &&
	        tip->ti_stamp.tv_usec == inarch.rp->timestamp.tv_usec) {
		/*
		 * timestamp on input pmResult matches next temporal index
		 * entry for input archive ... make sure matching temporal
		 * index entry added to output archive
		 */
		needti = 1;
		ti_idx++;
	    }
	}

	/*
	 * optionally rewrite timestamp in pmResult for global time
	 * adjustment ... flows to output pmResult, indom entries in
	 * metadata, temporal index entries and label records
	 * */
	fixstamp(&inarch.rp->timestamp);

	/*
	 * process metadata until find an indom record with timestamp
	 * after the current log record, or a metric record for a pmid
	 * that is not in the current log record
	 */
	for ( ; ; ) {
	    pmID	pmid;			/* pmid for TYPE_DESC */
	    pmInDom	indom;			/* indom for TYPE_INDOM */

	    if (stsmeta == 0) {
		in_offset = ftell(inarch.ctxp->c_archctl->ac_log->l_mdfp);
		stsmeta = nextmeta();
#if PCP_DEBUG
		if (stsmeta < 0 && pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read EOF @ offset=%ld\n", in_offset);
#endif
	    }
	    if (stsmeta < 0) {
		break;
	    }
	    if (stsmeta == TYPE_DESC) {
		int	i;
		pmid = ntoh_pmID(inarch.metarec[2]);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read PMID %s @ offset=%ld\n", pmIDStr(pmid), in_offset);
#endif
		/*
		 * if pmid not in next pmResult, we're done ...
		 */
		for (i = 0; i < inarch.rp->numpmid; i++) {
		    if (pmid == inarch.rp->vset[i]->pmid)
			break;
		}
		if (i == inarch.rp->numpmid)
		    break;
		/*
		 * rewrite if needed, delete if needed else output
		 */
		do_desc();
	    }
	    else if (stsmeta == TYPE_INDOM) {
		struct timeval	stamp;
		__pmTimeval	*tvp = (__pmTimeval *)&inarch.metarec[2];
		indom = ntoh_pmInDom((unsigned int)inarch.metarec[4]);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read InDom %s @ offset=%ld\n", pmInDomStr(indom), in_offset);
#endif
		stamp.tv_sec = ntohl(tvp->tv_sec);
		stamp.tv_usec = ntohl(tvp->tv_usec);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    tvp->tv_sec = htonl(stamp.tv_sec);
		    tvp->tv_usec = htonl(stamp.tv_usec);
		}
		/* if time of indom > next pmResult stop processing metadata */
		if (stamp.tv_sec > inarch.rp->timestamp.tv_sec)
		    break;
		if (stamp.tv_sec == inarch.rp->timestamp.tv_sec &&
		    stamp.tv_usec > inarch.rp->timestamp.tv_usec)
		    break;
		needti = 1;
		do_indom();
	    }
	    else {
		fprintf(stderr, "%s: Error: unrecognised meta data type: %d\n",
		    pmProgname, stsmeta);
		abandon();
		/*NOTREACHED*/
	    }
	    free(inarch.metarec);
	    stsmeta = 0;
	}

	if (first_datarec) {
	    first_datarec = 0;
	    /* any global time adjustment done after nextlog() above */
	    outarch.logctl.l_label.ill_start.tv_sec = inarch.rp->timestamp.tv_sec;
	    outarch.logctl.l_label.ill_start.tv_usec = inarch.rp->timestamp.tv_usec;
	    /* need to fix start-time in label records */
	    writelabel(1);
	    needti = 1;
	}

	tstamp.tv_sec = inarch.rp->timestamp.tv_sec;
	tstamp.tv_usec = inarch.rp->timestamp.tv_usec;

	if (needti) {
	    fflush(outarch.logctl.l_mdfp);
	    fflush(outarch.logctl.l_mfp);
	    new_meta_offset = ftell(outarch.logctl.l_mdfp);
	    assert(new_meta_offset >= 0);
            fseek(outarch.logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);
            __pmLogPutIndex(&outarch.logctl, &tstamp);
            fseek(outarch.logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);
	    needti = 0;
	    doneti = 1;
        }
	else
	    doneti = 0;

	old_log_offset = ftell(outarch.logctl.l_mfp);
	assert(old_log_offset >= 0);

	if (inarch.rp->numpmid == 0)
	    /* mark record, need index entry @ next log record */
	    needti = 1;

	do_result();
    }

    if (!doneti) {
	/* Final temporal index entry */
	fflush(outarch.logctl.l_mfp);
	fseek(outarch.logctl.l_mfp, (long)old_log_offset, SEEK_SET);
	__pmLogPutIndex(&outarch.logctl, &tstamp);
    }

    if (iflag) {
	/*
	 * fsync() to make sure new archive is safe before we start
	 * renaming ...
	 */
	if (fsync(fileno(outarch.logctl.l_mdfp)) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output metadata file: %s\n",
		pmProgname, fileno(outarch.logctl.l_mdfp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (fsync(fileno(outarch.logctl.l_mfp)) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output data file: %s\n",
		pmProgname, fileno(outarch.logctl.l_mfp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (fsync(fileno(outarch.logctl.l_tifp)) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output index file: %s\n",
		pmProgname, fileno(outarch.logctl.l_tifp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (fsync(dir_fd) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output directory: %s\n",
		pmProgname, dir_fd, strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	close(dir_fd);
	if (_pmLogRename(inarch.name, bak_base) < 0) {
	    abandon();
	    /*NOTREACHED*/
	}
	if (_pmLogRename(outarch.name, inarch.name) < 0) {
	    abandon();
	    /*NOTREACHED*/
	}
	_pmLogRemove(bak_base);
    }

    exit(0);
}

void
abandon(void)
{
    char    path[MAXNAMELEN+1];
    if (dflag == 0) {
	if (Cflag == 0 && iflag == 0)
	    fprintf(stderr, "Archive \"%s\" not created.\n", outarch.name);

	_pmLogRemove(outarch.name);
	if (iflag)
	    _pmLogRename(bak_base, inarch.name);
	while (outarch.logctl.l_curvol >= 0) {
	    snprintf(path, sizeof(path), "%s.%d", outarch.name, outarch.logctl.l_curvol);
	    unlink(path);
	    outarch.logctl.l_curvol--;
	}
	snprintf(path, sizeof(path), "%s.meta", outarch.name);
	unlink(path);
	snprintf(path, sizeof(path), "%s.index", outarch.name);
	unlink(path);
    }
    else
	fprintf(stderr, "Archive \"%s\" creation truncated.\n", outarch.name);

    exit(1);
}
