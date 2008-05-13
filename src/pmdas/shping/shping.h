/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

typedef struct {
    char		*tag;
    char		*cmd;
    int			status;
    int			error;
    float		real;
    float		usr;
    float		sys;
} cmd_t;

#define STATUS_NOTYET	-1
#define STATUS_OK	0
#define STATUS_EXIT	1
#define STATUS_SIG	2
#define STATUS_TIMEOUT	3
#define STATUS_SYS	4

extern cmd_t		*cmdlist;

extern __uint32_t	cycletime;	/* seconds per command cycle */
extern __uint32_t	timeout;	/* response timeout in seconds */
#ifdef HAVE_SPROC
extern pid_t		sprocpid;	/* for refresh() */
#endif
extern pmdaIndom	indomtab;	/* cmd tag indom */

extern int		pmDebug;
extern char		*pmProgname;

extern void		shping_init(pmdaInterface *);

extern void		logmessage(int, const char *, ...);
