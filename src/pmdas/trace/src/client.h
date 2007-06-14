/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef CLIENT_H
#define CLIENT_H

#ident "$Id: client.h,v 1.3 1997/09/09 00:13:09 nathans Exp $"

typedef struct {
    int			fd;		/* socket descriptor  */
    struct sockaddr_in	addr;		/* address of client  */
    struct {				/* connection status  */
	unsigned int	connected : 1;	/* client connected   */
	unsigned int	version   : 8;	/* client pdu version */
	unsigned int	protocol  : 1;	/* synchronous or not */
	unsigned int	padding   :22;	/* currently unused   */
    } status;
    unsigned int	denyOps;
} client_t;

extern client_t *acceptClient(int);
extern void deleteClient(client_t *);
extern void showClients(void);

#endif	/* CLIENT_H */
