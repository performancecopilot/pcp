/*
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmapi.h"
#include "impl.h"

int lineno = 0;

#include "./lex.h"

#ifdef FLEX_SCANNER
#define YY_NO_UNPUT
#include "./gram.tab.h"
static int dbpmdaFlexInput (char *, int);
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=dbpmdaFlexInput(b, ms))
#else /* AT&T Lex */
#undef input
#undef unput
#undef yywrap
#undef yyinput
#include "./gram.h"
#endif

#include "./dbpmda.h"

%}

%s FNAME
%a 2100

%%

add		{ return ADD; }
all		{ return ALL; }
children	{ return PMNS_CHILDREN; }
close		{ return CLOSE; }
debug		{ return DBG; }
delete		{ return DEL; }
desc		{ return DESC; }
dso		{ BEGIN FNAME; return DSO; }
exit		{ return QUIT; }
fetch		{ return FETCH; }
getdesc		{ return GETDESC; }
help		{ return HELP; }
indom		{ return INDOM; }
instance	{ return INSTANCE; }
name		{ return PMNS_NAME; }
namespace	{ BEGIN FNAME; return NAMESPACE; }
none		{ return NONE; }
off		{ return OFF; }
on		{ return ON; }
open		{ return OPEN; }
pipe		{ BEGIN FNAME; return PIPE; }
pmid		{ return PMNS_PMID; }
profile		{ return PROFILE; }
q		{ return QUIT; }
quit		{ return QUIT; }
status		{ return STATUS; }
store		{ return STORE; }
text		{ return INFO; }
timer		{ return TIMER; }
traverse	{ return PMNS_TRAVERSE; }
wait		{ return WAIT; }
watch		{ BEGIN FNAME; return WATCH; }
\?		{ return HELP; }
\=		{ return EQUAL; }
\,		{ return COMMA; }
\+		{ return PLUS; }

[A-Za-z][A-Za-z0-9_\./:-]* {
	    yylval.y_str = (char *)malloc(yyleng+1);
	    strcpy(yylval.y_str, yytext);
	    return NAME;
	}

\$[A-Za-z][A-Za-z0-9_-]* {
	    yylval.y_str = (char *)malloc(yyleng+1);
	    strcpy(yylval.y_str, yytext);
	    return MACRO;
	}

0[xX][0-9]+	{
	    yylval.y_num = (int)strtol(&yytext[2], NULL, 16);	
	    return NUMBER;
	}

[0-9]+	{
	    yylval.y_num = atoi(yytext);
	    return NUMBER;
	}

-[0-9]+	{
	    yylval.y_num = atoi(yytext);
	    return NEGNUMBER;
	}

[0-9]+\.[0-9]+ {
	    sscanf(yytext, "%d.%d", &yylval.y_2num.num1, 
                   &yylval.y_2num.num2); 
	    return NUMBER2D;
	}

[0-9]+\.[0-9]+\.[0-9]+ {
	    sscanf(yytext, "%d.%d.%d", &yylval.y_3num.num1, 
                   &yylval.y_3num.num2, &yylval.y_3num.num3); 
	    return NUMBER3D;
	}


\"[^\"\n][^\"\n]*\"	{
	    yylval.y_str = (char *)malloc(yyleng-1);
	    strncpy(yylval.y_str, &yytext[1], yyleng-2);
	    yylval.y_str[yyleng-2] = '\0';
	    return STRING;
	}

\"[^\"\n][^\"\n]*\n	{
	    yyerror("Expected \"");
	}

\#.*\n	{ return EOL; }

[\r\t ]+	{ }

\n	{ return EOL; }

<FNAME>[^\t \n]+ {
	    yylval.y_str = (char *)malloc(yyleng+1);
	    strcpy(yylval.y_str, yytext);
	    BEGIN 0;
	    return PATHNAME;
	}


.	{
	    yyerror("Illegal character");
	}
%%

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char	*prompt = "dbpmda";
static char	*line = NULL;
static int	lastc = '\n';
static int	linepos = 0;
static int	linelen = 0;
static int	mark = -1;

extern char	*configfile;

extern int	eflag;
extern int	iflag;

#ifdef FLEX_SCANNER

static int
dbpmdaGetc (char * inbuf)
{
    int inch;

    while ((inch = fgetc (yyin)) != EOF ) {
	inbuf[0] = inch & 0xFFU;
	if (inbuf[0] == '\r') {
	    /* for windows, eat carriage returns */
	    continue;
	}
	if (eflag) {
	    putchar (inch);
	    fflush (stdout);
	}
	break;
    }
    if (inch == EOF)
	inch = '\0';

    return (inch);
}

static int 
dbpmdaFlexInput (char * inbuf, int ms)
{
    static FILE * inf = NULL;
    static int save_iflag, save_eflag;

    if (inf == NULL ) {
	if ( ! access (".dbpmdarc", R_OK) ) {
	    inf = yyin;
	    if ((yyin = fopen (".dbpmdarc", "r")) != NULL) {
		save_iflag = iflag;
		save_eflag = eflag;
		prompt = ".dbpmdarc";
		configfile = ".dbpmdarc";
		lineno = 0;
	    } else {
		yyin = inf;
	    }
	}
    }

    if ( lastc == '\n' ) {
	if ( iflag ) {
	    printf ("%s> ", prompt);
	    fflush (stdout);
	}
	lineno++;
	linepos = 0;
    }

    if ( linepos == linelen ) {
	linelen = (linelen) ? linelen * 2 : 128;
	if ((line = (char *)realloc(line, linelen * sizeof(char))) == NULL) {
	    fprintf(stderr, "%s: Lexer internal error\n", pmProgname);
	    exit(1);
	}
    }

    if (ms > 0 ) {
	while (1) {
	    if ( (lastc = dbpmdaGetc(inbuf)) ) {
		line[linepos++] = inbuf[0];
		return (1);
	    } else {
		/* It maybe an EOF */
		if ((inf != NULL) && (inf != yyin)) {
		    yyin = inf;
		    lineno = 1;
		    prompt = "dbpmda";
		    configfile = NULL;
		    if ( (iflag = save_iflag) ) {
			printf ("%s >", prompt);
			fflush (stdout);
		    }
		    eflag = save_eflag;
		} else {
		    return (0);
		}
	    }
        }
    }

    return (ms);
}

#else /* AT&T Lex */

static char	peekc = '\0';

char
input(void)
{
    int		get;
    static int	first = 1;
    static int	save_eflag;
    static int	save_iflag;
    static int	inrc;

    if (first) {
	if (access(".dbpmdarc", R_OK) == 0) {
	    int		fd = open(".dbpmdarc", O_RDONLY);
	    if (fd >= 0) {
		inrc = dup(0);
		close(0);
		dup(fd);
		close(fd);
		save_eflag = eflag;
		save_iflag = iflag;
		eflag = 1;
		iflag = 1;
		prompt = ".dbpmdarc";
		configfile = ".dbpmdarc";
	    }
	}
    }
	
    if (peekc) {
	lastc = peekc;
	peekc = '\0';
	return lastc;
    }
	
  again:
    if (lastc == '\n' || first) {
	if (iflag) {
	    printf("%s> ", prompt);
	    fflush(stdout);
	}
	if (first)
	    first = 0;
	else
	    lineno++;
	linepos = 0;
    }
    else if (lastc == '\0') {
	linepos = 0;
	return lastc;
    }

    if (linepos == linelen) {
	if (linelen == 0)
	    linelen = 128;
	else
	    linelen *= 2;
	line = (char*)realloc(line, linelen * sizeof(char)); 
    }

    get = getchar();
    
    line[linepos++] = (char)get;

    if (get == EOF) {
	if (inrc) {
	    close(0);
	    dup(inrc);
	    close(inrc);
	    inrc = 0;
	    eflag = save_eflag;
	    iflag = save_iflag;
	    prompt = "dbpmda";
	    configfile = NULL;
	    putchar('\n');
	    lineno = 0;
	    lastc = '\n';
	    goto again;
	}
	lastc = '\0';
    }
    else {
	lastc = get;
	if (eflag) {
	    putchar(lastc);
	    fflush(stdout);
	}
    }

    return lastc;
}

void
unput(char c)
{
    peekc = c;
}
#endif

int
yywrap(void)
{
    return lastc == '\0';
}

char
lastinput(void)
{
    return lastc;
}

int 
markpos(void)
{
    mark = linepos;
    return mark;
}

void
locateError(void)
{
    int i;

    if (mark < 0) {
	fprintf(stderr, "%s: Unrecoverable internal error in locateError()\n",
		pmProgname);
	exit(1);
    }

    for (i = 0; prompt[i]; i++)
	putchar(' ');

    putchar(' ');

    for (i = 0; i < mark; i++) {
	if (line[i] == '\t')
	    putchar('\t');
	else if (line[i] == '\n' || line[i] == '\0')
	    break;
	else
	    putchar(' ');
    }

    putchar('^');
    printf(" at or near here\n");
    fflush(stdout);
}


void
doargs(void)
{
    /*
     * a hack ... slide underneath lex/yacc to do the cmd-line args
     */
    char	buf[256];	/* big enough for a single arg? */
    char	*p;
    char	c;
    char	delim = '\0';

    initarglist();

    if (lastc == '\n') {
	addarglist(NULL);
	return;
    }

    p = buf;
    for ( ; ; ) {
#ifdef FLEX_SCANNER
	dbpmdaFlexInput (&c, 1);
#else
	c = input();
#endif
	if (delim) {
	    if (c == delim) {
		delim = '\0';
		continue;
	    }
	}
	else if (c == ' ' || c == '\t' || c == '\n' || c == '\0') {
	    if (p > buf) {
		*p = '\0';
		addarglist(buf);
		p = buf;
	    }
	    if (c == '\n' || c == '\0') {
#ifdef FLEX_SCANNER
		ungetc (c, yyin);
#else
		unput('\n');
	       
#endif
		addarglist(NULL);
		return;
	    }
	    continue;
	}
	else if (c == '"' || c == '\'') {
	    delim = c;
	    continue;
	}
	*p++ = c;
    }
}

void
yywarn(char *s)
{
    extern int	lineno;

    if (configfile == NULL)
	fprintf(stderr, "Warning: %s\n", s);
    else
	fprintf(stderr, "Warning [%s, line %d]\n%s\n",
		configfile, lineno, s);
}

void
yyerror(const char *s)
{
    extern int	lineno;
    extern int	stmt_type;
    char	c;

    markpos();

    c = lastinput();
    for ( ; ; ) {
	if (c == '\0')
	    break;
	if (c == '\n')
	    break;
#ifdef FLEX_SCANNER
	dbpmdaFlexInput (&c, 1);
#else
	c = input();
#endif
    }
    stmt_type = EOL;

    locateError();

    if (configfile == NULL)
	fprintf(stderr, "Error: %s\nType 'help' for a list of commands.\n", s);
    else
	fprintf(stderr, 
	"Error [%s, line %d]: %s\nType 'help' for a list of commands.\n",
		configfile, lineno, s);


}
