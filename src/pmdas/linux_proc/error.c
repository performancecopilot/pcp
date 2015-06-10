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
 */

#include <stdio.h>
#include <stdlib.h>

extern int  yylineno;
extern char yytext[];

void
yywarn(const char *s)
{
    fprintf(stderr, "Warning [line %d] : %s\n", yylineno, s);
}

void
yyerror(const char *s)
{
    fprintf(stderr, "Specification error in configuration\n");
    fprintf(stderr, "[line %d] %s: %s\n", yylineno, s, yytext);
}
