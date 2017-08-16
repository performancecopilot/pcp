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
#include <stdio.h> /* for EOF */
#include "pmapi.h"
#include "impl.h"

static void *
xz_open(__pmFILE *f, const char *path, const char *mode)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return NULL;
}

static void *
xz_fdopen(__pmFILE *f, int fd, const char *mode)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return NULL;
}

static int
xz_seek(__pmFILE *f, off_t offset, int whence)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static void
xz_rewind(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
}

static off_t
xz_tell(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_getc(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

static size_t
xz_read(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static size_t
xz_write(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static int
xz_flush(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

static int
xz_fsync(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_fileno(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static off_t
xz_lseek(__pmFILE *f, off_t offset, int whence)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_fstat(__pmFILE *f, struct stat *buf)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_feof(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static int
xz_ferror(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static void
xz_clearerr(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
}

static int
xz_setvbuf(__pmFILE *f, char *buf, int mode, size_t size)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_close(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

__pm_fops __pm_xz = {
    /*
     * xz decompression
     */
    .__pmopen = xz_open,
    .__pmfdopen = xz_fdopen,
    .__pmseek = xz_seek,
    .__pmrewind = xz_rewind,
    .__pmtell = xz_tell,
    .__pmfgetc = xz_getc,
    .__pmread = xz_read,
    .__pmwrite = xz_write,
    .__pmflush = xz_flush,
    .__pmfsync = xz_fsync,
    .__pmfileno = xz_fileno,
    .__pmlseek = xz_lseek,
    .__pmfstat = xz_fstat,
    .__pmfeof = xz_feof,
    .__pmferror = xz_ferror,
    .__pmclearerr = xz_clearerr,
    .__pmsetvbuf = xz_setvbuf,
    .__pmclose = xz_close
};
