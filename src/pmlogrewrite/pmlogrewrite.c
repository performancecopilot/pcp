/*
 * pmlogrewrite - config-driven stream editor for PCP archives
 *
 * Copyright (c) 2013-2018,2021-2022 Red Hat.
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
 *
 * Debug flags
 * appl0	I/O
 * appl1	metdata changes
 * appl2	pmResult changes
 * appl3	-q and reason for not taking quick exit
 * appl4	config parser
 * appl5	regexp matching for metric value changes and iname changes
 */

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>
#include "pcp/pmapi.h"
#include "pcp/libpcp.h"
#include "pcp/archive.h"
#include "../libpcp/src/internal.h"
#include "./logger.h"
#include <assert.h>

global_t	global;
indomspec_t	*indom_root;
metricspec_t	*metric_root;
textspec_t	*text_root;
labelspec_t	*label_root;
int		lineno;

__pmHashCtl	indom_hash = { 0 };

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
    { "version", 1, 'V', "N", "output archive in version N [2/3] format" },
    { "warnings", 0, 'w', 0, "emit warnings [default is silence]" },
    PMOPT_HELP,
    PMAPI_OPTIONS_TEXT(""),
    PMAPI_OPTIONS_TEXT("output-archive is required unless -C or -i is specified"),
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:CdD:iqsvV:w?",
    .long_options = longopts,
    .short_usage = "[options] input-archive [output-archive]",
};

/*
 *  Global variables
 */
static int	needti = 0;		/* need time index record */
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
int	out_version;			/* -V log version */
int	wflag;				/* -w emit warnings */

/*
 *  report that archive is corrupted
 */
static void
_report(__pmFILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = __pmLseek(fp, 0L, SEEK_CUR);
    fprintf(stderr, "%s: Error occurred at byte offset %ld into a file of",
	    pmGetProgname(), (long)here);
    if (__pmFstat(fp, &sbuf) < 0)
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
    __pmFILE		*newfp;

    if ((newfp = __pmLogNewFile(outarch.name, vol)) != NULL) {
	__pmFclose(outarch.archctl.ac_mfp);
	outarch.archctl.ac_mfp = newfp;
	outarch.logctl.label.vol = outarch.archctl.ac_curvol = vol;
	__pmLogWriteLabel(outarch.archctl.ac_mfp, &outarch.logctl.label);
	__pmFflush(outarch.archctl.ac_mfp);
    }
    else {
	fprintf(stderr, "%s: __pmLogNewFile(%s,%d) Error: %s\n",
		pmGetProgname(), outarch.name, vol, pmErrStr(-oserror()));
	if (oserror() == EEXIST) {
	    /*
	     * We've written some files (.index, .meta, .0, ...) and then
	     * found a duplicate file name ... doesn't matter what you do
	     * here, badness will result
	     */
	    fprintf(stderr, "Removing output files we've created for archive \"%s\"\n", outarch.name);
	    _pmLogRemove(outarch.name, vol-1);
	    exit(1);
	    /*NOTREACHED*/
	}
	abandon();
	/*NOTREACHED*/
    }
}

/* construct new archive label */
static void
newlabel(void)
{
    __pmLogLabel	*lp = &outarch.logctl.label;

    /* create magic number with output version */
    lp->magic = PM_LOG_MAGIC | outarch.version;

    /* copy pid, host, timezone, etc */
    // TODO WARN about no-ops for changes to V3 label fields in V2 output?
    lp->pid = inarch.label.pid;
    lp->features = (global.flags & GLOBAL_CHANGE_FEATURES) ?
	global.features : inarch.label.features;
    if (lp->hostname)
	free(lp->hostname);
    lp->hostname = (global.flags & GLOBAL_CHANGE_HOSTNAME) ?
	global.hostname : inarch.label.hostname;
    if (lp->timezone)
	free(lp->timezone);
    lp->timezone = (global.flags & GLOBAL_CHANGE_TIMEZONE) ?
	global.timezone : inarch.label.timezone;
    if (lp->zoneinfo)
	free(lp->zoneinfo);
    lp->zoneinfo = (global.flags & GLOBAL_CHANGE_ZONEINFO) ?
	global.zoneinfo : inarch.label.zoneinfo;
}

/*
 * write label records at the start of each physical file
 */
void
writelabel(int do_rewind)
{
    off_t	old_offset;

    if (do_rewind) {
	old_offset = __pmFtell(outarch.logctl.tifp);
	assert(old_offset >= 0);
	__pmRewind(outarch.logctl.tifp);
    }
    outarch.logctl.label.vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(outarch.logctl.tifp, &outarch.logctl.label);
    if (do_rewind)
	__pmFseek(outarch.logctl.tifp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = __pmFtell(outarch.logctl.mdfp);
	assert(old_offset >= 0);
	__pmRewind(outarch.logctl.mdfp);
    }
    outarch.logctl.label.vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(outarch.logctl.mdfp, &outarch.logctl.label);
    if (do_rewind)
	__pmFseek(outarch.logctl.mdfp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = __pmFtell(outarch.archctl.ac_mfp);
	assert(old_offset >= 0);
	__pmRewind(outarch.archctl.ac_mfp);
    }
    outarch.logctl.label.vol = outarch.archctl.ac_curvol;
    __pmLogWriteLabel(outarch.archctl.ac_mfp, &outarch.logctl.label);
    if (do_rewind)
	__pmFseek(outarch.archctl.ac_mfp, (long)old_offset, SEEK_SET);
}

/*
 * read next metadata record 
 */
static int
nextmeta()
{
    int			sts;
    __pmArchCtl		*acp = inarch.ctxp->c_archctl;
    __pmLogCtl		*lcp = acp->ac_log;

    if ((sts = pmaGetLog(acp, PM_LOG_VOL_META, &inarch.metarec)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: pmaGetLog[meta %s]: %s\n",
		    pmGetProgname(), inarch.name, pmErrStr(sts));
	    _report(lcp->mdfp);
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
    __pmArchCtl		*acp = inarch.ctxp->c_archctl;
    int			sts;
    int			old_vol;

    old_vol = inarch.ctxp->c_archctl->ac_curvol;

    sts = __pmLogRead_ctx(inarch.ctxp, PM_MODE_FORW, NULL, &inarch.rp, PMLOGREAD_NEXT);
    if (sts < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
		    pmGetProgname(), inarch.name, pmErrStr(sts));
	    _report(acp->ac_mfp);
	}
	return -1;
    }

    return old_vol == acp->ac_curvol ? 0 : 1;
}

#ifndef S_ISLNK
#define S_ISLNK(mode) ((mode & S_IFMT) == S_IFLNK)
#endif

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			sep = pmPathSeparator();
    char		**cp;
    struct stat		sbuf;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* config file */
	    if (stat(opts.optarg, &sbuf) < 0) {
		pmprintf("%s: stat(%s) failed: %s\n",
			pmGetProgname(), opts.optarg, osstrerror());
		opts.errors++;
		break;
	    }
	    if (S_ISREG(sbuf.st_mode) || S_ISLNK(sbuf.st_mode)) {
		if ((cp = (char **)realloc(conf, (nconf+1)*sizeof(conf[0]))) != NULL) {
		    conf = cp;
		    conf[nconf++] = opts.optarg;
		}
	    }
	    else if (S_ISDIR(sbuf.st_mode)) {
		DIR		*dirp;
		struct dirent	*dp;
		char		path[MAXPATHLEN+1];

		if ((dirp = opendir(opts.optarg)) == NULL) {
		    pmprintf("%s: opendir(%s) failed: %s\n", pmGetProgname(), opts.optarg, osstrerror());
		    opts.errors++;
		}
		else while ((dp = readdir(dirp)) != NULL) {
		    /* skip ., .. and "hidden" files */
		    if (dp->d_name[0] == '.') continue;
		    pmsprintf(path, sizeof(path), "%s%c%s", opts.optarg, sep, dp->d_name);
		    if (stat(path, &sbuf) < 0) {
			pmprintf("%s: %s: %s\n", pmGetProgname(), path, osstrerror());
			opts.errors++;
		    }
		    else if (S_ISREG(sbuf.st_mode) || S_ISLNK(sbuf.st_mode)) {
			if ((cp = (char **)realloc(conf, (nconf+1)*sizeof(conf[0]))) == NULL)
			    break;
			conf = cp;
			if ((conf[nconf++] = strdup(path)) == NULL) {
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
		pmprintf("%s: Error: -c config %s is not a file or directory\n", pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    if (nconf > 0 && conf == NULL) {
		fprintf(stderr, "%s: Error: conf[%d] realloc(%d) failed: %s\n", pmGetProgname(), nconf, (int)(nconf*sizeof(conf[0])), strerror(errno));
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

	case 'D':	/* debug options */
	    sts = pmSetDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
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

	case 'V':	/* output log version */
	    outarch.version = atoi(opts.optarg);
	    if (outarch.version != PM_LOG_VERS02 && outarch.version != PM_LOG_VERS03) {
		pmprintf("%s: Error: unsupported version (%d) requested\n",
			pmGetProgname(), outarch.version);
		opts.errors++;
	    }
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
	if (iflag) {
	    if (opts.optind == argc-1)
		inarch.name = argv[argc-1];
	    else
		opts.errors++;
	}
	else if (Cflag) {
	    /* output-archive is sort of optional for -C */
	    if (opts.optind == argc-2)
		inarch.name = argv[argc-2];
	    else if (opts.optind == argc-1)
		inarch.name = argv[argc-1];
	    else
		opts.errors++;
	}
	else if (opts.optind == argc-2)
	    inarch.name = argv[argc-2];
	else
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
		pmGetProgname(), configfile, osstrerror());
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

    if (sem == PM_SEM_COUNTER) pmsprintf(buf, sizeof(buf), "counter");
    else if (sem == PM_SEM_INSTANT) pmsprintf(buf, sizeof(buf), "instant");
    else if (sem == PM_SEM_DISCRETE) pmsprintf(buf, sizeof(buf), "discrete");
    else pmsprintf(buf, sizeof(buf), "bad sem? %d", sem);

    return buf;
}

#if 0 /* not used (yet) */
static const char *
labelTypeStr(int type)
{
    if ((type & PM_LABEL_CONTEXT))
	return "Context";
    if ((type & PM_LABEL_DOMAIN))
	return "Domain";
    if ((type & PM_LABEL_CLUSTER))
	return "Cluster";
    if ((type & PM_LABEL_ITEM))
	return "Metric";
    if ((type & PM_LABEL_INDOM))
	return "InDom";
    if ((type & PM_LABEL_INSTANCES))
	return "Instance";
    return "Unknown";
}
#endif

static const char *
labelIDStr(int type, int id, char *buf, size_t buflen)
{
    if ((type & PM_LABEL_CONTEXT)) {
	*buf = '\0';
	return buf;
    }
    if ((type & PM_LABEL_DOMAIN)) {
	pmsprintf(buf, buflen, "Domain %u", id);
	return buf;
    }
    if ((type & PM_LABEL_CLUSTER)) {
	pmsprintf(buf, buflen, "Cluster %s", pmIDStr(id));
	return buf;
    }
    if ((type & PM_LABEL_ITEM)) {
	pmsprintf(buf, buflen, "PMID %s", pmIDStr(id));
	return buf;
    }
    if ((type & PM_LABEL_INDOM)) {
	pmsprintf(buf, buflen, "Indom %s", pmInDomStr(id));
	return buf;
    }
    if ((type & PM_LABEL_INSTANCES)) {
	pmsprintf(buf, buflen, "Instances %s", pmInDomStr(id));
	return buf;
    }
    return "Unknown";
}

static void
reportconfig(void)
{
    const indomspec_t	*ip;
    const metricspec_t	*mp;
    const textspec_t	*tp;
    const labelspec_t	*lp;
    int			i;
    int			change = 0;
    char		buf[64];

    printf("PCP Archive Rewrite Specifications Summary\n");
    change |= (global.flags != 0);
    // TODO WARN about no-ops for changes to V3 label fields in V2 output?
    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
	printf("Hostname:\t%s -> %s\n", inarch.label.hostname, global.hostname);
    if (global.flags & GLOBAL_CHANGE_TIMEZONE)
	printf("Timezone:\t%s -> %s\n", inarch.label.timezone, global.timezone);
    if (global.flags & GLOBAL_CHANGE_ZONEINFO)
	printf("Zoneinfo:\t%s -> %s\n", inarch.label.zoneinfo, global.zoneinfo);
    if (global.flags & GLOBAL_CHANGE_FEATURES)
	printf("Features:\t%d -> %d\n", inarch.label.features, global.features);
    if (global.flags & GLOBAL_CHANGE_TIME) {
	static struct tm	*tmp;
	char			*sign = "";
	time_t			ltime;
	if (global.time.sec < 0) {
	    ltime = (time_t)(-global.time.sec);
	    sign = "-";
	}
	else
	    ltime = (time_t)global.time.sec;
	tmp = gmtime(&ltime);
	tmp->tm_hour += 24 * tmp->tm_yday;
	if (tmp->tm_hour < 10)
	    printf("Delta:\t\t-> %s%02d:%02d:%02d.%09d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.nsec);
	else
	    printf("Delta:\t\t-> %s%d:%02d:%02d.%09d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.nsec);
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
	if (mp->flags != 0 || mp->ip != NULL || mp->nvc > 0) {
	    char	**names;
	    int		sts;

	    sts = pmNameAll(mp->old_desc.pmid, &names);
	    if (sts < 0) {
		printf("Warning: cannot get all names for PMID %s\n", pmIDStr(mp->old_desc.pmid));
		printf("\nMetric: %s (%s)\n", mp->old_name, pmIDStr(mp->old_desc.pmid));
	    }
	    else {
		printf("\nMetric");
		if (sts > 1)
		    putchar('s');
		putchar(':');
		/*
		 * Names are likely to be dups first, primary name last
		 */
		for (i = sts-1; i >= 0; i--) {
		    if (i == sts-2)
			printf(" [");
		    else
			putchar(' ');
		    printf("%s", names[i]);
		}
		if (sts > 1)
		    putchar(']');
		printf(" (%s)\n", pmIDStr(mp->old_desc.pmid));
		free(names);
	    }
	    change |= 1;
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
	if (mp->flags & METRIC_CHANGE_VALUE) {
	    for (i = 0; i < mp->nvc; i++) {
		printf("Value:\t\t/%s/ -> \"%s\"\n", mp->vc[i].pat, mp->vc[i].replace);
	    }
	}
	if (mp->flags & METRIC_DELETE)
	    printf("DELETE\n");
    }
    for (tp = text_root; tp != NULL; tp = tp->t_next) {
	if (tp->flags != 0) {
	    change |= 1;
	    printf("\nHelp Text: %s %s (%s)\n",
		   (tp->old_type & PM_TEXT_ONELINE) ? "One Line" : "full",
		   (tp->old_type & PM_TEXT_PMID) ? "pmID" : "pmInDom",
		   (tp->old_type & PM_TEXT_PMID) ? pmIDStr(tp->old_id) : pmInDomStr(tp->old_id));
	}
	if (tp->flags & TEXT_CHANGE_TYPE) {
	    printf("Type:\t\t%s %s -> %s %s\n",
		   (tp->old_type & PM_TEXT_ONELINE) ? "One Line" : "full",
		   (tp->old_type & PM_TEXT_PMID) ? "pmID" : "pmInDom",
		   (tp->new_type & PM_TEXT_ONELINE) ? "One Line" : "full",
		   (tp->new_type & PM_TEXT_PMID) ? "pmID" : "pmInDom");
	}
	if (tp->flags & TEXT_CHANGE_ID) {
	    printf("ID:\t\t%s ->",
		   (tp->old_type & PM_TEXT_PMID) ? pmIDStr(tp->old_id) : pmInDomStr(tp->old_id));
	    printf(" %s\n",
		   (tp->new_type & PM_TEXT_PMID) ? pmIDStr(tp->new_id) : pmInDomStr(tp->new_id));
	}
	if (tp->flags & TEXT_CHANGE_TEXT) {
	    printf("Text:\t\t\"%s\"\n", tp->old_text); 
	    printf("\t\t->\n");
	    printf("\t\t\"%s\"\n", tp->new_text); 
	}
	if (tp->flags & TEXT_DELETE)
	    printf("DELETE\n");
    }
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (lp->flags != 0) {
	    change |= 1;
	    printf("\nLabel: %s",
		   __pmLabelIdentString(lp->old_id, lp->old_type, buf, sizeof(buf)));
	    if (lp->old_type == PM_LABEL_INSTANCES) {
		if (lp->old_instance != -1)
		    printf(", Instance: %d", lp->old_instance);
		else
		    printf(", Instances: ALL");
	    }
	    if (lp->old_label != NULL)
		printf(", Label: %s", lp->old_label);
	    if (lp->old_value != NULL)
		printf(", Value: %s", lp->old_value);
	    putchar ('\n');
	}
	if (lp->flags & LABEL_NEW) {
	    printf("NEW:\n");
	    pmPrintLabelSets(stdout, lp->new_id, lp->new_type, lp->new_labels, 1);
	}
	if (lp->flags & LABEL_CHANGE_ID) {
	    printf("ID:\t\t%s -> ", 
		   labelIDStr(lp->old_type, lp->old_id, buf, sizeof(buf)));
	    printf("%s\n", 
		   labelIDStr(lp->new_type, lp->new_id, buf, sizeof(buf)));
	}
	if (lp->flags & LABEL_CHANGE_LABEL) {
	    printf("Label:\t\t%s -> %s\n",
		   lp->old_label ? lp->old_label : "ALL",
		   lp->new_label);
	}
	if (lp->flags & LABEL_CHANGE_VALUE) {
	    printf("Value:\t\t%s -> %s\n",
		   lp->old_value ? lp->old_value : "ALL",
		   lp->new_value);
	}
	if (lp->flags & LABEL_DELETE)
	    printf("DELETE\n");
    }
    if (change == 0)
	printf("No changes\n");
}

static int
anychange(void)
{
    const indomspec_t	*ip;
    const metricspec_t	*mp;
    const textspec_t	*tp;
    const labelspec_t	*lp;
    int			i;

    if (global.flags != 0) {
    // TODO WARN about no-ops for changes to V3 label fields in V2 output?
	if (pmDebugOptions.appl3) {
	    fprintf(stderr, "anychange: global.flags (%d,", global.flags);
	    if (global.flags & GLOBAL_CHANGE_TIME)
		fprintf(stderr, " CHANGE_TIME");
	    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
		fprintf(stderr, " CHANGE_HOSTNAME");
	    if (global.flags & GLOBAL_CHANGE_TIMEZONE)
		fprintf(stderr, " CHANGE_TIMEZONE");
	    if (global.flags & GLOBAL_CHANGE_ZONEINFO)
		fprintf(stderr, " CHANGE_ZONEINFO");
	    if (global.flags & GLOBAL_CHANGE_FEATURES)
		fprintf(stderr, " CHANGE_FEATURES");
	    fprintf(stderr, ") != 0\n");
	}
	return 1;
    }
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (ip->new_indom != ip->old_indom) {
	    if (pmDebugOptions.appl3) {
		fprintf(stderr, "anychange: indom %s changed\n", pmInDomStr(ip->old_indom));
	    }
	    return 1;
	}
	for (i = 0; i < ip->numinst; i++) {
	    if (ip->inst_flags[i]) {
		if (pmDebugOptions.appl3) {
		    fprintf(stderr, "anychange: indom %s inst %d flags (%d,",
			pmInDomStr(ip->old_indom), i, ip->inst_flags[i]);
		    if (ip->inst_flags[i] & INST_CHANGE_INST)
			fprintf(stderr, " CHANGE_INST");
		    if (ip->inst_flags[i] & INST_CHANGE_INAME)
			fprintf(stderr, " CHANGE_INAME");
		    if (ip->inst_flags[i] & INST_DELETE)
			fprintf(stderr, " DELETE");
		    fprintf(stderr, ") != 0\n");
		}
		return 1;
	    }
	}
    }
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->flags != 0 || mp->ip != NULL) {
	    if (pmDebugOptions.appl3) {
		if (mp->flags != 0) {
		    fprintf(stderr, "anychange: metric %s flags (%d,",
			mp->old_name, mp->flags);
		    if (mp->flags & METRIC_CHANGE_PMID)
			fprintf(stderr, " CHANGE_PMID");
		    if (mp->flags & METRIC_CHANGE_NAME)
			fprintf(stderr, " CHANGE_NAME");
		    if (mp->flags & METRIC_CHANGE_TYPE)
			fprintf(stderr, " CHANGE_TYPE");
		    if (mp->flags & METRIC_CHANGE_INDOM)
			fprintf(stderr, " CHANGE_INDOM");
		    if (mp->flags & METRIC_CHANGE_SEM)
			fprintf(stderr, " CHANGE_SEM");
		    if (mp->flags & METRIC_CHANGE_UNITS)
			fprintf(stderr, " CHANGE_UNITS");
		    if (mp->flags & METRIC_DELETE)
			fprintf(stderr, " DELETE");
		    if (mp->flags & METRIC_RESCALE)
			fprintf(stderr, " RESCALE");
		    fprintf(stderr, ") != 0\n");
		}
		if (mp->ip != NULL)
		    fprintf(stderr, "anychange: metric %s ip NULL\n", mp->old_name);
	    }
	    return 1;
	}
    }
    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (lp->flags != 0 || lp->ip != NULL) {
	    if (pmDebugOptions.appl3) {
		if (lp->flags != 0) {
		    fprintf(stderr, "anychange: label %s flags (%d,",
			lp->old_label, lp->flags);
		    if (lp->flags & LABEL_ACTIVE)
			fprintf(stderr, " ACTIVE");
		    if (lp->flags & LABEL_CHANGE_ID)
			fprintf(stderr, " CHANGE_ID");
		    if (lp->flags & LABEL_CHANGE_LABEL)
			fprintf(stderr, " CHANGE_LABEL");
		    if (lp->flags & LABEL_CHANGE_INSTANCE)
			fprintf(stderr, " CHANGE_INSTANCE");
		    if (lp->flags & LABEL_DELETE)
			fprintf(stderr, " DELETE");
		    if (lp->flags & LABEL_NEW)
			fprintf(stderr, " NEW");
		    fprintf(stderr, ") != 0\n");
		}
		if (lp->ip != NULL)
		    fprintf(stderr, "anychange: label %s ip NULL\n", lp->old_label);
	    }
	    return 1;
	}
    }
    for (tp = text_root; tp != NULL; tp = tp->t_next) {
	if (tp->flags != 0 || tp->ip != NULL) {
	    if (pmDebugOptions.appl3) {
		if (tp->flags != 0)
		    fprintf(stderr, "anychange: %s %s text flags (%d) != 0\n",
		       (tp->old_type & PM_TEXT_PMID) ? "pmID" : "pmInDom",
		       (tp->old_type & PM_TEXT_PMID) ? pmIDStr(tp->old_id) : pmInDomStr(tp->old_id),
		       tp->flags);
		if (tp->ip != NULL)
		    fprintf(stderr, "anychange: %s %s text ip NULL\n",
		       (tp->old_type & PM_TEXT_PMID) ? "pmID" : "pmInDom",
		       (tp->old_type & PM_TEXT_PMID) ? pmIDStr(tp->old_id) : pmInDomStr(tp->old_id));
	    }
	    return 1;
	}
    }
    
    return 0;
}

static int
fixstamp(__pmTimestamp *tsp)
{
    if (global.flags & GLOBAL_CHANGE_TIME) {
	if (global.time.sec > 0) {
	    __pmTimestampInc(tsp, &global.time);
	    return 1;
	}
	else if (global.time.sec < 0) {
	    /*
	     * parser makes sec < 0 and nsec >= 0 ...
	     */
	    global.time.sec = -global.time.sec;
	    __pmTimestampDec(tsp, &global.time);
	    global.time.sec = -global.time.sec;
	    return 1;
	}
    }
    return 0;
}

/*
 * Link metricspec_t entries to corresponding indom_t entry if there
 * are changes to instance identifiers or instance names (includes
 * instance deletion)
 *
 * Do the same for textspec_t and labelspec_t entries to their corresponding
 * metricspec_t and indomspec_t entries.
 */
static void
link_entries(void)
{
    indomspec_t			*ip;
    metricspec_t		*mp;
    textspec_t			*tp;
    labelspec_t			*lp;
    const pmLabelSet		*lsp;
    const __pmLogLabelSet	*llsp;
    __pmHashCtl			*hcp, *hcp2;
    __pmHashNode		*node, *node2;
    int				old_id, new_id;
    int				i;
    int				type;
    int				change;
    char			strbuf[64];

    /* Link metricspec_t entries to indomspec_t entries */
    hcp = &inarch.ctxp->c_archctl->ac_log->hashpmid;
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
	    if (mp->old_desc.indom == ip->old_indom ||
	        mp->new_desc.indom == ip->new_indom) {
		if (change)
		    mp->ip = ip;
		if (ip->new_indom != ip->old_indom) {
		    if (mp->flags & METRIC_CHANGE_INDOM) {
			/* indom already changed via metric clause */
			if (mp->new_desc.indom != ip->new_indom) {
			    pmsprintf(strbuf, sizeof(strbuf), "%s", pmInDomStr(mp->new_desc.indom));
			    pmsprintf(mess, sizeof(mess), "Conflicting indom change for metric %s (%s from metric clause, %s from indom clause)", mp->old_name, strbuf, pmInDomStr(ip->new_indom));
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

    /* Link textspec_t entries to indomspec_t entries */
    hcp = &inarch.ctxp->c_archctl->ac_log->hashtext;
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	change = 0;
	for (i = 0; i < ip->numinst; i++)
	    change |= (ip->inst_flags[i] != 0);
	if (change == 0 && ip->new_indom == ip->old_indom)
	    continue;

	for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	    /* We are only interested in help text for indoms. */
	    type = (int)(node->key);
	    if (!(type & PM_TEXT_INDOM))
		continue;

	    /* Look for help text for the current indom spec. */
	    hcp2 = (__pmHashCtl *)(node->data);
	    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
		 node2 != NULL;
		 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
		if ((int)(node2->key) != ip->old_indom)
		    continue;

		/* Found one. */
		tp = start_text(type, (int)(node2->key), NULL);
		assert(tp->old_id == ip->old_indom);
		if (change)
		    tp->ip = ip;
		if (ip->new_indom != ip->old_indom) {
		    if (tp->flags & TEXT_CHANGE_ID) {
			/* indom already changed via text clause */
			if (tp->new_id != ip->new_indom) {
			    pmsprintf(strbuf, sizeof(strbuf), "%s", pmInDomStr(tp->new_id));
			    pmsprintf(mess, sizeof(mess), "Conflicting indom change for help text (%s from text clause, %s from indom clause)", strbuf, pmInDomStr(ip->new_indom));
			    yysemantic(mess);
			}
		    }
		    else {
			tp->flags |= TEXT_CHANGE_ID;
			tp->new_id = ip->new_indom;
		    }
		}
	    }
	}
    }

    /* Link textspec_t entries to metricspec_t entries */
    assert(hcp == &inarch.ctxp->c_archctl->ac_log->hashtext);
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->new_desc.pmid == mp->old_desc.pmid)
	    continue;

	for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	    /* We are only interested in help text for pmids. */
	    type = (int)(node->key);
	    if (!(type & PM_TEXT_PMID))
		continue;

	    /* Look for help text for the current metric spec. */
	    hcp2 = (__pmHashCtl *)(node->data);
	    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
		 node2 != NULL;
		 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
		if ((int)(node2->key) != mp->old_desc.pmid)
		    continue;

		/* Found one. */
		tp = start_text(type, (int)(node2->key), NULL);
		assert(tp->old_id == mp->old_desc.pmid);
		if (mp->new_desc.pmid != mp->old_desc.pmid) {
		    if (tp->flags & TEXT_CHANGE_ID) {
			/* pmid already changed via text clause */
			if (tp->new_id != mp->new_desc.pmid) {
			    pmsprintf(strbuf, sizeof(strbuf), "%s", pmIDStr(tp->new_id));
			    pmsprintf(mess, sizeof(mess), "Conflicting pmid change for help text (%s from text clause, %s from pmid clause)", strbuf, pmIDStr(mp->new_desc.pmid));
			    yysemantic(mess);
			}
		    }
		    else {
			tp->flags |= TEXT_CHANGE_ID;
			tp->new_id = mp->new_desc.pmid;
		    }
		}
	    }
	}
    }

    /* Link labelspec_t entries to indomspec_t entries */
    hcp = &inarch.ctxp->c_archctl->ac_log->hashlabels;
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	change = 0;
	for (i = 0; i < ip->numinst; i++)
	    change |= (ip->inst_flags[i] != 0);
	if (change == 0 && ip->new_indom == ip->old_indom)
	    continue;

	for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	    /* We are only interested in labels for indoms. */
	    type = (int)(node->key);
	    if (!(type & (PM_LABEL_INDOM | PM_LABEL_INSTANCES)))
		continue;

	    /* Look for labels for the current indom spec. */
	    hcp2 = (__pmHashCtl *)(node->data);
	    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
		 node2 != NULL;
		 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
		if ((int)(node2->key) != ip->old_indom)
		    continue;

		/* Found one. */
		llsp = (__pmLogLabelSet *)node2->data;
		for (i = 0; i < llsp->nsets; ++i) {
		    lsp = &llsp->labelsets[i];
		    lp = start_label(type, (int)node2->key, lsp->inst, NULL, NULL, NULL);
		    assert(lp->old_id == ip->old_indom);
		    if (change)
			lp->ip = ip;
		    if (ip->new_indom != ip->old_indom) {
			if (lp->flags & LABEL_CHANGE_ID) {
			    /* indom already changed via label clause */
			    if (lp->new_id != ip->new_indom) {
				pmsprintf(strbuf, sizeof(strbuf), "%s", pmInDomStr(lp->new_id));
				pmsprintf(mess, sizeof(mess), "Conflicting indom change for label set (%s from text clause, %s from indom clause)", strbuf, pmInDomStr(ip->new_indom));
				yysemantic(mess);
			    }
			}
			else {
			    lp->flags |= LABEL_CHANGE_ID;
			    lp->new_id = ip->new_indom;
			}
		    }
		}
	    }
	}
    }

    /* Link labelspec_t entries to metricspec_t entries */
    assert(hcp == &inarch.ctxp->c_archctl->ac_log->hashlabels);
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->new_desc.pmid == mp->old_desc.pmid)
	    continue;

	for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
	     node != NULL;
	     node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
	    /* We are only interested in label sets for pmids. */
	    type = (int)(node->key);
	    if (!(type & (PM_LABEL_DOMAIN | PM_LABEL_CLUSTER | PM_LABEL_ITEM)))
		continue;

	    /* Look for label sets for the current metric spec. */
	    hcp2 = (__pmHashCtl *)(node->data);
	    for (node2 = __pmHashWalk(hcp2, PM_HASH_WALK_START);
		 node2 != NULL;
		 node2 = __pmHashWalk(hcp2, PM_HASH_WALK_NEXT)) {
		if ((type & PM_LABEL_DOMAIN)) {
		    old_id = pmID_domain(mp->old_desc.pmid);
		    new_id = pmID_domain(mp->new_desc.pmid);
		}
		else if ((type & PM_LABEL_CLUSTER)) {
		    old_id = pmID_domain(mp->old_desc.pmid) |
			pmID_cluster(mp->old_desc.pmid);
		    new_id = pmID_domain(mp->new_desc.pmid) |
			pmID_cluster(mp->new_desc.pmid);
		}
		else {
		    old_id = mp->old_desc.pmid;
		    new_id = mp->new_desc.pmid;
		}
		if ((int)(node2->key) != old_id)
		    continue;

		/* Found one. */
		lp = start_label(type, old_id, 0, NULL, NULL, NULL);
		assert(lp->old_id == old_id);
		if (old_id != new_id) {
		    if (lp->flags & LABEL_CHANGE_ID) {
			/* pmid already changed via label clause */
			if (lp->new_id != new_id) {
			    pmsprintf(strbuf, sizeof(strbuf), "%s", pmIDStr(lp->new_id));
			    pmsprintf(mess, sizeof(mess), "Conflicting pmid change for help label (%s from label clause, %s from pmid clause)", strbuf, pmIDStr(mp->new_desc.pmid));
			    yysemantic(mess);
			}
		    }
		    else {
			lp->flags |= LABEL_CHANGE_ID;
			lp->new_id = new_id;
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
     * For each metric, labelset, and help text, make sure the output instance
     * domain will be in the output archive.
     * Called after link_entries(), so if an item is associated
     * with an instance domain that has any instance rewriting, we're OK.
     * The case to be checked here is a rewritten item with an indom
     * clause and no associated indomspec_t (so no instance domain changes,
     * but the new indom may not match any indom in the archive.
     */
    const metricspec_t	*mp;
    const indomspec_t	*ip;
    const textspec_t	*tp;
    const labelspec_t	*lp;
    __pmHashCtl		*hcp;
    __pmHashNode	*node;
    char		buf[64];

    hcp = &inarch.ctxp->c_archctl->ac_log->hashindom;

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
		    if (ip->new_indom == mp->new_desc.indom)
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
		pmsprintf(mess, sizeof(mess), "New indom (%s) for metric %s is not in the output archive", pmInDomStr(mp->new_desc.indom), mp->old_name);
		yysemantic(mess);
	    }
	}
    }

    for (tp = text_root; tp != NULL; tp = tp->t_next) {
	if (tp->ip != NULL)
	    /* associated indom has instance changes, we're OK */
	    continue;
	if (!(tp->new_type & PM_TEXT_INDOM))
	    continue;
	if ((tp->flags & TEXT_CHANGE_ID)) {
	    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
		 node != NULL;
		 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
		/*
		 * if this indom has an indomspec_t, check that, else
		 * this indom will go to the archive without change
		 */
		for (ip = indom_root; ip != NULL; ip = ip->i_next) {
		    if (ip->old_indom == tp->old_id)
			break;
		}
		if (ip == NULL) {
		    if ((pmInDom)(node->key) == tp->new_id)
			/* we're OK */
			break;
		}
		else {
		    if (ip->new_indom != ip->old_indom &&
		        ip->new_indom == tp->new_id)
			/* we're OK */
			break;
		}
	    }
	    if (node == NULL) {
		pmsprintf(mess, sizeof(mess), "New indom (%s) for help text is not in the output archive", pmInDomStr(tp->new_id));
		yysemantic(mess);
	    }
	}
    }

    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	if (lp->ip != NULL)
	    /* associated indom has instance changes, we're OK */
	    continue;
	if (!(lp->new_type & (PM_LABEL_INDOM | PM_LABEL_INSTANCES)))
	    continue;
	if ((lp->flags & LABEL_CHANGE_ID)) {
	    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
		 node != NULL;
		 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
		/*
		 * if this indom has an indomspec_t, check that, else
		 * this indom will go to the archive without change
		 */
		for (ip = indom_root; ip != NULL; ip = ip->i_next) {
		    if (ip->old_indom == lp->old_id)
			break;
		}
		if (ip == NULL) {
		    if ((pmInDom)(node->key) == lp->new_id)
			/* we're OK */
			break;
		}
		else {
		    if (ip->new_indom != ip->old_indom &&
		        ip->new_indom == lp->new_id)
			/* we're OK */
			break;
		}
	    }
	    if (node == NULL) {
		pmsprintf(mess, sizeof(mess), "New indom (%s) for label set %s is not in the output archive",
			  pmInDomStr(lp->new_id),
			  __pmLabelIdentString(lp->new_id, lp->new_type,
					       buf, sizeof(buf)));
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
		    pmsprintf(mess, sizeof(mess), "Duplicate instance id %d (\"%s\" and \"%s\") for indom %s", insti, namei, namej, pmInDomStr(ip->old_indom));
		    yysemantic(mess);
		}
		if (inst_name_eq(namei, namej) > 0) {
		    pmsprintf(mess, sizeof(mess), "Duplicate instance name \"%s\" (%d) and \"%s\" (%d) for indom %s", namei, insti, namej, instj, pmInDomStr(ip->old_indom));
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
     * Note instance renumbering happens _after_ value selection from
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
		    ip = start_indom(mp->old_desc.indom, 0);
		    if (ip == NULL) {
			pmsprintf(mess, sizeof(mess), "Botch: old indom %s for metric %s not found", pmInDomStr(mp->old_desc.indom), mp->old_name);
			yyerror(mess);
		    }
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
				pmsprintf(mess, sizeof(mess), "Instance \"%s\" from OUTPUT clause not found in old indom %s", mp->one_name, pmInDomStr(mp->old_desc.indom));
			    else
				pmsprintf(mess, sizeof(mess), "Instance %d from OUTPUT clause not found in old indom %s", mp->one_inst, pmInDomStr(mp->old_desc.indom));
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
		    ip = start_indom(mp->new_desc.indom, 1);
		    if (ip == NULL) {
			/*
			 * can't find new indom, perhaps it is the result of
			 * renumbering ...
			 */
			for (ip = indom_root; ip != NULL; ip = ip->i_next) {
			    if (ip->new_indom == mp->new_desc.indom)
				break;
			}
		    }
		    if (ip == NULL) {
			pmsprintf(mess, sizeof(mess), "Botch: new indom %s for metric %s not found", pmInDomStr(mp->new_desc.indom), mp->old_name);
			yyerror(mess);
		    }
		    else {
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
				    pmsprintf(mess, sizeof(mess), "Instance \"%s\" from OUTPUT clause not found in new indom %s", mp->one_name, pmInDomStr(mp->new_desc.indom));
				else
				    pmsprintf(mess, sizeof(mess), "Instance %d from OUTPUT clause not found in new indom %s", mp->one_inst, pmInDomStr(mp->new_desc.indom));
				yywarn(mess);
			    }
			}
		    }
		    /*
		     * use default rule (inst id 0) if INAME not found and
		     * and instance id is needed for output value
		     */
		    if (mp->old_desc.indom == PM_INDOM_NULL && mp->one_inst == PM_IN_NULL)
			mp->one_inst = 0;
		}
	    }
	}
    }
}

static void
do_newlabelsets(void)
{
    long		out_offset;
    unsigned int	type;
    unsigned int	ident;
    int			nsets;
    pmLabelSet		*labellist = NULL;
    __pmTimestamp	stamp;
    labelspec_t		*lp;
    int			sts;
    char		buf[64];

    out_offset = __pmFtell(outarch.logctl.mdfp);

    /*
     * Traverse the list of label change records and emit any new label sets
     * at the globally adjusted start time.
     */
    stamp = inarch.rp->timestamp;	/* struct assignment */

    for (lp = label_root; lp != NULL; lp = lp->l_next) {
	/* Is this a new label record? */
	if (! ((lp->flags & LABEL_NEW)))
	    continue;

	/*
	 * Write the record.
	 * libpcp, via __pmLogPutLabels(), assumes control of the storage pointed
	 * to by labellist.
	 */
	ident = lp->new_id;
	type = lp->new_type;
	nsets = 1;
	labellist = lp->new_labels;

	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "New: labels for ");
	    if ((lp->old_type & PM_LABEL_CONTEXT))
		fprintf(stderr, " context\n");
	    else if ((lp->old_type & PM_LABEL_DOMAIN))
		fprintf(stderr, " domain %d\n", pmID_domain(lp->old_id));
	    else if ((lp->old_type & PM_LABEL_CLUSTER))
		fprintf(stderr, " cluster %d.%d\n", pmID_domain(lp->old_id), pmID_cluster (lp->old_id));
	    else if ((lp->old_type & PM_LABEL_ITEM))
		fprintf(stderr, " item %s\n", pmIDStr(lp->old_id));
	    else if ((lp->old_type & PM_LABEL_INDOM))
		fprintf(stderr, " indom %s\n", pmInDomStr(lp->old_id));
	    else if ((lp->old_type & PM_LABEL_INSTANCES))
		fprintf(stderr, " the instances of indom %s\n", pmInDomStr(lp->old_id));
	    pmPrintLabelSets(stderr, ident, type, labellist, nsets);
	}

	if ((sts = __pmLogPutLabels(&outarch.archctl, type, ident,
				   nsets, labellist, &stamp)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutLabels: %s: %s\n",
		    pmGetProgname(),
		    __pmLabelIdentString(ident, type, buf, sizeof(buf)),
		    pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}

	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "Metadata: write ");
	    if (outarch.version == PM_LOG_VERS02)
		fprintf(stderr, "V2 ");
	    fprintf(stderr, "LabelSet %s @ offset=%ld\n",
		    __pmLabelIdentString(ident, type, buf, sizeof(buf)), out_offset);
	}

	if (first_datarec) {
	    first_datarec = 0;
	    /*
	     * Any global time adjustment done after the first record is output
	     * above
	     */
	    outarch.logctl.label.start = inarch.rp->timestamp;
	    /* need to fix start-time in label records */
	    writelabel(1);
	    needti = 1;
	}
    }
}

void
open_input(int flags)
{
    int		sts;

    inarch.logrec = inarch.metarec = NULL;
    inarch.mark = 0;
    inarch.rp = NULL;

    if ((inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE | flags, inarch.name)) < 0) {
	if (inarch.ctx == PM_ERR_NODATA) {
	    fprintf(stderr, "%s: Warning: empty archive \"%s\" will be skipped\n",
		    pmGetProgname(), inarch.name);
	    exit(0);
	}
	if (inarch.ctx == PM_ERR_FEATURE) {
	    /* try w/out feature bits checking */
	    fprintf(stderr, "%s: Warning: archive \"%s\": unsupported feature bits, other errors may follow ...\n",
		    pmGetProgname(), inarch.name);
	    inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE | flags | PM_CTXFLAG_NO_FEATURE_CHECK, inarch.name);
	}
	if (inarch.ctx < 0) {
	    fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		    pmGetProgname(), inarch.name, pmErrStr(inarch.ctx));
	    exit(1);
	}
    }
    inarch.ctxp = __pmHandleToPtr(inarch.ctx);
    assert(inarch.ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(inarch.ctxp->c_lock);

    if ((sts = __pmLogLoadLabel(inarch.ctxp->c_archctl->ac_log->mdfp, &inarch.label)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n",
		pmGetProgname(), inarch.name, pmErrStr(sts));
	exit(1);
    }

    inarch.version = (inarch.label.magic & 0xff);
    if (inarch.version != PM_LOG_VERS02 && inarch.version != PM_LOG_VERS03) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmGetProgname(), inarch.version, inarch.name);
	exit(1);
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
    int		doneti = 0;
    int		in_vol_missing = 0;	/* == 1 if one or more input data volumes missing */
    __pmTimestamp	tstamp = { 0, 0 };	/* for last log record */
    off_t	old_log_offset = 0;	/* log offset before last log record */
    off_t	old_meta_offset;
    int		old_in_vol;		/* previous input data volume */
    int		seen_event = 0;
    metricspec_t	*mp;

    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* input archive ... inarch.name set in parseargs() */
    if (qflag)
	/* -q => "peek" only at metadata initially */
	open_input(PM_CTXFLAG_METADATA_ONLY);
    else
	/* no -q => fully functional context needed */
	open_input(0);

    if (outarch.version == 0)
	outarch.version = inarch.version;
    if (outarch.version == PM_LOG_VERS02 && inarch.version == PM_LOG_VERS03) {
	fprintf(stderr,"%s: Error: cannot create a v2 archive from v3 (%s)\n",
		pmGetProgname(), inarch.name);
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
	int	sep = pmPathSeparator();

#if HAVE_MKSTEMP
	strncpy(path, argv[argc-1], sizeof(path));
	path[sizeof(path)-1] = '\0';
	strncpy(dname, dirname(path), sizeof(dname));
	dname[sizeof(dname)-1] = '\0';
	if ((dir_fd = open(dname, O_RDONLY)) < 0) {
	    fprintf(stderr, "%s: Error: cannot open directory \"%s\" for reading: %s\n", pmGetProgname(), dname, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	pmsprintf(path, sizeof(path), "%s%cXXXXXX", dname, sep);
	cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	tmp_f1 = mkstemp(path);
	umask(cur_umask);
	outarch.name = strdup(path);
	if (outarch.name == NULL) {
	    fprintf(stderr, "%s: Error: temp file strdup(%s) failed: %s\n", pmGetProgname(), path, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	pmsprintf(bak_base, sizeof(bak_base), "%s%cXXXXXX", dname, sep);
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
	    fprintf(stderr, "%s: Error: first tempnam() failed: %s\n", pmGetProgname(), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	else {
	    outarch.name = strdup(s);
	    if (outarch.name == NULL) {
		fprintf(stderr, "%s: Error: temp file strdup(%s) failed: %s\n", pmGetProgname(), s, strerror(errno));
		abandon();
		/*NOTREACHED*/
	    }
	    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	    tmp_f1 = open(outarch.name, O_WRONLY|O_CREAT|O_EXCL, 0600);
	    umask(cur_umask);
	}
	if ((s = tempnam(dname, fname)) == NULL) {
	    fprintf(stderr, "%s: Error: second tempnam() failed: %s\n", pmGetProgname(), strerror(errno));
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
	    fprintf(stderr, "%s: Error: create first temp (%s) failed: %s\n", pmGetProgname(), outarch.name, strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	if (tmp_f2 < 0) {
	    fprintf(stderr, "%s: Error: create second temp (%s) failed: %s\n", pmGetProgname(), bak_base, strerror(errno));
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
	if (pmDebugOptions.appl4) {
	    fprintf(stderr, "Start config: %s\n", conf[i]);
	}
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

    if (qflag && anychange() == 0) {
	if (pmDebugOptions.appl3) {
	    fprintf(stderr, "Done, no rewriting required\n");
	}
	exit(0);
    }

    if (qflag) {
	/*
	 * rewriting is in the wind, so need a fully functional
	 * context now
	 */
	pmDestroyContext(inarch.ctx);
	open_input(0);
    }

    /*
     * create output log - must be done before writing label
     * ... and start at the same initial volume (volume 0, 1, ...
     * may be missing)
     */
    outarch.archctl.ac_log = &outarch.logctl;
    if ((sts = __pmLogCreate("", outarch.name, outarch.version, &outarch.archctl, inarch.ctxp->c_archctl->ac_curvol)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate(%s,v%d): %s\n",
		pmGetProgname(), outarch.name, outarch.version, pmErrStr(sts));
	/*
	 * do not cleanup ... if error is EEXIST we should not clobber an
	 * existing (and persumably) good archive
	 */
	exit(1);
	/*NOTREACHED*/
    }
    outarch.archctl.ac_curvol = inarch.ctxp->c_archctl->ac_curvol;
    if (outarch.archctl.ac_curvol != 0) {
	/* volume 0 is missing */
	in_vol_missing =  1;
    }

    /* initialize and write label records */
    newlabel();
    outarch.logctl.state = PM_LOG_STATE_INIT;
    writelabel(0);

    /*
     * build per-indom reference counts
     * ... first all metrics in the archive (note metrics with
     * duplicate names are only counted once because deleting
     * a metric by any name deletes the metric _and_ all of the
     * names)
     */
    for (i = 0; i < inarch.ctxp->c_archctl->ac_log->hashpmid.hsize; i++) {
	__pmHashNode	*hp;
	for (hp = inarch.ctxp->c_archctl->ac_log->hashpmid.hash[i]; hp != NULL; hp = hp->next) {
	    pmDesc		*dp;
	    __pmHashNode	*ip;
	    int			*refp;
	    dp = (pmDesc *)hp->data;
	    if (dp->indom == PM_INDOM_NULL)
		continue;
	    if ((ip = __pmHashSearch((unsigned int)dp->indom, &indom_hash)) == NULL) {
		/* first time we've seen this indom */
		if ((refp = (int *)malloc(sizeof(int))) == NULL) {
		    /*
		     * pretty sure we'll die with malloc() failure
		     * later, but for the moment just ignore this
		     * indom ... worst result is we don't cull an
		     * unreferenced indom
		     */
		    continue;
		}
		*refp = 1;
		sts = __pmHashAdd((unsigned int)dp->indom, (void *)refp, &indom_hash);
		if (sts < 0) {
		    fprintf(stderr, "__pmHashAdd: failed for indom %s: %s\n", pmInDomStr(dp->indom), pmErrStr(sts));
		    free(refp);
		    /*
		     * see comment above about "worst result ..."
		     */
		    continue;
		}
	    }
	    else {
		/* not the first time, bump the refcount */
		refp = (int *)ip->data;
		(*refp)++;
	    }
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "ref++ InDom: %s refcnt: %d (for PMID: %s)\n",
		    pmInDomStr(dp->indom), *refp, pmIDStr(dp->pmid));
	    }
	}
    }
    /*
     * ... next walk the metrics from the config, decrementing the
     * reference count for any metrics to be deleted and maybe
     * for metrics that are moving from one indom to another
     */
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->flags & METRIC_DELETE) {
	    __pmHashNode	*hp;
	    int			*refp;
	    if (mp->old_desc.indom == PM_INDOM_NULL)
		continue;
	    if ((hp = __pmHashSearch((unsigned int)mp->old_desc.indom, &indom_hash)) == NULL) {
		fprintf(stderr, "Botch: InDom: %s (PMID: %s): not in indom_hash table\n",
		    pmInDomStr(mp->old_desc.indom), pmIDStr(mp->old_desc.pmid));
		continue;
	    }
	    refp = (int *)hp->data;
	    (*refp)--;
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "delete metric ref-- InDom: %s refcnt: %d (for PMID: %s)\n",
		    pmInDomStr(mp->old_desc.indom), *refp, pmIDStr(mp->old_desc.pmid));
	    }
	}
	else if (mp->flags & METRIC_CHANGE_INDOM) {
	    __pmHashNode	*hp;
	    int			*refp;
	    if (mp->old_desc.indom != PM_INDOM_NULL) {
		/* decrement refcount for old indom */
		if ((hp = __pmHashSearch((unsigned int)mp->old_desc.indom, &indom_hash)) == NULL) {
		    fprintf(stderr, "Botch: old InDom: %s (PMID: %s): not in indom_hash table\n",
			pmInDomStr(mp->old_desc.indom), pmIDStr(mp->old_desc.pmid));
		    continue;
		}
		refp = (int *)hp->data;
		(*refp)--;
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "renumber indom ref-- InDom: %s refcnt: %d (for PMID: %s)\n",
			pmInDomStr(mp->old_desc.indom), *refp, pmIDStr(mp->old_desc.pmid));
		}
	    }
	    if (mp->new_desc.indom != PM_INDOM_NULL) {
		pmInDom	indom;
		/*
		 * increment refcount for new indom ... wrinkle here is
		 * that new indom id may have been rewritten and then
		 * we need to increment the refcount for the corresponding
		 * input archive indom
		 */
		if ((hp = __pmHashSearch((unsigned int)mp->new_desc.indom, &indom_hash)) == NULL) {
		    indomspec_t		*ip;
		    /* indom is not used in input archive ... */
		    /*
		     * if the indom is not being changed, ip will be NULL
		     * after this loop
		     */
		    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
			if (ip->new_indom == mp->new_desc.indom)
			    break;
		    }
		    if (ip == NULL) {
			fprintf(stderr, "Botch: new InDom: %s (PMID: %s): not in indom_root table\n",
			    pmInDomStr(mp->new_desc.indom), pmIDStr(mp->old_desc.pmid));
			continue;
		    }
		    if ((hp = __pmHashSearch((unsigned int)ip->old_indom, &indom_hash)) == NULL) {
			fprintf(stderr, "Botch: input InDom: %s (PMID: %s): not in indom_hash table\n",
			    pmInDomStr(ip->old_indom), pmIDStr(mp->old_desc.pmid));
			continue;
		    }
		    indom = ip->old_indom;
		}
		else
		    indom = mp->new_desc.indom;
		refp = (int *)hp->data;
		(*refp)++;
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "renumber indom ref++ InDom: %s refcnt: %d (for PMID: %s)\n",
			pmInDomStr(indom), *refp, pmIDStr(mp->old_desc.pmid));
		}
	    }
	}
    }

    first_datarec = 1;
    ti_idx = 0;

    /*
     * loop
     *	- get next log record
     *	- write out new/changed metadata required by this log record
     *	- write out log
     *	- do ti update if necessary
     */
    while (1) {
	static long	in_offset;		/* for -Dappl0 */

	__pmFflush(outarch.logctl.mdfp);
	old_meta_offset = __pmFtell(outarch.logctl.mdfp);
	assert(old_meta_offset >= 0);
	old_in_vol = inarch.ctxp->c_archctl->ac_curvol;

	in_offset = __pmFtell(inarch.ctxp->c_archctl->ac_mfp);
	stslog = nextlog();
	if (stslog < 0) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "Log: read EOF @ offset=%ld\n", in_offset);
	    break;
	}
	if (stslog == 1) {
	    /* volume change */
	    if (inarch.ctxp->c_archctl->ac_curvol != old_in_vol + 1)
		in_vol_missing = 1;
	    if (inarch.ctxp->c_archctl->ac_curvol >= outarch.archctl.ac_curvol+1)
		/* track input volume numbering */
		newvolume(inarch.ctxp->c_archctl->ac_curvol);
	    else
		/*
		 * output archive volume number is ahead, probably because
		 * rewriting has forced an earlier volume change
		 */
		newvolume(outarch.archctl.ac_curvol+1);
	    needti = 1;
	}
	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "Log: read ");
	    __pmPrintTimestamp(stderr, &inarch.rp->timestamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, in_offset);
	}

	if (ti_idx < inarch.ctxp->c_archctl->ac_log->numti) {
	    __pmLogTI	*tip = &inarch.ctxp->c_archctl->ac_log->ti[ti_idx];
	    if (tip->stamp.sec == inarch.rp->timestamp.sec &&
	        tip->stamp.nsec == inarch.rp->timestamp.nsec) {
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
	 * Write out any new label sets before any other data using the adjusted
	 * time stamp of the first data record.
	 */
	if (first_datarec)
	    do_newlabelsets();

	/*
	 * process metadata until we find an indom or label record with
	 * timestamp after the current log record, or a metric record
	 * for a pmid that is not in the current log record
	 */
	for ( ; ; ) {
	    if (stsmeta == 0) {
		in_offset = __pmFtell(inarch.ctxp->c_archctl->ac_log->mdfp);
		stsmeta = nextmeta();
		if (pmDebugOptions.appl0) {
		    if (stsmeta < 0)
			fprintf(stderr, "Metadata: read EOF @ offset=%ld\n", in_offset);
		    else if (stsmeta == TYPE_DESC)
			fprintf(stderr, "Metadata: read PMID %s @ offset=%ld\n", pmIDStr(ntoh_pmID(inarch.metarec[2])), in_offset);
		    else if (stsmeta == TYPE_INDOM_V2)
			fprintf(stderr, "Metadata: read V2 InDom %s @ offset=%ld\n", pmInDomStr(ntoh_pmInDom((unsigned int)inarch.metarec[4])), in_offset);
		    else if (stsmeta == TYPE_LABEL_V2)
			fprintf(stderr, "Metadata: read V2 LabelSet @ offset=%ld\n", in_offset);
		    else if (stsmeta == TYPE_TEXT)
			fprintf(stderr, "Metadata: read Text @ offset=%ld\n", in_offset);
		    else if (stsmeta == TYPE_INDOM)
			fprintf(stderr, "Metadata: read InDom %s @ offset=%ld\n", pmInDomStr(ntoh_pmInDom((unsigned int)inarch.metarec[5])), in_offset);
		    else if (stsmeta == TYPE_INDOM_DELTA)
			fprintf(stderr, "Metadata: read Delta InDom %s @ offset=%ld\n", pmInDomStr(ntoh_pmInDom((unsigned int)inarch.metarec[5])), in_offset);
		    else if (stsmeta == TYPE_LABEL)
			fprintf(stderr, "Metadata: read LabelSet @ offset=%ld\n", in_offset);
		    else
			fprintf(stderr, "Metadata: read botch type=%d\n", stsmeta);
		}
	    }
	    if (stsmeta < 0) {
		break;
	    }

	    if (stsmeta == TYPE_DESC) {
		pmDesc	*dp = (pmDesc *)&inarch.metarec[2];
		pmID	pmid = ntoh_pmID(dp->pmid);
		int	type = ntohl(dp->type);
		/*
		 * We used to apply an optimization here to stop
		 * processing pmDesc metadata if the PMID of the
		 * next metadata did not match one of the PMIDs
		 * in the current pmResult.
		 *
		 * Unfortunately "event" records contain packed
		 * PMIDs for the event records and pseudo PMIDs
		 * for the metrics used to encode the event record
		 * parameters ... these just confuse the optimization
		 * and unpacking the event records does not help
		 * because the pmDesc for the pseudo PMIDs are 
		 * written in a batch when the first event record
		 * is output, even if they may not appear in a packed
		 * pmResult until later, or not at all.
		 *
		 * So if we've seen a metric of type PM_TYPE_EVENT
		 * or PM_TYPE_HIGHRES_EVENT, just keep processing the
		 * pmDesc metadata until it is exhausted, or we find
		 * a pmInDom metadata record with a timestamp after the
		 * current pmResult.
		 *
		 * The other bad news case is when an input volume is
		 * missing, and then we may have pmDesc metadata but no
		 * corresponding pmResult, so if this happens all we
		 * can do is push on until the timestamp in an indom
		 * record stops us.
		 */

		if (!seen_event && !in_vol_missing) {
		    if (type == PM_TYPE_EVENT || type == PM_TYPE_HIGHRES_EVENT)
			seen_event = 1;
		    else {
			/*
			 * if pmid not in next pmResult, we're done ...
			 */
			for (i = 0; i < inarch.rp->numpmid; i++) {
			    if (pmid == inarch.rp->vset[i]->pmid)
				break;
			}
			if (i == inarch.rp->numpmid)
			    break;
		    }
		}

		/*
		 * rewrite if needed, delete if needed else output
		 */
		do_desc();
	    }
	    else if (stsmeta == TYPE_INDOM || stsmeta == TYPE_INDOM_DELTA) {
		__pmTimestamp	stamp;
		__pmLoadTimestamp((__int32_t *)&inarch.metarec[2], &stamp);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    __pmPutTimestamp(&stamp, (__int32_t *)&inarch.metarec[2]);
		}
		/* if time of indom > next pmResult stop processing metadata */
		if (stamp.sec > inarch.rp->timestamp.sec)
		    break;
		if (stamp.sec == inarch.rp->timestamp.sec &&
		    stamp.nsec > inarch.rp->timestamp.nsec)
		    break;
		needti = 1;
		do_indom(stsmeta);
	    }
	    else if (stsmeta == TYPE_INDOM_V2) {
		__pmTimestamp	stamp;
		__pmLoadTimeval((__int32_t *)&inarch.metarec[2], &stamp);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    __pmPutTimeval(&stamp, (__int32_t *)&inarch.metarec[2]);
		}
		/* if time of indom > next pmResult stop processing metadata */
		if (stamp.sec > inarch.rp->timestamp.sec)
		    break;
		if (stamp.sec == inarch.rp->timestamp.sec &&
		    stamp.nsec > inarch.rp->timestamp.nsec)
		    break;
		needti = 1;
		do_indom(stsmeta);
	    }
	    else if (stsmeta == TYPE_LABEL) {
		__pmTimestamp	stamp;
		__pmLoadTimestamp((__int32_t *)&inarch.metarec[2], &stamp);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    __pmPutTimestamp(&stamp, (__int32_t *)&inarch.metarec[2]);
		}
		/* if time of label set  > next pmResult stop processing metadata */
		if (stamp.sec > inarch.rp->timestamp.sec)
		    break;
		if (stamp.sec == inarch.rp->timestamp.sec &&
		    stamp.nsec > inarch.rp->timestamp.nsec)
		    break;
		needti = 1;
		do_labelset();
	    }
	    else if (stsmeta == TYPE_LABEL_V2) {
		__pmTimestamp	stamp;
		__pmLoadTimeval((__int32_t *)&inarch.metarec[2], &stamp);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    __pmPutTimeval(&stamp, (__int32_t *)&inarch.metarec[2]);
		}
		/* if time of label set  > next pmResult stop processing metadata */
		if (stamp.sec > inarch.rp->timestamp.sec)
		    break;
		if (stamp.sec == inarch.rp->timestamp.sec &&
		    stamp.nsec > inarch.rp->timestamp.nsec)
		    break;
		needti = 1;
		do_labelset();
	    }
	    else if (stsmeta == TYPE_TEXT) {
		needti = 1;
		do_text();
	    }
	    else {
		fprintf(stderr, "%s: Error: unrecognised metadata type: %d\n",
		    pmGetProgname(), stsmeta);
		abandon();
		/*NOTREACHED*/
	    }
	    free(inarch.metarec);
	    stsmeta = 0;
	}

	if (first_datarec) {
	    first_datarec = 0;
	    /* any global time adjustment done after nextlog() above */
	    outarch.logctl.label.start = inarch.rp->timestamp;
	    /* need to fix start-time in label records */
	    writelabel(1);
	    needti = 1;
	}

	tstamp = inarch.rp->timestamp;

	if (needti) {
	    __pmFflush(outarch.logctl.mdfp);
	    __pmFflush(outarch.archctl.ac_mfp);
	    new_meta_offset = __pmFtell(outarch.logctl.mdfp);
	    assert(new_meta_offset >= 0);
            __pmFseek(outarch.logctl.mdfp, (long)old_meta_offset, SEEK_SET);
            __pmLogPutIndex(&outarch.archctl, &tstamp);
            __pmFseek(outarch.logctl.mdfp, (long)new_meta_offset, SEEK_SET);
	    needti = 0;
	    doneti = 1;
        }
	else
	    doneti = 0;

	old_log_offset = __pmFtell(outarch.archctl.ac_mfp);
	assert(old_log_offset >= 0);

	if (inarch.rp->numpmid == 0)
	    /* mark record, need index entry @ next log record */
	    needti = 1;

	do_result();
    }

    if (!doneti) {
	/* Final temporal index entry */
	__pmFflush(outarch.archctl.ac_mfp);
	__pmFseek(outarch.archctl.ac_mfp, (long)old_log_offset, SEEK_SET);
	__pmLogPutIndex(&outarch.archctl, &tstamp);
    }

    if (iflag) {
	/*
	 * __pmFsync() to make sure new archive is safe before we start
	 * renaming ...
	 */
	if (__pmFsync(outarch.logctl.mdfp) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output metadata file: %s\n",
		pmGetProgname(), __pmFileno(outarch.logctl.mdfp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (__pmFsync(outarch.archctl.ac_mfp) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output data file: %s\n",
		pmGetProgname(), __pmFileno(outarch.archctl.ac_mfp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (__pmFsync(outarch.logctl.tifp) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output index file: %s\n",
		pmGetProgname(), __pmFileno(outarch.logctl.tifp), strerror(errno));
		abandon();
		/*NOTREACHED*/
	}
	if (fsync(dir_fd) < 0) {
	    fprintf(stderr, "%s: Error: fsync(%d) failed for output directory: %s\n",
		pmGetProgname(), dir_fd, strerror(errno));
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
	_pmLogRemove(bak_base, -1);
    }

    if (pmDebugOptions.pdubuf) {
	/* dump record buffer state ... looking for mem leaks here */
	(void)__pmFindPDUBuf(-1);
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

	_pmLogRemove(outarch.name, -1);
	if (iflag)
	    _pmLogRename(bak_base, inarch.name);
	while (outarch.archctl.ac_curvol >= 0) {
	    pmsprintf(path, sizeof(path), "%s.%d", outarch.name, outarch.archctl.ac_curvol);
	    unlink(path);
	    outarch.archctl.ac_curvol--;
	}
	pmsprintf(path, sizeof(path), "%s.meta", outarch.name);
	unlink(path);
	pmsprintf(path, sizeof(path), "%s.index", outarch.name);
	unlink(path);
    }
    else
	fprintf(stderr, "Archive \"%s\" creation truncated.\n", outarch.name);

    exit(1);
    /*NOTREACHED*/
}
