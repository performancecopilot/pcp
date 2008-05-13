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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

%{
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "./gram_node.h"
#include "./gram.h"

void yyerror(char *s);

static char emsg[256]; /* error message */
static char yy_ichar;  /* input char from conf_buffer */
extern char *conf_buffer_ptr;

#undef input
#undef unput
#define input() ( (yy_ichar = *conf_buffer_ptr++) == '\n' ? (yylineno++, yy_ichar) : yy_ichar)
#define unput(c) { if ((*--conf_buffer_ptr = c) == '\n') yylineno--; }

%}

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
		(void)sprintf(emsg, "malloc failed: %s", strerror(errno));
		yyerror(emsg);
	        /*NOTREACHED*/
	    }
	    strncpy(yylval.y_str, &yytext[1], yyleng-2);
	    yylval.y_str[yyleng-2] = '\0';
	    if ((str = re_comp(yylval.y_str)) != 0) {
	        yyerror(str);
	        /*NOTREACHED*/
	    }
	    return PATTERN;
	}

\"[^"\n]*["\n] {
	    yylval.y_str = (char *)malloc(yyleng-1);
	    if (yylval.y_str == 0) {
		(void)sprintf(emsg, "malloc failed: %s", strerror(errno));
		yyerror(emsg);
	        /*NOTREACHED*/
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

[\t \n]+ { }


[a-zA-Z]+ {
	    yyerror("Illegal word");
	}

.	{
	    yyerror("Illegal character");
	}
%%

