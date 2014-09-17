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

#include <stdio.h>
#include <stdlib.h>

void
yywarn(s)
char	*s;
{
    extern int	yylineno;

    (void)fprintf(stderr, "Warning [line %d]\n%s\n", yylineno, s);
}

void
yyerror(s)
char	*s;
{
    extern int	yylineno;
    extern char yytext[];

    (void)fprintf(stderr, "Specification error in configuration\n");
    (void)fprintf(stderr, "[line %d] %s: %s\n", yylineno, s, yytext);
}
