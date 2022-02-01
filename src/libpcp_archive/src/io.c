/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2014-2018,2021 Red Hat.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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
#include "libpcp.h"
#include "archive.h"
#include <assert.h>

/*
 * raw read of next archive record ...
 * largely stolen from __pmLogRead in libpcp
 *
 * record buffer is malloc'd here and returned via rbuf
 *
 * return 0 for success, PM_ERR_EOL for end of file, else some
 * other error code
 */
int
pmaLogGet(__pmArchCtl *acp, int vol, __int32_t **rbuf)
{
    __pmLogCtl	*lcp = acp->ac_log;
    int		head;
    int		tail;
    int		sts;
    int		type;
    long	offset;
    char	*p;
    __int32_t	*lbuf;		/* build record buffer here */
    __pmFILE	*f;

    if (vol == PM_LOG_VOL_META)
	f = lcp->mdfp;
    else
	f = acp->ac_mfp;

    offset = __pmFtell(f);
    assert(offset >= 0);
    if (pmDebugOptions.log) {
	fprintf(stderr, "pmaLogGet: fd=%d vol=%d posn=%ld ",
	    __pmFileno(f), vol, offset);
    }

again:
    sts = (int)__pmFread(&head, 1, sizeof(head), f);
    if (sts != sizeof(head)) {
	if (sts == 0) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "AFTER end\n");
	    __pmFseek(f, offset, SEEK_SET);
	    if (vol != PM_LOG_VOL_META) {
		if (acp->ac_curvol < lcp->maxvol) {
		    if (__pmLogChangeVol(acp, acp->ac_curvol+1) == 0) {
			f = acp->ac_mfp;
			goto again;
		    }
		}
	    }
	    return PM_ERR_EOL;
	}
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: hdr fread=%d %s\n", sts, osstrerror());
	if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -oserror();
    }

    if ((lbuf = (__int32_t *)malloc(ntohl(head))) == NULL) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: pmaLogGet:(%d) %s\n",
		(int)ntohl(head), osstrerror());
	__pmFseek(f, offset, SEEK_SET);
	return -oserror();
    }

    lbuf[0] = head;
    if ((sts = (int)__pmFread(&lbuf[1], 1, ntohl(head) - sizeof(head), f)) != ntohl(head) - sizeof(head)) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: data fread=%d %s\n", sts, osstrerror());
	free(lbuf);
	if (sts == 0) {
	    __pmFseek(f, offset, SEEK_SET);
	    return PM_ERR_EOL;
	}
	else if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -oserror();
    }


    p = (char *)lbuf;
    memcpy(&tail, &p[ntohl(head) - sizeof(head)], sizeof(head));
    if (head != tail) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: head-tail mismatch (%d-%d)\n",
		(int)ntohl(head), (int)ntohl(tail));
	free(lbuf);
	return PM_ERR_LOGREC;
    }

    type = ntohl(lbuf[1]);

    if (pmDebugOptions.log) {
	if (vol != PM_LOG_VOL_META ||
	    type == TYPE_INDOM || type ==TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
	    fprintf(stderr, "@");
	    if (sts >= 0) {
		__pmTimestamp	stamp;
		if (vol != PM_LOG_VOL_META)
		    __pmLoadTimeval(&lbuf[2], &stamp);
		else if (type == TYPE_INDOM || type ==TYPE_INDOM_DELTA)
		    __pmLoadTimestamp(&lbuf[1], &stamp);
		else if (type == TYPE_INDOM_V2)
		    __pmLoadTimeval(&lbuf[1], &stamp);
		__pmPrintTimestamp(stderr, &stamp);
	    }
	    else
		fprintf(stderr, "unknown time");
	}
	fprintf(stderr, " len=%d (incl head+tail)\n", (int)ntohl(head));
    }

    if (pmDebugOptions.pdu) {
	int		i, j;
	__pmTimestamp	stamp;
	fprintf(stderr, "pmaLogGet");
	if (vol != PM_LOG_VOL_META ||
	    type == TYPE_INDOM || type ==TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
	    if (vol != PM_LOG_VOL_META)
		__pmLoadTimeval(&lbuf[2], &stamp);
	    else if (type == TYPE_INDOM || type ==TYPE_INDOM_DELTA)
		__pmLoadTimestamp(&lbuf[1], &stamp);
	    else if (type == TYPE_INDOM_V2)
		__pmLoadTimeval(&lbuf[1], &stamp);
	    fprintf(stderr, " timestamp=");
	    __pmPrintTimestamp(stderr, &stamp);
	}
	fprintf(stderr, " " PRINTF_P_PFX "%p ... " PRINTF_P_PFX "%p", lbuf, &lbuf[ntohl(head)/sizeof(__int32_t) - 1]);
	fputc('\n', stderr);
	fprintf(stderr, "%03d: ", 0);
	for (j = 0, i = 0; j < ntohl(head)/sizeof(__int32_t); j++) {
	    if (i == 8) {
		fprintf(stderr, "\n%03d: ", j);
		i = 0;
	    }
	    fprintf(stderr, "0x%x ", lbuf[j]);
	    i++;
	}
	fputc('\n', stderr);
    }

    *rbuf = lbuf;
    return 0;
}

/*
 * raw write of next record to an archive file
 *
 * returns 0 on success, else error code
 */
int
pmaLogPut(__pmFILE *f, __int32_t *rbuf)
{
    int		rlen = ntohl(rbuf[0]);
    int		sts;

    if (pmDebugOptions.log) {
	fprintf(stderr, "pmaLogPut: fd=%d rlen=%d\n",
	    __pmFileno(f), rlen);
    }

    if ((sts = (int)__pmFwrite(rbuf, 1, rlen, f)) != rlen) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "pmaLogPut: fwrite=%d %s\n", sts, osstrerror());
	return -oserror();
    }
    return 0;
}
