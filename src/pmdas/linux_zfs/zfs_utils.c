/*
 * Copyright (c) 2021 Red Hat.
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

#include <sys/stat.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "zfs_utils.h"

char ZFS_PATH[MAXPATHLEN];

int
zfs_stats_file_check(char *fname, char *sname)
{
    struct stat buffer;
    sprintf(fname, "%s%c%s", ZFS_PATH, pmPathSeparator(), sname);
    if (stat(fname, &buffer) != 0) {
        pmNotifyErr(LOG_WARNING, "File does not exist: %s", fname);
        return 1;
    }
    else return 0;
}


