/*
 * Copyright (c) 2021 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/libpcp.h>
#include "ss_stats.h"

#define SS_OPTIONS "-noemitauO"

char *ss_filter;	/* storable: network.persocket.filter */
static int using_pipe;	/* pipe is normal operation, QA uses files */

FILE *
ss_open_stream()
{
    FILE *fp = NULL;
    char *path;

    if (ss_filter == NULL) {
	/* pmstore to network.persocket.filter frees this if changing */
    	if ((ss_filter = strdup("")) == NULL)
	    return NULL;
    }

    if ((path = getenv("PCPQA_PMDA_SOCKETS")) != NULL) {
	/* PCPQA input file */
    	fp = fopen(path, "r");
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "ss_open_stream: open PCPQA_PMDA_SOCKETS=%s\n", path);
	using_pipe = 0;
    } else {
	__pmExecCtl_t	*argp = NULL;
	int		sts;

	if (access((path = "/usr/sbin/ss"), X_OK) != 0) {
	    if (access((path = "/usr/bin/ss"), X_OK) != 0) {
	    	fprintf(stderr, "Error: no \"ss\" binary found\n");
		return NULL;
	    }
	}
	if ((sts = __pmProcessAddArg(&argp, path)) < 0 ||
	    (sts = __pmProcessAddArg(&argp, SS_OPTIONS)) < 0) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "ss_open_stream: __pmProcessAddArg failed: %s\n",
		    pmErrStr(sts));
	    return NULL;
	}
	if (ss_filter[0] != '\0') {
	    char	*s, *tok, *saveptr;

	    if ((s = strdup(ss_filter)) == NULL)
		return NULL;
	    for (tok = strtok_r(s, " \t", &saveptr); tok != NULL;
		 tok = strtok_r(NULL, " \t", &saveptr)) {
		if ((sts = __pmProcessAddArg(&argp, tok)) < 0) {
		    free(s);
		    if (pmDebugOptions.appl0)
			fprintf(stderr, "ss_open_stream: __pmProcessAddArg failed: %s\n",
			    pmErrStr(sts));
		    return NULL;
		}
	    }
	    free(s);
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &fp)) < 0) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "ss_open_stream: __pmProcessPipe failed: %s\n",
		    pmErrStr(sts));
	    return NULL;
	}
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "ss_open_stream: exec %s %s %s\n",
		path, SS_OPTIONS, ss_filter);
	using_pipe = 1;
    }

    return fp;
}

void
ss_close_stream(FILE *fp)
{
    if (using_pipe)
    	__pmProcessPipeClose(fp);
    else
    	fclose(fp);
}
