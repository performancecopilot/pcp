/*
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
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
#include <sys/stat.h>
#include "logger.h"

/*
 * Simple FSA lexical scanner for a pmlogger config file _after_ it has
 * been processed by pmcpp(1)
 *
 * State	Char		State
 * INIT		# 		INIT_EOL
 * INIT		{		METRICLIST
 * INIT		?		INIT
 * INIT_EOL	\n		INIT
 * INIT_EOL	?		INIT_EOL
 * METRICLIST	#		MLIST_EOL
 * METRICLIST	}		INIT
 * METRICLIST	[ \t]		METRICLIST
 * METRICLIST	alpha		NAME
 * METRICLIST	[		INST
 * MLIST_EOL	\n		METRICLIST
 * MLIST_EOL	?		MLIST_EOL
 * NAME		alpha|digit|_|.	NAME
 * NAME		[		INST
 * NAME		?		METRICLIST
 * INST		]		METRICLIST
 * INST		?		INST
 * INIT		EOF		DONE
 */

#define S_INIT	0
#define S_INIT_EOL	1
#define S_METRICLIST	2
#define S_MLIST_EOL	3
#define S_NAME		4
#define S_INST		5

/*
 * If anything fatal happens, we do not return!
 */
FILE *
pass0(FILE *fpipe)
{
    int		state = S_INIT;
    int		c;
    int		linenum = 1;
    char	name[1024];
#if HAVE_MKSTEMP
    char	tmp[MAXPATHLEN];
#endif
    char	*tmpfname;
    char	*p;
    int		fd;
    FILE	*fp;			/* temp file, ready for next pass */

#if HAVE_MKSTEMP
    pmsprintf(tmp, sizeof(tmp), "%s%cpmlogger_configXXXXXX", pmGetConfig("PCP_TMPFILE_DIR"), pmPathSeparator());
    fd = mkstemp(tmp);
    tmpfname = tmp;
#else
    if ((tmpfname = tmpnam(NULL)) != NULL)
	fd = open(tmpfname, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
    if (fd >= 0)
	fp = fdopen(fd, "w+");
    if (fd < 0 || fp == NULL) {
	fprintf(stderr, "\nError: failed create temporary config file (%s)\n", tmpfname);
	fprintf(stderr, "Reason? %s\n", osstrerror());
	(void)unlink(tmpfname);
	exit(1);
    }

    while ((c = fgetc(fpipe)) != EOF) {
	fputc(c, fp);
	if (c == '\n') {
	    linenum++;
	}
	switch (state) {
	    case S_INIT:
	    	if (c == '#')
		    state = S_INIT_EOL;
		else if (c == '{')
		    state = S_METRICLIST;
		break;
	    case S_INIT_EOL:
	    	if (c == '\n')
		    state = S_INIT;
		break;
	    case S_METRICLIST:
	    	if (c == '#')
		    state = S_MLIST_EOL;
		else if (c == '}')
		    state = S_INIT;
		else if (isalpha(c)) {
		    state = S_NAME;
		    p = name;
		    *p++ = c;
		}
		else if (c == '[')
		    state = S_INST;
		break;
	    case S_MLIST_EOL:
	    	if (c == '\n')
		    state = S_METRICLIST;
		break;
	    case S_NAME:
		if (isalpha(c) || isdigit(c) || c == '_' || c == '.')
		    *p++ = c;
		else {
		    *p = '\0';
		    if (pmDebugOptions.appl6)
			fprintf(stderr, "pass0:%d name=\"%s\"\n", linenum, name);
		    if (c == '[')
			state = S_INST;
		    else
			state = S_METRICLIST;
		}
		break;
	    case S_INST:
		if (c == ']')
		    state = S_METRICLIST;
		break;
	    default:
		fprintf(stderr, "pass0: botch state=%d at line %d\n", state, linenum);
		exit(1);
	}
    }

    fflush(fp);
    rewind(fp);
    (void)unlink(tmpfname);

    return fp;
}
