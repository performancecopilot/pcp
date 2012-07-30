/*
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat, Inc. All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "filesys.h"

char *
scan_filesys_options(const char *options, const char *option)
{
    static char buffer[128];
    char *s;

    strncpy(buffer, options, sizeof(buffer));

    s = strtok(buffer, ",");
    while (s) {
	if (strcmp(s, option) == 0)
	    return s;
        s = strtok(NULL, ",");
    }
    return NULL;
}

