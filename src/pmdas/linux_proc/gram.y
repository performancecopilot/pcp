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
 */

%{
#include <stdio.h>
#include "./gram_node.h"

extern void yyerror(const char *s);
extern int yylex(void);
extern int yyparse(void);

extern int yy_scan_string(const char *);
#define YYDEBUG 0
#define YYERROR_VERBOSE 0

int need_psusage;
int need_accounting;
static bool_node *pred_tree;
extern char *conf_buffer;

%}

%union {
	char		*y_str;
	double		y_number;
	bool_node	*y_node;
	}

%token <y_number>
	NUMBER

%token <y_str>
	STRING
	PATTERN

%term   AND OR NOT 
	LPAREN RPAREN TRUE FALSE
	EQUAL NEQUAL
	LTHAN LEQUAL GTHAN GEQUAL
	MATCH NMATCH
	GID UID CPUBURN GNAME UNAME FNAME PSARGS
	SYSCALLS CTXSWITCH VIRTUALSIZE RESIDENTSIZE
	IODEMAND IOWAIT SCHEDWAIT
	VERSION
	
%type <y_node>
	predicate comparison
	num_compar numvar
	str_compar strvar
	pattern_compar

%type <y_number>
	version

%left OR 
%left AND
%left NOT

%token END 0 "end of file"

%%

pred_tree: END { pred_tree = NULL; }
	| predicate END { pred_tree = $1;}
	| version predicate END { pred_tree = $2;}
	;

version: VERSION NUMBER { 
	float version_num = $2;

	if (version_num != 1.0) {
	    fprintf(stderr, "Wrong version number in configuration predicate\n");
	    fprintf(stderr, "Expected version %.2f, but was given version %.2f .\n",
		1.0, version_num);
	    YYABORT;
	}
    }

predicate:
	  predicate AND predicate { $$ = create_tnode(N_and, $1, $3); }
	| predicate OR  predicate { $$ = create_tnode(N_or,  $1, $3); }
	| NOT predicate { $$ = create_tnode(N_not, $2, NULL); }
	| LPAREN predicate RPAREN { $$ = $2; }
	| comparison
	| TRUE { $$ = create_tag_node(N_true); }
	| FALSE { $$ = create_tag_node(N_false); }
	;

comparison:
	  num_compar
	| str_compar
	| pattern_compar
	;

num_compar:
	  numvar LTHAN numvar { $$ = create_tnode(N_lt, $1, $3); }
	| numvar LEQUAL numvar { $$ = create_tnode(N_le, $1, $3); }
	| numvar GTHAN numvar { $$ = create_tnode(N_gt, $1, $3); }
	| numvar GEQUAL numvar { $$ = create_tnode(N_ge, $1, $3); }
	| numvar EQUAL numvar { $$ = create_tnode(N_eq, $1, $3); }
	| numvar NEQUAL numvar { $$ = create_tnode(N_neq, $1, $3); }
	;

numvar:   NUMBER  { $$ = create_number_node($1); }
	| GID { $$ = create_tag_node(N_gid); }
	| UID { $$ = create_tag_node(N_uid); }
	| CPUBURN  { $$ = create_tag_node(N_cpuburn); }
	| SYSCALLS { need_psusage = 1; $$ = create_tag_node(N_syscalls); }
	| CTXSWITCH { need_psusage = 1; $$ = create_tag_node(N_ctxswitch); }
	| VIRTUALSIZE { $$ = create_tag_node(N_virtualsize); }
	| RESIDENTSIZE { $$ = create_tag_node(N_residentsize); }
	| IODEMAND { need_psusage = 1; $$ = create_tag_node(N_iodemand); }
	| IOWAIT { need_accounting = 1; $$ = create_tag_node(N_iowait); }
	| SCHEDWAIT { need_accounting = 1; $$ = create_tag_node(N_schedwait); }
	;

str_compar:
	  strvar EQUAL strvar { $$ = create_tnode(N_seq, $1, $3); } 
	| strvar NEQUAL strvar { $$ = create_tnode(N_sneq, $1, $3); } 
	;

strvar:	  STRING { $$ = create_str_node($1); }
	| GNAME  { $$ = create_tag_node(N_gname); }
	| UNAME  { $$ = create_tag_node(N_uname); }
	| FNAME  { $$ = create_tag_node(N_fname); }
	| PSARGS { $$ = create_tag_node(N_psargs); }
	;	

pattern_compar:
	  strvar MATCH PATTERN { $$ = create_tnode(N_match, $1, create_pat_node($3)); }
	| strvar NMATCH PATTERN { $$ = create_tnode(N_nmatch, $1, create_pat_node($3)); }
	;

%%

int
parse_predicate(bool_node **tree)
{
    int sts; 
    extern int yylineno; /* defined by lex */

    yylineno=1;

    start_tree();
    yy_scan_string( conf_buffer );
    /* yyparse returns 0 on success, 1 on parse error, 2 on memory error */
    sts = yyparse();

    /* free any partial trees */
    if (sts != 0) {
	free_tree(NULL);
	return sts;
    }

    *tree = pred_tree;
    return 0;
}
