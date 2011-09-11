/*
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

%{
/*
 *  pmlogrewrite configfile lexical scanner
 */
#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <errno.h>

int	lineno = 1;
char	mess[256];

#define LEX_NONE	0
#define	LEX_GLOBAL	1
#define LEX_INDOM	2
#define LEX_METRIC	3

int mystate = LEX_NONE;

#include "gram.tab.h"

#ifdef FLEX_SCANNER
#define YY_NO_UNPUT
#else
#undef input
#define input() ((yytchar=fgetc(yyin)) == EOF ? 0 : yytchar)
#undef unput
#define unput(c) {yytchar=(c); ungetc(yytchar, yyin);}
#endif

static char *
dupstr(char *s, int strip_quotes)
{
    char	*p;
    if (strip_quotes)
	p = strdup(&s[1]);
    else
	p = strdup(s);
    if (p == NULL) {
	fprintf(stderr, "Failed strdup(\"%s\") in lexer: %s\n", s, strerror(errno));
	exit(1);
    }
    if (strip_quotes) {
	char	*pend = p;
	while (*pend != '\0')
	    pend++;
	*--pend = '\0';
    }
    return p;
}

%}

%s none inglob inindom inmetric

%%

<INITIAL>"global"		{ mystate= LEX_GLOBAL; return GLOBAL; }
<INITIAL>"metric"		{ mystate = LEX_METRIC; return METRIC; }
<INITIAL>"indom"		{ mystate = LEX_INDOM; return INDOM; }

<inglob>"hostname"		{ return HOSTNAME; }
<inglob>"time"			{ return TIME; }
<inglob>"tz"			{ return TZ; }

<inindom>"indom"		{ return INDOM; }
<inindom>"name"			{ return NAME; }
<inindom>"inst"			{ return INST; }

\"[^\"\n][^\"\n]*\"	{ yylval.str = dupstr(yytext, 1); return STRING; }

[0-9]+			{ yylval.str = dupstr(yytext, 0); return NUMBER; }

[0-9]+\.[0-9]*		{ yylval.str = dupstr(yytext, 0); return FLOAT; }

[0-9]+\.\*		{ yylval.str = dupstr(yytext, 0); return INDOM_STAR; }

	/* Generic name, e.g. for identifier or metric or hostname */
[A-Za-z][A-Za-z0-9_.]*	{ yylval.str = dupstr(yytext, 0); return GNAME; }

	/* Hostname */
[A-Za-z0-9][A-Za-z0-9.-]*	{ yylval.str = dupstr(yytext, 0); return HNAME; }

\#.*			{ }

[ \t\r]+		{ }

"->"			{ return ASSIGN; }
"{"			{
			    if (mystate == LEX_GLOBAL) BEGIN(inglob);
			    if (mystate == LEX_INDOM) BEGIN(inindom);
			    if (mystate == LEX_METRIC) BEGIN(inmetric);
			    return LBRACE;
			}
"}"			{ BEGIN(INITIAL); return RBRACE; }
"+"			{ return PLUS; }
"-"			{ return MINUS; }
"."			{ return DOT; }
":"			{ return COLON; }
","			{ return COMMA; }

\n			{ lineno++; }
.			{ 
			    sprintf(mess, "Unexpected character '%c'",
				yytext[0]);
			    yyerror(mess);
			}
%%

int
yywrap(void)
{
    return(1);
}
