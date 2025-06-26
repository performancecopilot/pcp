/*
 * Copyright (c) 2012-2017,2020-2022,2025 Red Hat.
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
    __int32_t	start[2];	/* start time of this archive (pmTimeval) */
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
    __int32_t	start[3];	/* start time of this archive (__pmTimestamp) */
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
__pmLogLabelSizeByVersion(int version)
{
    size_t	bytes;

    if (version == PM_LOG_VERS03)
	bytes = sizeof(__pmLabel_v3);
    else if (version == PM_LOG_VERS02)
	bytes = sizeof(__pmLabel_v2);
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "%s: label version %d not supported",
			   __FUNCTION__, version);
	return 0;
    }
    return bytes + 2 * sizeof(__int32_t);	/* header + trailer length */
}

size_t
__pmLogLabelSize(const __pmLogCtl *lcp)
{
    return __pmLogLabelSizeByVersion(__pmLogVersion(lcp));
}

int
__pmLogEncodeLabel(const __pmLogLabel *lp, void **buffer, size_t *length)
{
    int		version = lp->magic & 0xff;
    __int32_t	header;
    size_t	bytes;
    void	*buf;

    if (version == PM_LOG_VERS03)
	header = htonl((__int32_t)sizeof(__pmLabel_v3)+ 2*sizeof(__int32_t));
    else if (version == PM_LOG_VERS02)
	header = htonl((__int32_t)sizeof(__pmLabel_v2)+ 2*sizeof(__int32_t));
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "%s: label version %d not supported",
			    __FUNCTION__, version);
	return PM_ERR_LABEL;
    }

    bytes = __pmLogLabelSizeByVersion(version);
    if ((*buffer = buf = (__int32_t *)malloc(bytes)) == NULL)
	return -oserror();
    *length = bytes;

    memcpy(buf, &header, sizeof(header));
    buf = (unsigned char *)buf + sizeof(header);

    if (version == PM_LOG_VERS03) {
	__pmLabel_v3	label;

	/* swab */
	label.magic = htonl(lp->magic);
	label.pid = htonl(lp->pid);
	__pmPutTimestamp(&lp->start, label.start);
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

	memcpy(buf, &label, sizeof(label));
	buf = (unsigned char *)buf + sizeof(label);
    }
    else {
	/* version == PM_LOG_VERS02 */
	__pmLabel_v2	label;

	/* swab */
	label.magic = htonl(lp->magic);
	label.pid = htonl(lp->pid);
	__pmPutTimeval(&lp->start, label.start);
	label.vol = htonl(lp->vol);
	memset(label.hostname, 0, sizeof(label.hostname));
	bytes = MINIMUM(strlen(lp->hostname), PM_LOG_MAXHOSTLEN - 1);
	memcpy((void *)label.hostname, (void *)lp->hostname, bytes);
	memset(label.timezone, 0, sizeof(label.timezone));
	bytes = MINIMUM(strlen(lp->timezone), PM_TZ_MAXLEN - 1);
	memcpy((void *)label.timezone, (void *)lp->timezone, bytes);

	memcpy(buf, &label, sizeof(label));
	buf = (unsigned char *)buf + sizeof(label);
    }

    memcpy(buf, &header, sizeof(header)); /* trailer */
    return 0;
}

int
__pmLogWriteLabel(__pmFILE *f, const __pmLogLabel *lp)
{
    void	*buffer;
    size_t	length;
    size_t	bytes;
    int		sts;

    if ((sts = __pmLogEncodeLabel(lp, &buffer, &length)) < 0)
	return sts;

    bytes = __pmFwrite(buffer, 1, length, f);
    free(buffer);

    if (bytes != length) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("%s: write failed: returns %zu expecting %zu: %s\n",
		 __FUNCTION__, bytes, length,
		 osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	return -oserror();
    }

    return 0;
}

static void
__pmLogDecodeLabelV3(__pmLabel_v3 *in, __pmLogLabel *lp)
{
    __pmLogFreeLabel(lp);	/* reset from earlier call */
    lp->pid = ntohl(in->pid);
    __pmLoadTimestamp(in->start, &lp->start);
    lp->vol = ntohl(in->vol);
    lp->features = ntohl(in->features);
    lp->hostname = strndup(in->hostname, PM_MAX_HOSTNAMELEN - 1);
    lp->timezone = strndup(in->timezone, PM_MAX_TIMEZONELEN - 1);
    lp->zoneinfo = strndup(in->zoneinfo, PM_MAX_ZONEINFOLEN - 1);
}

static void
__pmLogDecodeLabelV2(__pmLabel_v2 *in, __pmLogLabel *lp)
{
    __pmLogFreeLabel(lp);	/* reset from earlier call */
    lp->pid = ntohl(in->pid);
    __pmLoadTimeval(in->start, &lp->start);
    lp->vol = ntohl(in->vol);
    lp->features = 0;		/* not supported in v2 */
    lp->hostname = strndup(in->hostname, PM_LOG_MAXHOSTLEN - 1);
    lp->timezone = strndup(in->timezone, PM_TZ_MAXLEN - 1);
    lp->zoneinfo = NULL;	/* not supported in v2 */
}

/*
 * Decode an archive label ... no checking other than record
 * length and header-trailer consistency
 */
int
__pmLogDecodeLabel(const char *buffer, size_t length, __pmLogLabel *lp)
{
    const size_t	length_v3 = __pmLogLabelSizeByVersion(PM_LOG_VERS03);
    const size_t	length_v2 = __pmLogLabelSizeByVersion(PM_LOG_VERS02);
    const char		*offset;
    __uint32_t		version, magic;
    __int32_t		*peek;
    size_t		bytes;

    /* input length must be valid for one of the fixed-size label versions */
    if (length != length_v2 && length != length_v3)
	return -EINVAL;

    peek = (__int32_t *)buffer;
    bytes = ntohl(peek[0]);
    magic = ntohl(peek[1]);
    if ((magic & 0xffffff00) != PM_LOG_MAGIC)
	return PM_ERR_LABEL;
    version = magic & 0xff;

    /* label header must be valid for the given fixed-size label version */
    if ((version == PM_LOG_VERS03 && bytes != length_v3) ||
	(version == PM_LOG_VERS02 && bytes != length_v2))
	return PM_ERR_LABEL;

    /* label trailer must be valid for the given fixed-size label version */
    peek = (__int32_t *)(buffer + bytes - sizeof(__int32_t));
    bytes = ntohl(*peek);
    if ((version == PM_LOG_VERS03 && bytes != length_v3) ||
	(version == PM_LOG_VERS02 && bytes != length_v2))
	return PM_ERR_LABEL;

    lp->magic = magic;

    /* swab external log label record */
    offset = buffer + sizeof(__int32_t);
    if (version == PM_LOG_VERS03)
	__pmLogDecodeLabelV3((__pmLabel_v3 *)offset, lp);
    else if (version == PM_LOG_VERS02)
	__pmLogDecodeLabelV2((__pmLabel_v2 *)offset, lp);
    else {
	if (pmDebugOptions.log)
	    fprintf(stderr, "%s: label version %u not supported\n", __FUNCTION__, version);
	return PM_ERR_LABEL;
    }

    return 0;
}

/*
 * Load an archive label ... no checking other than record
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
	__pmLogDecodeLabelV3(&label, lp);
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
	__pmLogDecodeLabelV2(&label, lp);
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

    if (acp->ac_flags & PM_CTXFLAG_NO_FEATURE_CHECK) {
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
     * If we have the label record and nothing else this is really
     * an empty archive; either pmlogger was killed off before any
     * data records were written or we are streaming this archive.
     * In the former case it's better to return PM_ERR_NODATA here
     * rather than to stumble into PM_ERR_LOGREC at the first call
     * to __pmLogRead*().  In the latter case all is well.
     */
    if (!(acp->ac_flags & PM_CTXFLAG_LAST_VOLUME)) {
	if ((sts = __pmFstat(f, &sbuf)) >= 0 &&
	    (sbuf.st_size == __pmLogLabelSize(lcp))) {
		if (pmDebugOptions.log)
		    fprintf(stderr, " file is empty");
		version = PM_ERR_NODATA;
	}
    }

    if (vol >= 0 && vol < lcp->numseen)
	lcp->seen[vol] = 1;

func_return:
    if (pmDebugOptions.log && diag_output) {
	fprintf(stderr, " version=%d", version);
	fputc('\n', stderr);
    }

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

