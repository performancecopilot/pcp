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
extern __pm_fops __pm_xz;

/*
 * Open a PCP archive file with given mode and return a __pmFILE. An i/o
 * handler is automatically chosen based on filename suffix, e.g. .xz, .gz,
 * etc. The stdio pass-thru handler will be chosen for the standard suffixes
 * ".meta", ".index" and for uncompressed data volumes matching "*.[0-9]+".
 * The stdio handler is the only handler currently supporting write operations.
 * Return a valid __pmFILE pointer on success or NULL on failure.
 */
__pmFILE *
__pmOpen(const char *path, const char *mode)
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
    if (f->fops->open(f, path, mode) == NULL) {
	free(f);
    	return NULL;
    }

    /*
     * This __pmFILE can now be used for archive i/o by calling f->fops.read(),
     * and other functions as needed, etc. 
     */
    return f;
}

/*
 * Deallocate and close a PCP archive file that was previously opened
 * with __pmOpenArchive(). Return 0 for success.
 */
int
__pmClose(__pmFILE *f)
{
    int err;
    
    err = f->fops->close(f);
    memset(f, 0, sizeof(__pmFILE));
    free(f);

    return err;
}
