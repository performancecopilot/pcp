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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

%{
/*
 *  pmlogrewrite configfile lexical scanner
 */
#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"
#include <errno.h>

char	mess[256];

#define LEX_NONE	0
#define	LEX_GLOBAL	1
#define LEX_INDOM	2
#define LEX_METRIC	3
#define LEX_TEXT	4
#define LEX_LABEL	5

int mystate = LEX_NONE;

static int	comma_count;

#include "gram.tab.h"

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
	abandon();
	/*NOTREACHED*/
    }
    if (strip_quotes) {
	char	*pend = p;
	while (*pend != '\0')
	    pend++;
	*--pend = '\0';
    }
    return p;
}

/* Same as dupstr(), except allow escaped characters. */
static char *
duptextstr(char *s, int strip_quotes)
{
    const char	*p;
    char	*q;

    s = dupstr(s, strip_quotes);
    q = s;
    for (p = s; *p != '\0'; ++p) {
        if (p[0] == '\\' && p[1] != '\0')
            ++p;
        if (p != q)
            *q = *p;
        ++q;
    }
    *q = '\0';

    return s;
}

%}

%option noinput
%option nounput

%{
#ifdef FLEX_SCANNER
#ifndef YY_NO_UNPUT
#define YY_NO_UNPUT
#endif
#endif
%}

%s none glob host indom metric text label type iftype sem units space time count output
%option case-insensitive

%%

<INITIAL>"global"	{ mystate= LEX_GLOBAL; return TOK_GLOBAL; }
<INITIAL>"metric"	{ mystate = LEX_METRIC; return TOK_METRIC; }
<INITIAL>"indom"	{ mystate = LEX_INDOM; return TOK_INDOM; }
<INITIAL>"text"		{ mystate = LEX_TEXT; BEGIN(text); return TOK_TEXT; }
<INITIAL>"label"	{ mystate = LEX_LABEL; BEGIN(label); return TOK_LABEL; }

<glob>"hostname"	{ BEGIN(host); return TOK_HOSTNAME; }
<glob>"time"		{ return TOK_TIME; }
<glob>"tz"		{ /* for backwards compatibility */ return TOK_TIMEZONE; }
<glob>"timezone"	{ return TOK_TIMEZONE; }
<glob>"zoneinfo"	{ return TOK_ZONEINFO; }
<glob>"features"	{ return TOK_FEATURES; }
<glob>"bits"		{ return TOK_BITS; }
	/* Hostname */
<host>[A-Za-z0-9][A-Za-z0-9.-]*	{ yylval.str = dupstr(yytext, 0); BEGIN(glob); return TOK_HNAME; }

<indom,metric,text,label>"delete"	{ return TOK_DELETE; }
<indom>"indom"		{ return TOK_INDOM; }
<indom>"duplicate"	{ return TOK_DUPLICATE; }
<indom>"iname"		{ return TOK_INAME; }
<indom>"redact"		{ return TOK_REDACT; }
<indom>"replace"	{ return TOK_REPLACE; }
<indom>"inst"		{ return TOK_INST; }

<metric>"name"		{ return TOK_NAME; }
<metric>"pmid"		{ return TOK_PMID; }
<metric>"type"		{ BEGIN(type); return TOK_TYPE; }
<metric>"indom"		{ return TOK_INDOM; }
<metric>"NULL"		{ return TOK_NULL_INT; }
<metric>"output"	{ BEGIN(output); return TOK_OUTPUT; }
<metric>"sem"		{ BEGIN(sem); return TOK_SEM; }
<metric>"units"		{ BEGIN(units); comma_count = 0; return TOK_UNITS; }
<metric>"value"		{ return TOK_METRIC_VALUE; }
<metric>"replace"	{ return TOK_REPLACE; }

<text>"metric"		{ return TOK_METRIC; }
<text>"indom"		{ return TOK_INDOM; }
<text>"oneline"		{ return TOK_ONELINE; }
<text>"help"		{ return TOK_HELP; }
<text>"text"		{ return TOK_TEXT; }
<text>\*		{ return TOK_TEXT_STAR; }

<label>"context"	{ return TOK_CONTEXT; }
<label>"domain"		{ return TOK_DOMAIN; }
<label>"cluster"	{ return TOK_CLUSTER; }
<label>"item"		{ return TOK_ITEM; }
<label>"indom"		{ return TOK_INDOM; }
<label>"instances"	{ return TOK_INSTANCES; }
<label>"instance"	{ return TOK_INSTANCE; }
<label>"new"		{ return TOK_NEW; }
<label>"label"		{ return TOK_LABEL; }
<label>"value"		{ return TOK_VALUE; }
<label>"true"		{ yylval.str = dupstr(yytext, 0); return TOK_JSON_TRUE; }
<label>"false"		{ yylval.str = dupstr(yytext, 0); return TOK_JSON_FALSE; }
<label>"null"		{ yylval.str = dupstr(yytext, 0); return TOK_JSON_NULL; }
<label>\*		{ return TOK_LABEL_STAR; }

<type>"32"		{ yylval.ival = PM_TYPE_32; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"U32"		{ yylval.ival = PM_TYPE_U32; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"64"		{ yylval.ival = PM_TYPE_64; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"U64"		{ yylval.ival = PM_TYPE_U64; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"FLOAT"		{ yylval.ival = PM_TYPE_FLOAT; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"DOUBLE"		{ yylval.ival = PM_TYPE_DOUBLE; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"STRING"		{ yylval.ival = PM_TYPE_STRING; BEGIN(metric); return TOK_TYPE_NAME; }
<type>"if"		{ BEGIN(iftype); return TOK_IF; }

<iftype>"32"		{ yylval.ival = PM_TYPE_32; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"U32"		{ yylval.ival = PM_TYPE_U32; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"64"		{ yylval.ival = PM_TYPE_64; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"U64"		{ yylval.ival = PM_TYPE_U64; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"FLOAT"		{ yylval.ival = PM_TYPE_FLOAT; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"DOUBLE"	{ yylval.ival = PM_TYPE_DOUBLE; BEGIN(type); return TOK_TYPE_NAME; }
<iftype>"STRING"	{ yylval.ival = PM_TYPE_STRING; BEGIN(type); return TOK_TYPE_NAME; }

<output>"first"		{ yylval.ival = OUTPUT_FIRST; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"last"		{ yylval.ival = OUTPUT_LAST; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"min"		{ yylval.ival = OUTPUT_MIN; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"max"		{ yylval.ival = OUTPUT_MAX; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"sum"		{ yylval.ival = OUTPUT_SUM; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"avg"		{ yylval.ival = OUTPUT_AVG; BEGIN(metric); return TOK_OUTPUT_TYPE; }
<output>"inst"		{ BEGIN(metric); return TOK_INST; }
<output>"iname"		{ BEGIN(metric); return TOK_INAME; }

<sem>"COUNTER"		{ yylval.ival = PM_SEM_COUNTER; BEGIN(metric); return TOK_SEM_NAME; }
<sem>"INSTANT"		{ yylval.ival = PM_SEM_INSTANT; BEGIN(metric); return TOK_SEM_NAME; }
<sem>"DISCRETE"		{ yylval.ival = PM_SEM_DISCRETE; BEGIN(metric); return TOK_SEM_NAME; }

<units>","		{
			    ++comma_count;
			    switch (comma_count) {
				case 1:
				case 2:
				    break;
				case 3:
				    BEGIN(space);
				    break;
				case 4:
				    BEGIN(time);
				    break;
				case 5:
				    BEGIN(count);
				    break;
			    }
			    return TOK_COMMA;
			}
<metric>"rescale"	{ return TOK_RESCALE; }

<space>"BYTE"		{ yylval.ival = PM_SPACE_BYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"KBYTE"		{ yylval.ival = PM_SPACE_KBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"MBYTE"		{ yylval.ival = PM_SPACE_MBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"GBYTE"		{ yylval.ival = PM_SPACE_GBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"TBYTE"		{ yylval.ival = PM_SPACE_TBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"PBYTE"		{ yylval.ival = PM_SPACE_PBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"EBYTE"		{ yylval.ival = PM_SPACE_EBYTE; BEGIN(units); return TOK_SPACE_NAME; }
<space>"0"		{ yylval.ival = 0; BEGIN(units); return TOK_SPACE_NAME; }

<time>"NSEC"		{ yylval.ival = PM_TIME_NSEC; BEGIN(units); return TOK_TIME_NAME; }
<time>"USEC"		{ yylval.ival = PM_TIME_USEC; BEGIN(units); return TOK_TIME_NAME; }
<time>"MSEC"		{ yylval.ival = PM_TIME_MSEC; BEGIN(units); return TOK_TIME_NAME; }
<time>"SEC"		{ yylval.ival = PM_TIME_SEC; BEGIN(units); return TOK_TIME_NAME; }
<time>"MIN"		{ yylval.ival = PM_TIME_MIN; BEGIN(units); return TOK_TIME_NAME; }
<time>"HOUR"		{ yylval.ival = PM_TIME_HOUR; BEGIN(units); return TOK_TIME_NAME; }
<time>"0"		{ yylval.ival = 0; BEGIN(units); return TOK_TIME_NAME; }

<count>"ONE"		{ yylval.ival = PM_COUNT_ONE; BEGIN(metric); return TOK_COUNT_NAME; }
<count>[0-9]+		{ yylval.ival = atoi(yytext); BEGIN(metric); return TOK_COUNT_NAME; }

\"[^\"\n][^\"\n]*\"	{ yylval.str = dupstr(yytext, 1); return TOK_STRING; }
\"(\\\"|[^\"])+\"	{ yylval.str = duptextstr(yytext, 1); return TOK_TEXT_STRING; }
\/[^/\n][^/\n]*\/	{ yylval.str = dupstr(yytext, 1); return TOK_PATTERN; }

[0-9]+			{ yylval.str = dupstr(yytext, 0); return TOK_NUMBER; }

[0-9]+\.[0-9]*		{ yylval.str = dupstr(yytext, 0); return TOK_FLOAT; }

[0-9]+\.\*		{ yylval.str = dupstr(yytext, 0); return TOK_INDOM_STAR; }

[0-9]+\.[0-9]+\.[0-9]+	{ yylval.str = dupstr(yytext, 0); return TOK_PMID_INT; }

[0-9]+\.[0-9]+\.\*	{ yylval.str = dupstr(yytext, 0); return TOK_PMID_STAR; }
[0-9]+\.\*\.\*		{ yylval.str = dupstr(yytext, 0); return TOK_PMID_STAR; }

(0|[1-9][0-9]*)(\.[0-9]+)?([eE][\+-]?[0-9]+)? { yylval.str = dupstr(yytext, 0); return TOK_JSON_NUMBER; }

\"(\\[\"\\/bfnrt]|\\u[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]|[^\"\\\n])*\"	{ yylval.str = dupstr(yytext, 0); return TOK_JSON_STRING; }

	/*
	 * Generic name, e.g. for identifier or metric or hostname
	 *
	 * Note: pattern expanded to include embedded '=' or '-' characters
	 *       that maybe inappropriate metric names we're trying to
	 *       correct by rewriting.
	 */
[A-Za-z]([A-Za-z0-9_.]|([=-][A-Za-z0-9_.]))*   { yylval.str = dupstr(yytext, 0); return TOK_GNAME; }

\#.*			{ }

[ \t\r]+		{ }

"->"			{ return TOK_ASSIGN; }
"{"			{
			    if (mystate == LEX_GLOBAL) BEGIN(glob);
			    if (mystate == LEX_INDOM) BEGIN(indom);
			    if (mystate == LEX_METRIC) BEGIN(metric);
    if (pmDebugOptions.appl6)
	fprintf(stderr, "lex: [%d] { begin state=%d\n", lineno, mystate);
			    return TOK_LBRACE;
			}
"}"			{
    if (pmDebugOptions.appl6)
	fprintf(stderr, "lex: [%d] } end state=%d\n", lineno, mystate);
			    mystate = LEX_NONE;
			    BEGIN(INITIAL); return TOK_RBRACE;
			}
"+"			{ return TOK_PLUS; }
"-"			{ return TOK_MINUS; }
":"			{ return TOK_COLON; }
","			{ return TOK_COMMA; }
"("			{ return TOK_LPAREN; }
")"			{ return TOK_RPAREN; }

\n			{ lineno++; }
\"			{ 
			    pmsprintf(mess, sizeof(mess), "Missing closing \" for string expression");
			    yyerror(mess);
			}
.			{ 
			    pmsprintf(mess, sizeof(mess), "Unexpected character '%c'",
				yytext[0]);
			    yyerror(mess);
			}
%%

int
yywrap(void)
{
    return(1);
}
