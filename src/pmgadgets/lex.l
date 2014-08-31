/*
 * Copyright (c) 1996-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdlib.h>
#include <string.h>
#include "tokens.h"

extern unsigned lineCount;

int dostring();

#ifndef FLEX_SCANNER
#ifdef output
#undef output
#define output(c)
#endif
#else
#define YY_NO_UNPUT
#endif

%}

%%

[-]?[0-9]+	{
		    /*
		     * bigger than, 999,999,999 (or smaller than
		     * -99,999,999), let's force it to be a real, to
		      avoid overflow issues
		     */
		    if (strlen(yytext) > 9) {
			tokenRealVal = atof(yytext);
			return(TOK_REAL);
		    }
		    tokenIntVal = atoi(yytext);
		    return(TOK_INTEGER);
		}
[-]?[0-9]+\.[0-9]+ {
		    tokenRealVal = atof(yytext);
		    return(TOK_REAL);
		}

\(		return TOK_LPAREN;
\)		return TOK_RPAREN;
\[		return TOK_LBRACKET;
\]		return TOK_RBRACKET;
\:		return TOK_COLON;

_line		return TOK_LINE;
_label		return TOK_LABEL;
_bar		return TOK_BAR;
_multibar	return TOK_MULTIBAR;
_bargraph	return TOK_BARGRAPH;
_led		return TOK_LED;

_legend		return TOK_LEGEND;
_colour		return TOK_COLOUR;
_color		return TOK_COLOUR;
_colourlist	return TOK_COLOURLIST;
_colorlist	return TOK_COLOURLIST;
_actions	return TOK_ACTIONLIST;

_update		return TOK_UPDATE;
_history	return TOK_HISTORY;
_noborder	return TOK_NOBORDER;
_metric		return TOK_METRIC;
_horizontal	return TOK_HORIZONTAL;
_vertical	return TOK_VERTICAL;
_metrics	return TOK_METRICS;
_min		return TOK_MIN;
_mainimum	return TOK_MIN;
_max		return TOK_MAX;
_maximum	return TOK_MAX;
_default	return TOK_DEFAULT;
_fixed		return TOK_FIXED;

_[a-zA-Z0-9_.]	return TOK_BAD_RES_WORD;

\#		{
		    while (input() != '\n')
			;
		    nLines++;
		}

\"[^\"\n][^\"\n]*\"	return dostring();

\n		nLines++;

[A-Za-z][A-Za-z0-9_.\-]* {
		    tokenStringVal = strdup(yytext);
		    return TOK_IDENTIFIER;
		}

[0-9\-]+[A-Za-z_.\-]+[A-Za-z0-9.\-]* {
		    tokenStringVal = strdup(yytext);
		    return TOK_IDENTIFIER;
		}

[ \t]*		{ }
%%

int
yywrap(void)
{
    return 1;
}

int
dostring(void)
{
    size_t szResult;
    char* result;

    szResult = yyleng - 1; /* ignore 1st '"', 2nd clobbered by '\0' */
    result = (char*)malloc(szResult);
    strncpy(result, yytext + 1, szResult - 1);
    result[szResult - 1] = '\0';
    tokenStringVal = result;
    return TOK_STRING;
}
