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

%{
#include "./gram_node.h"
#include "./gram.tab.h"
#include "pmapi.h"

#define _REGEX_RE_COMP
#include <sys/types.h>
#include <regex.h>


extern void yyerror(const char *s);
extern void yywarn(const char *s);

static char emsg[256]; /* error message */

#ifdef FLEX_SCANNER
#ifndef YY_NO_UNPUT
#define YY_NO_UNPUT
#endif
int	yylineno;
#else
#undef input
#define input() ((yytchar=fgetc(yyin)) == EOF ? 0 : yytchar)
#undef unput
#define unput(c) {yytchar=(c); ungetc(yytchar, yyin);}
#endif /* FLEX_SCANNER */

#define yyterminate() return END;

%}

%option noinput
%option nounput

%%

Version		{ return VERSION; }
schedwait	{ return SCHEDWAIT; }
iowait		{ return IOWAIT; }
iodemand	{ return IODEMAND; }
residentsize	{ return RESIDENTSIZE; }
virtualsize	{ return VIRTUALSIZE; }
ctxswitch	{ return CTXSWITCH; }
syscalls	{ return SYSCALLS; }
gid		{ return GID; }
uid		{ return UID; }
uname		{ return UNAME; }
gname		{ return GNAME; }
fname		{ return FNAME; }
psargs		{ return PSARGS; }
cpuburn		{ return CPUBURN; }
"&&"		{ return AND; }
"||"		{ return OR; }
"!"		{ return NOT; }
"("		{ return LPAREN; }	
")"		{ return RPAREN; }	
true		{ return TRUE; }
false		{ return FALSE; }
"=="		{ return EQUAL; }
"!="		{ return NEQUAL; }
"<"		{ return LTHAN; }
"<="		{ return LEQUAL; }
">"		{ return GTHAN; }
">="		{ return GEQUAL; }
"~"		{ return MATCH; }
"!~"		{ return NMATCH; }

\/[^/\n]*[/\n] {
	    char *str;
	    yylval.y_str = (char *)malloc(yyleng-1);
	    if (yylval.y_str == 0) {
		(void)sprintf(emsg, "malloc failed: %s", osstrerror());
		yyerror(emsg);
	    }
	    strncpy(yylval.y_str, &yytext[1], yyleng-2);
	    yylval.y_str[yyleng-2] = '\0';
	    if ((str = re_comp(yylval.y_str)) != 0) {
	        yyerror(str);
	    }
	    return PATTERN;
	}

\"[^"\n]*["\n] {
	    yylval.y_str = (char *)malloc(yyleng-1);
	    if (yylval.y_str == 0) {
		(void)sprintf(emsg, "malloc failed: %s", osstrerror());
		yyerror(emsg);
	    }
	    strncpy(yylval.y_str, &yytext[1], yyleng-2);
	    yylval.y_str[yyleng-2] = '\0';
	    return STRING;
	}


[0-9]+ |
[0-9]*"."[0-9]+ |
[0-9]+"."[0-9]*	{
	    yylval.y_number = atof(yytext);
	    return NUMBER;
	}

\#.*\n	{ }

[\t \r\n]+ { }

<<EOF>> {
            yyterminate();
	}

[a-zA-Z]+ {
	    yyerror("Illegal word");
	}

.	{
	    yyerror("Illegal character");
	}
%%

int
yywrap (void)
{
    return 1;
}

