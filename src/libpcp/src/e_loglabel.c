/*
 * Copyright (c) 2012-2017,2020-2022 Red Hat.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021, Ken McDonell.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Thread-safe notes:
 *
 * None.
 */

#include <inttypes.h>
#include <assert.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * On-Disk Log Label, Version 2
 */
typedef struct {
    __uint32_t	magic;		/* PM_LOG_MAGIC|PM_LOG_VERS02 */
    __int32_t	pid;		/* PID of logger */
    __int32_t	start_sec;	/* start of this log (pmTimeval) */
    __int32_t	start_usec;
    __int32_t	vol;		/* current log volume no. */
    char	hostname[PM_LOG_MAXHOSTLEN]; /* name of collection host */
    char	timezone[PM_TZ_MAXLEN];	/* $TZ at collection host */
} __pmLabel_v2;

/*
 * On-Disk Log Label, Version 3
 */
typedef struct {
    __uint32_t	magic;		/* PM_LOG_MAGIC|PM_LOG_VERS03 */
    __int32_t	pid;		/* PID of logger */
    __int32_t	start_sec[2];	/* start of this log (__pmTimestamp) */
    __int32_t	start_nsec;
    __int32_t	vol;		/* current log volume no. */
    __uint32_t	features;	/* enabled archive feature bits */
    __uint32_t	reserved;	/* reserved for future use, zero padded */
    char	hostname[PM_MAX_HOSTNAMELEN];  /* collection host full name */
    char	timezone[PM_MAX_TIMEZONELEN];  /* generic "squashed" $TZ */
    char	zoneinfo[PM_MAX_ZONEINFOLEN];  /* local platform $TZ */
} __pmLabel_v3;

/*
 * Return the size of the log label record on disk ... other records
 * start immediately after this
 */
size_t
__pmLogLabelSize(const __pmLogCtl *lcp)
{
    size_t	bytes;
    int		version = __pmLogVersion(lcp);

    if (version == PM_LOG_VERS03)
	bytes = sizeof(__pmLabel_v3);
    else if (version == PM_LOG_VERS02)
	bytes = sizeof(__pmLabel_v2);
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogLabelSize: label version %d not supported", version);
	return 0;
    }
    return bytes + 2 * sizeof(__int32_t);	/* header + trailer length */
}

int
__pmLogWriteLabel(__pmFILE *f, const __pmLogLabel *lp)
{
    int		version = lp->magic & 0xff;
    __int32_t	header;		/* and trailer */
    size_t	bytes;

    if (version == PM_LOG_VERS03)
	header = htonl((__int32_t)sizeof(__pmLabel_v3)+ 2*sizeof(__int32_t));
    else if (version == PM_LOG_VERS02)
	header = htonl((__int32_t)sizeof(__pmLabel_v2)+ 2*sizeof(__int32_t));
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogWriteLabel: label version %d not supported", version);
	return PM_ERR_LABEL;
    }

    /* header */
    bytes = __pmFwrite(&header, 1, sizeof(header), f);
    if (bytes != sizeof(header)) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("%s: header write failed: returns %zu expecting %zu: %s\n",
		"__pmLogWriteLabel", bytes, sizeof(header),
		osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	return -oserror();
    }

    if (version == PM_LOG_VERS03) {
	__pmLabel_v3	label;

	/* swab */
	label.magic = htonl(lp->magic);
	label.pid = htonl(lp->pid);
	__pmPutTimestamp(&lp->start, &label.start_sec[0]);
	label.vol = htonl(lp->vol);
	label.features = htonl(lp->features);
	label.reserved = 0;
	memset(label.hostname, 0, sizeof(label.hostname));
	bytes = MINIMUM(pmstrlen(lp->hostname), PM_MAX_HOSTNAMELEN - 1);
	memcpy((void *)label.hostname, (void *)lp->hostname, bytes);
	memset(label.timezone, 0, sizeof(label.timezone));
	bytes = MINIMUM(pmstrlen(lp->timezone), PM_MAX_TIMEZONELEN - 1);
	memcpy((void *)label.timezone, (void *)lp->timezone, bytes);
	memset(label.zoneinfo, 0, sizeof(label.zoneinfo));
	bytes = MINIMUM(pmstrlen(lp->zoneinfo), PM_MAX_ZONEINFOLEN - 1);
	memcpy((void *)label.zoneinfo, (void *)lp->zoneinfo, bytes);

	bytes = __pmFwrite(&label, 1, sizeof(label), f);
	if (bytes != sizeof(label)) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("%s: write failed: returns %zu expecting %zu: %s\n",
		    "__pmLogWriteLabel", bytes, sizeof(label),
		    osstrerror_r(errmsg, sizeof(errmsg)));
	    pmflush();
	    return -oserror();
	}
    }
    else {
	/* version == PM_LOG_VERS02 */
	__pmLabel_v2	label;

	/* swab */
	label.magic = htonl(lp->magic);
	label.pid = htonl(lp->pid);
	__pmPutTimeval(&lp->start, &label.start_sec);
	label.vol = htonl(lp->vol);
	memset(label.hostname, 0, sizeof(label.hostname));
	bytes = MINIMUM(strlen(lp->hostname), PM_LOG_MAXHOSTLEN - 1);
	memcpy((void *)label.hostname, (void *)lp->hostname, bytes);
	memset(label.timezone, 0, sizeof(label.timezone));
	bytes = MINIMUM(strlen(lp->timezone), PM_TZ_MAXLEN - 1);
	memcpy((void *)label.timezone, (void *)lp->timezone, bytes);

	bytes = __pmFwrite(&label, 1, sizeof(label), f);
	if (bytes != sizeof(label)) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("%s: write failed: returns %zu expecting %zu: %s\n",
		    "__pmLogWriteLabel", bytes, sizeof(label),
		    osstrerror_r(errmsg, sizeof(errmsg)));
	    pmflush();
	    return -oserror();
	}
    }

    /* trailer */
    bytes = __pmFwrite(&header, 1, sizeof(header), f);
    if (bytes != sizeof(header)) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("%s: trailer write failed: returns %zu expecting %zu: %s\n",
		"__pmLogWriteLabel", bytes, sizeof(header),
		osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	return -oserror();
    }
    
    return 0;
}

/*
 * Load an archive log label ... no checking other than record
 * length and header-trailer consistency
 */
int
__pmLogLoadLabel(__pmFILE *f, __pmLogLabel *lp)
{
    size_t	bytes;
    __int32_t	peek[2];	/* for header and magic, then trailer */
    size_t	length;
    int		version;

    __pmFseek(f, 0, SEEK_SET);
    bytes = __pmFread(peek, 1, sizeof(peek), f);
    if (bytes != sizeof(peek)) {
	if (__pmFeof(f)) {
	    __pmClearerr(f);
	    if (pmDebugOptions.log)
		fprintf(stderr, "__pmLogLoadLabel: file is empty\n");
	    return PM_ERR_NODATA;
	}
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogLoadLabel: peek read returned %zd, expected %zd\n", bytes, sizeof(peek));
	if (__pmFerror(f)) {
	    __pmClearerr(f);
	    return -oserror();
	}
	return PM_ERR_LABEL;
    }
    length = ntohl(peek[0]);
    lp->magic = ntohl(peek[1]);
    version = lp->magic & 0xff;

    if (version == PM_LOG_VERS03) {
	__pmLabel_v3	label;

	/* check the length from header */
	if (length != sizeof(label) + 2*sizeof(__int32_t)) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "__pmLogLoadLabel: bad header len=%zu (expected %zu)\n", length, sizeof(label) + 2*sizeof(__int32_t));
	    return PM_ERR_LABEL;
	}

	/* read the rest of the v3 label */
	bytes = __pmFread(&label.pid, 1, sizeof(label) - sizeof(label.magic), f);
	if (bytes != sizeof(label) - sizeof(label.magic)) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "__pmLogLoadLabel: bad label read len=%zu: expected %zu\n",
			bytes, sizeof(label) - sizeof(label.magic));
	    if (__pmFerror(f)) {
		__pmClearerr(f);
		return -oserror();
	    }
	    return PM_ERR_LABEL;
	}

	/* swab external log label record */
	lp->pid = ntohl(label.pid);
	__pmLoadTimestamp(&label.start_sec[0], &lp->start);
	lp->vol = ntohl(label.vol);
	lp->features = ntohl(label.features);
	if (lp->hostname)
	    free(lp->hostname);
	lp->hostname = strndup(label.hostname, PM_MAX_HOSTNAMELEN - 1);
	if (lp->timezone)
	    free(lp->timezone);
	lp->timezone = strndup(label.timezone, PM_MAX_TIMEZONELEN - 1);
	if (lp->zoneinfo)
	    free(lp->zoneinfo);
	lp->zoneinfo = strndup(label.zoneinfo, PM_MAX_ZONEINFOLEN - 1);
    }
    else if (version == PM_LOG_VERS02) {
	__pmLabel_v2	label;

	/* check the length from header */
	if (length != sizeof(label) + 2*sizeof(__int32_t)) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "__pmLogLoadLabel: bad header len=%zu (expected %zu)\n", length, sizeof(label) + 2*sizeof(__int32_t));
	    return PM_ERR_LABEL;
	}

	/* read the rest of the v2 label */
	bytes = __pmFread(&label.pid, 1, sizeof(label) - sizeof(label.magic), f);
	if (bytes != sizeof(label) - sizeof(label.magic)) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "__pmLogLoadLabel: bad label read len=%zu: expected %zu\n",
			bytes, sizeof(label) - sizeof(label.magic));
	    if (__pmFerror(f)) {
		__pmClearerr(f);
		return -oserror();
	    }
	    return PM_ERR_LABEL;
	}

	/* swab external log label record */
	lp->pid = ntohl(label.pid);
	__pmLoadTimeval(&label.start_sec, &lp->start);
	lp->vol = ntohl(label.vol);
	lp->features = 0;		/* not supported in v2 */
	if (lp->hostname)
	    free(lp->hostname);
	lp->hostname = strndup(label.hostname, PM_LOG_MAXHOSTLEN - 1);
	if (lp->timezone)
	    free(lp->timezone);
	lp->timezone = strndup(label.timezone, PM_TZ_MAXLEN - 1);
	if (lp->zoneinfo)
	    free(lp->zoneinfo);
	lp->zoneinfo = NULL;	/* not supported in v2 */
    }
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogLoadLabel: label version %d not supported\n", version);
	return PM_ERR_LABEL;
    }

    /* check length from trailer */
    bytes = __pmFread(&peek[0], 1, sizeof(__int32_t), f);
    peek[0] = ntohl(peek[0]);
    if (bytes != sizeof(__int32_t) || length != peek[0]) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "__pmLogLoadLabel: trailer read -> %zu (expect %zu) or bad trailer len=%d (expected %zu)\n",
			    bytes, sizeof(__int32_t), peek[0], length);
	__pmLogFreeLabel(lp);
	if (__pmFerror(f)) {
	    __pmClearerr(f);
	    return -oserror();
	} else {
	    return PM_ERR_LABEL;
	}
    }

    return 0;
}

int
__pmLogChkLabel(__pmArchCtl *acp, __pmFILE *f, __pmLogLabel *lp, int vol)
{
    __pmLogCtl	*lcp = acp->ac_log;
    struct stat	sbuf;
    size_t	bytes;
    int		version = UNKNOWN_VERSION;
    int		sts;
    int		diag_output = 0;

    if (vol >= 0 && vol < lcp->numseen && lcp->seen[vol]) {
	/* FastPath, cached result of previous check for this volume */
	__pmFseek(f, (long)__pmLogLabelSize(lcp), SEEK_SET);
	version = 0;
	goto func_return;
    }

    if (vol >= 0 && vol >= lcp->numseen) {
	bytes = (vol + 1) * sizeof(lcp->seen[0]);
	if ((lcp->seen = (int *)realloc(lcp->seen, bytes)) == NULL) {
	    lcp->numseen = 0;
	} else {
	    int 	i;
	    for (i = lcp->numseen; i < vol; i++)
		lcp->seen[i] = 0;
	    lcp->numseen = vol + 1;
	}
    }

    if ((sts = __pmLogLoadLabel(f, lp)) < 0) {
	version = sts;
	goto func_return;
    }

    if (pmDebugOptions.log) {
	fprintf(stderr, "__pmLogChkLabel: fd=%d vol=%d", __pmFileno(f), vol);
	diag_output = 1;
    }

    version = lp->magic & 0xff;
    if ((lp->magic & 0xffffff00) != PM_LOG_MAGIC) {
	version = PM_ERR_LABEL;
	if (pmDebugOptions.log)
	    fprintf(stderr, " label magic 0x%x not 0x%x as expected",
			    (lp->magic & 0xffffff00), PM_LOG_MAGIC);
	goto func_return;
    }

    if (lp->vol != vol) {
	if (pmDebugOptions.log)
	    fprintf(stderr, " label volume %d not %d as expected", lp->vol, vol);
	version = PM_ERR_LABEL;
	goto func_return;
    }

    if (acp->ac_chkfeatures) {
	/*
	 * Check feature bits
	 */
	__uint32_t	myfeatures = PM_LOG_FEATURES;
	__uint32_t	mask = ~myfeatures;

	if (lp->features & mask) {
	    /*
	     * Oops, archive features include at least one "bit" that we
	     * don't know how to support
	     */
	    if (pmDebugOptions.log) {
		char		*bits = __pmLogFeaturesStr(lp->features & mask);
		if (bits != NULL) {
		    fprintf(stderr, " features 0x%x [unknown: %s]", lp->features, bits);
		    free(bits);
		}
		else
		    fprintf(stderr, " features 0x%x [unknown: 0x%x]", lp->features, lp->features & mask);
	    }
	    version = PM_ERR_FEATURE;
	    goto func_return;
	}
    }

    if (__pmSetVersionIPC(__pmFileno(f), PDU_VERSION) < 0) {
	version = -oserror();
	goto func_return;
    }

    if (pmDebugOptions.log) {
	fprintf(stderr, " [magic=0x%08x version=%d vol=%d pid=%d start=",
		lp->magic, version, lp->vol, lp->pid);
	__pmPrintTimestamp(stderr, &lp->start);
	if (lp->features != 0) {
	    char	*bits = __pmLogFeaturesStr(lp->features);
	    if (bits != NULL) {
		fprintf(stderr, " features=0x%x \"%s\"", lp->features, bits);
		free(bits);
	    }
	    else
		fprintf(stderr, " features=0x%x \"???\"", lp->features);
	}
	fprintf(stderr, " host=%s", lp->hostname);
	if (lp->timezone)
	    fprintf(stderr, " tz=%s", lp->timezone);
	if (lp->zoneinfo)
	    fprintf(stderr, " zoneinfo=%s", lp->zoneinfo);
	fputc(']', stderr);
    }

    /*
     * If we have the label record, and nothing else this is really
     * an empty archive (probably pmlogger was killed off before any
     * data records were written) ... better to return PM_ERR_NODATA
     * here, rather than to stumble into PM_ERR_LOGREC at the first
     * call to __pmLogRead*()
     */
    if ((sts = __pmFstat(f, &sbuf)) >= 0) {
	if (sbuf.st_size == __pmLogLabelSize(lcp)) {
	    if (pmDebugOptions.log)
		fprintf(stderr, " file is empty");
	    version = PM_ERR_NODATA;
	}
    }

    if (vol >= 0 && vol < lcp->numseen)
	lcp->seen[vol] = 1;

func_return:
    if (pmDebugOptions.log && diag_output)
	fputc('\n', stderr);

    return version;
}

/*
 * free memory allocated in __pmLogLoadLabel()
 */
void
__pmLogFreeLabel(__pmLogLabel *lp)
{
    if (lp->hostname != NULL) {
	free(lp->hostname);
	lp->hostname = NULL;
    }
    if (lp->timezone != NULL) {
	free(lp->timezone);
	lp->timezone = NULL;
    }
    if (lp->zoneinfo != NULL) {
	free(lp->zoneinfo);
	lp->zoneinfo = NULL;
    }
}

