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

#ident "$Id: summary.h,v 2.4 2000/08/21 06:10:48 mg Exp $"

/*
 * exported summary metrics
 */
typedef struct {
    char	*name;		/* name space path */
    pmDesc	desc;		/* descriptor, inc. pmid */
} meta_t;

extern meta_t	*meta;
extern int	nmeta;
extern char	*command;
extern char	*helpfile;
extern int	cmdpipe;
extern pid_t	clientPID;

extern void summaryMainLoop(char *, int, int, pmdaInterface *);
extern void mainLoopFreeResultCallback(void (*)(pmResult *));
extern void service_client(__pmPDU *);
extern void service_config(__pmPDU *);


