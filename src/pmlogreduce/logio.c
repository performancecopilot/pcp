/*
 * utils for pmlogextract
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

int
_pmLogPut(__pmFILE *f, __pmPDU *pb)
{
    int		rlen = ntohl(pb[0]);
    int		sts;

    if (pmDebugOptions.log) {
	fprintf(stderr, "_pmLogPut: fd=%d rlen=%d\n",
	    __pmFileno(f), rlen);
    }

    if ((sts = (int)__pmFwrite(pb, 1, rlen, f)) != rlen) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "_pmLogPut: fwrite=%d %s\n", sts, osstrerror());
	return -oserror();
    }
    return 0;
}

/*
 * construct new external label, and check label records from
 * input archives
 */
void
newlabel(void)
{
    __pmLogLabel	*lp = &logctl.l_label;

    /* check version number */
#ifdef __PCP_EXPERIMENTAL_ARCHIVE_VERSION3
    if ((ilabel.ll_magic & 0xff) != PM_LOG_VERS02 &&
        (ilabel.ll_magic & 0xff) != PM_LOG_VERS03) {
	fprintf(stderr,"%s: Error: version number %d (not %d or %d as expected) in archive (%s)\n",
		pmGetProgname(), ilabel.ll_magic & 0xff, PM_LOG_VERS02, PM_LOG_VERS03, iname);
	exit(1);
    }
#else
    if ((ilabel.ll_magic & 0xff) != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: version number %d (not %d as expected) in archive (%s)\n",
		pmGetProgname(), ilabel.ll_magic & 0xff, PM_LOG_VERS02, iname);
	exit(1);
    }
#endif

    /* copy magic number, host and timezone info, use our pid */
    lp->magic = ilabel.ll_magic;
    lp->pid = (int)getpid();
    if (lp->hostname)
	free(lp->hostname);
    lp->hostname = strdup(ilabel.ll_hostname);
    if (lp->timezone)
	free(lp->timezone);
    lp->timezone = strdup(ilabel.ll_tz);
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
    logctl.l_label.vol = 0;
    __pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
    logctl.l_label.vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);
    logctl.l_label.vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
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
	logctl.l_label.vol = archctl.ac_curvol = nextvol;
	__pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
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
