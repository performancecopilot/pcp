/*
 * Utiility routines for pmlogrewrite
 *
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <assert.h>

void
yywarn(char *s)
{
    fprintf(stderr, "Warning [%s, line %d]\n%s\n", configfile, lineno, s);
}

void
yyerror(char *s)
{
    fprintf(stderr, "Specification error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "[line %d] %s\n", lineno, s);
    exit(1);
}

void
yysemantic(char *s)
{
    fprintf(stderr, "Semantic error in configuration file (%s)\n",
	    configfile);
    fprintf(stderr, "%s\n", s);
    exit(1);
}


/*
 * Walk a hash list ... mode is W_START ... W_NEXT ... W_NEXT ...
 */
__pmHashNode *
__pmHashWalk(__pmHashCtl *hcp, int mode)
{
    static int		hash_idx;
    static __pmHashNode	*next;
    __pmHashNode	*this;

    if (mode == W_START) {
	hash_idx = 0;
	next = hcp->hash[0];
    }

    while (next == NULL) {
	hash_idx++;
	if (hash_idx >= hcp->hsize)
	    return NULL;
	next = hcp->hash[hash_idx];
    }

    this = next;
    next = next->next;

    return this;
}
