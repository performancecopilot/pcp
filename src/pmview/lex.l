/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifdef FLEX_SCANNER

int pmviewFlexInput (char * buf, int ms);

#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=pmviewFlexInput(b, ms))

#ifdef __cplusplus
extern "C" {
#endif
int yylex (void);
#ifdef __cplusplus
}
#endif

#else /* AT&T Lex */

#undef input
#undef unput
#undef yywrap

char input(void);
void unput(char);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "viewobj.h"
#include "barobj.h"
#include "text.h"
#include "stackobj.h"
#include "gridobj.h"
#include "gram.h"

extern void yywarn(const char *);
extern void yyerror(const char *);
extern FILE *theAltConfig;

#ifdef YYLMAX
#undef YYLMAX
#endif

#define YYLMAX  2048

#ifdef LEX_DEBUG
#define RETURN(c) if (pmDebug & DBG_TRACE_APPL2) { pmprintf("Lex: %d\n", c); pmflush (); } return c
#else
#define RETURN(c) if (theAltConfig) fprintf(theAltConfig, "%s", yytext); return c
#endif

static void lexerror (const char * msg, const char * yytext);
%}


%%

[-]?[0-9]+[\.][0-9]+	{
	    yylval.y_real = strtod(yytext, (char **)0);
	    RETURN(REAL);
	}

0[xX][0-9]+	{
	    yylval.y_int = (int)strtol(&yytext[2], (char **)0, 16);	
	    RETURN(INT);
	}

[-]?[0-9]+	{
	    yylval.y_int = atoi(yytext);
	    RETURN(INT);
	}

[Pp][Mm][Vv][Ii][Ee][Ww][\t ]+[vV]ersion[\t ]+[0-9]+\.[0-9]+ { 
	    yylval.y_str = strdup (yytext+15);
	    RETURN(PMVIEW);
	}

_grid			{ RETURN(GRID); }
_bar			{ RETURN(BAR); }
_labeledbar		{ RETURN(BAR); }
_stack			{ RETURN(STACK); }
_util			{ RETURN(UTIL); }
_metrics		{ RETURN(METRICLIST); }
_color[lL]ist		{ RETURN(COLORLIST); }
_color[sS]cale		{ RETURN(COLORSCALE); }
_label			{ RETURN(LABEL); }
_metric[lL]abels	{ RETURN(METRICLABEL); }
_inst[lL]abels		{ RETURN(INSTLABEL); }
_direction		{ RETURN(DIRECTION); }
_size			{ RETURN(SIZE); }
_text			{ RETURN(TEXT); }
_scale			{ RETURN(SCALE); }
_history		{ RETURN(HISTORY); }
_bar[lL]ength		{ RETURN(BAR_LENGTH); }
_bar[xX][mM]argin	{ RETURN(MARGIN_WIDTH); }
_bar[yY][mM]argin	{ RETURN(MARGIN_DEPTH); }
_margin[wW]idth		{ RETURN(MARGIN_WIDTH); }
_margin[dD]epth		{ RETURN(MARGIN_DEPTH); }
_bar[hH]eight		{ RETURN(BAR_HEIGHT); }
_bar[bB]ase[hH]eight	{ RETURN(BASE_HEIGHT); }
_base[hH]eight          { RETURN(BASE_HEIGHT); }
_stack[lL]ength 	{ RETURN(BAR_LENGTH); }
_stack[xX][mM]argin	{ RETURN(MARGIN_WIDTH); }
_stack[yY][mM]argin	{ RETURN(MARGIN_DEPTH); }
_stack[hH]eight		{ RETURN(BAR_HEIGHT); }
_stack[bB]ase[hH]eight	{ RETURN(BASE_HEIGHT); }
_grid[sS]pace		{ RETURN(GRID_SPACE); }
_grid[wW]idth		{ RETURN(GRID_WIDTH); }
_grid[dD]epth		{ RETURN(GRID_DEPTH); }
_grid[hH]eight		{ RETURN(BASE_HEIGHT); }
_grid[cC]olor		{ RETURN(BASE_COLOR); }
_base[cC]olor		{ RETURN(BASE_COLOR); }
_gap[wW]idth		{ RETURN(GAP_WIDTH); }
_gap[dD]epth		{ RETURN(GAP_DEPTH); }
_gap[lL]abel		{ RETURN(GAP_LABEL); }
_label[mM]argin		{ RETURN(LABEL_MARGIN); }
_label[cC]olor		{ RETURN(LABEL_COLOR); }
_stack[lL]abel		{ RETURN(STACK_LABEL); }
_base[lL]abel		{ RETURN(BASE_LABEL); }
_box			{ RETURN(BOX); }
_pipe[Ll]ength		{ RETURN(PIPE_LENGTH); }
_pipe			{ RETURN(PIPE); }
_pipe[Tt]ag	 	{ RETURN(PIPETAG); }
_link			{ RETURN(GR_LINK); }
_xing			{ RETURN(GR_XING); }
_sceneFile		{ RETURN(SCENE_FILE); }

[_]?[nN]	{ yylval.y_align = ViewObj::north; RETURN(ALIGN); }
[_]?north	{ yylval.y_align = ViewObj::north; RETURN(ALIGN); }
[_]?[sS]	{ yylval.y_align = ViewObj::south; RETURN(ALIGN); }
[_]?south	{ yylval.y_align = ViewObj::south; RETURN(ALIGN); }
[_]?[eE]	{ yylval.y_align = ViewObj::east; RETURN(ALIGN); }
[_]?east	{ yylval.y_align = ViewObj::east; RETURN(ALIGN); }
[_]?[wW]	{ yylval.y_align = ViewObj::west; RETURN(ALIGN); }
[_]?west	{ yylval.y_align = ViewObj::west; RETURN(ALIGN); }
[_]?[nN][wW]	{ yylval.y_align = ViewObj::northWest; RETURN(ALIGN); }
[_]?northwest   { yylval.y_align = ViewObj::northWest; RETURN(ALIGN); }
[_]?[nN][eE]	{ yylval.y_align = ViewObj::northEast; RETURN(ALIGN); }
[_]?northeast   { yylval.y_align = ViewObj::northEast; RETURN(ALIGN); }
[_]?[sS][wW]	{ yylval.y_align = ViewObj::southWest; RETURN(ALIGN); }
[_]?southwest   { yylval.y_align = ViewObj::southWest; RETURN(ALIGN); }
[_]?[sS][eE]	{ yylval.y_align = ViewObj::southEast; RETURN(ALIGN); }
[_]?southeast	{ yylval.y_align = ViewObj::southEast; RETURN(ALIGN); }
[_]?center	{ yylval.y_align = ViewObj::center; RETURN(ALIGN); }

[_]?up		{ yylval.y_textDir = Text::up; RETURN(DIRVAL); }
[_]?down	{ yylval.y_textDir = Text::down; RETURN(DIRVAL); }
[_]?left	{ yylval.y_textDir = Text::left; RETURN(DIRVAL); }
[_]?right	{ yylval.y_textDir = Text::right; RETURN(DIRVAL); }

[_]?small	{ yylval.y_fontSize = Text::small; RETURN(SIZ); }
[_]?medium	{ yylval.y_fontSize = Text::medium; RETURN(SIZ); }
[_]?large	{ yylval.y_fontSize = Text::large; RETURN(SIZ); }

[_]?show	{ yylval.y_bool = true; RETURN(BOOL); }
[_]?hide	{ yylval.y_bool = false; RETURN(BOOL); }

[_]?towards	{ yylval.y_labelDir = BarObj::towards; RETURN(LABEL_DIR); }
[_]?away	{ yylval.y_labelDir = BarObj::away; RETURN(LABEL_DIR); }

[_]?row		{ yylval.y_instDir = BarMod::instPerRow; RETURN(INST_DIR); }
[_]?col		{ yylval.y_instDir = BarMod::instPerCol; RETURN(INST_DIR); }

_cube		{ yylval.y_shape = ViewObj::cube; RETURN(SHAPE); }
_cylinder	{ yylval.y_shape = ViewObj::cylinder; RETURN(SHAPE); }

_[yY]mod	 { yylval.y_barMod = BarMod::yScale; RETURN(BAR_TYPE); }
_color[mM]od	 { yylval.y_barMod = BarMod::color; RETURN(BAR_TYPE); }
_color[yY][mM]od { yylval.y_barMod = BarMod::colYScale; RETURN(BAR_TYPE); }

_util[mM]od	{ yylval.y_stackMod = StackMod::util; RETURN(STACK_TYPE); }
_fill[mM]od	{ yylval.y_stackMod = StackMod::fixed; RETURN(STACK_TYPE); }

_group[bB]y[rR]ow	{ yylval.y_barGroup = BarMod::groupByRow; RETURN(GROUP); }
_group[bB]y[cC]ol	{ yylval.y_barGroup = BarMod::groupByCol; RETURN(GROUP); }
_group[bB]y[mM]etric	{ yylval.y_barGroup = BarMod::groupByMetric; RETURN(GROUP); }
_group[bB]y[iI]nst	{ yylval.y_barGroup = BarMod::groupByInst; RETURN(GROUP); }

[A-Za-z0-9_./\-,:?=+&]+(\[[ \t]*((\"[^\"\n]*\"|[^\"\n\]]*)[ \t]*\,?[ \t]*)+[ \t]*\]) {
	    /*
	     * Metric could be in the form: name + quoted instance or
	     * name + non-quoted instance (i,e no " between the [ ] ) or
	     * just name with no instance spec. Name with no instance spec
	     * is delegated to the NAME token, the rest is handled here.
	     */
	    yylval.y_str = strdup (yytext);
	    RETURN(METRIC);
	}

[A-Za-z0-9_\.\/\-\,:\?\=\+\&]+ {
	    /*
	     * While parsing, NAME token can be used both in general context
	     * or as name for METRIC. I don't want to do context-specific
	     * lexical analisys, so NAME is restricted to the list of chars
	     * above and it cannot have opening or closing square brackets
	     */
	    yylval.y_str = strdup (yytext);
	    RETURN(NAME);
	}

\"[^\"\n][^\"\n]*\"	{
	    yylval.y_str = (char *)malloc(yyleng-1);
	    strncpy(yylval.y_str, &yytext[1], yyleng-2);
	    yylval.y_str[yyleng-2] = '\0';
	    RETURN(STRING);
	}

\"[^\"\n][^\"\n]*\n	{
	    lexerror("Expected closing \"", yytext);
	    /*NOTREACHED*/ 
	}

\(	{ RETURN(OPENB); }
\)	{ RETURN(CLOSEB); }

\#.*\n	{}
\n	{ if (theAltConfig) fputc('\n', theAltConfig); }

[\t ]+	{ if (theAltConfig) fputc(' ', theAltConfig); }

.	{
	    pmsprintf(theBuffer, theBufferLen,
			"Illegal character '%c'", yytext[0]);
	    lexerror(theBuffer, yytext);
	    /*NOTREACHED*/
	}
%%

static char     *line;
static int      linepos;
static int      linelen;
static int      mark = -1;

extern int lineNum;

#ifdef FLEX_SCANNER
static int in = '\0';

int yywrap(void)
{
    return in == EOF;
}

int
pmviewFlexInput(char *buf, int ms)
{
    if (in == '\n') {
	lineNum++;
	linepos=0;
    }

    if (linepos >= linelen - 1) {
        if (linelen == 0)
            linelen = 128;
        else
            linelen *= 2;
        line = (char *)realloc(line, linelen * sizeof(char)); 
    }

    if (ms > 0) {
	if ((in = fgetc(yyin)) != EOF) {
	    line[linepos++] = buf[0] = in & 0xFFU;
	    ms = 1;
	} else {
	    ms = 0;
	}
    }

    return ms;
}

void
skipAhead(void) 
{
    while ((in != '\n') && (in != EOF)) {
	char c;
	pmviewFlexInput(&c, 1);
    }
}

char
input(void)
{
    char c;
    if (pmviewFlexInput(&c, 1))
	return c;
    return 0;
}

char
lastinput(void)
{
    return (in & 0xFFU);
}

#else

static char     lastc = 'A';
static char     peekc = '\0';

extern int lineNum;

extern FILE	*yyin;

int
yywrap(void)
{
    return lastc == '\0';
}

char
input(void)
{
    int         get;

    if (peekc != '\0') {
        lastc = peekc;
        peekc = '\0';
        return lastc;
    }
        
    if (lastc == '\n') {
	lineNum++;
        linepos = 0;
    }
    else if (lastc == '\0') {
        linepos = 0;
        return lastc;
    }

    if (linepos >= linelen - 1) {
        if (linelen == 0)
            linelen = 128;
        else
            linelen *= 2;
        line = (char *)realloc(line, linelen * sizeof(char)); 
    }

    get = fgetc(yyin);

    if (get == EOF)
        lastc = '\0';
    else
        lastc = get;

    line[linepos++] = (char)get;

    return lastc;
}

void
unput(char c)
{
    peekc = c;
}

char
lastinput(void)
{
    return lastc;
}

#endif

int
markpos(void)
{
    mark = linepos;
    return mark;
}

int
locateError(void)
{
    int i;

    if (mark < 0) {
        pmprintf("%s: Unrecoverable internal error in locateError()\n",
                pmProgname);
        pmflush();
        exit(1);
    }

    if (feof(yyin)) {
        return 0;
    }

    for (i = 0; i < mark; i++)
        if (line[i] == '\n' || line[i] == '\0')
            break;
        else
            pmprintf("%c", line[i]);
    pmprintf("\n");

    for (i = 0; i < mark - 1; i++)
        if (line[i] == '\n' || line[i] == '\0')
            break;
        else if (line[i] == '\t')
            pmprintf("\t");
        else
            pmprintf(" ");

    pmprintf("^ at or near here\n");

    return 1;
}

/* This is 'die now' version of yyerror - static and only used for errors
 * in lexer, i.e those, where we've got no reasonable chance to do any
 * recovery. */
static void
lexerror(const char * msg, const char * yytext)
{
    char *pos;

    if ((pos = strstr(line, yytext)) != NULL) {
	int i;

	for (i = 0; (line[i] != '\n') && (line[i] != '\0'); i++)
            pmprintf("%c", line[i]);
	pmprintf("\n");

	for (i = 0; line+i != pos; i++) {
	    if (line[i] == '\n' || line[i] == '\0')
		break; 
	    else if (line[i] == '\t')
		pmprintf("\t");
	    else
		pmprintf(" ");
	}
	pmprintf("^ at or near here\n");
    }

    pmprintf("%s: Fatal error in configuration file at line %d:\n\t%s\n", 
	     pmProgname, lineNum + 1, msg);

    pmflush();
    exit(1);
}
