/*
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "libpcp.h"

int
main(int argc, char **argv)
{
    char	*res;
    if (argc != 2) {
	fprintf(stderr, "Usage: pmlogbasename filename\n");
	exit(1);
    }
    /*
     * need strdup() 'cause __pmLogBaseName() clobbers the argument
     * in place
     */
    if ((res = strdup(argv[1])) == NULL) {
	fprintf(stderr, "pmlogbasename: strdup(%s) failed!\n", argv[1]);
	exit(1);
    }
    res = __pmLogBaseName(res);
    if (res == NULL)
	printf("%s\n", argv[1]);
    else
	printf("%s\n", res);

    return 0;
}
