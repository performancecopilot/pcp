/*
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 *  pmlogextract configfile lexer
 */
#include "pmapi.h"
#include "impl.h"
#include "logger.h"

int lineno = 1;

#ifdef FLEX_SCANNER
#define YY_NO_UNPUT
#include "gram.tab.h"
#else
#include "gram.h"
#undef input
#define input() ((yytchar=fgetc(yyin)) == EOF ? 0 : yytchar)
#undef unput
#define unput(c) {yytchar=(c); ungetc(yytchar, yyin);}
#endif

%}

%%
"["			{ return LSQB; }
"]"			{ return RSQB; }
","			{ return COMMA; }

[A-Za-z][A-Za-z0-9_.]*	{ yylval.str = yytext; return NAME; }

\"[^\"\n][^\"\n]*\"	{ yylval.str = yytext; return STRING; }

[0-9]+			{ yylval.lval = atol(yytext); return NUMBER; }

\#.*			{ }

[ \t]+			{ }

\n			{ lineno++; }
.			{ 
			    sprintf(emess, "Unexpected character '%c'",
				yytext[0]);
			    yyerror(emess);
			}
%%

#ifdef FLEX_SCANNER
int
yywrap(void)
{
    if (lineno < 0) {
	/* never executed, but removes compiler warnings! */
	yyunput(1, "?");
	input();
    }
    return(1);
}
#endif
