/*
 * Mailq PMDA
 *
 * Copyright (c) 2012,2014 Red Hat.
 * Copyright (c) 1997-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmda.h"
#include "domain.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <sys/stat.h>

/*
 * histogram for binning messages based on queue time
 */
typedef struct {
    long	count;		/* number in this bin */
    time_t	delay;		/* in queue for at least this long (seconds) */
} histo_t;

static histo_t	*histo;
static int	numhisto;
static int	queue;

/*
 * list of instances - indexes must match histo[] above
 */
static pmdaInstid *_delay;

static char	*queuedir = "/var/spool/mqueue";
static char	startdir[MAXPATHLEN];
static char	*username;

static char	*regexstring;
static regex_t	mq_regex;

/*
 * list of instance domains
 */
static pmdaIndom indomtab[] = {
#define DELAY_INDOM	0
    { DELAY_INDOM, 0, NULL },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric metrictab[] = {
/* length */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* deferred */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_U32, DELAY_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

static int
mailq_histogram(char *option)
{
    struct timeval	tv;
    char		*errmsg;
    char		*q;
    int			sts;

    q = strtok(option, ",");
    while (q != NULL) {
	if ((sts = pmParseInterval((const char *)q, &tv, &errmsg)) < 0) {
	    pmprintf("%s: bad historgram bins argument:\n%s\n", pmProgname, errmsg);
	    free(errmsg);
	    return -EINVAL;
	}
	numhisto++;
	histo = (histo_t *)realloc(histo, numhisto * sizeof(histo[0]));
	if (histo == NULL)
	    __pmNoMem("histo", numhisto * sizeof(histo[0]), PM_FATAL_ERR);
	histo[numhisto-1].delay = tv.tv_sec;
	q = strtok(NULL, ",");
    }
    return 0;
}

static int
compare_delay(const void *a, const void *b)
{
    histo_t	*ha = (histo_t *)a;
    histo_t	*hb = (histo_t *)b;

    return hb->delay - ha->delay;
}

/*
 * callback provided to pmdaFetch
 */
static int
mailq_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int		b;

    if (idp->cluster == 0) {
	if (idp->item == 0) {			/* mailq.length */
	    if (inst == PM_IN_NULL)
		atom->ul = queue;
	    else
		return PM_ERR_INST;
	}
	else if (idp->item == 1) {		/* mailq.deferred */
	    /* inst is unsigned, so always >= 0 */
	    for (b = 0; b < numhisto; b++) {
		if (histo[b].delay == inst) break;
	    }
	    if (b < numhisto)
	    	atom->ul = histo[b].count;
	    else
		return PM_ERR_INST;
	}
	else
	    return PM_ERR_PMID;
    }
    else
	return PM_ERR_PMID;

    return 0;
}

/*
 * wrapper for pmdaFetch which refreshes the metrics
 */
static int
mailq_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    static int		warn = 0;
    int			num;
    int			i;
    int			b;
    struct stat		sbuf;
    time_t		now;
    static time_t	last_refresh = 0;
    time_t		waiting;
    char		*p;
    struct dirent 	**list;

    time(&now);

    /* clip refresh rate to at most once per 30 seconds */
    if (now - last_refresh > 30) {
	last_refresh = now;

	queue = 0;
	for (b = 0; b < numhisto; b++)
	    histo[b].count = 0;

	if (chdir(queuedir) < 0) {
	    if (warn == 0) {
		__pmNotifyErr(LOG_ERR, "chdir(\"%s\") failed: %s\n",
		    queuedir, osstrerror());
		warn = 1;
	    }
	}
	else {
	    if (warn == 1) {
		__pmNotifyErr(LOG_INFO, "chdir(\"%s\") success\n", queuedir);
		warn = 0;
	    }

	    num = scandir(".", &list, NULL, NULL);

	    for (i = 0; i < num; i++) {
		p = list[i]->d_name;
		/* only file names that match the regular expression */
		if (regexstring && regexec(&mq_regex, list[i]->d_name, 0, NULL, 0))
		    continue;
		else if (!regexstring && (*p != 'd' || *(p+1) != 'f'))
		    continue;
		if (stat(p, &sbuf) != 0) {
		    /*
		     * ENOENT expected sometimes if sendmail is doing its job
		     */
		    if (oserror() == ENOENT)
			continue;
		    fprintf(stderr, "stat(\"%s\"): %s\n", p, osstrerror());
		    continue;
		}
		if (sbuf.st_size > 0 && S_ISREG(sbuf.st_mode)) {
		    /* really in the queue */
#if defined(HAVE_ST_MTIME_WITH_E)
		    waiting = now - sbuf.st_mtime;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
		    waiting = now - sbuf.st_mtimespec.tv_sec;
#else
		    waiting = now - sbuf.st_mtim.tv_sec;
#endif
		    for (b = 0; b < numhisto; b++) {
			if (waiting >= histo[b].delay) {
			    histo[b].count++;
			    break;
			}
		    }
		    queue++;
		}
	    }
	    for (i = 0; i < num; i++)
		free(list[i]);
	    if (num > 0)
		free(list);
	}
	if (chdir(startdir) < 0) {
	    __pmNotifyErr(LOG_ERR, "chdir(\"%s\") failed: %s\n",
			startdir, osstrerror());
	}
    }

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * Initialise the agent (daemon only).
 */
void 
mailq_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    __pmSetProcessIdentity(username);
    dp->version.two.fetch = mailq_fetch;
    pmdaSetFetchCallBack(dp, mailq_fetchCallBack);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
}

pmdaInterface	dispatch;

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "binlist", 1, 'b', "TIMES", "comma-separated histogram bins times" },
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "regex", 1, 'r', "RE", "regular expression for matching mail file names" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "b:D:d:l:r:U:?",
    .long_options = longopts,
    .short_usage = "[options] [queuedir]",
};

/*
 * Set up the agent, running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    int			c;
    int			i;
    char		namebuf[30];
    char		mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    if (getcwd(startdir, sizeof(startdir)) == NULL) {
	fprintf(stderr, "%s: getcwd() failed: %s\n",
	    pmProgname, pmErrStr(-oserror()));
	exit(1);
    }

    pmsprintf(mypath, sizeof(mypath), "%s%c" "mailq" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, MAILQ,
		"mailq.log", mypath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch (c) {
	case 'b':
	    if (mailq_histogram(opts.optarg) < 0)
		opts.errors++;
	    break;

	case 'r':
	    regexstring = opts.optarg;
	    c = regcomp(&mq_regex, regexstring, REG_EXTENDED | REG_NOSUB);
	    if (c != 0) {
		regerror(c, &mq_regex, mypath, sizeof(mypath));
		pmprintf("%s: cannot compile regular expression: %s\n",
			pmProgname, mypath);
		opts.errors++;
	    }
	    break;
	}
    }

    if (opts.optind == argc - 1)
	queuedir = argv[opts.optind];
    else if (opts.optind != argc)
	opts.errors++;

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    if (opts.username)
	username = opts.username;

    if (histo == NULL) {
	/* default histo bins, if not already done above ... */
	numhisto = 7;
	histo = (histo_t *)malloc(numhisto * sizeof(histo[0]));
	if (histo == NULL) {
	     __pmNoMem("histo", numhisto * sizeof(histo[0]), PM_FATAL_ERR);
	}
	histo[0].delay = 7 * 24 * 3600;
	histo[1].delay = 3 * 24 * 3600;
	histo[2].delay = 24 * 3600;
	histo[3].delay = 8 * 3600;
	histo[4].delay = 4 * 3600;
	histo[5].delay = 1 * 3600;
	histo[6].delay = 0;
    }
    else {
	/* need to add last one and sort on descending time */
	numhisto++;
	histo = (histo_t *)realloc(histo, numhisto * sizeof(histo[0]));
	if (histo == NULL) {
	     __pmNoMem("histo", numhisto * sizeof(histo[0]), PM_FATAL_ERR);
	}
	histo[numhisto-1].delay = 0;
	qsort(histo, numhisto, sizeof(histo[0]), compare_delay);
    }

    _delay = (pmdaInstid *)malloc(numhisto * sizeof(_delay[0]));
    if (_delay == NULL)
	__pmNoMem("_delay", numhisto * sizeof(_delay[0]), PM_FATAL_ERR);

    for (i = 0; i < numhisto; i++) {
	time_t	tmp;
	_delay[i].i_inst = histo[i].delay;
	histo[i].count = 0;
	if (histo[i].delay == 0)
	    pmsprintf(namebuf, sizeof(namebuf), "recent");
	else if (histo[i].delay < 60)
	    pmsprintf(namebuf, sizeof(namebuf), "%d-secs", (int)histo[i].delay);
	else if (histo[i].delay < 60 * 60) {
	    tmp = histo[i].delay / 60;
	    if (tmp <= 1)
		pmsprintf(namebuf, sizeof(namebuf), "1-min");
	    else
		pmsprintf(namebuf, sizeof(namebuf), "%d-mins", (int)tmp);
	}
	else if (histo[i].delay < 24 * 60 * 60) {
	    tmp = histo[i].delay / (60 * 60);
	    if (tmp <= 1)
		pmsprintf(namebuf, sizeof(namebuf), "1-hour");
	    else
		pmsprintf(namebuf, sizeof(namebuf), "%d-hours", (int)tmp);
	}
	else {
	    tmp = histo[i].delay / (24 * 60 * 60);
	    if (tmp <= 1)
		pmsprintf(namebuf, sizeof(namebuf), "1-day");
	    else
		pmsprintf(namebuf, sizeof(namebuf), "%d-days", (int)tmp);
	}
	_delay[i].i_name = strdup(namebuf);
	if (_delay[i].i_name == NULL) {
	     __pmNoMem("_delay[i].i_name", strlen(namebuf), PM_FATAL_ERR);
	}
    }

    indomtab[DELAY_INDOM].it_numinst = numhisto;
    indomtab[DELAY_INDOM].it_set = _delay;

    pmdaOpenLog(&dispatch);
    mailq_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
