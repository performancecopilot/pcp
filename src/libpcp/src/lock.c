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
pthread_mutex_t		__pmLock_libpcp;
#endif

void
__pmInitLocks(void)
{
#ifdef PM_MULTI_THREAD
    static pthread_mutex_t	init = PTHREAD_MUTEX_INITIALIZER;
    static int			done = 0;
    pthread_mutex_lock(&init);
    if (!done) {
	pthread_mutexattr_t    attr;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&__pmLock_libpcp, &attr);
	done = 1;
    }
    pthread_mutex_unlock(&init);
#endif
}

int
__pmMultiThreaded(void)
{
#ifdef PM_MULTI_THREAD
    static int		first = 1;
    int			sts = 0;
    static pthread_t	seen;

    PM_LOCK(__pmLock_libpcp);
    if (first) {
	first = 0;
	seen = pthread_self();
    }
    else {
	if (!pthread_equal(seen, pthread_self()))
	    sts = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
#else
    return 0;
#endif
}

