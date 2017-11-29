/*
 * Instance Domain Data Structures, suitable for a PMDA
 *
 * Copyright (c) 2012-2014 Red Hat.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _CISCO_H
#define _CISCO_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "domain.h"

typedef struct {
    char		*host;		/* CISCO hostname */
    __pmHostEnt		*hostinfo;	/* Address info for 'host' */
    int			port;           /* port */
    char		*username;	/* username */
    char		*passwd;	/* password */
    char		*prompt;	/* command prompt */
    FILE		*fout;		/* write cmds here */
    FILE		*fin;		/* read output here */
} cisco_t;

typedef struct {

    cisco_t		*cp;		/* which CISCO? */
    char		*interface;	/* interface name, e.g. s0 or e10/10 */
    int			fetched;	/* valid stats? */
    __uint32_t		bandwidth;	/* peak bandwidth */
    __uint32_t		rate_in;
    __uint32_t		rate_out;
    __uint64_t		bytes_in;	/* stats */
    __uint64_t		bytes_out;
    __uint64_t		bytes_out_bcast;
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
extern int dousername(cisco_t *, char **);
extern int dopasswd(cisco_t *, char *);
extern char *mygetwd(FILE *, char *);

extern int parse_only;

#endif /* _CISCO_H */
