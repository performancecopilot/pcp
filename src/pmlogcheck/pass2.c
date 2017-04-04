/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "logcheck.h"

/* TODO */

int
pass2(__pmContext *ctxp, char *archname)
{
    (void)ctxp;

    if (vflag)
	fprintf(stderr, "%s: start pass2\n", archname);

    return 0;
}
