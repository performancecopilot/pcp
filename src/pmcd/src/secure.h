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
extern int pmcd_secure_server_setup(const char *, const char *);
extern void pmcd_secure_server_shutdown(void);

extern int pmcd_secure_handshake(int, int);
extern int pmcd_encryption_enabled(void);
extern int pmcd_compression_enabled(void);
#else
#define pmcd_secure_server_setup(x,y)	0
#define pmcd_secure_server_shutdown()	do { } while (0)

#define pmcd_secure_handshake(x,y)	(-EOPNOTSUPP)
#define pmcd_encryption_enabled()	0	/* false */
#define pmcd_compression_enabled()	0	/* false */
#endif

#endif /* _SECURE_H */
