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

#include "pcp/pmapi.h"
#include "pcp/libpcp.h"
#include "pcp/archive.h"
#include "../libpcp/src/internal.h"
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
pmaGetLog(__pmArchCtl *acp, int vol, __int32_t **rbuf)
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
	fprintf(stderr, "pmaGetLog: fd=%d", __pmFileno(f));
	if (vol == PM_LOG_VOL_TI)
	    fprintf(stderr, " index,");
	else if (vol == PM_LOG_VOL_META) 
	    fprintf(stderr, " meta,");
	else
	    fprintf(stderr, "vol=%d", vol);
	fprintf(stderr, " posn=%ld", offset);
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
	    fprintf(stderr, "Error: pmaGetLog:(%d) %s\n",
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

    if (vol == PM_LOG_VOL_META)
	type = ntohl(lbuf[1]);
    else
	type = -1;

    if (pmDebugOptions.log) {
	if (vol != PM_LOG_VOL_META ||
	    type == TYPE_INDOM || type ==TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
	    if (sts >= 0) {
		__pmTimestamp	stamp = { -1, 0 };
		if (vol != PM_LOG_VOL_META) {
		    if (__pmLogVersion(lcp) >= PM_LOG_VERS03)
			__pmLoadTimestamp(&lbuf[1], &stamp);
		    else
			__pmLoadTimeval(&lbuf[1], &stamp);
		}
		else if (type == TYPE_INDOM || type ==TYPE_INDOM_DELTA) {
		    fprintf(stderr, " %s", __pmLogMetaTypeStr(type));
		    __pmLoadTimestamp(&lbuf[2], &stamp);
		}
		else if (type == TYPE_INDOM_V2) {
		    fprintf(stderr, " %s", __pmLogMetaTypeStr(type));
		    __pmLoadTimeval(&lbuf[2], &stamp);
		}
		if (stamp.sec != -1) {
		    fprintf(stderr, " @");
		    __pmPrintTimestamp(stderr, &stamp);
		}
	    }
	    else
		fprintf(stderr, "unknown time");
	}
	fprintf(stderr, " len=%d (incl head+tail)\n", (int)ntohl(head));
    }

    if (pmDebugOptions.pdu) {
	int		i, j;
	__pmTimestamp	stamp = { -1, 0 };
	fprintf(stderr, "pmaGetLog");
	if (vol != PM_LOG_VOL_META ||
	    type == TYPE_INDOM || type ==TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
	    if (vol != PM_LOG_VOL_META) {
		    if (__pmLogVersion(lcp) >= PM_LOG_VERS03)
			__pmLoadTimestamp(&lbuf[1], &stamp);
		    else
			__pmLoadTimeval(&lbuf[1], &stamp);
	    }
	    else if (type == TYPE_INDOM || type ==TYPE_INDOM_DELTA)
		__pmLoadTimestamp(&lbuf[2], &stamp);
	    else if (type == TYPE_INDOM_V2)
		__pmLoadTimeval(&lbuf[2], &stamp);
	    if (stamp.sec != -1) {
		fprintf(stderr, " timestamp=");
		__pmPrintTimestamp(stderr, &stamp);
	    }
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
pmaPutLog(__pmFILE *f, __int32_t *rbuf)
{
    int		rlen = ntohl(rbuf[0]);
    int		sts;

    if (pmDebugOptions.log) {
	fprintf(stderr, "pmaPutLog: fd=%d rlen=%d\n",
	    __pmFileno(f), rlen);
    }

    if ((sts = (int)__pmFwrite(rbuf, 1, rlen, f)) != rlen) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "pmaPutLog: fwrite=%d %s\n", sts, osstrerror());
	return -oserror();
    }
    return 0;
}

/*
 * rbuf is a physical data record (pmResult) from an archive
 *
 * pmaRewriteData() is used to rewrite (reformat) rbuf from version
 * invers format into the equivalent data record for a version
 * outvers archive.
 */
int
pmaRewriteData(__pmLogCtl *inlcp, __pmLogCtl *outlcp, __int32_t **rbuf)
{
    int		invers = __pmLogVersion(inlcp);
    int		outvers = __pmLogVersion(outlcp);

    if (invers == PM_LOG_VERS02) {
	if (outvers == invers)
	    return 0;	/* no-op */
	if (outvers != PM_LOG_VERS03)
	    return PM_ERR_APPVERSION;	/* only V2 -> V3 makes sense */
    }
    else if (invers == PM_LOG_VERS03) {
	if (outvers == invers)
	    return 0;	/* no-op */
	return PM_ERR_APPVERSION;	/* only V3 -> V3 makes sense */
    }

    return PM_ERR_NYI;
}

/*
 * rbuf is a physical metadata record from an archive
 *
 * pmaRewriteMeta() is used to rewrite (reformat) rbuf from version
 * invers format into the equivalent metadata record for a version
 * outvers archive.
 */
int
pmaRewriteMeta(__pmLogCtl *inlcp, __pmLogCtl *outlcp, __int32_t **rbuf)
{
    int			invers = __pmLogVersion(inlcp);
    int			outvers = __pmLogVersion(outlcp);
    int			rlen;
    int			type;
    int			sts;
    pmInDom		indom;
    __pmLogInDom	lid;
    __int32_t		*ibuf = *rbuf;
    __int32_t		*tmp;
    __int32_t		*new;
    __pmTimestamp	stamp;
    int			labtype;
    int			ident;
    int			nsets;
    pmLabelSet		*labelsetp;
    int			i;

    if (invers == PM_LOG_VERS02) {
	if (outvers == invers)
	    return 0;	/* no-op */
	if (outvers != PM_LOG_VERS03)
	    return PM_ERR_APPVERSION;	/* only V2 -> V3 makes sense */
    }
    else if (invers == PM_LOG_VERS03) {
	if (outvers == invers)
	    return 0;	/* no-op */
	return PM_ERR_APPVERSION;	/* only V3 -> V3 makes sense */
    }

    /*
     * only have to worry about V2 -> v3 translation from here on
     */
    rlen = ntohl(ibuf[0]);
    type = ntohl(ibuf[1]);

    sts = 0;
    switch (type) {
	case TYPE_DESC:
	case TYPE_TEXT:
	    /* nothing to be done */
	    break;

	case TYPE_INDOM_V2:
	    /* committed to rewrite, don't worry about trashing rbuf */
	    indom = __ntohpmInDom(ibuf[5]);
	    tmp = &ibuf[2];
	    if ((sts = __pmLogLoadInDom(NULL, rlen, type, &lid, &tmp)) < 0) {
		fprintf(stderr, "pmaRewriteMeta: Botch: __pmLogLoadInDom for indom %s: %s\n",
		    pmInDomStr(indom), pmErrStr(sts));
		exit(1);
	    }

	    sts = __pmLogEncodeInDom(NULL, TYPE_INDOM, &lid, &new);
	    if (sts < 0) {
		fprintf(stderr, "pmaRewriteMeta: Botch: __pmLogEncodeInDom for indom %s: %s\n",
		    pmInDomStr(indom), pmErrStr(sts));
		exit(1);
	    }
	    __pmFreeLogInDom(&lid);
	    free(*rbuf);
	    *rbuf = new;
	    sts = 1;
	    break;

	case TYPE_LABEL_V2:
	    tmp = &ibuf[2];
	    if ((sts = __pmLogLoadLabelSet((char *)tmp, rlen, TYPE_LABEL_V2, &stamp, &labtype, &ident, &nsets, &labelsetp)) < 0) {
		fprintf(stderr, "pmaRewriteMeta: Botch: __pmLogLoadLabelSets: %s\n",
		    pmErrStr(sts));
		exit(1);
	    }

	    sts = __pmLogEncodeLabels(outlcp, labtype, ident, nsets, labelsetp, &stamp, &new);
	    if (sts < 0) {
		fprintf(stderr, "pmaRewriteMeta: Botch: __pmLogEncodeLabels: %s\n",
		    pmErrStr(sts));
		exit(1);
	    }
	    for (i = 0; i < nsets; i++) {
		free(labelsetp[i].json);
		free(labelsetp[i].labels);
	    }
	    free(labelsetp);
	    free(*rbuf);
	    *rbuf = new;

	    sts = 1;
	    break;

	default:
	    sts = PM_ERR_RECTYPE;
    }

    return sts;
}
