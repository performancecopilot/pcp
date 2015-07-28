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

/*
 * handle pmcpp line marker lines of the form
 * # <lineno> "filename"
 * (lexer ensures syntaxt is correct before we get here)
 */
void
yylinemarker(char *ibuf)
{
    char	*ip = &ibuf[2];
    char	*ep;

    /* lineno will get incremented in lexer with the following newline */
    lineno = atoi(ip)-1;

    while (*ip != '"') ip++;
    ip++;
    ep = &ip[1];
    while (*ep != '"') ep++;
    free(configfile);	/* we're sure this was previously *alloc()d */
    configfile = strndup(ip, ep-ip);
}
