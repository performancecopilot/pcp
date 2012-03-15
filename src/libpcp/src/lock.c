/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "impl.h"

/* the big libpcp lock */
#ifdef PM_MULTI_THREAD
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
pthread_mutex_t		__pmLock_libpcp = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
pthread_mutex_t		__pmLock_libpcp;
#endif
#endif

void
__pmInitLocks(void)
{
#ifdef PM_MULTI_THREAD
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
    /*
     * Unable to initialize at compile time, need to do it here in
     * a one trip run-time initialization.
     */
    static pthread_mutex_t	init = PTHREAD_MUTEX_INITIALIZER;
    static int			done = 0;
    pthread_mutex_lock(&init);
    if (!done) {
	/* one-trip initialization */
	pthread_mutexattr_t    attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&__pmLock_libpcp, &attr);
	done = 1;
    }
    pthread_mutex_unlock(&init);
#endif
#endif
}

#ifdef PM_MULTI_THREAD
static int		multi_init[PM_SCOPE_MAX+1];
static pthread_t	multi_seen[PM_SCOPE_MAX+1];
#endif

int
__pmMultiThreaded(int scope)
{
#ifdef PM_MULTI_THREAD
    int			sts = 0;

    PM_LOCK(__pmLock_libpcp);
    if (!multi_init[scope]) {
	multi_init[scope] = 1;
	multi_seen[scope] = pthread_self();
    }
    else {
	if (!pthread_equal(multi_seen[scope], pthread_self()))
	    sts = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
#else
    return 0;
#endif
}

