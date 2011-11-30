/*
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

#include "pmapi.h"
#include "impl.h"

int
main(int argc, char **argv)
{
    char		*name;
    char        	host[MAXHOSTNAMELEN];
    struct hostent      *hep = NULL;

    __pmSetProgname(argv[0]);

    if (argc == 1) {
	(void)gethostname(host, MAXHOSTNAMELEN);
	name = host;
    }
    else if (argc == 2 && argv[1][0] != '-')
	name = argv[1];
    else {
	fprintf(stderr, "Usage: pmhostname [hostname]\n");
	exit(0);
    }

    hep = gethostbyname(name);
    if (hep == NULL)
        printf("%s\n", name);
    else
        printf("%s\n", hep->h_name);

    exit(0);
}
