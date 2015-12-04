/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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

#include <QtCore/QStack>

#include "main.h"
#include "gridobj.h"
#include "stackobj.h"
#include "barobj.h"
#include "labelobj.h"
#include "pipeobj.h"
#include "scenefileobj.h"
#include "link.h"
#include "xing.h"

extern void yywarn(const char *s);
extern void yyerror(const char *s);

#ifdef __cplusplus
extern "C" {
#endif
extern int yylex(void);
#ifdef __cplusplus
}
#endif

static DefaultObj dobj;
static QStack<ViewObj *> objstack;

ViewObj * rootObj = 0;

bool				theColorScaleFlag;
int				theCol;
int				theRow;
int				theNumCols;
int				theNumRows;
int				theHistory = 0;
ViewObj::Alignment		theAlignment;
int				theColorListCount = 0;

%}

%union {
    char			*y_str;
    int				y_int;
    double			y_real;
    bool			y_bool;
    ViewObj::Alignment		y_align;
    ViewObj::Shape		y_shape;
    Text::Direction		y_textDir;
    Text::FontSize		y_fontSize;
    BarMod::Direction		y_instDir;
    BarObj::LabelDir		y_labelDir;
    BarMod::Modulation		y_barMod;
    StackMod::Height		y_stackMod;
    BarMod::Grouping		y_barGroup;
    ViewObj 			* y_object;
}

%token	<y_str>
	NAME STRING PMVIEW METRIC

%token	<y_int>
	INT

%token	<y_real>
	REAL

%token	<y_bool>
	BOOL

%token	<y_align>
	ALIGN

%token	<y_textDir>
	DIRVAL

%token	<y_fontSize>
	SIZ

%token	<y_instDir>
	INST_DIR

%token	<y_barMod>
	BAR_TYPE

%token	<y_shape>
	SHAPE

%token	<y_stackMod>
	STACK_TYPE

%token	<y_labelDir>
	LABEL_DIR

%token	<y_barGroup>
	GROUP

%token	SCALE BAR_HEIGHT BAR_LENGTH MARGIN_WIDTH MARGIN_DEPTH BASE_HEIGHT
	BASE_COLOR BASE_LABEL GRID_WIDTH GRID_DEPTH GAP_LABEL GRID_SPACE
	GAP_WIDTH  GAP_DEPTH COLORLIST OPENB CLOSEB COLORSCALE GRID SHOW HIDE 
	LABEL LABEL_MARGIN LABEL_COLOR DIRECTION SIZE TEXT BAR METRICLIST 
	METRICLABEL INSTLABEL STACK STACK_LABEL UTIL AWAY TOWARDS
	BOX SOCKET PIPE PIPE_LENGTH PIPETAG GR_LINK GR_XING HISTORY SCENE_FILE

%type	<y_real>
	real

%type	<y_bool>
	show_or_hide
	hide_or_show

%type	<y_textDir>
	direction_cmd

%type	<y_fontSize>
	size_cmd

%type	<y_instDir>
	col_or_row

%type	<y_barMod>
	bar_type

%type	<y_shape>
	shape_type

%type	<y_stackMod>
	stack_type

%type	<y_labelDir>
	away_or_towards

%type	<y_barGroup>
	bar_group

%type	<y_object> 
	scene object grid bar stack label pipe link xing scenefile

%type	<y_str>
	symname nameval colordecl linktag metricname
%%

config		: header scene;

header 		: pmview arglist
                | pmview
                ;

pmview		: PMVIEW
		{
		    if (strcmp($1, "1.2") < 0) {
			pmprintf("Version %s is no longer supported\n",
				 $1);
			pmflush();
			exit(1);
		    }
		}
		;
 
arglist		: arg 
		| arglist arg
		;

arg		: STRING 
		{
		    extern char **frontend_argv;
		    extern int  frontend_argc;

                    frontend_argv = (char **)realloc(frontend_argv,
						     (frontend_argc+1)*sizeof(char *));
                    if (frontend_argv == NULL) {
                        frontend_argc = 0;
                    } else {
			frontend_argv[frontend_argc] =$1;
			if (frontend_argv[frontend_argc] == NULL) {
			    frontend_argc = 0;
			} else {
			    frontend_argc++;
			}
		    }
		}
		;

scene		: object		{ rootObj = $1; }
		| sceneattrlist object	{ rootObj = $2; }
		;

object		: grid 
		| label
		| bar
		| pipe 
		| link
		| xing
		| stack
		| scenefile
		;

sceneattrlist	: sceneattr
		| sceneattrlist sceneattr
		;

sceneattr	: scale			 
		| named_color_list
		| named_color_scale
		| most_opts
		;

most_opts	: mod_opts
		| base_opts
		| grid_opts
		| bar_opts
		| label_opts
		| pipe_opts
		;

scale		: SCALE REAL 
		{
		    if ($2 <= 0.0)
			yyerror("_scale requires a positive real number");
		    else 
			theGlobalScale = $2;
		}
		;

pipe_opts	: PIPE_LENGTH INT
		{
		    if ($2 <= 0)
			yyerror("_pipeLength requires a positive integer");
		    else if ( objstack.empty () )
			dobj.pipeLength() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->pipeLength() = $2;
		    } 
		    else
			yyerror ("cannot change pipe length - not a grid");
		}
		;

mod_opts	: BAR_HEIGHT INT 
		{
		    if ($2 <= 0)
			yyerror("_barHeight requires a positive integer");
		    else if ( objstack.empty () )
			dobj.barHeight() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->barHeight() = $2;
		    }
		    else
			yyerror ("cannot change bar height - not a grid");
		}
		| BAR_LENGTH INT 
		{
		    if ($2 <= 0)
			yyerror("_barLength requires a positive integer");
		    else if ( objstack.empty () )
			dobj.barLength() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->barLength() = $2;
		    }
		    else 
			yyerror ("cannot change bar length - not a grid");
		}
		;

base_opts	: MARGIN_WIDTH INT 
		{
		    if ($2 <= 0)
			yyerror("_marginWidth requires a positive integer");
		    else if ( objstack.empty () )
			dobj.baseBorderX() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->baseBorderX() = $2;
		    }
		    else
			yyerror("cannot change width margin - not a grid");
		}
		| MARGIN_DEPTH INT 
		{
		    if ($2 <= 0)
			yyerror("_marginDepth requires a positive integer");
		    else if ( objstack.empty () )
			dobj.baseBorderZ() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
 			go->defs()->baseBorderZ() = $2;
		    }
		    else
			yyerror ("cannot change depth margin - not a grid");
		}
		| BASE_HEIGHT INT 
		{
		    if ($2 <= 0)
			yyerror("_baseHeight requires a positive integer");
		    else if ( objstack.empty () )
			dobj.baseHeight() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->baseHeight() = $2;
		    }
		    else 
			yyerror ("cannot change base height - not a grid");
		}
		| BASE_COLOR symname
		{
		    float r, g, b;
		    
		    if (ColorList::findColor($2, r, g, b) == true ) {
			if ( objstack.empty () )
			    dobj.baseColor (r,g,b);
			else if(objstack.top()->objbits()&ViewObj::GRIDOBJ){
			    GridObj * go = 
				static_cast<GridObj*>(objstack.top());

			    go->defs()->baseColor(r, g, b);
			}
 			else
			    yyerror ("Cannot change base color - not a grid");

		    } else {
			sprintf(theBuffer, 
				"unable to map _baseColor color \"%s\"",
				$2);
			yyerror(theBuffer);
		    }
		}
		| BASE_COLOR REAL REAL REAL
		{
		    if ( $2 < 0.0 || $2 > 1.0 || 
			 $3 < 0.0 || $3 > 1.0 || 
			 $4 < 0.0 || $4 > 1.0) {
			sprintf(theBuffer, 
				"_baseColor colors %f,%f,%f must be "
				"between 0.0 and 1.0",
				$2, $3, $4);
			yyerror(theBuffer);
		    } else if ( objstack.empty () ) {
			dobj.baseColor($2, $3, $4);
		    } else if(objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->baseColor($2, $3, $4);
		    } else {
			yyerror("Cannot change base color - not a grid");
		    }
		}
		;

grid_opts	: GRID_WIDTH INT 
		{
		    if ($2 <= 0)
			yyerror("_gridWidth requires a positive integer");
		    else if ( objstack.empty() )
			dobj.gridMinWidth() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->gridMinWidth() = $2;
		    }
		    else
			yyerror("Cannot change min width - not a grid");
		}
		| GRID_DEPTH INT 
		{
		    if ($2 <= 0)
			yyerror("_gridDepth requires a positive integer");
		    else if ( objstack.empty() )
			dobj.gridMinDepth() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->gridMinDepth() = $2;
		    }
		    else
			yyerror("Cannot change min depth - not a grid");
		}
		| GRID_SPACE INT 
		{
		    if ($2 <= 0) {
			yyerror("_gridSpace requires a positive integer");
		    } else if ( objstack.empty() ) {
			dobj.gridMinWidth() = $2;
			dobj.gridMinDepth() = $2;
		    } else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->gridMinWidth() = $2;
			go->defs()->gridMinDepth() = $2;
		    } else {
			yyerror("Cannot change grid size - not a grid");
		    }
		}
		;

bar_opts	: GAP_WIDTH INT 
		{
		    if ($2 <= 0)
			yyerror("_gapWidth requires a positive integer");
		    else if ( objstack.empty() )
			dobj.barSpaceX() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->barSpaceX() = $2;
		    }
		    else
			yyerror ("Cannot change bar width - not a grid");
		}
		| GAP_DEPTH INT
		{
		    if ($2 <= 0)
			yyerror("_gapDepth requires a positive integer");
		    else if ( objstack.empty() )
			dobj.barSpaceZ() = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->barSpaceZ() = $2;
		    }
		    else
			yyerror ("Cannot change bar depth - not a grid");
		}
		| GAP_LABEL INT 
		{
		    if ($2 <= 0)
			yyerror("_gapLabel requires a positive integer");
		    else if ( objstack.empty() )
			dobj.barSpaceLabel () = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->barSpaceLabel() = $2;
		    }
		    else
			yyerror ("Cannot change label space - not a grid");
		}
		;

label_opts	: LABEL_MARGIN INT
		{
		    if ($2 <= 0)
			yyerror("_labelMargin requires a positive integer");
		    else  if ( objstack.empty() )
 			dobj.labelMargin () = $2;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj*>(objstack.top());
			go->defs()->labelMargin() = $2;
		    }
		    else
			yyerror ("Cannot change label margin - not a grid");
		}
		| LABEL_COLOR symname
		{
		    float r, g, b;

		    if (ColorList::findColor($2, r, g, b) == true) {
			if ( objstack.empty() )
			    dobj.labelColor(r, g, b);
			else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			    GridObj * go = 
				static_cast<GridObj*>(objstack.top());
			    go->defs()->labelColor(r, g, b);
			}
			else
			    yyerror ("Cannot change label color - not a grid");
		    } else {
			sprintf(theBuffer, "unable to map color \"%s\"", $2);
			yyerror(theBuffer);
		    }
		}
		| LABEL_COLOR REAL REAL REAL 
		{
		    if ($2 < 0.0 || $2 > 1.0 || $3 < 0.0 || $3 > 1.0 || 
			$4 < 0.0 || $4 > 1.0) {
			sprintf(theBuffer, "unable to map color %f,%f,%f",
				$2, $3, $4);
			yyerror(theBuffer);
		    } else {
			if ( objstack.empty () )
			    dobj.labelColor ($2, $3, $4);
			else if(objstack.top()->objbits()&ViewObj::GRIDOBJ){
			    GridObj * go = 
				static_cast<GridObj*>(objstack.top());
			    go->defs()->labelColor($2, $3, $4);
			}
			else
			    yyerror ("Cannot change label color - not a grid");
		    }
		}
		;

symname		: NAME | STRING;

named_color_list: COLORLIST NAME OPENB 
		{
		    if (theColorLists.add($2) == false) {
			sprintf(theBuffer,
				"Color list \"%s\" is already defined", $2);
			yywarn(theBuffer);
		    }
		} colors CLOSEB
		;

colors		: color
		| colors color
		;

color		: symname 
		{
		    if (theColorLists.addColor($1) == false) {
			sprintf(theBuffer, "Unable to map color \"%s\"", $1);
			yywarn(theBuffer);
		    }
		}
		| REAL REAL REAL 
		{
		    if (theColorLists.addColor($1, $2, $3) == false) {
			sprintf(theBuffer, 
				"Unable to map color %f,%f,%f, "
				"values may be out of range",
				$1, $2, $3);
			yywarn(theBuffer);
		    }			
		}
		;

named_color_scale: COLORSCALE NAME symname OPENB
		{
		    if (theColorLists.list($2) == NULL) {
			if (theColorLists.add($2, $3) == false) {
			    sprintf(theBuffer, 
				    "Unable to map color \"%s\", "
				    "defaulting to blue",
				    $3);
			}
			theColorScaleFlag = true;
		    } else {
			sprintf(theBuffer, 
				"Color scale \"%s\" is already defined",
				$2);
			yywarn(theBuffer);
			theColorScaleFlag = false;
		    }
		} scaled_color_list CLOSEB
		| COLORSCALE NAME REAL REAL REAL OPENB
		{
		    if (theColorLists.list($2) == NULL) {
			if (theColorLists.add($2, $3, $4, $5) == false) {
			    sprintf(theBuffer, 
				    "Unable to map color %f,%f,%f, "
				    "defaulting to blue",
				    $3, $4, $5);
			}
			theColorScaleFlag = true;
		    } else {
			sprintf(theBuffer, 
				"Color scale \"%s\" is already defined", $2);
			yywarn(theBuffer);
			theColorScaleFlag = false;
		    }
		} scaled_color_list CLOSEB
		;

scaled_color_list : scaled_color
		| scaled_color_list scaled_color
		;

scaled_color	: symname REAL 
		{
		    if (theColorLists.addColor($1, $2) == false) {
			sprintf(theBuffer, "Unable to map color \"%s\"", $1);
			yywarn(theBuffer);
		    }
		}
		| REAL REAL REAL REAL
		{
		    if (theColorLists.addColor($1, $2, $3, $4) == false) {
			sprintf(theBuffer, 
				"Unable to map color %f,%f,%f, "
				"values may be out of range",
				$1, $2, $3);
			yywarn(theBuffer);
		    }			
		}
		;

pos		: grid_pos {
			theNumCols = 1;
			theNumRows = 1;
			theAlignment = ViewObj::center;		
		}
		| grid_pos grid_size {
			theAlignment = ViewObj::center;
		}
		| grid_pos alignment {
			theNumCols = 1;
			theNumRows = 1;
		}
		| grid_pos grid_size alignment
		| /* Empty position */ {
			theCol = 0;
			theRow = 0;
			theNumCols = 1;
			theNumRows = 1;
			theAlignment = ViewObj::center;
		}
		;

grid_pos	: INT INT
		{
		    if ($1 < 0) {
			sprintf(theBuffer,
				"Column index must be positive, was %d",
				$1);
			yyerror(theBuffer);
			theCol = 0;
		    } else
			theCol = $1;

		    if ($2 < 0) {
			sprintf(theBuffer,
				"Row index must be positive, was %d",
				$2);
			yyerror(theBuffer);
			theRow = 0;
		    } else
			theRow = $2;
		}
		;

grid_size	: INT INT 
		{
		    if ($1 < 0) {
			sprintf(theBuffer,
				"Number of columns must be positive, was %d",
				$1);
			yyerror(theBuffer);
			theNumCols = 1;
		    } else
			theNumCols = $1;
		    if ($2 < 0) {
			sprintf(theBuffer,
				"Number of rows must be positive, was %d",
				$2);
			yyerror(theBuffer);
			theNumRows = 1;
		    } else
			theNumRows = $2;
		}
		;

alignment	: ALIGN { theAlignment = $1; };

baselabelspec	: BASE_LABEL symname
		{ 
		    if ( objstack.empty () ) {
			yyerror ("Syntax error - no object to label");
		    } else if(objstack.top()->objbits() & ViewObj::BASEOBJ){
			BaseObj * bo = static_cast<BaseObj *>(objstack.top());
			int i;
			QString str = $2;

			for (i = 0; i < str.size(); i++) {
			    if (str[i] == '\n')
				break;
			    if (str[i] == '\\' &&
				i + 1 < str.size() &&
				str[i + 1] == 'n') {
				str[i] = '\n';
				str.remove(i+1, 1);
				break;
			    }
			}
    
			if (i == str.length())
			    str.append(QChar('\n'));
    
			bo->label() = str;
		    } else {
			yyerror ("Syntax error - wrong object");
		    }
		}
		;

scenefile	: scenefile_decl
		{
		    if ( ($$ = objstack.top()) ) {
			$$->finishedAdd();
		    }
		    objstack.pop();
		}
		;

scenefile_decl	: SCENE_FILE pos STRING
		{
		    const DefaultObj * dob = 0;
		
		    if ( objstack.empty() )
			dob = & dobj;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * parent =
			    static_cast<GridObj *>(objstack.top());
			dob = parent->defs();
		    }

		    if (dob) {
			if ( SceneFileObj * so = new SceneFileObj (*dob,
							 theCol, theRow,
							 theNumCols,
							 theNumRows,
							 theAlignment) ) {
			    so->setSceneFileName($3);
			    objstack.push (so);
			}
			else 
			    yyerror (
				"Cannot create a scene file object - out of memory");
		    }
		    else
			yyerror (
			    "Syntax error - Scene File inside simple object!"); 
		}
		;

pipe		: pipedecl OPENB pipespec CLOSEB
		{
		    if ( ($$ = objstack.top()) ) {
			$$->finishedAdd();
		    }
		    objstack.pop();
		}
		;

pipedecl	: PIPE pos
		{
		    if ( theNumRows > 1 && theNumCols > 1 ) {
			yyerror ("Diagonal pipes are not supported");
			objstack.push (0); // So that we would pop grid up
		    } else {
			const DefaultObj * dob = 0;
		    
			if ( objstack.empty() )
			    dob = & dobj;
			else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			    GridObj * parent =
				static_cast<GridObj *>(objstack.top());
			    dob = parent->defs();
			}

			if ( dob )
			    if ( PipeObj * po = new PipeObj (*dob,
							     theCol, theRow,
							     theNumCols,
							     theNumRows,
							     theAlignment) )
				objstack.push (po);
			    else 
				yyerror (
				    "Cannot create a pipe - out of memory");
			else
			    yyerror (
				"Syntax error - Pipe inside simple object"); 
		    }
		}
		;

pipespec	: pipeattr
		| pipespec pipeattr
		;

pipeattr	: metric_list
		| named_color
		| color_list
		| pipetag
		;

pipetag		: PIPETAG symname
		{
		    if (objstack.empty () ||
			((objstack.top()->objbits() & ViewObj::PIPEOBJ) == 0)) { 
			yyerror ("No pipe to attach tag to");
		    } else {
			PipeObj * p = static_cast<PipeObj*>(objstack.top());
			p->setTag($2);
		    }
		}
		;

link		: GR_LINK pos linktag
		{
		    const DefaultObj * dob = 0;
		    
		    if ( objstack.empty() )
			dob = & dobj;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * parent =
			    static_cast<GridObj *>(objstack.top());
			dob = parent->defs();
		    }

		    if ( dob )
			if ( Link * l = new Link (*dob, theCol, theRow,
						  theNumCols, theNumRows,
						  theAlignment) ) {
			    if ( $3 != NULL ) {
				l->setTag ($3);
				free ($3);
			    }

			    l->finishedAdd ();
			    $$ = l;
			} else {
			    yyerror ("Cannot create a link - out of memory");
			}
		    else
			yyerror ("Syntax error - link inside simple object"); 
		}
		;

linktag		: symname	{ $$ = strdup ($1); }
		| /* nothing */	{ $$ = NULL; } 
		;

grid_object	: object
		{
		    if ( $1 ) {
			if ( (! objstack.empty()) &&
			     (objstack.top()->objbits() & ViewObj::GRIDOBJ) ) {
			    GridObj * go = 
				static_cast<GridObj *>(objstack.top());
			    go->addObj ($1 , $1->col(), $1->row());
			} else {
			    yyerror ("Syntax error - no gird to add to"); 
			}
		    }
		}
		| most_opts
		| baselabelspec
		;

grid_object_list : grid_object
		| grid_object_list grid_object
		;

grid		: griddecl grid_object_list CLOSEB 
		{
		    $$ = objstack.top();
		    $$->finishedAdd();
		    objstack.pop();
		}
		;

griddecl	: GRID pos hide_or_show OPENB 
		{
		    const DefaultObj * dob = 0;
		    
		    if ( objstack.empty() )
			dob = & dobj;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * parent =
			    static_cast<GridObj *>(objstack.top());
			dob = parent->defs();
		    }

		    if ( dob )
			if (GridObj * go = new GridObj($3, *dob,
						       theCol, theRow,
						       theNumCols, theNumRows,
						       theAlignment)) {
			    objstack.push (go);
			} else {
			    yyerror ("Cannot create new grid - out of memory");
			}
		    else
			yyerror ("Syntax error - grid inside a simple object");
		}
		;

hide_or_show	: BOOL { $$ = $1; }
		| { $$ = false; }
		;

label		: labeldecl OPENB label_stuff CLOSEB
		{
		    $$ = objstack.top();
		    $$->finishedAdd();
		    objstack.pop();
		}
		| labeldecl direction_cmd size_cmd STRING
		{
		    if ( ($$ = objstack.top()) ) {
			if ( $$->objbits() & ViewObj::LABELOBJ ) {
			    LabelObj * lo = static_cast<LabelObj*>($$);
			    lo->dir() = $2;
			    lo->size() = $3;
			    lo->str() = $4;
			    lo->finishedAdd ();
			}
		    }
		    objstack.pop();
		}
		;

labeldecl	: LABEL pos
		{
		    const DefaultObj * dob = 0;

		    if ( objstack.empty() )
			dob = & dobj;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * parent =
			    static_cast<GridObj *>(objstack.top());
			dob = parent->defs();
		    }
		    else
			yyerror ("Syntax error - label inside simple object");


		    if ( dob ) {
			if (LabelObj * lo = new LabelObj(*dob, 
							 theCol, theRow,
							 theNumCols,
							 theNumRows,
							 theAlignment))
			    objstack.push (lo);
			else {
			    yyerror ("Cannot create label - out of memory");
			    objstack.push (NULL);
			}
		    }
		}
		;

		
direction_cmd	: DIRVAL { $$ = $1; }
		| { $$ = Text::right; }
		;

size_cmd	: SIZ { $$ = $1; }
		| { $$ = Text::medium; }
		;

label_stuff	: label_item
		| label_stuff label_item
		;

label_item	: DIRECTION DIRVAL
		{
		    if ( objstack.empty () )
			yyerror ("cannot change direction - no label");
		    else if (objstack.top()->objbits() & ViewObj::LABELOBJ) {
			LabelObj * lo =
			    static_cast<LabelObj*>(objstack.top());
			lo->dir() = $2;
		    }
		    else
			yyerror ("Syntax error - not a label");
		}
		| SIZE SIZ
		{
		    if ( objstack.empty () )
			yyerror ("cannot set label size");
		    else if (objstack.top()->objbits() & ViewObj::LABELOBJ) {
			LabelObj * lo =
			    static_cast<LabelObj*>(objstack.top());
			lo->size() = $2;
		    }
		    else
			yyerror ("Syntax error - not a label");
		}
		| TEXT symname
		{
		    if ( objstack.empty () )
			yyerror ("cannot set label text");
		    else if (objstack.top()->objbits() & ViewObj::LABELOBJ) {
			LabelObj * lo =
			    static_cast<LabelObj*>(objstack.top());
			lo->str() = $2;
		    }
		    else
			yyerror ("Syntax error - not a label");
		}
		;

bar		: bardecl bar_stuff CLOSEB 
		{ 
		    $$ = objstack.top();
		    $$->finishedAdd();
		    objstack.pop();
		}
		;

history		: HISTORY INT
		{
			theHistory = $2;
		}
		|
		;

bardecl		: BAR pos col_or_row show_or_hide bar_type shape_type bar_group history OPENB 
		{
		    BarObj * bo = 0;

		    if ( objstack.empty () )
			bo = new BarObj($6, $3, $5, $7, $4, dobj,
					theCol, theRow,
					theNumCols, theNumRows, theAlignment);
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ){
			GridObj * go =
			    static_cast<GridObj *>(objstack.top());
			bo = new BarObj($6, $3, $5, $7, $4, *go->defs(),
					theCol, theRow,
					theNumCols, theNumRows, theAlignment);
		    }
		    else
			yyerror ("Syntax error - bar inside simple object");
		
		    if ( bo ) {
		    	bo->setHistory(theHistory);
			objstack.push (bo);
		    }
		} 
		;
		
show_or_hide	: BOOL { $$ = $1; }
		| { $$ = true; }
		;

col_or_row	: INST_DIR { $$ = $1; }
		| { $$ = BarMod::instPerCol; }
		;

bar_type	: BAR_TYPE { $$ = $1; }
		| { $$ = BarMod::yScale; }
		;
shape_type	: SHAPE { $$ = $1 ; }
		| { $$ = ViewObj::cube; }
		;

bar_group	: GROUP { $$ = $1; }
		| { $$ = BarMod::groupByMetric; }
		;

bar_stuff	: bar_item
		| bar_stuff bar_item
		;

bar_item	: labelled_metric_list
		| named_color
		| color_list
		| metric_labels
		| inst_labels
		| baselabelspec 
		;

labelled_metric_list: METRICLIST OPENB labelled_metrics CLOSEB;

labelled_metrics: labelled_metric
		| labelled_metrics labelled_metric
		;

metricname	: METRIC | NAME ;

labelled_metric	: metric 
		| metricname real STRING
		{
		    if ( objstack.empty() )
			yyerror ("No object to add metrics to");
		    else if (objstack.top()->objbits() & ViewObj::BAROBJ) {
			BarObj * bo =
			    static_cast<BarObj *>(objstack.top());
			bo->addMetric($1, $2, $3);
		    } else {
			yyerror ("Syntax error - not a bar object");
		    }
		}
		;

real		: REAL	{ $$ = $1; }
		| INT	{ $$ = (double)$1; }
		;

colordecl	: COLORLIST NAME	{ $$ = $2; }
		| COLORSCALE NAME	{ $$ = $2; }
		;

named_color	: colordecl
		{
		    if ( objstack.empty() )
			yyerror ("No object to add colors to");
		    else if (objstack.top()->objbits() & ViewObj::MODOBJ) {
			ModObj * mo =
			    static_cast<ModObj *>(objstack.top());
			mo->setColorList ($1);
		    } else {
			sprintf (theBuffer, 
				 "Syntax error - %s cannot have colors",
				 objstack.top()->name());
			yyerror (theBuffer);
		    }
		}
		;

color_list	: COLORLIST OPENB 
		{
		    if ( objstack.empty() )
			yyerror ("No object to add colors to");
		    else if (objstack.top()->objbits() & ViewObj::MODOBJ) {
			ModObj * mo = static_cast<ModObj *>(objstack.top());
			sprintf(theBuffer, "@tmp%d", theColorListCount++);
			theColorLists.add(theBuffer); 
			mo->setColorList (theBuffer);
		    } else {
			sprintf (theBuffer, 
				 "Syntax error - %s cannot have colors",
				 objstack.top()->name());
			yyerror (theBuffer);
		    }
		} colors CLOSEB 
		| COLORSCALE symname OPENB
		{
		    if ( objstack.empty() )
			yyerror ("No object to add colors to");
		    else if (objstack.top()->objbits() & ViewObj::MODOBJ) {
			ModObj * mo = static_cast<ModObj *>(objstack.top());
			sprintf(theBuffer, "@tmp%d", theColorListCount++);
			theColorLists.add(theBuffer, $2);
			mo->setColorList (theBuffer);
		    } else {
			sprintf (theBuffer, 
				 "Syntax error - %s cannot have colors",
				 objstack.top()->name());
			yyerror (theBuffer);
		    }
		} scaled_color_list CLOSEB
		| COLORSCALE REAL REAL REAL OPENB
		{
		    if ( objstack.empty() )
			yyerror ("No object to add colors to");
		    else if (objstack.top()->objbits() & ViewObj::MODOBJ) {
			ModObj * mo = static_cast<ModObj *>(objstack.top());

			sprintf(theBuffer, "@tmp%d", theColorListCount++);
			theColorLists.add(theBuffer, $2, $3, $4);
			mo->setColorList (theBuffer);
		    } else {
			sprintf (theBuffer, 
				 "Syntax error - %s cannot have colors",
				 objstack.top()->name());
			yyerror (theBuffer);
		    }
		} scaled_color_list CLOSEB
		;

away_or_towards	: LABEL_DIR	{ $$ = $1; }
		|		{ $$ = BarObj::towards; }
		;

metric_labels	: metriclabeldecl
		| metriclabeldecl OPENB metric_name_list CLOSEB
		;

metriclabeldecl	: METRICLABEL away_or_towards 
		{
		    if ( objstack.empty () )
			yyerror ("No object to add metric labels to");
		    else if (objstack.top()->objbits() & ViewObj::BAROBJ) {
			BarObj * bo = static_cast<BarObj *>(objstack.top());
			bo->metricLabelDir() = $2;
		    }
		    else
			yyerror ("Syntax error - not a bar object");
		}
		;

metric_name_list: metric_name
		| metric_name_list metric_name
		;

metric_name	: nameval
		{
		    if ( objstack.empty() )
			yyerror ("No object to add metric names to");
		    else if (objstack.top()->objbits() & ViewObj::BAROBJ) {
			BarObj * bo = static_cast<BarObj *>(objstack.top());
			bo->addMetricLabel($1);
		    }
		    else
			yyerror ("Syntax error - not a bar object");
		    free ($1);
		}
		;

nameval		: symname { $$ = strdup ($1); }
		| INT  { sprintf(theBuffer,"%d",$1); $$ = strdup (theBuffer); }
		| REAL { sprintf(theBuffer,"%f",$1); $$ = strdup (theBuffer); }
		;

inst_labels	: INSTLABEL away_or_towards OPENB inst_name_list CLOSEB
		{
		    if ( objstack.empty () )
			yyerror ("No object to add instance labels to");
		    else if (objstack.top()->objbits() & ViewObj::BAROBJ) {
			BarObj * bo = static_cast<BarObj *>(objstack.top());
			bo->instLabelDir() = $2;
		    }
		    else
			yyerror ("Syntax error - not a bar object");

		}
		;

inst_name_list	: inst_name
		| inst_name_list inst_name
		;

inst_name	: nameval 
		{
		    if ( objstack.empty() )
			yyerror ("No object to add instance labels to");
		    else if (objstack.top()->objbits() & ViewObj::BAROBJ) {
			BarObj * bo = static_cast<BarObj *>(objstack.top());
			bo->addInstLabel($1);
		    }
		    else
			yyerror ("Syntax error - not a bar object");
		    free ($1);
		}
		;

stack		: stackdecl stack_stuff CLOSEB 
		{
		    $$ = objstack.top();
		    $$->finishedAdd();
		    objstack.pop();
		}
		;

stackdecl	: STACK pos show_or_hide stack_type shape_type history OPENB 
		{
		    StackObj * so = 0;
 
		    if ( objstack.empty () )
			so = new StackObj($4, $5, $3, dobj,
					  theCol, theRow,
					  theNumCols, theNumRows,
					  theAlignment);
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj *>(objstack.top());
			so = new StackObj($4, $5, $3, *go->defs(),
					  theCol, theRow,
					  theNumCols, theNumRows,
					  theAlignment);
		    }
		    else
			yyerror ("Syntax error - stack inside simple object");
		
		    if ( so ) {
			so->setHistory(theHistory);
			objstack.push (so);
		    }
		}
		| UTIL pos OPENB 
		{
		    StackObj * so = 0;

		    if ( objstack.empty () )
			so = new StackObj(StackMod::unfixed, ViewObj::cube,
					  false, dobj,
					  theCol, theRow,
					  theNumCols, theNumRows, 
					  theAlignment);
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * go = static_cast<GridObj *>(objstack.top());

			so = new StackObj(StackMod::unfixed, ViewObj::cube,
					  false, *go->defs(),
					  theCol, theRow,
					  theNumCols, theNumRows, 
					  theAlignment);
		    }
		    else
			yyerror ("Syntax error - stack inside simple object");
		
		    if ( so ) {
			objstack.push (so);
		    }
		}
		;

stack_type	: STACK_TYPE { $$ = $1; }
		| { $$ = StackMod::unfixed; }
		;

stack_stuff	: stack_item
		| stack_stuff stack_item
		;

stack_item	: metric_list
		| named_color
		| color_list
		| baselabelspec
		| STACK_LABEL symname 
		{
		    if ( objstack.empty() ) {
			yyerror ("Syntax error - no stack to label");
		    } else if (objstack.top()->objbits() & ViewObj::STACKOBJ) {
			StackObj * so = static_cast<StackObj*>(objstack.top());

			if (so->height() == StackMod::fixed) {
			    int i;
			
			    QString str = $2;
			    for (i = 0; i < str.size(); i++) {
				if (str[i] == '\n')
				    break;
				if (str[i] == '\\' &&
				    i + 1 < str.size() &&
				    str[i + 1] == 'n') {
				    str[i] = '\n';
				    str.remove(i+1, 1);
				    break;
				}
			    }
			    if (i == str.length())
				str.append(QChar('\n'));
			    so->setFillText((const char *)str.toLatin1());
			} else {
			    yyerror("_stackLabel may only be applied to "
				    "filled stacks");
			}
		    } else {
			yyerror ("Syntax error - not a stack");
		    }
		}
		;

metric_list	: METRICLIST OPENB metrics CLOSEB;

metrics 	: metric
		| metrics metric
		;

metric		: metricname real 
		{ 
		    if ( objstack.empty () ) 
			yyerror ("Syntax error - no object");
		    else if (objstack.top()->objbits() & ViewObj::MODOBJ) {
			ModObj * mo = static_cast<ModObj *>(objstack.top());
			mo->addMetric($1, $2);
		    }
		    else
			yyerror ("The object has no metrics");
		}
		;

xing		: GR_XING INT INT INT INT ALIGN ALIGN ALIGN ALIGN
		{
		    const DefaultObj * dob = 0;
		    ViewObj::Alignment c[4] = { $6, $7, $8, $9};

		    if ( objstack.empty() )
			dob = & dobj;
		    else if (objstack.top()->objbits() & ViewObj::GRIDOBJ) {
			GridObj * parent =
			    static_cast<GridObj *>(objstack.top());
			dob = parent->defs();
		    }
		    else
			yyerror ("Syntax error - label inside simple object");

		    if ( Xing * xo = new Xing (*dob, $2, $3, $4, $5, c) ) {
			xo->finishedAdd ();
			$$ = xo;
		    } else {
			$$ = 0;
		    }
		}
		;
%%
