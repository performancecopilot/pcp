/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * __pmLogReads is a diagnostic counter that is maintained with
 * non-atomic updates ... we've decided that it is acceptable for the
 * value to be subject to possible (but unlikely) missed updates
 */

#include <inttypes.h>
#include <assert.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

PCP_DATA int	__pmLogReads;

/*
 * Suffixes and associated compresssion application for compressed filenames.
 * These can appear _after_ the volume number in the name of a file for an
 * archive metric log file, e.g. /var/log/pmlogger/myhost/20101219.0.bz2
 */
#define	USE_NONE	0
#define	USE_BZIP2	1
#define USE_GZIP	2
#define USE_XZ		3
static const struct {
    const char	*suff;
    const int	appl;
} compress_ctl[] = {
    { ".xz",	USE_XZ },
    { ".lzma",	USE_XZ },
    { ".bz2",	USE_BZIP2 },
    { ".bz",	USE_BZIP2 },
    { ".gz",	USE_GZIP },
    { ".Z",	USE_GZIP },
    { ".z",	USE_GZIP },
};
static const int ncompress = sizeof(compress_ctl) / sizeof(compress_ctl[0]);

/*
 * first two fields are made to look like a pmValueSet when no values are
 * present ... used to populate the pmValueSet in a pmResult when values
 * for particular metrics are not available from this log record.
 */
typedef struct {
    pmID	pc_pmid;
    int		pc_numval;	/* MUST be 0 */
    				/* value control for interpolation */
} pmid_ctl;

/*
 * Hash control for requested metrics, used to construct 'No values'
 * result when the corresponding metric is requested but there is
 * no values available in the pmResult
 *
 * Note, this hash table is global across all contexts.
 */
static __pmHashCtl	pc_hc;

#ifdef PCP_DEBUG
static void
dumpbuf(int nch, __pmPDU *pb)
{
    int		i, j;

    nch /= sizeof(__pmPDU);
    fprintf(stderr, "%03d: ", 0);
    for (j = 0, i = 0; j < nch; j++) {
	if (i == 8) {
	    fprintf(stderr, "\n%03d: ", j);
	    i = 0;
	}
	fprintf(stderr, "%8x ", pb[j]);
	i++;
    }
    fputc('\n', stderr);
}
#endif

int
__pmLogChkLabel(__pmLogCtl *lcp, FILE *f, __pmLogLabel *lp, int vol)
{
    int		len;
    int		version = UNKNOWN_VERSION;
    int		xpectlen = sizeof(__pmLogLabel) + 2 * sizeof(len);
    int		n;

    if (vol >= 0 && vol < lcp->l_numseen && lcp->l_seen[vol]) {
	/* FastPath, cached result of previous check for this volume */
	fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	return 0;
    }

    if (vol >= 0 && vol >= lcp->l_numseen) {
	lcp->l_seen = (int *)realloc(lcp->l_seen, (vol+1)*(int)sizeof(lcp->l_seen[0]));
	if (lcp->l_seen == NULL)
	    lcp->l_numseen = 0;
	else {
	    int 	i;
	    for (i = lcp->l_numseen; i < vol; i++)
		lcp->l_seen[i] = 0;
	    lcp->l_numseen = vol+1;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "__pmLogChkLabel: fd=%d vol=%d", fileno(f), vol);
#endif

    fseek(f, (long)0, SEEK_SET);
    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
	if (feof(f)) {
	    clearerr(f);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " file is empty\n");
#endif
	    return PM_ERR_NODATA;
	}
	else {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " header read -> %d (expect %d) or bad header len=%d (expected %d)\n",
		    n, (int)sizeof(len), len, xpectlen);
#endif
	    if (ferror(f)) {
		clearerr(f);
		return -oserror();
	    }
	    else
		return PM_ERR_LABEL;
	}
    }

    if ((n = (int)fread(lp, 1, sizeof(__pmLogLabel), f)) != sizeof(__pmLogLabel)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " bad label len=%d: expected %d\n",
		n, (int)sizeof(__pmLogLabel));
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -oserror();
	}
	else
	    return PM_ERR_LABEL;
    }
    else {
	/* swab internal log label */
	lp->ill_magic = ntohl(lp->ill_magic);
	lp->ill_pid = ntohl(lp->ill_pid);
	lp->ill_start.tv_sec = ntohl(lp->ill_start.tv_sec);
	lp->ill_start.tv_usec = ntohl(lp->ill_start.tv_usec);
	lp->ill_vol = ntohl(lp->ill_vol);
    }

    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " trailer read -> %d (expect %d) or bad trailer len=%d (expected %d)\n",
		n, (int)sizeof(len), len, xpectlen);
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -oserror();
	}
	else
	    return PM_ERR_LABEL;
    }

    version = lp->ill_magic & 0xff;
    if ((lp->ill_magic & 0xffffff00) != PM_LOG_MAGIC ||
	(version != PM_LOG_VERS02) || lp->ill_vol != vol) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    if ((lp->ill_magic & 0xffffff00) != PM_LOG_MAGIC)
		fprintf(stderr, " label magic 0x%x not 0x%x as expected", (lp->ill_magic & 0xffffff00), PM_LOG_MAGIC);
	    if (version != PM_LOG_VERS02)
		fprintf(stderr, " label version %d not supported", version);
	    if (lp->ill_vol != vol)
		fprintf(stderr, " label volume %d not %d as expected", lp->ill_vol, vol);
	    fputc('\n', stderr);
	}
#endif
	return PM_ERR_LABEL;
    }
    else {
	if (__pmSetVersionIPC(fileno(f), version) < 0)
	    return -oserror();
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " [magic=%8x version=%d vol=%d pid=%d host=%s]\n",
		lp->ill_magic, version, lp->ill_vol, lp->ill_pid, lp->ill_hostname);
#endif
    }

    if (vol >= 0 && vol < lcp->l_numseen)
	lcp->l_seen[vol] = 1;

    return version;
}

static int
popen_uncompress(const char *cmd, const char *fname, const char *suffix, int fd)
{
    char	pipecmd[2*MAXPATHLEN+2];
    char	buffer[4096];
    FILE	*finp;
    ssize_t	bytes;
    int		sts, infd;

    snprintf(pipecmd, sizeof(pipecmd), "%s %s%s", cmd, fname, suffix);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "__pmLogOpen: uncompress using: %s\n", pipecmd);
#endif

    if ((finp = popen(pipecmd, "r")) == NULL)
	return -1;
    infd = fileno(finp);

    while ((bytes = read(infd, buffer, sizeof(buffer))) > 0) {
	if (write(fd, buffer, bytes) != bytes) {
	    bytes = -1;
	    break;
	}
    }

    if ((sts = pclose(finp)) != 0)
	return sts;
    return (bytes == 0) ? 0 : -1;
}

static int
fopen_securetmp(const char *fname)
{
    char	tmpname[MAXPATHLEN];
    mode_t	cur_umask;
    char	*msg;
    int		fd;

    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
#if HAVE_MKSTEMP
    if ((msg = pmGetOptionalConfig("PCP_TMPFILE_DIR")) == NULL) {
	umask(cur_umask);
	return -1;
    }
    snprintf(tmpname, sizeof(tmpname), "%s/XXXXXX", msg);
    msg = tmpname;
    fd = mkstemp(tmpname);
#else
    if ((msg = tmpnam(NULL)) == NULL) {
	umask(cur_umask);
	return -1;
    }
    fd = open(msg, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
    /*
     * unlink temporary file to avoid namespace pollution and allow O/S
     * space cleanup on last close
     */
    unlink(msg);
    umask(cur_umask);
    return fd;
}

/*
 * Lookup whether the suffix matches one of the compression styles,
 * and if so return the matching index into the compress_ctl table.
 */
static int
index_compress(const char *fname)
{
    int		i;
    char	tmpname[MAXPATHLEN];

    for (i = 0; i < ncompress; i++) {
	snprintf(tmpname, sizeof(tmpname), "%s%s", fname, compress_ctl[i].suff);
	if (access(tmpname, R_OK) == 0)
	    return i;
    }
    /* end up here if it does not look like a compressed file */
    return -1;
}

static FILE *
fopen_compress(const char *fname)
{
    int		sts;
    int		fd;
    int		i;
    char	*cmd;
    FILE	*fp;

    if ((i = index_compress(fname)) < 0)
	return NULL;

    if (compress_ctl[i].appl == USE_XZ)
	cmd = "xz -dc";
    else if (compress_ctl[i].appl == USE_BZIP2)
	cmd = "bzip2 -dc";
    else if (compress_ctl[i].appl == USE_GZIP)
	cmd = "gzip -dc";
    else {
	/* botch in compress_ctl[] ... should not happen */
	return NULL;
    }

    if ((fd = fopen_securetmp(fname)) < 0) {
	sts = oserror();
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "__pmLogOpen: temp file create failed: %s\n", osstrerror());
#endif
	setoserror(sts);
	return NULL;
    }

    sts = popen_uncompress(cmd, fname, compress_ctl[i].suff, fd);
    if (sts == -1) {
	sts = oserror();
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogOpen: uncompress command failed: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	}
#endif
	close(fd);
	setoserror(sts);
	return NULL;
    }
    if (sts != 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
#if defined(HAVE_SYS_WAIT_H)
	    if (WIFEXITED(sts))
		fprintf(stderr, "__pmLogOpen: uncompress failed, exit status: %d\n", WEXITSTATUS(sts));
	    else if (WIFSIGNALED(sts))
		fprintf(stderr, "__pmLogOpen: uncompress failed, signal: %d\n", WTERMSIG(sts));
	    else
#endif
		fprintf(stderr, "__pmLogOpen: uncompress failed, popen() returns: %d\n", sts);
	}
#endif
	close(fd);
	/* not a great error code, but the best we can do */
	setoserror(-PM_ERR_LOGREC);
	return NULL;
    }
    if ((fp = fdopen(fd, "r")) == NULL) {
	sts = oserror();
	close(fd);
	setoserror(sts);
	return NULL;
    }
    /* success */
    return fp;
}

static FILE *
_logpeek(__pmLogCtl *lcp, int vol)
{
    int			sts;
    FILE		*f;
    __pmLogLabel	label;
    char		fname[MAXPATHLEN];

    snprintf(fname, sizeof(fname), "%s.%d", lcp->l_name, vol);
    if ((f = fopen(fname, "r")) == NULL) {
	if ((f = fopen_compress(fname)) == NULL)
	    return f;
    }

    if ((sts = __pmLogChkLabel(lcp, f, &label, vol)) < 0) {
	fclose(f);
	setoserror(sts);
	return NULL;
    }
    
    return f;
}

int
__pmLogChangeVol(__pmLogCtl *lcp, int vol)
{
    char	name[MAXPATHLEN];
    int		sts;

    if (lcp->l_curvol == vol)
	return 0;

    if (lcp->l_mfp != NULL) {
	__pmResetIPC(fileno(lcp->l_mfp));
	fclose(lcp->l_mfp);
    }
    snprintf(name, sizeof(name), "%s.%d", lcp->l_name, vol);
    if ((lcp->l_mfp = fopen(name, "r")) == NULL) {
	/* try for a compressed file */
	if ((lcp->l_mfp = fopen_compress(name)) == NULL)
	    return -oserror();
    }

    if ((sts = __pmLogChkLabel(lcp, lcp->l_mfp, &lcp->l_label, vol)) < 0)
	return sts;

    lcp->l_curvol = vol;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "__pmLogChangeVol: change to volume %d\n", vol);
#endif
    return sts;
}

int
__pmLogLoadIndex(__pmLogCtl *lcp)
{
    int		sts = 0;
    FILE	*f = lcp->l_tifp;
    int		n;
    __pmLogTI	*tip;

    lcp->l_numti = 0;
    lcp->l_ti = NULL;

    if (lcp->l_tifp != NULL) {
	fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	for ( ; ; ) {
	    lcp->l_ti = (__pmLogTI *)realloc(lcp->l_ti, (1 + lcp->l_numti) * sizeof(__pmLogTI));
	    if (lcp->l_ti == NULL) {
		sts = -oserror();
		break;
	    }
	    tip = &lcp->l_ti[lcp->l_numti];
	    n = (int)fread(tip, 1, sizeof(__pmLogTI), f);

            if (n != sizeof(__pmLogTI)) {
		if (feof(f)) {
		    clearerr(f);
		    sts = 0; 
		    break;
		}
#ifdef PCP_DEBUG
	  	if (pmDebug & DBG_TRACE_LOG)
	    	    fprintf(stderr, "__pmLogLoadIndex: bad TI entry len=%d: expected %d\n",
		            n, (int)sizeof(__pmLogTI));
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -oserror();
		    break;
		}
		else {
		    sts = PM_ERR_LOGREC;
		    break;
		}
	    }
	    else {
		/* swab the temporal index record */
		tip->ti_stamp.tv_sec = ntohl(tip->ti_stamp.tv_sec);
		tip->ti_stamp.tv_usec = ntohl(tip->ti_stamp.tv_usec);
		tip->ti_vol = ntohl(tip->ti_vol);
		tip->ti_meta = ntohl(tip->ti_meta);
		tip->ti_log = ntohl(tip->ti_log);
	    }

	    lcp->l_numti++;
	}/*for*/
    }/*not null*/

    return sts;
}

const char *
__pmLogName_r(const char *base, int vol, char *buf, int buflen)
{
    switch (vol) {
	case PM_LOG_VOL_TI:
	    snprintf(buf, buflen, "%s.index", base);
	    break;

	case PM_LOG_VOL_META:
	    snprintf(buf, buflen, "%s.meta", base);
	    break;

	default:
	    snprintf(buf, buflen, "%s.%d", base, vol);
	    break;
    }

    return buf;
}

const char *
__pmLogName(const char *base, int vol)
{
    static char		tbuf[MAXPATHLEN];

    return __pmLogName_r(base, vol, tbuf, sizeof(tbuf));
}

FILE *
__pmLogNewFile(const char *base, int vol)
{
    char	fname[MAXPATHLEN];
    FILE	*f;
    int		save_error;

    __pmLogName_r(base, vol, fname, sizeof(fname));

    if (access(fname, R_OK) != -1) {
	/* exists and readable ... */
	pmprintf("__pmLogNewFile: \"%s\" already exists, not over-written\n", fname);
	pmflush();
	setoserror(EEXIST);
	return NULL;
    }

    if ((f = fopen(fname, "w")) == NULL) {
	char	errmsg[PM_MAXERRMSGLEN];
	save_error = oserror();
	pmprintf("__pmLogNewFile: failed to create \"%s\": %s\n", fname, osstrerror_r(errmsg, sizeof(errmsg)));

	pmflush();
	setoserror(save_error);
	return NULL;
    }
    /*
     * Want unbuffered I/O for all files of the archive, so a single
     * fwrite() maps to one logical record for each of the metadata
     * records, the index records and the data (pmResult) records.
     */
    setvbuf(f, NULL, _IONBF, 0);

    if ((save_error = __pmSetVersionIPC(fileno(f), PDU_VERSION)) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogNewFile: failed to setup \"%s\": %s\n", fname, osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	fclose(f);
	setoserror(save_error);
	return NULL;
    }

    return f;
}

int
__pmLogWriteLabel(FILE *f, const __pmLogLabel *lp)
{
    int		sts = 0;
    struct {				/* skeletal external record */
	int		header;
	__pmLogLabel	label;
	int		trailer;
    } out;

    out.header = out.trailer = htonl((int)sizeof(out));

    /* swab */
    out.label.ill_magic = htonl(lp->ill_magic);
    out.label.ill_pid = htonl(lp->ill_pid);
    out.label.ill_start.tv_sec = htonl(lp->ill_start.tv_sec);
    out.label.ill_start.tv_usec = htonl(lp->ill_start.tv_usec);
    out.label.ill_vol = htonl(lp->ill_vol);
    memmove((void *)out.label.ill_hostname, (void *)lp->ill_hostname, sizeof(lp->ill_hostname));
    memmove((void *)out.label.ill_tz, (void *)lp->ill_tz, sizeof(lp->ill_tz));

    if ((sts = fwrite(&out, 1, sizeof(out), f)) != sizeof(out)) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogWriteLabel: write failed: returns %d expecting %d: %s\n",
	    sts, (int)sizeof(out), osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	sts = -oserror();
    }
    else
	sts = 0;

    return sts;
}

int
__pmLogCreate(const char *host, const char *base, int log_version,
	      __pmLogCtl *lcp)
{
    int		save_error = 0;
    char	fname[MAXPATHLEN];

    lcp->l_minvol = lcp->l_maxvol = lcp->l_curvol = 0;
    lcp->l_hashpmid.nodes = lcp->l_hashpmid.hsize = 0;
    lcp->l_hashindom.nodes = lcp->l_hashindom.hsize = 0;
    lcp->l_tifp = lcp->l_mdfp = lcp->l_mfp = NULL;

    if ((lcp->l_tifp = __pmLogNewFile(base, PM_LOG_VOL_TI)) != NULL) {
	if ((lcp->l_mdfp = __pmLogNewFile(base, PM_LOG_VOL_META)) != NULL) {
	    if ((lcp->l_mfp = __pmLogNewFile(base, 0)) != NULL) {
		char	tzbuf[PM_TZ_MAXLEN];
		char	*tz;
                int	sts;

		tz = __pmTimezone_r(tzbuf, sizeof(tzbuf));

		lcp->l_label.ill_magic = PM_LOG_MAGIC | log_version;
		/*
		 * Warning	ill_hostname may be truncated, but we
		 *		guarantee it will be null-byte terminated
		 */
		strncpy(lcp->l_label.ill_hostname, host, PM_LOG_MAXHOSTLEN-1);
		lcp->l_label.ill_hostname[PM_LOG_MAXHOSTLEN-1] = '\0';
		lcp->l_label.ill_pid = (int)getpid();
		/*
		 * hack - how do you get the TZ for a remote host?
		 */
		strcpy(lcp->l_label.ill_tz, tz ? tz : "");
		lcp->l_state = PM_LOG_STATE_NEW;

                /*
                 * __pmLogNewFile sets the IPC version to PDU_VERSION
                 * we want log_version instead
                 */
		sts = __pmSetVersionIPC(fileno(lcp->l_tifp), log_version);
		if (sts < 0)
                    return sts;
		sts = __pmSetVersionIPC(fileno(lcp->l_mdfp), log_version);
		if (sts < 0)
                    return sts;
		sts = __pmSetVersionIPC(fileno(lcp->l_mfp), log_version);
		return sts;
	    }
	    else {
		save_error = oserror();
		unlink(__pmLogName_r(base, PM_LOG_VOL_TI, fname, sizeof(fname)));
		unlink(__pmLogName_r(base, PM_LOG_VOL_META, fname, sizeof(fname)));
		setoserror(save_error);
	    }
	}
	else {
	    save_error = oserror();
	    unlink(__pmLogName_r(base, PM_LOG_VOL_TI, fname, sizeof(fname)));
	    setoserror(save_error);
	}
    }

    lcp->l_tifp = lcp->l_mdfp = lcp->l_mfp = NULL;
    return oserror() ? -oserror() : -EPERM;
}

/*
 * Close the log files.
 * Free up the space used by __pmLogCtl.
 */

void
__pmLogClose(__pmLogCtl *lcp)
{
    if (lcp->l_tifp != NULL) {
	__pmResetIPC(fileno(lcp->l_tifp));
	fclose(lcp->l_tifp);
	lcp->l_tifp = NULL;
    }
    if (lcp->l_mdfp != NULL) {
	__pmResetIPC(fileno(lcp->l_mdfp));
	fclose(lcp->l_mdfp);
	lcp->l_mdfp = NULL;
    }
    if (lcp->l_mfp != NULL) {
	__pmResetIPC(fileno(lcp->l_mfp));
	fclose(lcp->l_mfp);
	lcp->l_mfp = NULL;
    }
    if (lcp->l_name != NULL) {
	free(lcp->l_name);
	lcp->l_name = NULL;
    }
    if (lcp->l_seen != NULL) {
	free(lcp->l_seen);
	lcp->l_seen = NULL;
	lcp->l_numseen = 0;
    }
    if (lcp->l_pmns != NULL) {
	__pmFreePMNS(lcp->l_pmns);
	lcp->l_pmns = NULL;
    }

    if (lcp->l_ti != NULL)
	free(lcp->l_ti);

    if (lcp->l_hashpmid.hsize != 0) {
	__pmHashCtl	*hcp = &lcp->l_hashpmid;
	__pmHashNode	*hp;
	__pmHashNode	*prior_hp;
	int		i;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i], prior_hp = NULL; hp != NULL; hp = hp->next) {
		if (hp->data != NULL)
		    free(hp->data);
		if (prior_hp != NULL)
		    free(prior_hp);
		prior_hp = hp;
	    }
	    if (prior_hp != NULL)
		free(prior_hp);
	}
	free(hcp->hash);
    }

    if (lcp->l_hashindom.hsize != 0) {
	__pmHashCtl	*hcp = &lcp->l_hashindom;
	__pmHashNode	*hp;
	__pmHashNode	*prior_hp;
	__pmLogInDom	*idp;
	__pmLogInDom	*prior_idp;
	int		i;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i], prior_hp = NULL; hp != NULL; hp = hp->next) {
		for (idp = (__pmLogInDom *)hp->data, prior_idp = NULL;
		     idp != NULL; idp = idp->next) {
		    if (idp->buf != NULL)
			free(idp->buf);
		    if (idp->allinbuf == 0 && idp->namelist != NULL)
			free(idp->namelist);
		    if (prior_idp != NULL)
			free(prior_idp);
		    prior_idp = idp;
		}
		if (prior_idp != NULL)
		    free(prior_idp);
		if (prior_hp != NULL)
		    free(prior_hp);
		prior_hp = hp;
	    }
	    if (prior_hp != NULL)
		free(prior_hp);
	}
	free(hcp->hash);
    }

}

int
__pmLogLoadLabel(__pmLogCtl *lcp, const char *name)
{
    int		sts;
    int		blen;
    int		exists = 0;
    int		i;
    int		sep = __pmPathSeparator();
    char	*q;
    char	*base;
    char	*tbuf;
    char	*tp;
    char	*dir;
    DIR		*dirp = NULL;
    char	filename[MAXPATHLEN];
#if defined(HAVE_READDIR64)
    struct dirent64	*direntp;
#else
    struct dirent	*direntp;
#endif

    /*
     * find directory name component ... copy as dirname() may clobber
     * the string
     */
    if ((tbuf = strdup(name)) == NULL)
	return -oserror();
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    dir = dirname(tbuf);

    /*
     * find file name component
     */
    strncpy(filename, name, MAXPATHLEN);
    filename[MAXPATHLEN-1] = '\0';
    if ((base = strdup(basename(filename))) == NULL) {
	sts = -oserror();
	free(tbuf);
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }
    PM_UNLOCK(__pmLock_libpcp);

    if (access(name, R_OK) == 0) {
	/*
	 * file exists and is readable ... if name contains '.' and
	 * suffix is "index", "meta" or a string of digits or a string
	 * of digits followed by one of the compression suffixes,
	 * strip the suffix
	 */
	int	strip = 0;
	if ((q = strrchr(base, '.')) != NULL) {
	    if (strcmp(q, ".index") == 0) {
		strip = 1;
		goto done;
	    }
	    if (strcmp(q, ".meta") == 0) {
		strip = 1;
		goto done;
	    }
	    for (i = 0; i < ncompress; i++) {
		if (strcmp(q, compress_ctl[i].suff) == 0) {
		    char	*q2;
		    /*
		     * name ends with one of the supported compressed file
		     * suffixes, check for a string of digits before that,
		     * e.g. if base is initially "foo.0.bz2", we want it
		     * stripped to "foo"
		     */
		    *q = '\0';
		    if ((q2 = strrchr(base, '.')) == NULL) {
			/* no . to the left of the suffix */
			*q = '.';
			goto done;
		    }
		    q = q2;
		    break;
		}
	    }
	    if (q[1] != '\0') {
		char	*end;
		/*
		 * Below we don't care about the value from strtol(),
		 * we're interested in updating the pointer "end".
		 * The messiness is thanks to gcc and glibc ... strtol()
		 * is marked __attribute__((warn_unused_result)) ...
		 * to avoid warnings on all platforms, assign to a
		 * dummy variable that is explicitly marked unused.
		 */
		long	tmpl __attribute__((unused));
		tmpl = strtol(q+1, &end, 10);
		if (*end == '\0') strip = 1;
	    }
	}
done:
	if (strip) {
	    *q = '\0';
	}
    }

    snprintf(filename, sizeof(filename), "%s%c%s", dir, sep, base);
    if ((lcp->l_name = strdup(filename)) == NULL) {
	sts = -oserror();
	free(tbuf);
	free(base);
	return sts;
    }

    lcp->l_minvol = -1;
    lcp->l_tifp = lcp->l_mdfp = lcp->l_mfp = NULL;
    lcp->l_ti = NULL;
    lcp->l_hashpmid.nodes = lcp->l_hashpmid.hsize = 0;
    lcp->l_hashindom.nodes = lcp->l_hashindom.hsize = 0;
    lcp->l_numseen = 0; lcp->l_seen = NULL;
    lcp->l_pmns = NULL;

    blen = (int)strlen(base);
    PM_LOCK(__pmLock_libpcp);
    if ((dirp = opendir(dir)) != NULL) {
#if defined(HAVE_READDIR64)
	while ((direntp = readdir64(dirp)) != NULL)
#else
	while ((direntp = readdir(dirp)) != NULL)
#endif
	{
	    if (strncmp(base, direntp->d_name, blen) != 0)
		continue;
	    if (direntp->d_name[blen] != '.')
		continue;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		snprintf(filename, sizeof(filename), "%s%c%s", dir, sep, direntp->d_name);
		fprintf(stderr, "__pmLogOpen: inspect file \"%s\"\n", filename);
	    }
#endif
	    tp = &direntp->d_name[blen+1];
	    if (strcmp(tp, "index") == 0) {
		exists = 1;
		snprintf(filename, sizeof(filename), "%s%c%s", dir, sep, direntp->d_name);
		if ((lcp->l_tifp = fopen(filename, "r")) == NULL) {
		    sts = -oserror();
		    PM_UNLOCK(__pmLock_libpcp);
		    goto cleanup;
		}
	    }
	    else if (strcmp(tp, "meta") == 0) {
		exists = 1;
		snprintf(filename, sizeof(filename), "%s%c%s", dir, sep, direntp->d_name);
		if ((lcp->l_mdfp = fopen(filename, "r")) == NULL) {
		    sts = -oserror();
		    PM_UNLOCK(__pmLock_libpcp);
		    goto cleanup;
		}
	    }
	    else {
		char	*q;
		int	vol;
		vol = (int)strtol(tp, &q, 10);
		if (*q != '\0') {
		    /* may have one of the trailing compressed file suffixes */
		    int		i;
		    for (i = 0; i < ncompress; i++) {
			if (strcmp(q, compress_ctl[i].suff) == 0) {
			    /* match */
			    *q = '\0';
			    break;
			}
		    }
		}
		if (*q == '\0') {
		    exists = 1;
		    if (lcp->l_minvol == -1) {
			lcp->l_minvol = vol;
			lcp->l_maxvol = vol;
		    }
		    else {
			if (vol < lcp->l_minvol)
			    lcp->l_minvol = vol;
			if (vol > lcp->l_maxvol)
			    lcp->l_maxvol = vol;
		    }
		}
	    }
	}
	closedir(dirp);
	dirp = NULL;
    }
    else {
#ifdef PCP_DEBUG
	sts = -oserror();
	if (pmDebug & DBG_TRACE_LOG) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogOpen: cannot scan directory \"%s\": %s\n", dir, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
	PM_UNLOCK(__pmLock_libpcp);
	goto cleanup;
	
#endif
    }
    PM_UNLOCK(__pmLock_libpcp);

    if (lcp->l_minvol == -1 || lcp->l_mdfp == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    if (lcp->l_minvol == -1)
		fprintf(stderr, "__pmLogOpen: Not found: data file \"%s.0\" (or similar)\n", base);
	    if (lcp->l_mdfp == NULL)
		fprintf(stderr, "__pmLogOpen: Not found: metadata file \"%s.meta\"\n", base);
	}
#endif
	if (exists)
	    sts = PM_ERR_LOGFILE;
	else
	    sts = -ENOENT;
	goto cleanup;
    }
    free(tbuf);
    free(base);
    return 0;

cleanup:
    if (dirp != NULL)
	closedir(dirp);
    __pmLogClose(lcp);
    free(tbuf);
    free(base);
    return sts;
}

int
__pmLogOpen(const char *name, __pmContext *ctxp)
{
    __pmLogCtl	*lcp = ctxp->c_archctl->ac_log;
    __pmLogLabel label;
    int		version;
    int		sts;

    if ((sts = __pmLogLoadLabel(lcp, name)) < 0)
	return sts;

    lcp->l_curvol = -1;
    if ((sts = __pmLogChangeVol(lcp, lcp->l_minvol)) < 0)
	goto cleanup;
    else
	version = sts;

    ctxp->c_origin = lcp->l_label.ill_start;

    if (lcp->l_tifp) {
	sts = __pmLogChkLabel(lcp, lcp->l_tifp, &label, PM_LOG_VOL_TI);
	if (sts < 0)
	    goto cleanup;
	else if (sts != version) {
	    /* mismatch between meta & actual data versions! */
	    sts = PM_ERR_LABEL;
	    goto cleanup;
	}

	if (lcp->l_label.ill_pid != label.ill_pid ||
		strcmp(lcp->l_label.ill_hostname, label.ill_hostname) != 0) {
	    sts = PM_ERR_LABEL;
	    goto cleanup;
	}
    }

    if ((sts = __pmLogChkLabel(lcp, lcp->l_mdfp, &label, PM_LOG_VOL_META)) < 0)
	goto cleanup;
    else if (sts != version) {	/* version mismatch between meta & ti */
	sts = PM_ERR_LABEL;
	goto cleanup;
    }

    if ((sts = __pmLogLoadMeta(lcp)) < 0)
	goto cleanup;

    if ((sts = __pmLogLoadIndex(lcp)) < 0)
	goto cleanup;

    if (lcp->l_label.ill_pid != label.ill_pid ||
	strcmp(lcp->l_label.ill_hostname, label.ill_hostname) != 0) {
	    sts = PM_ERR_LABEL;
	    goto cleanup;
    }
    
    lcp->l_refcnt = 0;
    lcp->l_physend = -1;

    ctxp->c_mode = (ctxp->c_mode & 0xffff0000) | PM_MODE_FORW;

    return 0;

cleanup:
    __pmLogClose(lcp);
    return sts;
}

void
__pmLogPutIndex(const __pmLogCtl *lcp, const __pmTimeval *tp)
{
    __pmLogTI	ti;
    __pmLogTI	oti;
    int		sts;

    if (lcp->l_tifp == NULL || lcp->l_mdfp == NULL || lcp->l_mfp == NULL) {
	/*
	 * archive not really created (failed in __pmLogCreate) ...
	 * nothing to be done
	 */
	return;
    }

    if (tp == NULL) {
	struct timeval	tmp;

	__pmtimevalNow(&tmp);
	ti.ti_stamp.tv_sec = (__int32_t)tmp.tv_sec;
	ti.ti_stamp.tv_usec = (__int32_t)tmp.tv_usec;
    }
    else
	ti.ti_stamp = *tp;		/* struct assignment */
    ti.ti_vol = lcp->l_curvol;
    fflush(lcp->l_mdfp);
    fflush(lcp->l_mfp);

    if (sizeof(off_t) > sizeof(__pm_off_t)) {
	/* check for overflow of the offset ... */
	off_t	tmp;

	tmp = ftell(lcp->l_mdfp);
	assert(tmp >= 0);
	ti.ti_meta = (__pm_off_t)tmp;
	if (tmp != ti.ti_meta) {
	    __pmNotifyErr(LOG_ERR, "__pmLogPutIndex: PCP archive file (meta) too big\n");
	    return;
	}
	tmp = ftell(lcp->l_mfp);
	assert(tmp >= 0);
	ti.ti_log = (__pm_off_t)tmp;
	if (tmp != ti.ti_log) {
	    __pmNotifyErr(LOG_ERR, "__pmLogPutIndex: PCP archive file (data) too big\n");
	    return;
	}
    }
    else {
	ti.ti_meta = (__pm_off_t)ftell(lcp->l_mdfp);
	ti.ti_log = (__pm_off_t)ftell(lcp->l_mfp);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "__pmLogPutIndex: timestamp=%d.06%d vol=%d meta posn=%ld log posn=%ld\n",
	    (int)ti.ti_stamp.tv_sec, (int)ti.ti_stamp.tv_usec,
	    ti.ti_vol, (long)ti.ti_meta, (long)ti.ti_log);
    }
#endif

    oti.ti_stamp.tv_sec = htonl(ti.ti_stamp.tv_sec);
    oti.ti_stamp.tv_usec = htonl(ti.ti_stamp.tv_usec);
    oti.ti_vol = htonl(ti.ti_vol);
    oti.ti_meta = htonl(ti.ti_meta);
    oti.ti_log = htonl(ti.ti_log);
    if ((sts = fwrite(&oti, 1, sizeof(oti), lcp->l_tifp)) != sizeof(oti)) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogPutIndex: write failed: returns %d expecting %d: %s\n",
	    sts, (int)sizeof(oti), osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
    }
    if (fflush(lcp->l_tifp) != 0)
	__pmNotifyErr(LOG_ERR, "__pmLogPutIndex: PCP archive temporal index flush failed\n");
}

static int
logputresult(int version,__pmLogCtl *lcp, __pmPDU *pb)
{
    /*
     * This is a bit tricky ...
     *
     *  Input
     *  :---------:----------:----------:---------------- .........:---------:
     *  | int len | int type | int from | timestamp, .... pmResult | unused  |
     *  :---------:----------:----------:---------------- .........:---------:
     *  ^
     *  |
     *  pb
     *
     *  Output
     *  :---------:----------:----------:---------------- .........:---------:
     *  | unused  | unused   | int len  | timestamp, .... pmResult | int len |
     *  :---------:----------:----------:---------------- .........:---------:
     *                       ^
     *                       |
     *                       start
     *
     * If version == 1, pb[] does not have room for trailer len.
     * If version == 2, pb[] does have room for trailer len.
     */
    int			sz;
    int			sts = 0;
    int			save_from;
    __pmPDU		*start = &pb[2];

    if (lcp->l_state == PM_LOG_STATE_NEW) {
	int		i;
	__pmTimeval	*tvp;
	/*
	 * first result, do the label record
	 */
	i = sizeof(__pmPDUHdr) / sizeof(__pmPDU);
	tvp = (__pmTimeval *)&pb[i];
	lcp->l_label.ill_start.tv_sec = ntohl(tvp->tv_sec);
	lcp->l_label.ill_start.tv_usec = ntohl(tvp->tv_usec);
	lcp->l_label.ill_vol = PM_LOG_VOL_TI;
	__pmLogWriteLabel(lcp->l_tifp, &lcp->l_label);
	lcp->l_label.ill_vol = PM_LOG_VOL_META;
	__pmLogWriteLabel(lcp->l_mdfp, &lcp->l_label);
	lcp->l_label.ill_vol = 0;
	__pmLogWriteLabel(lcp->l_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
    }

    sz = pb[0] - (int)sizeof(__pmPDUHdr) + 2 * (int)sizeof(int);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "logputresult: pdubuf=" PRINTF_P_PFX "%p input len=%d output len=%d posn=%ld\n", pb, pb[0], sz, (long)ftell(lcp->l_mfp));
    }
#endif

    save_from = start[0];
    start[0] = htonl(sz);	/* swab */

    if (version == 1) {
	if ((sts = fwrite(start, 1, sz-sizeof(int), lcp->l_mfp)) != sz-sizeof(int)) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("__pmLogPutResult: write failed: returns %d expecting %d: %s\n",
		sts, (int)(sz-sizeof(int)), osstrerror_r(errmsg, sizeof(errmsg)));
	    pmflush();
	    sts = -oserror();
	}
	else {
	    if ((sts = fwrite(start, 1, sizeof(int), lcp->l_mfp)) != sizeof(int)) {
		char	errmsg[PM_MAXERRMSGLEN];
		pmprintf("__pmLogPutResult: trailer write failed: returns %d expecting %d: %s\n",
		    sts, (int)sizeof(int), osstrerror_r(errmsg, sizeof(errmsg)));
		pmflush();
		sts = -oserror();
	    }
	}
    }
    else {
	/* assume version == 2 */
	start[(sz-1)/sizeof(__pmPDU)] = start[0];
	if ((sts = fwrite(start, 1, sz, lcp->l_mfp)) != sz) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("__pmLogPutResult2: write failed: returns %d expecting %d: %s\n",
	    	sts, sz, osstrerror_r(errmsg, sizeof(errmsg)));
	    pmflush();
	    sts = -oserror();
	}
    }

    /* restore and unswab */
    start[0] = save_from;

    return sts;
}

/*
 * original routine, pb[] does not have room for trailer, so 2 writes
 * needed
 */
int
__pmLogPutResult(__pmLogCtl *lcp, __pmPDU *pb)
{
    return logputresult(1, lcp, pb);
}

/*
 * new routine, pb[] does have room for trailer, so only 1 write
 * needed
 */
int
__pmLogPutResult2(__pmLogCtl *lcp, __pmPDU *pb)
{
    return logputresult(2, lcp, pb);
}

/*
 * check if PDU buffer seems even half-way reasonable ...
 * only used when trying to locate end of archive.
 * return 0 for OK, -1 for bad.
 */
static int
paranoidCheck(int len, __pmPDU *pb)
{
    int			numpmid;
    size_t		hdrsz;		/* bytes for the PDU head+tail */
    int			i;
    int			j;
    int			vsize;		/* size of vlist_t's in PDU buffer */
    int			vbsize;		/* size of pmValueBlocks */
    int			numval;		/* number of values */
    int			valfmt;

    struct result_t {			/* from p_result.c */
	__pmPDUHdr		hdr;
	__pmTimeval		timestamp;	/* when returned */
	int			numpmid;	/* no. of PMIDs to follow */
	__pmPDU			data[1];	/* zero or more */
    }			*pp;
    struct vlist_t {			/* from p_result.c */
	pmID			pmid;
	int			numval;		/* no. of vlist els to follow, or error */
	int			valfmt;		/* insitu or pointer */
	__pmValue_PDU		vlist[1];	/* zero or more */
    }			*vlp;

    /*
     * to start with, need space for result_t with no data (__pmPDU)
     * ... this is the external size, which consists of
     * <header len>
     * <timestamp> (2 words)
     * <numpmid>
     * <trailer len>
     *
     * it is confusing because *pb and result_t include the fake
     * __pmPDUHdr which is not really in the external file
     */
    hdrsz = 5 * sizeof(__pmPDU);

    if (len < hdrsz) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    fprintf(stderr, "\nparanoidCheck: len=%d, min len=%d\n",
		len, (int)hdrsz);
	    dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
	}
#endif
	return -1;
    }

    pp = (struct result_t *)pb;
    numpmid = ntohl(pp->numpmid);

    /*
     * This is a re-implementation of much of __pmDecodeResult()
     */

    if (numpmid < 1) {
	if (len != hdrsz) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, "\nparanoidCheck: numpmid=%d len=%d, expected len=%d\n",
		    numpmid, len, (int)hdrsz);
		dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
	    }
#endif
	    return -1;
	}
    }

    /*
     * Calculate vsize and vbsize from the original PDU buffer ...
     * :---------:-----------:----------------:--------------------:
     * : numpmid : timestamp : ... vlists ... : .. pmValueBocks .. :
     * :---------:-----------:----------------:--------------------:
     *                        <---  vsize ---> <---   vbsize   --->
     *                              bytes             bytes
     */

    vsize = vbsize = 0;
    for (i = 0; i < numpmid; i++) {
	vlp = (struct vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	if (len < hdrsz + vsize + vbsize) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, "\nparanoidCheck: vset[%d] len=%d, need len>=%d (%d+%d+%d)\n",
		    i, len, (int)(hdrsz + vsize + vbsize), (int)hdrsz, vsize, vbsize);
		dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
	    }
#endif
	    return -1;
	}
	numval = ntohl(vlp->numval);
	if (numval > 0) {
#ifdef DESPERATE
	    pmID		pmid;
#endif
	    valfmt = ntohl(vlp->valfmt);
	    if (valfmt != PM_VAL_INSITU &&
		valfmt != PM_VAL_DPTR &&
		valfmt != PM_VAL_SPTR) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, "\nparanoidCheck: vset[%d] bad valfmt=%d\n",
			i, valfmt);
		    dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
		}
#endif
		return -1;
	    }
#ifdef DESPERATE
	    {
		char	strbuf[20];
		if (i == 0) fputc('\n', stderr);
		pmid = __ntohpmID(vlp->pmid);
		fprintf(stderr, "vlist[%d] pmid: %s numval: %d valfmt: %d\n",
		    i, pmIDStr_r(pmid, strbuf, sizeof(strbuf)), numval, valfmt);
	    }
#endif
	    vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
	    if (valfmt != PM_VAL_INSITU) {
		for (j = 0; j < numval; j++) {
		    int			index = (int)ntohl((long)vlp->vlist[j].value.pval);
		    pmValueBlock	*pduvbp;
		    int			vlen;
		    
		    if (index < 0 || index * sizeof(__pmPDU) > len) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOG) {
			    fprintf(stderr, "\nparanoidCheck: vset[%d] val[%d], bad pval index=%d not in range 0..%d\n",
				i, j, index, (int)(len / sizeof(__pmPDU)));
			    dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
			}
#endif
			return -1;
		    }
		    pduvbp = (pmValueBlock *)&pb[index];
		    __ntohpmValueBlock(pduvbp);
		    vlen = pduvbp->vlen;
		    __htonpmValueBlock(pduvbp);		/* restore pdubuf! */
		    if (vlen < sizeof(__pmPDU)) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOG) {
			    fprintf(stderr, "\nparanoidCheck: vset[%d] val[%d], bad vlen=%d\n",
				i, j, vlen);
			    dumpbuf(len, &pb[3]); /* skip first 3 words, start @ timestamp */
			}
#endif
			return -1;
		    }
		    vbsize += PM_PDU_SIZE_BYTES(vlen);
		}
	    }
	}
    }

    return 0;
}

static int
paranoidLogRead(__pmLogCtl *lcp, int mode, FILE *peekf, pmResult **result)
{
    return __pmLogRead(lcp, mode, peekf, result, PMLOGREAD_TO_EOF);
}

/*
 * read next forward or backward from the log
 *
 * by default (peekf == NULL) use lcp->l_mfp and roll volume
 * at end of file if another volume is available
 *
 * if peekf != NULL, use this stream, and do not roll volume
 */
int
__pmLogRead(__pmLogCtl *lcp, int mode, FILE *peekf, pmResult **result, int option)
{
    int		head;
    int		rlen;
    int		trail;
    int		sts;
    long	offset;
    __pmPDU	*pb;
    FILE	*f;
    int		n;

    /*
     * Strip any XTB data from mode, its not used here
     */
    mode &= __PM_MODE_MASK;

    if (peekf != NULL)
	f = peekf;
    else
	f = lcp->l_mfp;

    offset = ftell(f);
    assert(offset >= 0);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "__pmLogRead: fd=%d%s mode=%s vol=%d posn=%ld ",
	    fileno(f), peekf == NULL ? "" : " (peek)",
	    mode == PM_MODE_FORW ? "forw" : "back",
	    lcp->l_curvol, (long)offset);
    }
#endif

    if (mode == PM_MODE_BACK) {
       for ( ; ; ) {
	   if (offset <= sizeof(__pmLogLabel) + 2 * sizeof(int)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "BEFORE start\n");
#endif
		if (peekf == NULL) {
		    int		vol = lcp->l_curvol-1;
		    while (vol >= lcp->l_minvol) {
			if (__pmLogChangeVol(lcp, vol) >= 0) {
			    f = lcp->l_mfp;
			    fseek(f, 0L, SEEK_END);
			    offset = ftell(f);
			    assert(offset >= 0);
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_LOG) {
				fprintf(stderr, "vol=%d posn=%ld ",
				    lcp->l_curvol, (long)offset);
			    }
#endif
			    break;
			}
			vol--;
		    }
		    if (vol < lcp->l_minvol)
			return PM_ERR_EOL;
		}
		else
		    return PM_ERR_EOL;
	    }
	    else {
		fseek(f, -(long)sizeof(head), SEEK_CUR);
		break;
	    }
	}
    }

again:
    n = (int)fread(&head, 1, sizeof(head), f);
    head = ntohl(head); /* swab head */
    if (n != sizeof(head)) {
	if (feof(f)) {
	    /* no more data ... looks like End of Archive volume */
	    clearerr(f);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, "AFTER end\n");
#endif
	    fseek(f, offset, SEEK_SET);
	    if (peekf == NULL) {
		int	vol = lcp->l_curvol+1;
		while (vol <= lcp->l_maxvol) {
		    if (__pmLogChangeVol(lcp, vol) >= 0) {
			f = lcp->l_mfp;
			goto again;
		    }
		    vol++;
		}
	    }
	    return PM_ERR_EOL;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: header fread got %d expected %d\n", n, (int)sizeof(head));
#endif
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -oserror();
	}
	else
	    /* corrupted archive */
	    return PM_ERR_LOGREC;
    }

    /*
     * This is pretty ugly (forward case shown backwards is similar) ...
     *
     *  Input
     *                         head    <--- rlen bytes -- ...--->   tail
     *  :---------:---------:---------:---------------- .........:---------:
     *  |   ???   |   ???   | int len | timestamp, .... pmResult | int len |
     *  :---------:---------:---------:---------------- .........:---------:
     *  ^                             ^
     *  |                             |
     *  pb                            read into here
     *
     *  Decode
     *  <----  __pmPDUHdr  ----------->
     *  :---------:---------:---------:---------------- .........:---------:
     *  | length  | pdutype |  anon   | timestamp, .... pmResult | int len |
     *  :---------:---------:---------:---------------- .........:---------:
     *  ^
     *  |
     *  pb
     *
     * Note: cannot volume switch in the middle of a log record
     */

    rlen = head - 2 * (int)sizeof(head);
    if (rlen < 0 || (mode == PM_MODE_BACK && rlen > offset)) {
	/*
	 * corrupted! usually means a truncated log ...
	 */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: truncated log? rlen=%d (offset %d)\n",
		rlen, (int)offset);
#endif
	    return PM_ERR_LOGREC;
    }
    /*
     * need to add int at end for trailer in case buffer is used
     * subsequently by __pmLogPutResult2()
     */
    if ((pb = __pmFindPDUBuf(rlen + (int)sizeof(__pmPDUHdr) + (int)sizeof(int))) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "\nError: __pmFindPDUBuf(%d) %s\n",
		(int)(rlen + sizeof(__pmPDUHdr)),
		osstrerror_r(errmsg, sizeof(errmsg)));
	}
#endif
	fseek(f, offset, SEEK_SET);
	return -oserror();
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)(sizeof(head) + rlen), SEEK_CUR);

    if ((n = (int)fread(&pb[3], 1, rlen, f)) != rlen) {
	/* data read failed */
	__pmUnpinPDUBuf(pb);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: data fread got %d expected %d\n", n, rlen);
#endif
	fseek(f, offset, SEEK_SET);
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -oserror();
	}
	clearerr(f);

	/* corrupted archive */
	return PM_ERR_LOGREC;
    }
    else {
	__pmPDUHdr *header = (__pmPDUHdr *)pb;
	header->len = sizeof(*header) + rlen;
	header->type = PDU_RESULT;
	header->from = FROM_ANON;
	/* swab pdu buffer - done later in __pmDecodeResult */

#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    int	j;
	    char	*p;
	    int	jend = PM_PDU_SIZE(header->len);

	    /* clear the padding bytes, lest they contain garbage */
	    p = (char *)pb + header->len;
	    while (p < (char *)pb + jend*sizeof(__pmPDU))
		*p++ = '~';	/* buffer end */

	    fprintf(stderr, "__pmLogRead: PDU buffer\n");
	    for (j = 0; j < jend; j++) {
		if ((j % 8) == 0 && j > 0)
		    fprintf(stderr, "\n%03d: ", j);
		fprintf(stderr, "%8x ", pb[j]);
	    }
	    putc('\n', stderr);
	}
#endif
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)(rlen + sizeof(head)), SEEK_CUR);

    if ((n = (int)fread(&trail, 1, sizeof(trail), f)) != sizeof(trail)) {
	__pmUnpinPDUBuf(pb);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: trailer fread got %d expected %d\n", n, (int)sizeof(trail));
#endif
	fseek(f, offset, SEEK_SET);
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -oserror();
	}
	clearerr(f);

	/* corrupted archive */
	return PM_ERR_LOGREC;
    }
    else {
	/* swab trail */
	trail = ntohl(trail);
    }

    if (trail != head) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: record length mismatch: header (%d) != trailer (%d)\n", head, trail);
#endif
	__pmUnpinPDUBuf(pb);
	return PM_ERR_LOGREC;
    }

    if (option == PMLOGREAD_TO_EOF && paranoidCheck(head, pb) == -1) {
	__pmUnpinPDUBuf(pb);
	return PM_ERR_LOGREC;
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)sizeof(trail), SEEK_CUR);

    __pmOverrideLastFd(fileno(f));
    sts = __pmDecodeResult(pb, result); /* also swabs the result */

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	head -= sizeof(head) + sizeof(trail);
	if (sts >= 0) {
	    __pmTimeval	tmp;
	    fprintf(stderr, "@");
	    __pmPrintStamp(stderr, &(*result)->timestamp);
	    tmp.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    fprintf(stderr, " (t=%.6f)", __pmTimevalSub(&tmp, &lcp->l_label.ill_start));
	}
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogRead: __pmDecodeResult failed: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    fprintf(stderr, "@unknown time");
	}
	fprintf(stderr, " len=header+%d+trailer\n", head);
    }
#endif

    /* exported to indicate how efficient we are ... */
    __pmLogReads++;

    if (sts < 0) {
	__pmUnpinPDUBuf(pb);
	return PM_ERR_LOGREC;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	fprintf(stderr, "__pmLogRead timestamp=");
	__pmPrintStamp(stderr, &(*result)->timestamp);
	fprintf(stderr, " " PRINTF_P_PFX "%p ... " PRINTF_P_PFX "%p", &pb[3], &pb[head/sizeof(__pmPDU)+3]);
	fputc('\n', stderr);
	dumpbuf(rlen, &pb[3]);		/* see above to explain "3" */
    }
#endif

    __pmUnpinPDUBuf(pb);

    return 0;
}

static int
check_all_derived(int numpmid, pmID pmidlist[])
{
    int	i;

    /*
     * Special case ... if we ONLY have derived metrics in the input
     * pmidlist then all the derived metrics must be constant
     * expressions, so skip all the processing.
     * Derived metrics have domain == DYNAMIC_PMID and item != 0.
     * This rare, but avoids reading to the end of an archive
     * for no good reason.
     */

    for (i = 0; i < numpmid; i++) {
	if (pmid_domain(pmidlist[i]) != DYNAMIC_PMID ||
	    pmid_item(pmidlist[i]) == 0)
	    return 0;
    }
    return 1;
}

int
__pmLogFetch(__pmContext *ctxp, int numpmid, pmID pmidlist[], pmResult **result)
{
    int		i;
    int		j;
    int		u;
    int		all_derived;
    int		sts = 0;
    int		found;
    double	tdiff;
    pmResult	*newres;
    pmDesc	desc;
    int		kval;
    __pmHashNode	*hp;
    pmid_ctl	*pcp;
    int		nskip;
    __pmTimeval	tmp;
    int		ctxp_mode = ctxp->c_mode & __PM_MODE_MASK;

    if (ctxp_mode == PM_MODE_INTERP) {
	return __pmLogFetchInterp(ctxp, numpmid, pmidlist, result);
    }

    all_derived = check_all_derived(numpmid, pmidlist);

    /* re-establish position */
    __pmLogChangeVol(ctxp->c_archctl->ac_log, ctxp->c_archctl->ac_vol);
    fseek(ctxp->c_archctl->ac_log->l_mfp, 
	    (long)ctxp->c_archctl->ac_offset, SEEK_SET);

more:

    found = 0;
    nskip = 0;
    *result = NULL;
    while (!found) {
	if (ctxp->c_archctl->ac_serial == 0) {
	    /*
	     * no serial access, so need to make sure we are
	     * starting in the correct place
	     */
	    int		tmp_mode;
	    nskip = 0;
	    if (ctxp_mode == PM_MODE_FORW)
		tmp_mode = PM_MODE_BACK;
	    else
		tmp_mode = PM_MODE_FORW;
	    while (__pmLogRead(ctxp->c_archctl->ac_log, tmp_mode, NULL, result, PMLOGREAD_NEXT) >= 0) {
		nskip++;
		tmp.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		tmp.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
		tdiff = __pmTimevalSub(&tmp, &ctxp->c_origin);
		if ((tdiff < 0 && ctxp_mode == PM_MODE_FORW) ||
		    (tdiff > 0 && ctxp_mode == PM_MODE_BACK)) {
		    pmFreeResult(*result);
		    *result = NULL;
		    break;
		}
		else if (tdiff == 0) {
		    /* exactly the one we wanted */
		    found = 1;
		    break;
		}
		pmFreeResult(*result);
		*result = NULL;
	    }
	    ctxp->c_archctl->ac_serial = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		if (nskip) {
		    fprintf(stderr, "__pmLogFetch: ctx=%d skip reverse %d to ",
			pmWhichContext(), nskip);
		    if (*result  != NULL)
			__pmPrintStamp(stderr, &(*result)->timestamp);
		    else
			fprintf(stderr, "unknown time");
		    fprintf(stderr, ", found=%d\n", found);
		}
#ifdef DESPERATE
		else
		    fprintf(stderr, "__pmLogFetch: ctx=%d no skip reverse\n",
			pmWhichContext());
#endif
	    }
#endif
	    nskip = 0;
	}
	if (found)
	    break;
	if ((sts = __pmLogRead(ctxp->c_archctl->ac_log, ctxp->c_mode, NULL, result, PMLOGREAD_NEXT)) < 0)
	    break;
	tmp.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
	tmp.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	tdiff = __pmTimevalSub(&tmp, &ctxp->c_origin);
	if ((tdiff < 0 && ctxp_mode == PM_MODE_FORW) ||
	    (tdiff > 0 && ctxp_mode == PM_MODE_BACK)) {
		nskip++;
		pmFreeResult(*result);
		*result = NULL;
		continue;
	}
	found = 1;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    if (nskip) {
		fprintf(stderr, "__pmLogFetch: ctx=%d skip %d to ",
		    pmWhichContext(), nskip);
		    __pmPrintStamp(stderr, &(*result)->timestamp);
		    fputc('\n', stderr);
		}
#ifdef DESPERATE
	    else
		fprintf(stderr, "__pmLogFetch: ctx=%d no skip\n",
		    pmWhichContext());
#endif
	}
#endif
    }
    if (found) {
	ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
	ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
    }

    if (*result != NULL && (*result)->numpmid == 0) {
	/*
	 * mark record, and not interpolating ...
	 * if pmFetchArchive(), return it
	 * otherwise keep searching
	 */
	if (numpmid == 0)
	    newres = *result;
	else {
	    pmFreeResult(*result);
	    goto more;
	}
    }
    else if (found) {
	if (numpmid > 0) {
	    /*
	     * not necesssarily after them all, so cherry-pick the metrics
	     * we wanted ..
	     * there are two tricks here ...
	     * (1) pmValueSets for metrics requested, but not in the pmResult
	     *     from the log are assigned using the first two fields in the
	     *     pmid_ctl struct -- since these are allocated once as
	     *	   needed, and never free'd, we have to make sure pmFreeResult
	     *     finds a pmValueSet in a pinned pdu buffer ... this means
	     *     we must find at least one real value from the log to go
	     *     with any "unavailable" results
	     * (2) real pmValueSets can be ignored, they are in a pdubuf
	     *     and will be reclaimed when the buffer is unpinned in
	     *     pmFreeResult
	     */

	    i = (int)sizeof(pmResult) + numpmid * (int)sizeof(pmValueSet *);
	    if ((newres = (pmResult *)malloc(i)) == NULL) {
		__pmNoMem("__pmLogFetch.newres", i, PM_FATAL_ERR);
	    }
	    newres->numpmid = numpmid;
	    newres->timestamp = (*result)->timestamp;
	    u = 0;
	    PM_INIT_LOCKS();
	    PM_LOCK(__pmLock_libpcp);
	    for (j = 0; j < numpmid; j++) {
		hp = __pmHashSearch((int)pmidlist[j], &pc_hc);
		if (hp == NULL) {
		    /* first time we've been asked for this one */
		    if ((pcp = (pmid_ctl *)malloc(sizeof(pmid_ctl))) == NULL) {
			__pmNoMem("__pmLogFetch.pmid_ctl", sizeof(pmid_ctl), PM_FATAL_ERR);
		    }
		    pcp->pc_pmid = pmidlist[j];
		    pcp->pc_numval = 0;
		    sts = __pmHashAdd((int)pmidlist[j], (void *)pcp, &pc_hc);
		    if (sts < 0) {
			PM_UNLOCK(__pmLock_libpcp);
			return sts;
		    }
		}
		else
		    pcp = (pmid_ctl *)hp->data;
		for (i = 0; i < (*result)->numpmid; i++) {
		    if (pmidlist[j] == (*result)->vset[i]->pmid) {
			/* match */
			newres->vset[j] = (*result)->vset[i];
			u++;
			break;
		    }
		}
		if (i == (*result)->numpmid) {
		    /*
		     * requested metric not returned from the log, construct
		     * a "no values available" pmValueSet from the pmid_ctl
		     */
		    newres->vset[j] = (pmValueSet *)pcp;
		}
	    }
	    PM_UNLOCK(__pmLock_libpcp);
	    if (u == 0 && !all_derived) {
		/*
		 * not one of our pmids was in the log record, try
		 * another log record ...
		 */
		pmFreeResult(*result);
		free(newres);
		goto more;
	    }
	    /*
	     * *result malloc'd in __pmLogRead, but vset[]'s are either in
	     * pdubuf or the pmid_ctl struct
	     */
	    free(*result);
	    *result = newres;
	}
	else
	    /* numpmid == 0, pmFetchArchive() call */
	    newres = *result;
	/*
	 * Apply instance profile filtering ...
	 * Note. This is a little strange, as in the numpmid == 0,
	 *       pmFetchArchive() case, this for-loop is not executed ...
	 *       this is correct, the instance profile is ignored for
	 *       pmFetchArchive()
	 */
	for (i = 0; i < numpmid; i++) {
	    if (newres->vset[i]->numval <= 0) {
		/*
		 * no need to xlate numval for an error ... already done
		 * below __pmLogRead() in __pmDecodeResult() ... also xlate
		 * here would have been skipped in the pmFetchArchive() case
		 */
		continue;
	    }
	    sts = __pmLogLookupDesc(ctxp->c_archctl->ac_log, newres->vset[i]->pmid, &desc);
	    if (sts < 0) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		__pmNotifyErr(LOG_WARNING, "__pmLogFetch: missing pmDesc for pmID %s: %s",
			    pmIDStr_r(desc.pmid, strbuf, sizeof(strbuf)), pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		pmFreeResult(newres);
		break;
	    }
	    if (desc.indom == PM_INDOM_NULL)
		/* no instance filtering to be done for these ones */
		continue;

	    /*
	     * scan instances, keeping those "in" the instance profile
	     *
	     * WARNING
	     *		This compresses the pmValueSet INSITU, and since
	     *		these are in a pdu buffer, it trashes the the
	     *		pdu buffer and means there is no clever way of
	     *		re-using the pdu buffer to satisfy multiple
	     *		pmFetch requests
	     *		Fortunately, stdio buffering means copying to
	     *		make additional pdu buffers is not too expensive.
	     */
	    kval = 0;
	    for (j = 0; j < newres->vset[i]->numval; j++) {
		if (__pmInProfile(desc.indom, ctxp->c_instprof, newres->vset[i]->vlist[j].inst)) {
		    if (kval != j)
			 /* struct assignment */
			 newres->vset[i]->vlist[kval] = newres->vset[i]->vlist[j];
		    kval++;
		}
	    }
	    newres->vset[i]->numval = kval;
	}
    }

    /* remember your position in this context */
    ctxp->c_archctl->ac_offset = ftell(ctxp->c_archctl->ac_log->l_mfp);
    assert(ctxp->c_archctl->ac_offset >= 0);
    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;

    return sts;
}

/*
 * error handling wrapper around __pmLogChangeVol() to deal with
 * missing volumes ... return lcp->l_ti[] index for entry matching
 * success
 */
static int
VolSkip(__pmLogCtl *lcp, int mode,  int j)
{
    int		vol = lcp->l_ti[j].ti_vol;

    while (lcp->l_minvol <= vol && vol <= lcp->l_maxvol) {
	if (__pmLogChangeVol(lcp, vol) >= 0)
	    return j;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    fprintf(stderr, "VolSkip: Skip missing vol %d\n", vol);
	}
#endif
	if (mode == PM_MODE_FORW) {
	    for (j++; j < lcp->l_numti; j++)
		if (lcp->l_ti[j].ti_vol != vol)
		    break;
	    if (j == lcp->l_numti)
		return PM_ERR_EOL;
	    vol = lcp->l_ti[j].ti_vol;
	}
	else {
	    for (j--; j >= 0; j--)
		if (lcp->l_ti[j].ti_vol != vol)
		    break;
	    if (j < 0)
		return PM_ERR_EOL;
	    vol = lcp->l_ti[j].ti_vol;
	}
    }
    return PM_ERR_EOL;
}

void
__pmLogSetTime(__pmContext *ctxp)
{
    __pmLogCtl	*lcp = ctxp->c_archctl->ac_log;
    int		mode;

    mode = ctxp->c_mode & __PM_MODE_MASK; /* strip XTB data */

    if (mode == PM_MODE_INTERP)
	mode = ctxp->c_delta > 0 ? PM_MODE_FORW : PM_MODE_BACK;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "__pmLogSetTime(%d) ", pmWhichContext());
	__pmPrintTimeval(stderr, &ctxp->c_origin);
	fprintf(stderr, " delta=%d", ctxp->c_delta);
    }
#endif

    if (lcp->l_numti) {
	/* we have a temporal index, use it! */
	int		i;
	int		j = -1;
	int		toobig = 0;
	int		match = 0;
	int		vol;
	int		numti = lcp->l_numti;
	FILE		*f;
	__pmLogTI	*tip = lcp->l_ti;
	double		t_hi;
	double		t_lo;
	struct stat	sbuf;

	sbuf.st_size = -1;

	for (i = 0; i < numti; i++, tip++) {
	    if (tip->ti_vol < lcp->l_minvol)
		/* skip missing preliminary volumes */
		continue;
	    if (tip->ti_vol == lcp->l_maxvol) {
		/* truncated check for last volume */
		if (sbuf.st_size < 0) {
		    sbuf.st_size = 0;
		    vol = lcp->l_maxvol;
		    if (vol >= 0 && vol < lcp->l_numseen && lcp->l_seen[vol])
			fstat(fileno(lcp->l_mfp), &sbuf);
		    else if ((f = _logpeek(lcp, lcp->l_maxvol)) != NULL) {
			fstat(fileno(f), &sbuf);
			fclose(f);
		    }
		}
		if (tip->ti_log > sbuf.st_size) {
		    j = i;
		    toobig++;
		    break;
		}
	    }
	    t_hi = __pmTimevalSub(&tip->ti_stamp, &ctxp->c_origin);
	    if (t_hi > 0) {
		j = i;
		break;
	    }
	    else if (t_hi == 0) {
		j = i;
		match = 1;
		break;
	    }
	}
	if (i == numti)
	    j = numti;

	ctxp->c_archctl->ac_serial = 1;

	if (match) {
	    j = VolSkip(lcp, mode, j);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
	    if (mode == PM_MODE_BACK)
		ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " at ti[%d]@", j);
		__pmPrintTimeval(stderr, &lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else if (j < 1) {
	    j = VolSkip(lcp, PM_MODE_FORW, 0);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " before start ti@");
		__pmPrintTimeval(stderr, &lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else if (j == numti) {
	    j = VolSkip(lcp, PM_MODE_BACK, numti-1);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
	    if (mode == PM_MODE_BACK)
		ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " after end ti@");
		__pmPrintTimeval(stderr, &lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else {
	    /*
	     *    [j-1]             [origin]           [j]
	     *      <----- t_lo -------><----- t_hi ---->
	     *
	     * choose closest index point.  if toobig, [j] is not
	     * really valid (log truncated or incomplete)
	     */
	    t_hi = __pmTimevalSub(&lcp->l_ti[j].ti_stamp, &ctxp->c_origin);
	    t_lo = __pmTimevalSub(&ctxp->c_origin, &lcp->l_ti[j-1].ti_stamp);
	    if (t_hi <= t_lo && !toobig) {
		j = VolSkip(lcp, mode, j);
		if (j < 0)
		    return;
		fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
		if (mode == PM_MODE_FORW)
		    ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, " before ti[%d]@", j);
		    __pmPrintTimeval(stderr, &lcp->l_ti[j].ti_stamp);
		}
#endif
	    }
	    else {
		j = VolSkip(lcp, mode, j-1);
		if (j < 0)
		    return;
		fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
		if (mode == PM_MODE_BACK)
		    ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, " after ti[%d]@", j);
		    __pmPrintTimeval(stderr, &lcp->l_ti[j].ti_stamp);
		}
#endif
	    }
	    if (ctxp->c_archctl->ac_serial && mode == PM_MODE_FORW) {
		/*
		 * back up one record ...
		 * index points to the END of the record!
		 */
		pmResult	*result;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, " back up ...\n");
#endif
		if (__pmLogRead(lcp, PM_MODE_BACK, NULL, &result, PMLOGREAD_NEXT) >= 0)
		    pmFreeResult(result);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "...");
#endif
	    }
	}
    }
    else {
	/* index either not available, or not useful */
	if (mode == PM_MODE_FORW) {
	    __pmLogChangeVol(lcp, lcp->l_minvol);
	    fseek(lcp->l_mfp, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	}
	else if (mode == PM_MODE_BACK) {
	    __pmLogChangeVol(lcp, lcp->l_maxvol);
	    fseek(lcp->l_mfp, (long)0, SEEK_END);
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " index not useful\n");
#endif
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, " vol=%d posn=%ld serial=%d\n",
	    lcp->l_curvol, (long)ftell(lcp->l_mfp), ctxp->c_archctl->ac_serial);
#endif

    /* remember your position in this context */
    ctxp->c_archctl->ac_offset = ftell(lcp->l_mfp);
    assert(ctxp->c_archctl->ac_offset >= 0);
    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
}

int
pmGetArchiveLabel(pmLogLabel *lp)
{
    __pmContext		*ctxp;
    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL || ctxp->c_type != PM_CONTEXT_ARCHIVE)
	return PM_ERR_NOCONTEXT;
    else {
	__pmLogLabel	*rlp;
	/*
	 * we have to copy the structure to hide the differences
	 * between the internal __pmTimeval and the external struct timeval
	 */
	rlp = &ctxp->c_archctl->ac_log->l_label;
	lp->ll_magic = rlp->ill_magic;
	lp->ll_pid = (pid_t)rlp->ill_pid;
	lp->ll_start.tv_sec = rlp->ill_start.tv_sec;
	lp->ll_start.tv_usec = rlp->ill_start.tv_usec;
	memcpy(lp->ll_hostname, rlp->ill_hostname, PM_LOG_MAXHOSTLEN);
	memcpy(lp->ll_tz, rlp->ill_tz, sizeof(lp->ll_tz));
	PM_UNLOCK(ctxp->c_lock);
	return 0;
    }
}

int
pmGetArchiveEnd(struct timeval *tp)
{
    /*
     * set l_physend and l_endtime
     * at the end of ... ctxp->c_archctl->ac_log
     */
    __pmContext	*ctxp;
    int		sts;

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL || ctxp->c_type != PM_CONTEXT_ARCHIVE)
	return PM_ERR_NOCONTEXT;
    sts = __pmGetArchiveEnd(ctxp->c_archctl->ac_log, tp);
    PM_UNLOCK(ctxp->c_lock);
    return sts;
}

int
__pmGetArchiveEnd(__pmLogCtl *lcp, struct timeval *tp)
{
    struct stat	sbuf;
    FILE	*f;
    long	save = 0;
    pmResult	*rp = NULL;
    pmResult	*nrp;
    int		i;
    int		sts;
    int		found;
    int		head;
    long	offset;
    int		vol;
    __pm_off_t	logend;
    __pm_off_t	physend = 0;

    /*
     * default, when all else fails ...
     */
    tp->tv_sec = INT_MAX;
    tp->tv_usec = 0;

    /*
     * expect things to be stable, so l_maxvol is not empty, and
     * l_physend does not change for l_maxvol ... the ugliness is
     * to handle situations where these expectations are not met
     */
    found = 0;
    sts = PM_ERR_LOGREC;	/* default error condition */
    f = NULL;
    for (vol = lcp->l_maxvol; vol >= lcp->l_minvol; vol--) {
	if (lcp->l_curvol == vol) {
	    f = lcp->l_mfp;
	    save = ftell(f);
	    assert(save >= 0);
	}
	else if ((f = _logpeek(lcp, vol)) == NULL) {
	    /* failed to open this one, try previous volume(s) */
	    continue;
	}

	if (fstat(fileno(f), &sbuf) < 0) {
	    /* if we can't stat() this one, then try previous volume(s) */
	    fclose(f);
	    f = NULL;
	    continue;
	}

	if (vol == lcp->l_maxvol && sbuf.st_size == lcp->l_physend) {
	    /* nothing changed, return cached stuff */
	    tp->tv_sec = lcp->l_endtime.tv_sec;
	    tp->tv_usec = lcp->l_endtime.tv_usec;
	    sts = 0;
	    break;
	}

	/* if this volume is empty, try previous volume */
	if (sbuf.st_size <= (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int)) {
	    if (f != lcp->l_mfp) {
		fclose(f);
		f = NULL;
	    }
	    continue;
	}

	physend = (__pm_off_t)sbuf.st_size;
	if (sizeof(off_t) > sizeof(__pm_off_t)) {
	    if (physend != sbuf.st_size) {
		__pmNotifyErr(LOG_ERR, "pmGetArchiveEnd: PCP archive file"
			" (meta) too big (%"PRIi64" bytes)\n",
			(uint64_t)sbuf.st_size);
		sts = PM_ERR_TOOBIG;
		break;
	    }
	}

	/* try to read backwards for the last physical record ... */
	fseek(f, (long)physend, SEEK_SET);
	if (paranoidLogRead(lcp, PM_MODE_BACK, f, &rp) >= 0) {
	    /* success, we are done! */
	    found = 1;
	    break;
	}

	/*
	 * failure at the physical end of file may be related to a truncted
	 * block flush for a growing archive.  Scan temporal index, and use
	 * last entry at or before end of physical file for this volume
	 */
	logend = (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int);
	for (i = lcp->l_numti - 1; i >= 0; i--) {
	    if (lcp->l_ti[i].ti_vol != vol) {
		if (f != lcp->l_mfp) {
		    fclose(f);
		    f = NULL;
		}
		continue;
	    }
	    if (lcp->l_ti[i].ti_log <= physend) {
		logend = lcp->l_ti[i].ti_log;
		break;
	    }
	}

	/*
	 * Now chase it forwards from the last index entry ...
	 *
	 * BUG 357003 - pmchart can't read archive file
	 *	turns out the index may point to the _end_ of the last
	 *	valid record, so if not at start of volume, back up one
	 *	record, then scan forwards.
	 */
	fseek(f, (long)logend, SEEK_SET);
	if (logend > (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int)) {
	    if (paranoidLogRead(lcp, PM_MODE_BACK, f, &rp) < 0) {
		/* this is badly damaged! */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, "pmGetArchiveEnd: "
                            "Error reading record ending at posn=%d ti[%d]@",
			    logend, i);
		    __pmPrintTimeval(stderr, &lcp->l_ti[i].ti_stamp);
		    fputc('\n', stderr);
		}
#endif
		break;
	    }
	}

        /* Keep reading records from "logend" until can do so no more... */
	for ( ; ; ) {
	    offset = ftell(f);
	    assert(offset >= 0);
	    if ((int)fread(&head, 1, sizeof(head), f) != sizeof(head))
		/* cannot read header for log record !!?? */
		break;
	    head = ntohl(head);
	    if (offset + head > physend)
		/* last record is incomplete */
		break;
	    fseek(f, offset, SEEK_SET);
	    if (paranoidLogRead(lcp, PM_MODE_FORW, f, &nrp) < 0)
		/* this record is truncated, or bad, we lose! */
		break;
	    /* this one is ok, remember it as it may be the last one */
	    found = 1;
	    if (rp != NULL)
		pmFreeResult(rp);
	    rp = nrp;
	}
	if (found)
	    break;

	/*
	 * this probably means this volume contains no useful records,
	 * try the previous volume
	 */
    }/*for*/

    if (f == lcp->l_mfp)
	fseek(f, save, SEEK_SET); /* restore file pointer in current vol */ 
    else if (f != NULL)
	/* temporary FILE * from _logpeek() */
	fclose(f);

    if (found) {
	tp->tv_sec = (time_t)rp->timestamp.tv_sec;
	tp->tv_usec = (int)rp->timestamp.tv_usec;
	if (vol == lcp->l_maxvol) {
	    lcp->l_endtime.tv_sec = (__int32_t)rp->timestamp.tv_sec;
	    lcp->l_endtime.tv_usec = (__int32_t)rp->timestamp.tv_usec;
	    lcp->l_physend = physend;
	}
	sts = 0;
    }
    if (rp != NULL) {
	/*
	 * rp is not NULL from found==1 path _or_ from error break
	 * after an initial paranoidLogRead() success
	 */
	pmFreeResult(rp);
    }

    return sts;
}
