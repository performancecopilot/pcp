/*
 * Linux Infiniband metrics cluster
 *
 * Copyright (c) 2006 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: infiniband.h,v 1.4 2007/07/30 06:31:42 kimbrr Exp $"

typedef enum
{
	RcvData				= 0,
	RcvPkts				= 1,
	RcvSwRelayErrors		= 2,
	RcvConstraintErrors 		= 3,
        RcvErrors			= 4,
        RcvRemotePhysErrors		= 5,
	XmtData				= 6,
	XmtPkts				= 7,
        XmtDiscards			= 8,
        XmtConstraintErrors		= 9, 
        LinkDowned			= 10,
        LinkRecovers			= 11,
        LinkIntegrityErrors		= 12,
        VL15Dropped			= 13,
	ExcBufOverrunErrors 		= 14,
        SymbolErrors			= 15,	
} ibcounters;

#define IB_COUNTERS_IN   (RcvRemotePhysErrors+1)
#define IB_COUNTERS	 (SymbolErrors+1)
#define IB_COUNTERS_ALL  (IB_COUNTERS+4)  /* includes 4 synthetic counters */


typedef struct {
    char	*status;
    char	*card;
    uint64_t	portnum;    
    uint64_t	counters[IB_COUNTERS];
} ib_port_t;

extern int track_ib(void);
extern int refresh_ib(pmInDom);
extern int status_ib(ib_port_t * portp);
extern uint32_t get_control_ib(void);
extern void set_control_ib(uint32_t);

