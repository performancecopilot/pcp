/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

%{
#include "pmapi.h"
#include "impl.h"
#include "logger.h"

int lineno=1;

#include "gram.tab.h"

static int
ctx(int type)
{
    extern int	mystate;
    if (mystate == GLOBAL)
	return type;
    else {
	yylval.str = yytext;
    	return NAME;
    }
}

%}

%option noinput
%option nounput

%{
#ifdef FLEX_SCANNER
#ifndef YY_NO_UNPUT
#define YY_NO_UNPUT
#endif
#else
#undef input
#define input() ((yytchar=fgetc(yyin)) == EOF ? 0 : yytchar)
#undef unput
#define unput(c) {yytchar=(c); ungetc(yytchar, yyin);}
#endif /* FLEX_SCANNER */
%}

%%
"["	{ return LSQB; }
"]"	{ return RSQB; }
","	{ return COMMA; }
"{"	{ return LBRACE; }
"}"	{ return RBRACE; }
":"	{ return COLON; }
";"	{ return SEMICOLON; }

milliseconds?	{ return ctx(MSEC); }
mandatory	{ return ctx(MANDATORY); }
advisory	{ return ctx(ADVISORY); }
disallow	{ return ctx(DISALLOW); }
minutes?	{ return ctx(MINUTE); }
seconds?	{ return ctx(SECOND); }
default		{ return ctx(DEFAULT); }
enquire		{ return ctx(ENQUIRE); }
access		{ return ctx(ACCESS); }
except		{ return ctx(EXCEPT); }
allow		{ return ctx(ALLOW); }
every		{ return ctx(EVERY); }
maybe		{ return ctx(MAYBE); }
hours?		{ return ctx(HOUR); }
msecs?		{ return ctx(MSEC); }
mins?		{ return ctx(MINUTE); }
once		{ return ctx(ONCE); }
secs?		{ return ctx(SECOND); }
log		{ return ctx(LOG); }
all		{ return ctx(ALL); }
off		{ return ctx(OFF); }
on		{ return ctx(ON); }

[A-Za-z][A-Za-z0-9_.]*	{ yylval.str = yytext; return NAME; }

[A-Za-z][A-Za-z0-9_.-]*	{ yylval.str = yytext; return HOSTNAME; }

\"[^\"\n][^\"\n]*\"	{  /* strip quotes before returing */
			    yytext[strlen(yytext)-1] = '\0';
			    yylval.str = yytext+1;
			    return STRING;
			}

[0-9]+			{ yylval.lval = atol(yytext); return NUMBER; }

[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+	{ yylval.str = yytext; return IPSPEC; }
[0-9]+\.[0-9]+\.[0-9]+\.\*	{ yylval.str = yytext; return IPSPEC; }
[0-9]+\.[0-9]+\.\*	{ yylval.str = yytext; return IPSPEC; }
[0-9]+\.\*		{ yylval.str = yytext; return IPSPEC; }
\.\*			{ yylval.str = yytext; return IPSPEC; }

([A-Fa-f0-9]+:)*[A-Fa-f0-9]+			{ yylval.str = yytext; return IPSPEC; }
::						{ yylval.str = yytext; return IPSPEC; }
::([A-Fa-f0-9]+:)*[A-Fa-f0-9]+			{ yylval.str = yytext; return IPSPEC; }
([A-Fa-f0-9]+:)+:([A-Fa-f0-9]+:)*[A-Fa-f0-9]+	{ yylval.str = yytext; return IPSPEC; }
([A-Fa-f0-9]+:)+:				{ yylval.str = yytext; return IPSPEC; }

([A-Fa-f0-9]+:)+\*			{ yylval.str = yytext; return IPSPEC; }
::([A-Fa-f0-9]+:)*\*			{ yylval.str = yytext; return IPSPEC; }
([A-Fa-f0-9]+:)+:([A-Fa-f0-9]+:)*\*	{ yylval.str = yytext; return IPSPEC; }
:\*					{ yylval.str = yytext; return IPSPEC; }

\*			{ yylval.str = yytext; return IPSPEC; }

unix:[A-Za-z0-9_.-/]*\*		{ yylval.str = yytext; return URL; }
unix:[A-Za-z0-9_.-/]*		{ yylval.str = yytext; return URL; }
local:[A-Za-z0-9_.-/]*\*	{ yylval.str = yytext; return URL; }
local:[A-Za-z0-9_.-/]*		{ yylval.str = yytext; return URL; }

^\#\ [0-9][0-9]*\ \"[^"]*\"	{ yylinemarker(yytext); }

\#.*		{ }

[ \t\r]+	{ }

\n		{ lineno++; }

.		{
		    char emess[256];
		    snprintf(emess, sizeof(emess), "Unexpected character '%c'", yytext[0]);
		    yyerror(emess);
		}
%%

int 
yywrap (void)
{
	return 1;
}
