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
 */
#include "pmapi.h"
#include "impl.h"
#include "config.h"

extern int conf_gen;

void
hotproc_init(void)
{
    char    h_configfile[MAXPATHLEN];
    FILE    *conf;
    int	    sep = __pmPathSeparator();

    snprintf(h_configfile, sizeof(h_configfile),
	    "%s%c" "proc" "%c" "hotproc.conf",
	    pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    conf = open_config(h_configfile);

    /* Hotproc configured */
    if (conf != NULL) {
	if (read_config(conf) == 1 ) {
	    conf_gen = 1;
	}
	fclose(conf);
    }
}
