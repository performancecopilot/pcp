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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "logger.h"

void
buildinst(int *numinst, int **intlist, char ***extlist, int intid, char *extid)
{
    char	**el;
    int		*il;
    int		num = *numinst;

    if (num == 0) {
	il = NULL;
	el = NULL;
    }
    else {
	il = *intlist;
	el = *extlist;
    }

    el = (char **)realloc(el, (num+1)*sizeof(el[0]));
    il = (int *)realloc(il, (num+1)*sizeof(il[0]));

    il[num] = intid;

    if (extid == NULL)
	el[num] = NULL;
    else {
	if (*extid == '"') {
	    char	*p;
	    p = ++extid;
	    while (*p && *p != '"') p++;
	    *p = '\0';
	}
	el[num] = strdup(extid);
    }

    *numinst = ++num;
    *intlist = il;
    *extlist = el;
}

void
freeinst(int *numinst, int *intlist, char **extlist)
{
    int		i;

    if (*numinst) {
	free(intlist);
	for (i = 0; i < *numinst; i++)
	    free(extlist[i]);
	free(extlist);

	*numinst = 0;
    }
}
