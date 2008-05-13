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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ifndef HAVE_SPROC

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif
#include "weblog.h"


static pthread_t sproc_thread;

int sproc (void (*entry) (void *), int flags, void *arg)
{
    int	retval;

    retval = pthread_create(&sproc_thread, NULL, (void (*))entry, NULL);
    return retval;
}
#endif /*HAVE_SPROC*/
