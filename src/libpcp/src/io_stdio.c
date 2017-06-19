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

    if ((f->priv = malloc(sizeof(FILE *))) == NULL)
	return NULL;

    if ((fp = fopen(path, mode)) == NULL) {
	free(f->priv);
    	free(f);
	return NULL;
    }

    *((FILE **)f->priv) = fp;
    f->position = 0;

    return f;
}

static __pm_off_t
stdio_seek(__pmFILE *f, __pm_off_t offset, int whence)
{
    FILE *fp = *(FILE **)f->priv;

    f->position = fseek(fp, offset, whence);

    return f->position;
}

static ssize_t
stdio_read(__pmFILE *f, void *buf, size_t count)
{
    FILE *fp = *(FILE **)f->priv;
    size_t n = fread(buf, count, 1, fp);

    f->position = ftell(fp);

    return n;
}

static ssize_t
stdio_write(__pmFILE *f, void *buf, size_t count)
{
    FILE *fp = *(FILE **)f->priv;
    size_t n = fwrite(buf, count, 1, fp);

    f->position = ftell(fp);

    return n;
}

static int
stdio_fileno(__pmFILE *f)
{
    FILE *fp = *(FILE **)f->priv;

    return fileno(fp);
}

static int
stdio_flush(__pmFILE *f)
{
    FILE *fp = *(FILE **)f->priv;

    return fflush(fp);
}

static int
stdio_close(__pmFILE *f)
{
    /*
     * close file, deallocate f, free priv
     */
    FILE *fp = *(FILE **)f->priv;
    int ret = fclose(fp);

    free(f->priv);
    free(f);

    return ret;
}

__pm_fops __pm_stdio = {
    /*
     * stdio - pass-thru / no compression
     */
    .open = stdio_open,
    .seek = stdio_seek,
    .read = stdio_read,
    .write = stdio_write,
    .fileno = stdio_fileno,
    .flush = stdio_flush,
    .close = stdio_close
};

