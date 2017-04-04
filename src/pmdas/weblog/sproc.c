/*
 * Web PMDA, based on generic driver for a daemon-based PMDA
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "weblog.h"
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif
#if defined(HAVE_SCHED_H)
#include <sched.h>
#endif

#if defined(HAVE_PTHREAD_H)
static pthread_t sproc_thread;

int sproc (void (*entry) (void *), int flags, void *arg)
{
    int	retval;

    retval = pthread_create(&sproc_thread, NULL, (void (*))entry, NULL);
    return retval;
}
#endif
