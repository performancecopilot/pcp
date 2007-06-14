/*
 * Known Cisco Interfaces
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

#include "./cisco.h"

#ident "$Id: interface.c,v 1.7 2002/07/05 03:04:50 kenmcd Exp $"

/*
 * first letter defines command line argument for interface type
 * (0 means not allowed in command line)
 * full name of interface is as found in "show interface" command
 * output.
 */

intf_tab_t intf_tab[] = {
    { "s",	"Serial" },
    { "e",	"Ethernet" },
    { "E",	"FastEthernet" },
    { "f",	"Fddi" },
    { "h",	"Hssi" },
    { "a",	"ATM" },
    { "B",	"BRI" },
    { "Vl",	"Vlan" },
    { "G",	"GigabitEthernet" },
    { NULL,	"Controller" },
    { NULL,	"Port-channel" },
    { NULL,	"Dialer" },
    { NULL,	"Loopback" },
};

int	num_intf_tab = sizeof(intf_tab) / sizeof(intf_tab[0]);
