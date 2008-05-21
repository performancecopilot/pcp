/*
 * Instance Domain Data Structures, suitable for a PMDA
 *
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _CISCO_H
#define _CISCO_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"

typedef struct {
    char		*host;		/* CISCO hostname */
    struct sockaddr_in	ipaddr;		/* IP address for 'host' */
    char		*username;	/* username */
    char		*passwd;	/* password */
    FILE		*fout;		/* write cmds here */
    FILE		*fin;		/* read output here */
} cisco_t;

typedef struct {

    char		*interface;	/* interface name, e.g. s0 or e10/10 */
    int			fetched;	/* valid stats? */
    __uint32_t		bandwidth;	/* peak bandwidth */
    __uint32_t		bytes_in;	/* stats */
    __uint32_t		bytes_out;
    __uint32_t		bytes_out_bcast;
    __uint32_t		rate_in;
    __uint32_t		rate_out;
    cisco_t		*cp;		/* which CISCO? */
} intf_t;

extern cisco_t		*cisco;
extern int		n_cisco;
extern intf_t		*intf;
extern int		n_intf;

/*
 * Supported Cisco Interfaces
 */
typedef struct {
    char	*type;		/* NULL to skip, else unique per interface
				 * type */
    char	*name;		/* full name as per "show" */
} intf_tab_t;

extern intf_tab_t	intf_tab[];
extern int		num_intf_tab;

#define CISCO_INDOM	0
extern pmdaIndom	indomtab[];
extern pmdaInstid	*_router;

#define PWPROMPT "Password:"
#define USERPROMPT "Username:"

extern int conn_cisco(cisco_t *);
extern int grab_cisco(intf_t *);
extern int dousername(FILE *, FILE *, char *, char *);
extern int dopasswd(FILE *, FILE *, char *, char *);
extern char *mygetwd(FILE *);

#endif /* _CISCO_H */
