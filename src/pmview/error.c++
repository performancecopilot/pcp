/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include "pmapi.h"
#include "pmview.h"

int errorCount = 0;
int lineNum = 0;

extern char	*pmProgname;

void
yywarn(char *s)
{
    const char * fmt =  theConfigName.length() ? "%s: warning - %s(%d): %s\n":
					   "%s: warning - line %3$d: %4$s\n";

    pmprintf(fmt, pmProgname, theConfigName.ptr(), lineNum+1, s);
    pmflush();
}

void
yyerror(char *s)
{
    const char * fmt =  theConfigName.length() ? "%s: error - %s(%d): %s\n":
					   "%s: error - line %3$d: %4$s\n";
    markpos();
    if ( ! locateError()) {
	s = "unexpected end of file";
    }
	
    pmprintf(fmt, pmProgname, theConfigName.ptr(), lineNum+1, s);
    pmflush();
    errorCount++; /* It's used in pmview.c++ to about the execution */
}

