/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#include "main.h"

int errorCount;
int lineNum;

void
yywarn(const char *s)
{
    const char * fmt =  theConfigName.length() ? "%s: warning - %s(%d): %s\n":
					   "%s: warning - line %3$d: %4$s\n";
    const char * config = (const char *)theConfigName.toAscii();

    pmprintf(fmt, pmProgname, config, lineNum+1, s);
    pmflush();
}

void
yyerror(const char *s)
{
    const char * fmt =  theConfigName.length() ? "%s: error - %s(%d): %s\n":
					   "%s: error - line %3$d: %4$s\n";
    const char * config = (const char *)theConfigName.toAscii();
    const char badeof[] = "unexpected end of file";

    markpos();
    if (!locateError())
	s = (char *)badeof;
	
    pmprintf(fmt, pmProgname, config, lineNum+1, s);
    pmflush();
    errorCount++; /* It's used in pmview.cpp to abort the execution */
}

