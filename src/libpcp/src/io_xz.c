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

#if 0 /* not yet */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

static void *
xz_open(__pmFILE *f, const char *path, const char *mode)
{
    /* TODO */
    return NULL;
}

static __pm_off_t
xz_seek(__pmFILE *f, __pm_off_t offset, int whence)
{
    /* TODO */
    return 0;
}

static ssize_t
xz_read(__pmFILE *f, void *buf, size_t count)
{
    /* TODO */
    return 0;
}

static ssize_t
xz_write(__pmFILE *f, void *buf, size_t count)
{
    /* TODO */
    return 0;
}

static int
xz_fileno(__pmFILE *f)
{
    /* TODO */
    return 0;
}

static int
xz_flush(__pmFILE *f)
{
    /* TODO */
    return 0;
}

static int
xz_close(__pmFILE *f)
{
    /* TODO */
    return 0;
}

__pm_fops __pm_xz = {
    /*
     * xz - transparent decompression only
     * Write not supported.
     */
    .__pmopen = xz_open,
    .__pmseek = xz_seek,
    .__pmread = xz_read,
    .__pmwrite = xz_write,
    .__pmfileno = xz_fileno,
    .__pmflush = xz_flush,
    .__pmclose = xz_close
};

#endif /* not yet */
