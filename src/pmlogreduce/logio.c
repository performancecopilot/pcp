/*
 * utils for pmlogreduce
 *
 * Copyright (c) 2017 Red Hat.
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

#include "pmlogreduce.h"

/*
 * construct new external label, and check label records from
 * input archives
 */
void
newlabel(void)
{
    __pmLogLabel	*lp = &logctl.label;

    /* check version number */
    if ((ilabel.magic & 0xff) != PM_LOG_VERS02 &&
        (ilabel.magic & 0xff) != PM_LOG_VERS03) {
	fprintf(stderr,"%s: Error: version number %d (not %d or %d as expected) in archive (%s)\n",
		pmGetProgname(), ilabel.magic & 0xff, PM_LOG_VERS02, PM_LOG_VERS03, iname);
	exit(1);
    }

    /* copy magic number, host and timezone info, use our pid */
    lp->magic = ilabel.magic;
    lp->pid = (int)getpid();
    if (lp->hostname)
	free(lp->hostname);
    lp->hostname = strdup(ilabel.hostname);
    if (lp->timezone)
	free(lp->timezone);
    lp->timezone = strdup(ilabel.timezone);
    if (lp->zoneinfo)
	free(lp->zoneinfo);
    /* TODO: use v3 archive zoneinfo */
    lp->zoneinfo = NULL;
}


/*
 * write label records into all files of the output archive
 */
void
writelabel(void)
{
    logctl.label.vol = 0;
    __pmLogWriteLabel(archctl.ac_mfp, &logctl.label);
    logctl.label.vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.tifp, &logctl.label);
    logctl.label.vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.mdfp, &logctl.label);
}

/*
 *  switch output volumes
 */
void
newvolume(char *base, __pmTimestamp *tsp)
{
    __pmFILE		*newfp;
    int			nextvol = archctl.ac_curvol + 1;

    if ((newfp = __pmLogNewFile(base, nextvol)) != NULL) {
	__pmFclose(archctl.ac_mfp);
	archctl.ac_mfp = newfp;
	logctl.label.vol = archctl.ac_curvol = nextvol;
	__pmLogWriteLabel(archctl.ac_mfp, &logctl.label);
	__pmFflush(archctl.ac_mfp);
	fprintf(stderr, "%s: New log volume %d, at ",
		pmGetProgname(), nextvol);
	__pmPrintTimestamp(stderr, tsp);
	fputc('\n', stderr);
	return;
    }
    else {
	fprintf(stderr, "%s: Error: volume %d: %s\n",
		pmGetProgname(), nextvol, pmErrStr(-oserror()));
	exit(1);
    }
}
