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
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

static void *
stdio_open(__pmFILE *f, const char *path, const char *mode)
{
    FILE *fp;

    if ((fp = fopen(path, mode)) == NULL)
	return NULL;

    f->priv = (void *)fp;
    f->position = 0;

    return f;
}

static void *
stdio_fdopen(__pmFILE *f, int fd, const char *mode)
{
    FILE *fp;

    if ((fp = fdopen(fd, mode)) == NULL)
	return NULL;

    f->priv = (void *)fp;
    f->position = 0;

    return f;
}

static int
stdio_seek(__pmFILE *f, off_t offset, int whence)
{
    FILE *fp = (FILE *)f->priv;
    return fseek(fp, offset, whence);
}

static void
stdio_rewind(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    rewind(fp);
}

static off_t
stdio_tell(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return ftell(fp);
}

static int
stdio_getc(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return fgetc(fp);
}

static size_t
stdio_read(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    size_t n = fread(ptr, size, nmemb, fp);
    f->position = ftell(fp);
    return n;
}

static size_t
stdio_write(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    size_t n = fwrite(ptr, size, nmemb, fp);
    f->position = ftell(fp);
    return n;
}

static int
stdio_flush(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return fflush(fp);
}

static int
stdio_fsync(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return fsync(fileno(fp));
}

static int
stdio_fileno(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return fileno(fp);
}

static off_t
stdio_lseek(__pmFILE *f, off_t offset, int whence)
{
    FILE *fp = (FILE *)f->priv;
    return lseek(fileno(fp), offset, whence);
}

static int
stdio_fstat(__pmFILE *f, struct stat *buf)
{
    FILE *fp = (FILE *)f->priv;
    return fstat(fileno(fp), buf);
}

static int
stdio_feof(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return feof(fp);
}

static int
stdio_ferror(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return ferror(fp);
}

static void
stdio_clearerr(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    clearerr(fp);
}

static int
stdio_setvbuf(__pmFILE *f, char *buf, int mode, size_t size)
{
    FILE *fp = (FILE *)f->priv;
    return setvbuf(fp, buf, mode, size);
}

static int
stdio_close(__pmFILE *f)
{
    FILE *fp = (FILE *)f->priv;
    return fclose(fp);
}

__pm_fops __pm_stdio = {
    /*
     * stdio - pass-thru / no compression
     */
    .open = stdio_open,
    .fdopen = stdio_fdopen,
    .seek = stdio_seek,
    .rewind = stdio_rewind,
    .tell = stdio_tell,
    .fgetc = stdio_getc,
    .read = stdio_read,
    .write = stdio_write,
    .flush = stdio_flush,
    .fsync = stdio_fsync,
    .fileno = stdio_fileno,
    .lseek = stdio_lseek,
    .fstat = stdio_fstat,
    .feof = stdio_feof,
    .ferror = stdio_ferror,
    .clearerr = stdio_clearerr,
    .setvbuf = stdio_setvbuf,
    .close = stdio_close
};
