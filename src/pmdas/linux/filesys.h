/*
 * Linux Filesystem Cluster
 *
 * Copyright (c) 2000,2004,2007 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/vfs.h>

/* Values for flags in filesys_t */
#define FSF_FETCHED		(1U << 0)

typedef struct filesys {
    int		  id;
    unsigned int  flags;
    char	  *device;
    char	  *path;
    char	  *options;
    struct statfs stats;
} filesys_t;

struct linux_container;
extern int refresh_filesys(pmInDom, pmInDom, struct linux_container *);
extern char *scan_filesys_options(const char *, const char *);
