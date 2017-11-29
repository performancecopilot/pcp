/*
 * Sendmail PMDA
 *
 * Copyright (c) 1995-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"
#include <sys/stat.h>

/*
 * Sendmail PMDA
 *
 * This PMDA uses the statistics file that sendmail optionally maintains
 * -- see "OS<file>" or "O StatusFile=<file>" in sendmail.cf and sendmail(1)
 *
 * This file (defaults to /var/sendmail.st) must be created before sendmail
 * will update any statistics.
 */

/*
 * list of instances
 */

static pmdaIndom indomtab[] = {
#define MAILER_INDOM	0
    { MAILER_INDOM, 0, NULL },
};

static char		*statsfile = "/var/sendmail.st";
static char		*username;
static int		nmailer;
static void		*ptr;
static struct stat	laststatbuf;
static time_t		*start_date;
static __uint32_t	*msgs_from;
static __uint32_t	*kbytes_from;
static __uint32_t	*msgs_to;
static __uint32_t	*kbytes_to;

static pmdaMetric metrictab[] = {
/* start_date */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* permailer.msgs_from */
    { NULL, 
      { PMDA_PMID(1,0), PM_TYPE_U32, MAILER_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* permailer.bytes_from */
    { NULL, 
      { PMDA_PMID(1,1), PM_TYPE_U32, MAILER_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* permailer.msgs_to */
    { NULL, 
      { PMDA_PMID(1,2), PM_TYPE_U32, MAILER_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* permailer.bytes_to */
    { NULL, 
      { PMDA_PMID(1,3), PM_TYPE_U32, MAILER_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* total.msgs_from */
    { NULL, 
      { PMDA_PMID(2,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* total.bytes_from */
    { NULL, 
      { PMDA_PMID(2,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* total.msgs_to */
    { NULL, 
      { PMDA_PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* total.bytes_to */
    { NULL, 
      { PMDA_PMID(2,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
};

static void
map_stats(void)
{
    struct stat		statbuf;
    static int		fd;

    static int  	notified = 0;
#define MAPSTATS_NULL	    	0x01
#define MAPSTATS_NOTV2STRUCT 	0x02
#define MAPSTATS_MAPFAIL 	0x04

    /* From mailstats.h in sendmail(1) 8.x... */
    struct smstat_s
    {
#define MAXMAILERS  	    	25
#define STAT_VERSION	    	2
#define STAT_MAGIC	    	0x1B1DE
	int	stat_magic;		/* magic number */
	int	stat_version;		/* stat file version */
	time_t	stat_itime;		/* file initialization time */
	short	stat_size;		/* size of this structure */
	long	stat_nf[MAXMAILERS];	/* # msgs from each mailer */
	long	stat_bf[MAXMAILERS];	/* kbytes from each mailer */
	long	stat_nt[MAXMAILERS];	/* # msgs to each mailer */
	long	stat_bt[MAXMAILERS];	/* kbytes to each mailer */
	long	stat_nr[MAXMAILERS];	/* # rejects by each mailer */
	long	stat_nd[MAXMAILERS];	/* # discards by each mailer */
    } *smstat;
    

    if (pmDebugOptions.appl0) {
    	fprintf(stderr, "%s: map_stats: Entering:\n", pmGetProgname());
    	fprintf(stderr, "%s: map_stats:   Check: ptr       = " PRINTF_P_PFX "%p\n", pmGetProgname(), ptr);
    	fprintf(stderr, "%s: map_stats:   Check: statsfile = " PRINTF_P_PFX "%p\n", pmGetProgname(), statsfile);
    	if (statsfile != NULL)
    	    fprintf(stderr, "%s: map_stats:                    = %s\n", pmGetProgname(), statsfile);
    }

    if (statsfile == NULL || stat(statsfile, &statbuf) < 0) {
	/* if sendmail not collecting stats this is expected */
	if (ptr != NULL) {
	    /* must have gone away */
	    __pmMemoryUnmap(ptr, laststatbuf.st_size);
	    close(fd);
	    ptr = NULL;
	    notified &= ~MAPSTATS_NOTV2STRUCT;
	    if (pmDebugOptions.appl0) {
	    	fprintf(stderr, "%s: map_stats: (Maybe) stat() < 0; pmunmap() called\n", pmGetProgname());
	    }
	}
	return;
    }

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "%s: map_stats: Check: statbuf.st_ino =     %lu\n", pmGetProgname(), (unsigned long)statbuf.st_ino);
	fprintf(stderr, "%s: map_stats: Check: statbuf.st_dev =     %lu\n", pmGetProgname(), (unsigned long)statbuf.st_dev);
	fprintf(stderr, "%s: map_stats: Check: laststatbuf.st_ino = %lu\n", pmGetProgname(), (unsigned long)laststatbuf.st_ino);
	fprintf(stderr, "%s: map_stats: Check: laststatbuf.st_dev = %lu\n", pmGetProgname(), (unsigned long)laststatbuf.st_dev);
    }
    if (statbuf.st_ino != laststatbuf.st_ino ||
	statbuf.st_dev != laststatbuf.st_dev ||
	ptr == NULL) {
	/*
	 *  Not the same as the file we saw last time, or statsfile is
	 *  not mapped into memory (because it was zero length).
	 *
	 *  The file can change due to rotation or restarting sendmail...
	 *  note the times (st_atim, st_mtim and st_ctim) are all expected
	 *  to change as sendmail updates the file, hence we must use dev
	 *  and ino.
	 *
	 *  ino is guaranteed to change for different instances of the
	 *  sendmail stats file, since a mmap()'d file is never closed
	 *  until after it's munmap()'d.
	 */

	if (ptr != NULL) {
	    __pmMemoryUnmap(ptr, laststatbuf.st_size);
	    close(fd);
	    ptr = NULL;
	    notified &= ~MAPSTATS_NOTV2STRUCT;
	    if (pmDebugOptions.appl0) {
	    	fprintf(stderr, "%s: map_stats: statbuf.st_[dev|ido] changed; pmunmap() called\n", pmGetProgname());
	    }
	}

	if ((fd = open(statsfile, O_RDONLY)) < 0) {
	    pmNotifyErr(LOG_WARNING, "%s: map_stats: cannot open(\"%s\",...): %s",
			pmGetProgname(), statsfile, osstrerror());
	    return;
	}
	ptr = __pmMemoryMap(fd, statbuf.st_size, 0);
	if (ptr == NULL) {
	    if (!(notified & MAPSTATS_MAPFAIL)) {
		pmNotifyErr(LOG_ERR, "%s: map_stats: memmap of %s failed: %s",
			    pmGetProgname(), statsfile, osstrerror());
    	    }
	    close(fd);
	    ptr = NULL;
	    notified |= MAPSTATS_MAPFAIL;
	    return;
	}

	laststatbuf = statbuf;		/* struct assignment */
	notified &= ~(MAPSTATS_NULL | MAPSTATS_MAPFAIL);
	    if (pmDebugOptions.appl0) {
	    	fprintf(stderr, "%s: map_stats: mmap() called, succeeded\n", pmGetProgname());
	    }

	/*  Check for a statistics file from sendmail(1) 8.x: */
	smstat = (struct smstat_s *)ptr;
	if (smstat->stat_magic != STAT_MAGIC || 
	  smstat->stat_version != STAT_VERSION) {
	    if (! (notified & MAPSTATS_NOTV2STRUCT)) {
	    	pmNotifyErr(LOG_WARNING, "%s: map_stats: cannot find magic number in file %s; assuming version 1 format",
			pmGetProgname(), statsfile);
		if (pmDebugOptions.appl0) {
		    fprintf(stderr, "%s: map_stats: smstat_s contents:\n", pmGetProgname());
		    fprintf(stderr, "%s: map_stats:   Version 2 format:\n", pmGetProgname());
		    fprintf(stderr, "%s: map_stats:     Check: stat_magic =   0x%x\n", pmGetProgname(), smstat->stat_magic);
		    fprintf(stderr, "%s: map_stats:     Check: stat_version = 0x%x\n", pmGetProgname(), smstat->stat_version);
		    fprintf(stderr, "%s: map_stats:     Check: stat_itime =   %s", pmGetProgname(), ctime(&(smstat->stat_itime)));
		    fprintf(stderr, "%s: map_stats:     Check: stat_size =    %d\n", pmGetProgname(), smstat->stat_size);
		    
		    /* We're being difficult here... using smstat_s the wrong way! */
		    fprintf(stderr, "%s: map_stats:   Version 1 format:\n", pmGetProgname());
		    fprintf(stderr, "%s: map_stats:     Check: stat_itime =   %s", pmGetProgname(), ctime((time_t *)&(smstat->stat_magic)));
		    fprintf(stderr, "%s: map_stats:     Check: stat_size =    %d\n", pmGetProgname(), *((short *)&(smstat->stat_version)));
		}
		notified |= MAPSTATS_NOTV2STRUCT;
	    }
	    
	    /* Could be older version of stats file... here is the original code
	       that dealt with that case */
	    /*
	     * format of [older version] sendmail stats file:
	     *   word[0]			time_t file first created
	     *   word[1]			N/A
	     *   word[2] .. word[K+2]	msgs_from mailers 0 .. K
	     *   word[K+3] .. word[2*K+3]	kbytes_from mailers 0 .. K
	     *   word[2*K+3] .. word[3*K+4]	msgs_to mailers 0 .. K
	     *   word[3*K+4] .. word[4*K+5]	kbytes_to mailers 0 .. K
	     */
	    nmailer = (statbuf.st_size - sizeof(__int32_t) - sizeof(__int32_t)) / (4 * sizeof(__uint32_t));
	    msgs_from = &((__uint32_t *)ptr)[2];
	    kbytes_from = &msgs_from[nmailer];
	    msgs_to = &kbytes_from[nmailer];
	    kbytes_to = &msgs_to[nmailer];
	    start_date = (time_t *)ptr;
	}
	else {
	    /* Assign pointers to point to parts of the v2 struct */
	    nmailer = ((char *)smstat->stat_bf - (char *)smstat->stat_nf) / sizeof(long);
	    msgs_from = (__uint32_t *)&(smstat->stat_nf);
	    kbytes_from =  (__uint32_t *)&(smstat->stat_bf);
	    msgs_to =  (__uint32_t *)&(smstat->stat_nt);
	    kbytes_to =  (__uint32_t *)&(smstat->stat_bt);
	    start_date = &(smstat->stat_itime);
	}
    }
}

/*
 * logic here is similar to that used by mailstats(1)
 */
static void
do_sendmail_cf(void)
{
    FILE	*fp;
    char	buf[MAXPATHLEN+20];
    char	*bp;
    int		i;
    int		lineno = 0;

    if ((fp = fopen("/etc/sendmail.cf", "r")) == NULL) {
	if ((fp = fopen("/etc/mail/sendmail.cf", "r")) == NULL) {
	    /* this is pretty serious! */
	    nmailer = 0;
	    statsfile = NULL;
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "Warning: cannot find sendmail.cf, so no stats!\n");
	    return;
	}
    }

    nmailer = 3;
    indomtab[MAILER_INDOM].it_set = (pmdaInstid *)malloc(nmailer * sizeof(pmdaInstid));
    indomtab[MAILER_INDOM].it_set[0].i_inst = 0;
    indomtab[MAILER_INDOM].it_set[0].i_name = "prog";
    indomtab[MAILER_INDOM].it_set[1].i_inst = 1;
    indomtab[MAILER_INDOM].it_set[1].i_name = "*file*";
    indomtab[MAILER_INDOM].it_set[2].i_inst = 2;
    indomtab[MAILER_INDOM].it_set[2].i_name = "*include*";

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	lineno++;
	bp = buf;

	if (*bp == 'M') {
	    /* mailer definition */
	    bp++;
	    while (*bp != ',' && !isspace((int)*bp) && *bp != '\0')
		bp++;
	    *bp = '\0';
	    for (i = 0; i < nmailer; i++) {
		if (strcmp(&buf[1], indomtab[MAILER_INDOM].it_set[i].i_name) == 0)
		    break;
	    }
	    if (i == nmailer) {
		indomtab[MAILER_INDOM].it_set = (pmdaInstid *)realloc(indomtab[MAILER_INDOM].it_set, (nmailer+1) * sizeof(pmdaInstid));
		indomtab[MAILER_INDOM].it_set[nmailer].i_name = strdup(&buf[1]);
		indomtab[MAILER_INDOM].it_set[nmailer].i_inst = nmailer;
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "sendmail.cf[%d]: mailer \"%s\" inst=%d\n",
			lineno, &buf[1], nmailer);
		nmailer++;
	    }
	}
	else if (*bp == 'O') {
	    char	*tp;

	    if (strncasecmp(++bp, " StatusFile", 11) == 0 &&
		!isalnum((int)bp[11])) {
		bp = strchr(bp, '=');
		if (bp == NULL)
			continue;
		while (isspace((int)*++bp))
			continue;
	    }
	    else if (*bp == 'S')
		bp++;
	    else
		continue;

	    tp = bp++;
	    while (*bp && !isspace((int)*bp) && *bp != '#')
		bp++;
	    *bp = '\0';

	    statsfile = strdup(tp);
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "sendmail.cf[%d]: statsfile \"%s\"\n",
		    lineno, tp);
	}
    }
    fclose(fp);

    indomtab[MAILER_INDOM].it_numinst = nmailer;
}

/*
 * callback provided to pmdaFetch
 */
static int
sendmail_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);

    if (ptr == NULL)
	return 0;

    if (cluster == 0) {
	if (item == 0) {
		/* sendmail.start_date */
		atom->cp = ctime(start_date);
		atom->cp[24] = '\0';	/* no newline */
		return 1;
	}
    }
    else if (cluster == 1) {
	if (inst >= nmailer)
	    return 0;

	if (msgs_from[inst] == 0 && msgs_to[inst] == 0) {
	    return 0;
	}

	switch (item) {
	    case 0:			/* sendmail.permailer.msgs_from */
		atom->ul = msgs_from[inst];
		break;

	    case 1:			/* sendmail.permailer.bytes_from */
		atom->ul = kbytes_from[inst];
		break;

	    case 2:			/* sendmail.permailer.msgs_to */
		atom->ul = msgs_to[inst];
		break;

	    case 3:			/* sendmail.permailer.bytes_to */
		atom->ul = kbytes_to[inst];
		break;

	    default:
		return PM_ERR_PMID;
	}

	return 1;
    }
    else if (cluster == 2) {
	int		i;

	atom->ul = 0;

	switch (item) {
	    case 0:			/* sendmail.total.msgs_from */
		for (i = 0; i < nmailer; i++)
		    atom->ul += msgs_from[i];
		break;

	    case 1:			/* sendmail.total.bytes_from */
		for (i = 0; i < nmailer; i++)
		    atom->ul += kbytes_from[i];
		break;

	    case 2:			/* sendmail.total.msgs_to */
		for (i = 0; i < nmailer; i++)
		    atom->ul += msgs_to[i];
		break;

	    case 3:			/* sendmail.total.bytes_to */
		for (i = 0; i < nmailer; i++)
		    atom->ul += kbytes_to[i];
		break;

	    default:
		return PM_ERR_PMID;
	}

	return 1;
    }

    return PM_ERR_PMID;
}

static int
sendmail_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    map_stats();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * Initialise the agent
 */
void 
__PMDA_INIT_CALL
sendmail_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    if (username)
	pmSetProcessIdentity(username);

    do_sendmail_cf();
    map_stats();

    dp->version.two.fetch = sendmail_fetch;

    pmdaSetFetchCallBack(dp, sendmail_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = pmPathSeparator();
    pmdaInterface	dispatch;
    char		mypath[MAXPATHLEN];

    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "sendmail" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmGetProgname(), SENDMAIL,
		"sendmail.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    sendmail_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
