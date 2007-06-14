/*
 * General Utility Routines - auxilliary routines for Solaris port
 *
 * Original contributed by Alan Hoyt <ahoyt@moser-inc.com> as part of
 * the Solaris changes, completely re-written by Ken McDonell hence ...
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 *
 */

#ident "$Id: util_solaris.c,v 1.3 2003/10/02 07:14:43 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h> 
#include <dirent.h> 
#include <string.h> 

#if defined(HAVE_CONST_DIRENT)
#define MYDIRENT const struct dirent
#else
#define MYDIRENT struct dirent
#endif

/*
 * Scan the directory dirname, building an array of pointers to
 * dirent entries using malloc(3C).  select() and dcomp() are used
 * optionally filter and sort directory entries.
 */

int
scandir(const char *dirname, struct dirent ***namelist,
	int(*select)(MYDIRENT *), int(*dcomp)(MYDIRENT **, MYDIRENT **))
{

    DIR			*dirp;
    int			n = 0;
    struct dirent	**names = NULL;
    struct dirent	*dp;
    struct dirent	*tp;

    if ((dirp = opendir(dirname)) == NULL)
	return -1;

    while ((dp = readdir(dirp)) != NULL) {
	if (select && (*select)(dp) == 0)
	    continue;

	n++;
	if ((names = (struct dirent **)realloc(names, n * sizeof(dp))) == NULL)
	    return -1;

	if ((names[n-1] = tp = (struct dirent *)malloc(sizeof(*dp)-sizeof(dp->d_name)+strlen(dp->d_name)+1)) == NULL)
	    return -1;

	tp->d_ino = dp->d_ino;
	tp->d_off = dp->d_off;
	tp->d_reclen = dp->d_reclen;
	memcpy(tp->d_name, dp->d_name, strlen(dp->d_name)+1);
    }
    closedir(dirp);
    *namelist = names;

    if (n && dcomp)
	qsort(names, n, sizeof(names[0]), (int(*)(const void *, const void *))dcomp);

    return n;
}


/* 
 * Alphabetical sort for default use
 */

int alphasort(MYDIRENT **p, MYDIRENT **q)
{
    return strcmp((*p)->d_name, (*q)->d_name);
}
