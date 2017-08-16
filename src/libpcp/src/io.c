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

extern __pm_fops __pm_stdio;
#if 0 /* not yet .... */
extern __pm_fops __pm_xz;
#endif
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
    __pmFILE *f;

    if ((f = (__pmFILE *)malloc(sizeof(__pmFILE))) == NULL)
    	return NULL;
    memset(f, 0, sizeof(__pmFILE));

    /*
     * For now, we only support the stdio handler with standard
     * ".index", ".meta" or ".[0-9]+" suffixes.
     * TODO lookup other handlers, as in logutil.c:index_compress(),
     * something like that should be moved here.
     */
    f->fops = &__pm_stdio;

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
     * This __pmFILE can now be used for archive i/o by calling f->fops.read(),
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
     * For now, we only support the stdio handler with standard
     * ".index", ".meta" or ".[0-9]+" suffixes.
     * TODO lookup other handlers, as in logutil.c:index_compress(),
     * something like that should be moved here.
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
     * This __pmFILE can now be used for archive i/o by calling f->fops.read(),
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
 * Deallocate and close a PCP archive file that was previously opened
 * with __pmOpenArchive(). Return 0 for success.
 */
int
__pmFclose(__pmFILE *f)
{
    int err;
    
    err = f->fops->__pmclose(f);
    free(f);

    return err;
}
