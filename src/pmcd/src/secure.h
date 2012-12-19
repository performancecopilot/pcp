/*
 * Copyright (c) 2012 Red Hat.
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
 */

#ifndef _SECURE_H
#define _SECURE_H

#ifdef HAVE_SECURE_SOCKETS
extern int __pmSecureServerSetup(const char *, const char *);
extern void __pmSecureServerShutdown(void);
extern int __pmSecureServerHandshake(int, int);
extern int __pmEncryptionEnabled(void);
extern int __pmCompressionEnabled(void);
#else
#define __pmSecureServerSetup()		0
#define __pmSecureServerShutdown()	do { } while (0)
#define __pmSecureServerHandshake(x,y)	(-EOPNOTSUPP)
#define __pmEncryptionEnabled()		0	/* false */
#define __pmCompressionEnabled()	0	/* false */
#endif

#endif /* _SECURE_H */
