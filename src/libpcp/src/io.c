/*
 * Copyright (c) 2017 Red Hat.
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
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "config.h"
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

extern __pm_fops __pm_stdio;
#if HAVE_TRANSPARENT_DECOMPRESSION && HAVE_LZMA_DECOMPRESSION
extern __pm_fops __pm_xz;
#endif

/*
 * Suffixes and associated compresssion application for compressed filenames.
 * These can appear at the end of a variety of PCP file types for which
 * compressed files are supported.
 * e.g. /var/log/pmlogger/myhost/20101219.0.bz2
 */
#define	USE_NONE	0
#define	USE_BZIP2	1
#define USE_GZIP	2
#define USE_XZ		3

#if HAVE_TRANSPARENT_DECOMPRESSION && HAVE_LZMA_DECOMPRESSION
#define TRANSPARENT_XZ (&__pm_xz)
#else
#define TRANSPARENT_XZ NULL
#endif

static const struct {
    const char	*suff;
    const int	appl;
    __pm_fops   *handler;
} compress_ctl[] = {
    { ".xz",	USE_XZ,	 	TRANSPARENT_XZ },
    { ".lzma",	USE_XZ,		NULL },
    { ".bz2",	USE_BZIP2,	NULL },
    { ".bz",	USE_BZIP2,	NULL },
    { ".gz",	USE_GZIP,	NULL },
    { ".Z",	USE_GZIP,	NULL },
    { ".z",	USE_GZIP,	NULL },
};
static const int ncompress = sizeof(compress_ctl) / sizeof(compress_ctl[0]);

/*
 * If name contains '.' and the suffix is "index", "meta" or a string of
 * digits, all optionally followed by one of the compression suffixes,
 * strip the suffix.
 *
 * Modifications are performed on the argument string in-place. If modifications
 * are made, a pointer to the start of the modified string is returned.
 * Otherwise, NULL is returned.
 */
char *
__pmLogBaseName(char *name)
{
    char *q;
    int   strip;
    int   i;

    strip = 0;
    if ((q = strrchr(name, '.')) != NULL) {
	for (i = 0; i < ncompress; i++) {
	    if (strcmp(q, compress_ctl[i].suff) == 0) {
		char	*q2;
		/*
		 * The name ends with one of the supported compressed file
		 * suffixes. Strip it becore checking for other known suffixes.
		 */
		*q = '\0';
		if ((q2 = strrchr(name, '.')) == NULL) {
		    /* no . to the left of the suffix */
		    *q = '.';
		    goto done;
		}
		q = q2;
		break;
	    }
	}
	if (strcmp(q, ".index") == 0) {
	    strip = 1;
	    goto done;
	}
	if (strcmp(q, ".meta") == 0) {
	    strip = 1;
	    goto done;
	}
	/*
	 * Check for a string of digits as the suffix.
	 */
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
	return name;
    }

    return NULL; /* not the name of an archive file. */
}

static int
popen_uncompress(const char *cmd, const char *arg, const char *fname, int fd)
{
    char	buffer[4096];
    FILE*finp;
    ssize_t	bytes;
    int		sts, infd;
    __pmExecCtl_t	*argp = NULL;

    sts = __pmProcessAddArg(&argp, cmd);
    if (sts == 0)
	sts = __pmProcessAddArg(&argp, arg);
    if (sts == 0)
	sts = __pmProcessAddArg(&argp, fname);
    if (sts < 0)
	return sts;

    if (pmDebugOptions.log)
	fprintf(stderr, "__pmLogOpen: uncompress using: %s %s %s\n", cmd, arg, fname);

    if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_NONE, &finp)) < 0)
	return sts;
    infd = fileno(finp);

    while ((bytes = read(infd, buffer, sizeof(buffer))) > 0) {
	if (write(fd, buffer, bytes) != bytes) {
	    bytes = -1;
	    break;
	}
    }

    if ((sts = __pmProcessPipeClose(finp)) != 0)
	return sts;
    return (bytes == 0) ? 0 : -1;
}

static int
fopen_securetmp(void)
{
    char	tmpname[MAXPATHLEN];
    mode_t	cur_umask;
    char	*msg;
    int		fd;

    cur_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
#if HAVE_MKSTEMP
    if ((msg = pmGetOptionalConfig("PCP_TMPFILE_DIR")) == NULL) {
	if (pmDebugOptions.log) {
	    fprintf(stderr, "fopen_securetmp: pmGetOptionalConfig -> NULL\n");
	}
	umask(cur_umask);
	return -1;
    }
    pmsprintf(tmpname, sizeof(tmpname), "%s/XXXXXX", msg);
    msg = tmpname;
    fd = mkstemp(tmpname);
    if (fd < 0) {
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "fopen_securetmp: mkstemp(%s): %s\n", tmpname, osstrerror_r(errmsg, sizeof(errmsg)));
	}
    }
#else
    PM_LOCK(__pmLock_extcall);
    if ((msg = tmpnam(NULL)) == NULL) {		/* THREADSAFE */
	PM_UNLOCK(__pmLock_extcall);
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "fopen_securetmp: tmpname: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	}
	umask(cur_umask);
	return -1;
    }
    fd = open(msg, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (fd < 0) {
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "fopen_securetmp: open(%s): %s\n", msg, osstrerror_r(errmsg, sizeof(errmsg)));
	}
    }
#endif
    /*
     * unlink temporary file to avoid namespace pollution and allow O/S
     * space cleanup on last close
     */
#if HAVE_MKSTEMP
    unlink(msg);
#else
    unlink(msg);
    PM_UNLOCK(__pmLock_extcall);
#endif
    umask(cur_umask);
    return fd;
}

static __pmFILE *
fopen_compress(const char *fname, int compress_ix)
{
    int		sts;
    int		fd;
    char	*cmd;
    char	*arg;
    __pmFILE	*fp;

    /* We will need to decompress this file using an external program first. */
    if (compress_ctl[compress_ix].appl == USE_XZ) {
	cmd = "xz";
	arg = "-dc";
    }
    else if (compress_ctl[compress_ix].appl == USE_BZIP2) {
	cmd = "bzip2";
	arg = "-dc";
    }
    else if (compress_ctl[compress_ix].appl == USE_GZIP) {
	cmd = "gzip";
	arg = "-dc";
    }
    else {
	/* botch in compress_ctl[] ... should not happen */
	if (pmDebugOptions.log) {
	    fprintf(stderr, "__pmLogOpen: botch in compress_ctl[]: i=%d\n",
		    compress_ix);
	}
	return NULL;
    }

    if ((fd = fopen_securetmp()) < 0) {
	sts = oserror();
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogOpen: temp file create failed: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	}
	setoserror(sts);
	return NULL;
    }

    sts = popen_uncompress(cmd, arg, fname, fd);
    if (sts == -1) {
	sts = oserror();
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogOpen: uncompress command failed: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	}
	setoserror(sts);
	return NULL;
    }
    if (sts != 0) {
	if (pmDebugOptions.log) {
	    if (sts == 2000)
		fprintf(stderr, "__pmLogOpen: uncompress failed, unknown reason\n");
	    else if (sts > 1000)
		fprintf(stderr, "__pmLogOpen: uncompress failed, signal: %d\n", sts - 1000);
	    else
		fprintf(stderr, "__pmLogOpen: uncompress failed, exit status: %d\n", sts);
	}
	/* not a great error code, but the best we can do */
	setoserror(-PM_ERR_LOGREC);
	return NULL;
    }
    if ((fp = __pmFdopen(fd, "r")) == NULL) {
	if (pmDebugOptions.log) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmLogOpen: fdopen failed: %s\n", osstrerror_r(errmsg, sizeof(errmsg)));
	}
	sts = oserror();
	close(fd);
	setoserror(sts);
	return NULL;
    }
    __pmFseek(fp, 0L, SEEK_SET);	/* rewind temp file */
    /* success */
    return fp;
}

/*
 * Lookup whether the suffix matches one of the compression styles,
 * and if so return the matching index into the compress_ctl table.
 */
static int
index_compress(char *fname, size_t flen)
{
    const char	*suffix;
    int		i;
    char	tmpname[MAXPATHLEN];

    /*
     * We want to match when either:
     * 1) the file name as given is a compressed file
     * 2) a readable compressed version of the given file exists
     */
    suffix = strrchr(fname, '.');
    if (suffix != NULL) {
	for (i = 0; i < ncompress; i++) {
	    if (strcmp(suffix, compress_ctl[i].suff) == 0)
		return i;
	}
    }

    /*
     * The given fname is not a supported compressed file name.
     * If fname does not exist, then check whether there is a compressed
     * version of it.
     */
    errno = 0;
    if (access(fname, F_OK) != 0 && errno == ENOENT) {
	for (i = 0; i < ncompress; i++) {
	    suffix = compress_ctl[i].suff;
	    pmsprintf(tmpname, sizeof(tmpname), "%s%s", fname, suffix);
	    if (access(tmpname, R_OK) == 0) {
		/* Update fname with the compressed file name */
		strncpy(fname, tmpname, flen);
		return i;
	    }
	}
    }
    
    /* end up here if it does not look like a compressed file */
    return -1;
}

/*
 * Open a PCP file with given mode and return a __pmFILE. An i/o
 * handler is automatically chosen based on filename suffix, e.g. .xz, .gz,
 * etc. The stdio pass-thru handler will be chosen for other files.
 * The stdio handler is the only handler currently supporting write operations.
 * Return a valid __pmFILE pointer on success or NULL on failure.
 */
__pmFILE *
__pmFopen(const char *path, const char *mode)
{
    __pmFILE	*f;
    __pm_fops	*handler;
    int		compress_ix;
    char	tmpname[MAXPATHLEN];

    /* We don't know which I/O handler we will use yet. */
    handler = NULL;
    
    /*
     * Check to see if the file is compressed first.
     * index_compress may alter the name in order to add a compression suffix,
     * so pass a copy of path.
     */
    strncpy(tmpname, path, sizeof(tmpname));
    if ((compress_ix = index_compress(tmpname, sizeof(tmpname))) >= 0) {
	if (mode[0] != 'r' || mode[1] != '\0') {
	    /* We don't (yet) support opening compressed files for writing. */
	    return NULL;
	}

	/* Use the compressed file name and select a handler. */
	path = tmpname;
	handler = compress_ctl[compress_ix].handler;
	if (handler == NULL) {
	    /*
	     * We do not have the ability to decompress this file directly.
	     * Try decompressing it externally.
	     */
	    f = fopen_compress(path, compress_ix);
	    return f;
	}

 	/* Fall through and use the default handler. */
    }
    else {
	if (pmDebugOptions.log) {
	    fprintf(stderr, "__pmFopen: index_compress -> %d\n", compress_ix);
	}
    }

    /*
     * The file is either not compressed, or we can not decompress it directly.
     * Default to the stdio handler if one has not yet been chosen.
     */
    if (handler == NULL)
	handler = &__pm_stdio;

    /* Now allocate and open the __pmFile. */
    if ((f = (__pmFILE *)malloc(sizeof(__pmFILE))) == NULL)
    	return NULL;

    memset(f, 0, sizeof(__pmFILE));
    f->fops = handler;

    /*
     * Call the open method for chosen handler. Depending on the handler,
     * this may allocate private data, so the matching close method must
     * be used to deallocate and close, see __pmClose() below.
     */
    if (f->fops->__pmopen(f, path, mode) == NULL) {
	free(f);
    	return NULL;
    }

    /*
     * This __pmFILE can now be used for I/O by calling f->fops.read(),
     * and other functions as needed, etc. 
     */
    return f;
}

__pmFILE *
__pmFdopen(int fd, const char *mode)
{
    __pmFILE *f;

    if ((f = (__pmFILE *)malloc(sizeof(__pmFILE))) == NULL)
    	return NULL;
    memset(f, 0, sizeof(__pmFILE));

    /*
     * For now, we only support the stdio handler.
     */
    f->fops = &__pm_stdio;

    /*
     * Call the open method for chosen handler. Depending on the handler,
     * this may allocate private data, so the matching close method must
     * be used to deallocate and close, see __pmClose() below.
     */
    if (f->fops->__pmfdopen(f, fd, mode) == NULL) {
	free(f);
    	return NULL;
    }

    /*
     * This __pmFILE can now be used for I/O by calling f->fops.read(),
     * and other functions as needed, etc. 
     */
    return f;
}

int
__pmFseek(__pmFILE *f, long offset, int whence)
{
    return f->fops->__pmseek(f, offset, whence);
}

void
__pmRewind(__pmFILE *f)
{
    f->fops->__pmrewind(f);
}

long
__pmFtell(__pmFILE *f)
{
    return f->fops->__pmtell(f);
}

int
__pmFgetc(__pmFILE *f)
{
    return f->fops->__pmfgetc(f);
}

size_t
__pmFread(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    return f->fops->__pmread(ptr, size, nmemb, f);
}

size_t
__pmFwrite(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    return f->fops->__pmwrite(ptr, size, nmemb, f);
}

int
__pmFflush(__pmFILE *f)
{
    return f->fops->__pmflush(f);
}

int
__pmFsync(__pmFILE *f)
{
    return f->fops->__pmfsync(f);
}

off_t
__pmLseek(__pmFILE *f, off_t offset, int whence)
{
    return f->fops->__pmlseek(f, offset, whence);
}

int
__pmFstat(__pmFILE *f, struct stat *buf)
{
    return f->fops->__pmfstat(f, buf);
}

int
__pmFileno(__pmFILE *f)
{
    return f->fops->__pmfileno(f);
}

int
__pmFeof(__pmFILE *f)
{
    return f->fops->__pmfeof(f);
}

int
__pmFerror(__pmFILE *f)
{
    return f->fops->__pmferror(f);
}

void
__pmClearerr(__pmFILE *f)
{
    f->fops->__pmclearerr(f);
}

int
__pmSetvbuf(__pmFILE *f, char *buf, int mode, size_t size)
{
    return f->fops->__pmsetvbuf(f, buf, mode, size);
}

/*
 * Deallocate and close a PCP file that was previously opened
 * with __pmFopen(). Return 0 for success.
 */
int
__pmFclose(__pmFILE *f)
{
    int err;
    
    err = f->fops->__pmclose(f);
    free(f);

    return err;
}
