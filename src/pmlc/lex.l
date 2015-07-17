/*
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmapi.h"
#include "./pmlc.h"

extern int eflag;

#ifdef FLEX_SCANNER
#include "gram.tab.h"
int pmlcFlexInput (char *, int);
int lineno = 0;
#else /* AT&T lex */
#include "gram.h"
int lineno = 1;
#endif

static int
ctx(int type)
{
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
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=pmlcFlexInput(b, ms))
#else /* AT&T Lex */
#undef input
#undef unput
#undef yywrap
#endif
%}

%%
"["	{ return LSQB; }
"]"	{ return RSQB; }
","	{ return COMMA; }
"{"	{ return LBRACE; }
"}"	{ return RBRACE; }
"@"	{ return AT; }

milliseconds?	{ return ctx(MSEC); }
mandatory	{ return ctx(MANDATORY); }
advisory	{ return ctx(ADVISORY); }
timezone	{ return ctx(TIMEZONE); }
loggers?	{ return ctx(LOGGER); }
minutes?	{ return ctx(MINUTE); }
seconds?	{ return ctx(SECOND); }
connect		{ return ctx(CONNECT); }
primary		{ return ctx(PRIMARY); }
status		{ return ctx(STATUS); }
volume		{ return ctx(VOLUME); }
flush		{ return ctx(SYNC); }
every		{ return ctx(EVERY); }
local		{ return ctx(LOCAL); }
maybe		{ return ctx(MAYBE); }
query		{ return ctx(QUERY); }
hours?		{ return ctx(HOUR); }
msecs?		{ return ctx(MSEC); }
mins?		{ return ctx(MINUTE); }
secs?		{ return ctx(SECOND); }
help		{ return ctx(HELP); }
once		{ return ctx(ONCE); }
port		{ return ctx(PORT); }
quit		{ return ctx(QUIT); }
socket		{ return ctx(SOCK); }
show		{ return ctx(SHOW); }
sync		{ return ctx(SYNC); }
log		{ return ctx(LOG); }
new		{ return ctx(NEW); }
off		{ return ctx(OFF); }
on		{ return ctx(ON); }
qa		{ return ctx(QA); }
at		{ return ctx(AT); }
h		{ return ctx(HELP); }
q		{ return ctx(QUIT); }
\?		{ return ctx(HELP); }

unix:[A-Za-z0-9_.-/]*\*		{ yylval.str = yytext; return URL; }
unix:[A-Za-z0-9_.-/]*		{ yylval.str = yytext; return URL; }
local:[A-Za-z0-9_.-/]*\*	{ yylval.str = yytext; return URL; }
local:[A-Za-z0-9_.-/]*		{ yylval.str = yytext; return URL; }

[A-Za-z][A-Za-z0-9_.]*	{ yylval.str = yytext; return NAME; }

[A-Za-z][A-Za-z0-9_.-]* { yylval.str = yytext; return HOSTNAME; }

\"[^\"\n][^\"\n]*\"	{ /* strip quotes before returing */
			    yytext[strlen(yytext)-1] = '\0';
			    yylval.str = yytext+1; 
			    return STRING; 
    	                }

[0-9]+			{ yylval.lval = atol(yytext); return NUMBER; }

\#[^\n]*		{ }
\n			{ if (!(mystate & INSPECLIST)) return EOL; }

[ \r\t]+		{ }

.			{
			    char emess[160];
			    sprintf(emess, "Unexpected character '%c'", 
				    yytext[0]);
			    yyerror(emess);
			}
%%

#ifdef FLEX_SCANNER
static int in = '\n';

int yywrap(void)
{
    return in == EOF;
}

int
pmlcFlexInput (char * buf, int ms)
{
    extern int iflag;

    if ( in == '\n' ) {
	lineno++;
	if ( iflag ) {
	    if ( mystate == GLOBAL ) 
		printf ("pmlc> ");
	    else
		printf ("? ");
	    fflush (stdout);
	}
    }

    if (ms > 0 ) {
	if ( (in = fgetc (yyin)) != EOF ) {
	    buf[0] = in & 0xFFU;
	    ms = 1;
	    if ( eflag ) {
		putchar (in);
		fflush (stdout);
	    }
	} else {
	    ms = 0;
	}
    }

    return (ms);
}

void
skipAhead (void) 
{
    if ( mystate & INSPECLIST ) {
	while ( in != '}' && in != EOF ) {
	    char c;
	    pmlcFlexInput (&c, 1);
	}
	mystate = GLOBAL;
    }

    while ( (in != '\n') && (in != EOF) ) {
	char c;
	pmlcFlexInput (&c, 1);
    }
}

#else
static char	lastc;
static char	peekc = '\0';

char input(void)
{
    int		get;
    static int	first = 1;

    if (peekc) {
	lastc = peekc;
	peekc = '\0';
	return lastc;
    }
	
    if (lastc == '\n' || first) {
	extern int iflag;
	if (iflag) {
	    if (mystate == GLOBAL)
		printf("pmlc> ");
	    else
		printf("? ");
	    fflush(stdout);
	}
    	if (first)
    	    first = 0;
    	else
	    lineno++;
    }
    else if (lastc == '\0')
	return lastc;

    get = getchar();
    if (get == EOF)
	lastc = '\0';
    else {
	lastc = get;
	if (eflag) {
	    putchar(lastc);
	    fflush(stdout);
	}
    }

    return lastc;
}

void unput(char c)
{
    peekc = c;
}

void
skipAhead (void)
{
    int c = lastc;

    for ( ; ; ) {
	if (c == '\0')
	    break;
	if ((mystate == GLOBAL || (mystate & INSPEC)) && c == '\n')
	    break;
	if ((mystate & INSPECLIST) && c == '}')
	    break;
	c = input();
    }
}

int yywrap(void)
{
    return lastc == '\0';
}
#endif
