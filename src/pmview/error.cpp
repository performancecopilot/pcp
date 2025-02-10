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

int warnCount;
int errorCount;
int lineNum;

void
yywarn(const char *s)
{
    const char *config;

    if (theConfigName.length() > 0)
	config = (const char *)theConfigName.toLatin1();
    else
	config = "<stdin>";
    pmprintf("%s: warning - %s[%d]: %s\n", pmGetProgname(), config, lineNum+1, s);
    warnCount++;	/* used in main.cpp to call pmflush() */
}

void
yyerror(const char *s)
{
    const char *config;
    const char badeof[] = "unexpected end of file";

    if (theConfigName.length() > 0)
	config = (const char *)theConfigName.toLatin1();
    else
	config = "<stdin>";

    markpos();
    if (!locateError())
	s = (char *)badeof;
	
    pmprintf("%s: error - %s[%d]: %s\n", pmGetProgname(), config, lineNum+1, s);
    pmflush();
    errorCount++;	/* used in main.cpp to abort the execution */
}

