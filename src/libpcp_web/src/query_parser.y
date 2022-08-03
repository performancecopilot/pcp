%{
/*
 * query_parser.y - yacc/bison grammar for the PCP time series language
 *
 * Copyright (c) 2017-2022 Red Hat.
 * Copyright (c) 2020 Yushan ZHANG.
 * Copyright (c) 2022 Shiyao CHEN.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include <ctype.h>
#include "query.h"
#include "load.h"
#include "schema.h"
#include "util.h"
#include "libpcp.h"

typedef struct PARSER {
    char	*yy_input;
    char	*yy_lexicon;
    int		yy_lexpeek;
    int		yy_error;
    sds		yy_errstr;
    char	*yy_tokbuf;
    int 	yy_tokbuflen;
    meta_t	yy_meta;
    sds		yy_base;
    node_t	*yy_np;
    series_t	yy_series;
} PARSER;

typedef union {
    sds		s;
    node_t	*n;
    meta_t	m;
} YYSTYPE;

#define YYSTYPE_IS_DECLARED 1
static int yylex(YYSTYPE *, PARSER *);
static int yyerror(PARSER *, const char *);

static int series_lex(YYSTYPE *, PARSER *);
static int series_error(PARSER *, const char *);
static void gramerr(PARSER *, const char *, const char *, char *);
static node_t *newnode(int);
static node_t *newmetric(char *);
static node_t *newmetricquery(char *, node_t *);
static node_t *newtree(int, node_t *, node_t *);
static void newaligntime(PARSER *, const char *);
static void newstarttime(PARSER *, const char *);
static void newinterval(PARSER *, const char *);
static void newendtime(PARSER *, const char *);
static void newrange(PARSER *, const char *);
static void newtimezone(PARSER *, const char *);
static void newsamples(PARSER *, const char *);
static void newoffset(PARSER *, const char *);
static char *n_type_str(int);
static char *n_type_c(int);
static char *l_type_str(int);

/* strings for error reporting */
static const char follow[]       = "follow";
//static const char bexpr_str[]    = "Boolean expression";
//static const char aexpr_str[]    = "Arithmetic expression";
static const char op_str[]       = "Arithmetic or relational or boolean operator";
static const char name_str[]     = "Symbol";
static const char unexpected_str[]       = "Unexpected";
static const char initial_str[]  = "Unexpected initial";
%}

/*
 * seems like the %define variant is more bisonesque, while the
 * %pure-parser maybe a yaccism that bison silently accepts ...
 * using the %define makes "deprecated directive" warnings go
 * away on some platforms - kenj Oct 2019
 * old yacc declaration was
 * %pure-parser
 */
%define api.pure

%parse-param { PARSER *lp }
%lex-param { PARSER *lp }

/***********************************************************************
 * yacc token and operator declarations
 ***********************************************************************/

%expect 0
%start      query

%token	    L_UNDEF
%token	    L_ERROR
%token	    L_EOS
%token      L_PLUS
%token      L_MINUS
%token      L_STAR
%token      L_SLASH
%token      L_COLON
%token      L_LPAREN
%token      L_RPAREN
%token      L_LBRACE
%token      L_RBRACE
%token      L_LSQUARE
%token      L_RSQUARE
%token      L_ABS
%token      L_FLOOR
%token      L_LOG
%token      L_SQRT
%token      L_ROUND
%token      L_AVG
%token      L_AVG_INST
%token      L_AVG_SAMPLE
%token      L_COUNT
%token      L_DELTA
%token      L_MAX
%token      L_MAX_INST
%token      L_MAX_SAMPLE
%token      L_MIN
%token      L_MIN_INST
%token      L_MIN_SAMPLE
%token      L_SUM
%token      L_SUM_INST
%token      L_SUM_SAMPLE
%token      L_STDEV_INST
%token      L_STDEV_SAMPLE
%token      L_TOPK_INST
%token      L_TOPK_SAMPLE
%token      L_NTH_PERCENTILE_INST
%token      L_NTH_PERCENTILE_SAMPLE
%token      L_ANON
%token      L_RATE
%token      L_INSTANT
%token      L_LT
%token      L_LEQ
%token      L_EQ
%token      L_GLOB
%token      L_GEQ
%token      L_GT
%token      L_NEQ
%token      L_REQ
%token      L_RNE
%token      L_AND
%token      L_OR
%token      L_MKCONST
%token      L_RESCALE
%token      L_DEFINED
%token      L_TYPE
%token      L_SEMANTICS
%token      L_UNITS
%token      L_ASSIGN
%token      L_COMMA
%token      L_INTERVAL
%token      L_TIMEZONE
%token      L_ALIGN
%token      L_START
%token      L_FINISH
%token      L_SAMPLES
%token      L_OFFSET

%token <m>  EVENT_UNIT
%token <m>  TIME_UNIT
%token <m>  SPACE_UNIT
%token <s>  L_INTEGER
%token <s>  L_DOUBLE
%token <s>  L_NAME
%token <s>  L_STRING
%token <s>  L_RANGE

%type  <n>  query
%type  <n>  expr
%type  <n>  func
%type  <n>  func_sid
%type  <n>  arithmetic_expression
%type  <n>  arithmetic_expr_sid
%type  <n>  exprlist
%type  <n>  exprval
%type  <n>  number
%type  <n>  integer
%type  <n>  string
%type  <s>  timespec
%type  <n>  vector
%type  <n>  val_vec
%type  <n>  sid_vec

%left  L_AND L_OR
%left  L_LT L_LEQ L_EQ L_GLOB L_COLON L_ASSIGN L_GEQ L_GT L_NEQ L_REQ L_RNE
%left  L_PLUS L_MINUS
%left  L_STAR L_SLASH

%%

/***********************************************************************
 * yacc productions
 ***********************************************************************/

query	: vector
	| L_NAME L_ASSIGN vector
		{ lp->yy_series.name = $1;
		  $$ = lp->yy_series.expr;
		  YYACCEPT;
		}
	;

vector:	L_NAME L_LBRACE exprlist L_RBRACE L_EOS
		{ lp->yy_np = newmetricquery($1, $3);
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	| L_NAME L_LBRACE exprlist L_RBRACE L_LSQUARE timelist L_RSQUARE L_EOS
		{ lp->yy_np = newmetricquery($1, $3);
		  lp->yy_np->time = lp->yy_series.time;
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	| L_LBRACE exprlist L_RBRACE L_LSQUARE timelist L_RSQUARE L_EOS
		{ lp->yy_np = lp->yy_series.expr = $2; YYACCEPT; }
	| L_LBRACE exprlist L_RBRACE L_EOS
		{ lp->yy_np = lp->yy_series.expr = $2; YYACCEPT; }
	| L_NAME L_LSQUARE timelist L_RSQUARE L_EOS
		{ lp->yy_np = newmetric($1);
		  lp->yy_np->time = lp->yy_series.time;
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	| L_NAME L_EOS
		{ lp->yy_np = newmetric($1);
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	| func L_EOS
		{ lp->yy_np = $1;
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	| func_sid L_EOS
		{ lp->yy_np = $1;
		  $$ = lp->yy_series.expr = lp->yy_np;
		  YYACCEPT;
		}
	;

exprlist : exprlist L_COMMA expr
		{ lp->yy_np = newnode(N_AND);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_np;
		}
	| expr
	;

exprval	: string
	| number
	;

string	: L_STRING
		{ lp->yy_np = newnode(N_STRING);
		  lp->yy_np->value = sdsnew($1);
		  $$ = lp->yy_np;
		}
	| L_NAME
		{ lp->yy_np = newnode(N_NAME);
		  lp->yy_np->value = sdsnew($1);
		  $$ = lp->yy_np;
		}
	;

number	: integer
	| L_DOUBLE
		{ lp->yy_np = newnode(N_DOUBLE);
		  lp->yy_np->value = sdsnew($1);
		  $$ = lp->yy_np;
		}
	;

integer	: L_INTEGER
		{ lp->yy_np = newnode(N_INTEGER);
		  lp->yy_np->value = sdsnew($1);
		  $$ = lp->yy_np;
		}
	;

timelist : timelist L_COMMA timeexpr
	| timeexpr
	;

timespec :
	  L_STRING
	| L_INTEGER
	| L_DOUBLE
	;

timeexpr : /* time window specification */
	  L_INTERVAL L_COLON timespec
		{ newinterval(lp, $3); }
	| L_TIMEZONE L_COLON L_STRING
		{ newtimezone(lp, $3); }
	| L_SAMPLES L_COLON L_INTEGER
		{ newsamples(lp, $3); }
	| L_OFFSET L_COLON L_INTEGER
		{ newoffset(lp, $3); }
	| L_ALIGN L_COLON timespec
		{ newaligntime(lp, $3); }
	| L_START L_COLON timespec
		{ newstarttime(lp, $3); }
	| L_FINISH L_COLON timespec
		{ newendtime(lp, $3); }
	| L_RANGE
		{ newrange(lp, $1); }
	;

expr	: /* relational expressions */
	  string L_LT number
		{ $$ = lp->yy_np = newtree(N_LT, $1, $3); }
	| string L_LEQ number
		{ $$ = lp->yy_np = newtree(N_LEQ, $1, $3); }
	| string L_EQ exprval
		{ $$ = lp->yy_np = newtree(N_EQ, $1, $3); }
	| string L_GEQ number
		{ $$ = lp->yy_np = newtree(N_GEQ, $1, $3); }
	| string L_GT number
		{ $$ = lp->yy_np = newtree(N_GT, $1, $3); }
	| string L_NEQ exprval
		{ $$ = lp->yy_np = newtree(N_NEQ, $1, $3); }

	/* regular expressions and globbing */
	| string L_REQ string
		{ $$ = lp->yy_np = newtree(N_REQ, $1, $3); }
	| string L_RNE string
		{ $$ = lp->yy_np = newtree(N_RNE, $1, $3); }
	| string L_COLON exprval	/* L_GLOB synonym */
		{ $$ = lp->yy_np = newtree(N_GLOB, $1, $3); }
	| string L_GLOB exprval
		{ $$ = lp->yy_np = newtree(N_GLOB, $1, $3); }

	/* boolean expressions */
	| expr L_AND expr
		{ $$ = lp->yy_np = newtree(N_AND, $1, $3); }
	| expr L_OR expr
		{ $$ = lp->yy_np = newtree(N_OR, $1, $3); }
	;

val_vec
	: L_NAME L_LBRACE exprlist L_RBRACE L_LSQUARE timelist L_RSQUARE
		{ lp->yy_np = newmetricquery($1, $3);
		  lp->yy_np->time = lp->yy_series.time;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NAME L_LSQUARE timelist L_RSQUARE
		{ lp->yy_np = newmetric($1);
		  lp->yy_np->time = lp->yy_series.time;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	;

sid_vec 
	: L_NAME L_LBRACE exprlist L_RBRACE
                { lp->yy_np = newmetricquery($1, $3);
                  $$ = lp->yy_series.expr = lp->yy_np;
                }
	| L_NAME
                { lp->yy_np = newmetric($1);
                  $$ = lp->yy_series.expr = lp->yy_np;
                }
	;

func_sid
	: L_RATE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_RATE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RATE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_RATE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MAX);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_INST L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_INST L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MAX_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_SAMPLE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_SAMPLE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MAX_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MIN);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_INST L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_INST L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MIN_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_SAMPLE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_SAMPLE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_MIN_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RESCALE L_LPAREN sid_vec L_COMMA L_STRING L_RPAREN
		{ double		mult;
		  struct pmUnits	units;
		  char			*errmsg;
		
		  lp->yy_np = newnode(N_RESCALE);
		  lp->yy_np->left = $3;
		  if (pmParseUnitsStr($5, &units, &mult, &errmsg) < 0) {
		      gramerr(lp, "Illegal units:", NULL, errmsg);
		      free(errmsg);
		      series_error(lp, NULL);
		      return -1;
		  }
		  lp->yy_np->right = newnode(N_SCALE);
		  lp->yy_np->right->meta.units = units;	/* struct assign */
		  lp->yy_np->right->value = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RESCALE L_LPAREN func_sid L_COMMA L_STRING L_RPAREN
		{ double		mult;
		  struct pmUnits	units;
		  char			*errmsg;
		
		  lp->yy_np = newnode(N_RESCALE);
		  lp->yy_np->left = $3;
		  if (pmParseUnitsStr($5, &units, &mult, &errmsg) < 0) {
		      gramerr(lp, "Illegal units:", NULL, errmsg);
		      free(errmsg);
		      series_error(lp, NULL);
		      return -1;
		  }
		  lp->yy_np->right = newnode(N_SCALE);
		  lp->yy_np->right->meta.units = units;	/* struct assign */
		  lp->yy_np->right->value = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_SUM);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_INST L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_INST L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_SUM_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_SAMPLE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_SAMPLE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_SUM_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_INST L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_INST L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_SAMPLE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_SAMPLE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_INST L_LPAREN sid_vec L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_INST L_LPAREN func_sid L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_SAMPLE L_LPAREN sid_vec L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_SAMPLE L_LPAREN func_sid L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_INST L_LPAREN sid_vec L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_INST L_LPAREN func_sid L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_SAMPLE L_LPAREN sid_vec L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_SAMPLE L_LPAREN func_sid L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_AVG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_INST L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_INST L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_AVG_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_SAMPLE L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_SAMPLE L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_AVG_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ABS L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_ABS);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ABS L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_ABS);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_FLOOR L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_FLOOR);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_FLOOR L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_FLOOR);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN sid_vec L_COMMA number L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN func_sid L_COMMA number L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SQRT L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_SQRT);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SQRT L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_SQRT);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ROUND L_LPAREN sid_vec L_RPAREN
		{ lp->yy_np = newnode(N_ROUND);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ROUND L_LPAREN func_sid L_RPAREN
		{ lp->yy_np = newnode(N_ROUND);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| arithmetic_expr_sid
		{ lp->yy_np = $1;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	;

arithmetic_expr_sid
	: sid_vec L_PLUS sid_vec
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_PLUS func_sid
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_PLUS sid_vec
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_PLUS func_sid
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_MINUS sid_vec
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_MINUS func_sid
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_MINUS sid_vec
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_MINUS func_sid
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_STAR sid_vec
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_STAR func_sid
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_STAR sid_vec
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_STAR func_sid
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_SLASH sid_vec
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| sid_vec L_SLASH func_sid
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_SLASH sid_vec
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func_sid L_SLASH func_sid
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	;

func	: L_RATE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_RATE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RATE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_RATE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MAX);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_INST L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_INST L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MAX_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_SAMPLE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MAX_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MAX_SAMPLE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MAX_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MIN);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_INST L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_INST L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MIN_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_SAMPLE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_MIN_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_MIN_SAMPLE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_MIN_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RESCALE L_LPAREN val_vec L_COMMA L_STRING L_RPAREN
		{ double		mult;
		  struct pmUnits	units;
		  char			*errmsg;
		
		  lp->yy_np = newnode(N_RESCALE);
		  lp->yy_np->left = $3;
		  if (pmParseUnitsStr($5, &units, &mult, &errmsg) < 0) {
		      gramerr(lp, "Illegal units:", NULL, errmsg);
		      free(errmsg);
		      series_error(lp, NULL);
		      return -1;
		  }
		  lp->yy_np->right = newnode(N_SCALE);
		  lp->yy_np->right->meta.units = units;	/* struct assign */
		  lp->yy_np->right->value = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_RESCALE L_LPAREN func L_COMMA L_STRING L_RPAREN
		{ double		mult;
		  struct pmUnits	units;
		  char			*errmsg;
		
		  lp->yy_np = newnode(N_RESCALE);
		  lp->yy_np->left = $3;
		  if (pmParseUnitsStr($5, &units, &mult, &errmsg) < 0) {
		      gramerr(lp, "Illegal units:", NULL, errmsg);
		      free(errmsg);
		      series_error(lp, NULL);
		      return -1;
		  }
		  lp->yy_np->right = newnode(N_SCALE);
		  lp->yy_np->right->meta.units = units;	/* struct assign */
		  lp->yy_np->right->value = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ABS L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_ABS);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ABS L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_ABS);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_FLOOR L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_FLOOR);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_FLOOR L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_FLOOR);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN val_vec L_COMMA number L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN func L_COMMA number L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_LOG L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_LOG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SQRT L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_SQRT);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SQRT L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_SQRT);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ROUND L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_ROUND);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_ROUND L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_ROUND);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_SUM);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_INST L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_INST L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_SUM_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_SAMPLE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_SUM_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_SUM_SAMPLE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_SUM_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_INST L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_INST L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_SAMPLE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_STDEV_SAMPLE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_STDEV_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_INST L_LPAREN val_vec L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_INST L_LPAREN func L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_SAMPLE L_LPAREN val_vec L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_TOPK_SAMPLE L_LPAREN func L_COMMA integer L_RPAREN
		{ lp->yy_np = newnode(N_TOPK_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_INST L_LPAREN val_vec L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_INST L_LPAREN func L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_INST);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_SAMPLE L_LPAREN val_vec L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_NTH_PERCENTILE_SAMPLE L_LPAREN func L_COMMA integer L_RPAREN
		{ char *ptr;
		  long ret;
		  ret = strtoul($5->value, &ptr, 10);
		  if (*ptr != '\0' || ret < 0 || ret > 100){
			series_error(lp, NULL);
			return -1;
		  }
		  lp->yy_np = newnode(N_NTH_PERCENTILE_SAMPLE);
		  lp->yy_np->left = $3;
		  lp->yy_np->right = $5;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_AVG);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_INST L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_INST L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_AVG_INST);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_SAMPLE L_LPAREN val_vec L_RPAREN
		{ lp->yy_np = newnode(N_AVG_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| L_AVG_SAMPLE L_LPAREN func L_RPAREN
		{ lp->yy_np = newnode(N_AVG_SAMPLE);
		  lp->yy_np->left = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| arithmetic_expression
		{ lp->yy_np = $1;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	;

arithmetic_expression
	: val_vec L_PLUS val_vec
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_PLUS func
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_PLUS val_vec
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_PLUS func
		{ lp->yy_np = newnode(N_PLUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_MINUS val_vec
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_MINUS func
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_MINUS val_vec
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_MINUS func
		{ lp->yy_np = newnode(N_MINUS);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_STAR val_vec
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_STAR func
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_STAR val_vec
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_STAR func
		{ lp->yy_np = newnode(N_STAR);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_SLASH val_vec
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| val_vec L_SLASH func
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_SLASH val_vec
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	| func L_SLASH func
		{ lp->yy_np = newnode(N_SLASH);
		  lp->yy_np->left = $1;
		  lp->yy_np->right = $3;
		  $$ = lp->yy_series.expr = lp->yy_np;
		}
	;

%%

/* function table for lexer */
static const struct {
    int		f_type;
    int		f_namelen;
    char	*f_name;
} func[] = {
    { L_AVG,		sizeof("avg")-1,	"avg" },
    { L_AVG_INST,	sizeof("avg_inst")-1,	"avg_inst" },
    { L_AVG_SAMPLE,	sizeof("avg_sample")-1,	"avg_sample" },
    { L_COUNT,		sizeof("count")-1,	"count" },
    { L_MAX,		sizeof("max")-1,	"max" },
    { L_MAX_INST,	sizeof("max_inst")-1,	"max_inst" },
    { L_MAX_SAMPLE,	sizeof("max_sample")-1,	"max_sample" },
    { L_MIN,		sizeof("min")-1,	"min" },
    { L_MIN_INST,	sizeof("min_inst")-1,	"min_inst" },
    { L_MIN_SAMPLE,	sizeof("min_sample")-1,	"min_sample" },
    { L_SUM,		sizeof("sum")-1,	"sum" },
    { L_SUM_INST,	sizeof("sum_inst")-1,	"sum_inst" },
    { L_SUM_SAMPLE,	sizeof("sum_sample")-1,	"sum_sample" },
    { L_STDEV_INST,	sizeof("stdev_inst")-1,	"stdev_inst" },
    { L_STDEV_SAMPLE,	sizeof("stdev_sample")-1,	"stdev_sample" },
    { L_TOPK_INST,	sizeof("topk_inst")-1,	"topk_inst" },
    { L_TOPK_SAMPLE,	sizeof("topk_sample")-1,	"topk_sample" },
    { L_NTH_PERCENTILE_INST,	sizeof("nth_percentile_inst")-1,	"nth_percentile_inst" },
    { L_NTH_PERCENTILE_SAMPLE,	sizeof("nth_percentile_sample")-1,	"nth_percentile_sample" },
    { L_RATE,		sizeof("rate")-1,	"rate" },
    { L_ABS,		sizeof("abs")-1,	"abs" },
    { L_FLOOR,		sizeof("floor")-1,	"floor" },
    { L_LOG,		sizeof("log")-1,	"log" },
    { L_SQRT,		sizeof("sqrt")-1,	"sqrt" },
    { L_ROUND,		sizeof("round")-1,	"round" },
    { L_RESCALE,	sizeof("rescale")-1,	"rescale" },
    { L_UNDEF,		0,			NULL }
};

static struct {
    int         ltype;
    int         ntype;
    char        *long_name;
    char        *short_name;
} typetab[] = {
    { L_UNDEF,		0,		"UNDEF",	NULL },
    { L_ERROR,		0,		"ERROR",	NULL },
    { L_EOS,		0,		"EOS",		NULL },
    { L_INTEGER,	N_INTEGER,	"INTEGER",	NULL },
    { L_DOUBLE,		N_DOUBLE,	"DOUBLE",	NULL },
    { L_NAME,		N_NAME,		"NAME",		NULL },
    { L_NAME,		N_METRIC,	"METRIC",	NULL },
    { L_NAME,		N_QUERY,	"QUERY",	NULL },
    { L_PLUS,		N_PLUS,		"PLUS",		"+" },
    { L_MINUS,		N_MINUS,	"MINUS",	"-" },
    { L_STAR,		N_STAR,		"STAR",		"*" },
    { L_SLASH,		N_SLASH,	"SLASH",	"/" },
    { L_LPAREN,		0,		"LPAREN",	"(" },
    { L_RPAREN,		0,		"RPAREN",	")" },
    { L_LBRACE,		0,		"LBRACE",	"{" },
    { L_RBRACE,		0,		"RBRACE",	"}" },
    { L_TYPE,		0,		"TYPE",		NULL },
    { L_SEMANTICS,	0,		"SEMANTICS",	NULL },
    { L_UNITS,		0,		"UNITS",	NULL },
    { L_ASSIGN,		0,		"ASSIGN",	"=" },
    { L_COMMA,		0,		"COMMA",	"," },
    { L_STRING,		0,		"STRING",	"\"" },
    { L_ABS,		N_ABS,		"ABS",		NULL },
    { L_FLOOR,		N_FLOOR,	"FLOOR",	NULL },
    { N_LOG,		N_LOG,		"LOG",		NULL },
    { N_SQRT,		N_SQRT,		"SQRT",		NULL },
    { N_ROUND,		N_ROUND,	"ROUND",	NULL },
    { L_AVG,		N_AVG,		"AVG",		NULL },
    { L_AVG_INST,	N_AVG_INST,	"AVG_INST",	NULL },
    { L_AVG_SAMPLE,	N_AVG_SAMPLE,	"AVG_SAMPLE",	NULL },
    { L_COUNT,		N_COUNT,	"COUNT",	NULL },
    { L_DELTA,		N_DELTA,	"DELTA",	NULL },
    { L_MAX,		N_MAX,		"MAX",		NULL },
    { L_MAX_INST,	N_MAX_INST,	"MAX_INST",	NULL },
    { L_MAX_SAMPLE,	N_MAX_SAMPLE,	"MAX_SAMPLE",	NULL },
    { L_MIN,		N_MIN,		"MIN",		NULL },
    { L_MIN_INST,	N_MIN_INST,	"MIN_INST",	NULL },
    { L_MIN_SAMPLE,	N_MIN_SAMPLE,	"MIN_SAMPLE",	NULL },
    { L_SUM,		N_SUM,		"SUM",		NULL },
    { L_SUM_INST,	N_SUM_INST,	"SUM_INST",	NULL },
    { L_SUM_SAMPLE,	N_SUM_SAMPLE,	"SUM_SAMPLE",	NULL },
    { L_STDEV_INST,	N_STDEV_INST,	"STDEV_INST",	NULL },
    { L_STDEV_SAMPLE,	N_STDEV_SAMPLE,	"STDEV_SAMPLE",	NULL },
    { L_TOPK_INST,	N_TOPK_INST,	"TOPK_INST",	NULL },
    { L_TOPK_SAMPLE,	N_TOPK_SAMPLE,	"TOPK_SAMPLE",	NULL },
    { L_NTH_PERCENTILE_INST,	N_NTH_PERCENTILE_INST,	"NTH_PERCENTILE_INST",	NULL },
    { L_NTH_PERCENTILE_SAMPLE, N_NTH_PERCENTILE_SAMPLE, "NTH_PERCENTILE_SAMPLE", NULL },
    { L_ANON,		N_ANON,		"ANON",		NULL },
    { L_RATE,		N_RATE,		"RATE",		NULL },
    { L_INSTANT,	N_INSTANT,	"INSTANT",	NULL },
    { L_MKCONST,	0,		"MKCONST",	NULL },
    { L_RESCALE,	N_RESCALE,	"RESCALE",	NULL },
    { 0,		N_SCALE,	"SCALE",	NULL },
    { L_DEFINED,	N_DEFINED,	"DEFINED",	NULL },
    { L_INTERVAL,	N_INTERVAL,	"INTERVAL",	NULL },
    { L_START,		N_START,	"START",	NULL },
    { L_FINISH,		N_FINISH,	"FINISH",	NULL },
    { L_TIMEZONE,	N_TIMEZONE,	"TIMEZONE",	"TZ" },
    { L_SAMPLES,	N_SAMPLES,	"SAMPLES",	NULL },
    { L_OFFSET,		N_OFFSET,	"OFFSET",	NULL },
    { L_LT,		N_LT,		"LT",		"<" },
    { L_LEQ,		N_LEQ,		"LEQ",		"<=" },
    { L_EQ,		N_EQ,		"EQ",		"==" },
    { L_GLOB,		N_GLOB,		"GLOB",		"~~" },
    { L_GEQ,		N_GEQ,		"GEQ",		">=" },
    { L_GT,		N_GT,		"GT",		">" },
    { L_NEQ,		N_NEQ,		"NEQ",		"!=" },
    { L_REQ,		N_REQ,		"REQ",		"=~" },
    { L_RNE,		N_RNE,		"RNE",		"!~" },
    { L_AND,		N_AND,		"AND",		"&&" },
    { L_OR,		N_OR,		"OR",		"||" },
    { 0,		N_NEG,		"NEG",		"-" },
    { -1,		-1,		NULL,		NULL }
};

/* full name for all node types */
static char *
n_type_str(int type)
{
    int		i;
    static char n_eh_str[32];	/* big enough for "unknown type XXXXXXXXXXX!" */

    for (i = 0; typetab[i].ntype != -1; i++)
	if (type == typetab[i].ntype)
	    return typetab[i].long_name;
    pmsprintf(n_eh_str, sizeof(n_eh_str), "unknown type %d!", type);
    return n_eh_str;
}

/* short string for the operator node types */
static char *
n_type_c(int type)
{
    int		i;
    static char n_eh_c[20]; /* big enough for "op XXXXXXXXXXX!" */

    for (i = 0; typetab[i].ntype != -1; i++)
	if (type == typetab[i].ntype)
	    return typetab[i].short_name;
    pmsprintf(n_eh_c, sizeof(n_eh_c), "op %d!", type);
    return n_eh_c;
}

/* full name for all lex types */
static char *
l_type_str(int type)
{
    int		i;
    static char l_eh_str[32];	/* big enough for "unknown type XXXXXXXXXXX!" */

    for (i = 0; typetab[i].ltype != -1; i++)
	if (type == typetab[i].ltype)
	    return typetab[i].long_name;
    pmsprintf(l_eh_str, sizeof(l_eh_str), "unknown type %d!", type);
    return l_eh_str;
}

static node_t *
newnode(int type)
{
    node_t	*np;

    if ((np = (node_t *)calloc(1, sizeof(node_t))) == NULL) {
	pmNoMem("pmSeries: newnode", sizeof(node_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    np->type = type;
    return np;
}

static node_t *
newtree(int type, node_t *left, node_t *right)
{
    node_t	*tree = newnode(type);

    tree->left = left;
    tree->right = right;
    return tree;
}

static node_t *
newmetric(char *name)
{
    node_t	*node;
    char	*re;

    /* enable globbing if string contains a non-escaped '*' */
    if ((re = strchr(name, '*')) != NULL &&
	(re == name || (re[-1] != '%' && re[-1] != '\\')))
	node = newnode(N_GLOB);
    else
	node = newnode(N_EQ);

    node->left = newnode(N_NAME);
    node->left->value = sdsnew("metric.name");
    node->right = newnode(N_STRING);
    node->right->value = sdsnew(name);
    return node;
}

static node_t *
newmetricquery(char *name, node_t *query)
{
    node_t	*root = newnode(N_AND);
    node_t	*metric = newmetric(name);

    root->left = metric;
    root->right = query;
    return root;
}

static void
newinterval(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;
    char	*error;
    int		sts;

    if ((sts = pmParseInterval(string, &tp->delta, &error)) < 0) {
	lp->yy_error = sts;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Cannot parse time delta with pmParseInterval:\n%s", error);
	free(error);
    } else {
	tp->window.delta = sdsnew(string);
    }
}

static void
parsetime(PARSER *lp, struct timeval *result, const char *string)
{
    struct timeval start = { 0, 0 };
    struct timeval end = { PM_MAX_TIME_T, 0 };
    char	*error;
    int		sts;

    if ((sts = __pmParseTime(string, &start, &end, result, &error)) < 0) {
	lp->yy_error = sts;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Cannot parse time with __pmParseTime:\n%s", error);
	free(error);
    }
}

static void
newstarttime(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;

    parsetime(lp, &tp->start, string);
    if (!lp->yy_error)
	tp->window.start = sdsnew(string);
}

static void
newendtime(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;

    parsetime(lp, &tp->end, string);
    if (!lp->yy_error)
	tp->window.end = sdsnew(string);
}

static void
newrange(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;
    char	*error;
    int		sts;

    if ((sts = pmParseInterval(string, &tp->start, &error)) < 0) {
	lp->yy_error = sts;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Cannot parse range with pmParseInterval:\n%s", error);
	lp->yy_error = sts;
	free(error);
    } else {
	struct timeval offset;
	gettimeofday(&offset, NULL);
	tsub(&offset, &tp->start);
	tp->start = offset;
	tp->end.tv_sec = PM_MAX_TIME_T;
	tp->window.range = sdsnew(string);
    }
}

static void
newaligntime(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;

    parsetime(lp, &tp->align, string);
    if (!lp->yy_error)
	tp->window.align = sdsnew(string);
}

static void
newtimezone(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;
    char	e[PM_MAXERRMSGLEN];
    int		sts;

    if ((sts = pmNewZone(string)) < 0) {
	lp->yy_error = sts;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Cannot parse timezone with pmNewZone:\n\"%s\" - %s",
		string, pmErrStr_r(sts, e, sizeof(e)));
    } else {
	tp->zone = sts;
	tp->window.zone = sdsnew(string);
    }
}

static void
newsamples(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;
    int		sts;

    if ((sts = atoi(string)) < 0) {
	lp->yy_error = -EINVAL;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Invalid sample count requested - \"%s\"", string);
    } else {
	tp->count = sts;
	tp->window.count = sdsnew(string);
    }
}

static void
newoffset(PARSER *lp, const char *string)
{
    timing_t	*tp = &lp->yy_series.time;
    int		sts;

    if ((sts = atoi(string)) < 0) {
	lp->yy_error = -EINVAL;
	lp->yy_errstr = sdscatfmt(sdsempty(),
		"Invalid sample offset requested - \"%s\"", string);
    } else {
	tp->offset = sts;
	tp->window.offset = sdsnew(string);
    }
}

static void
gramerr(PARSER *lp, const char *phrase, const char *pos, char *arg)
{
   char errmsg[256];

   /* unless lexer has already found something amiss ... */
   if (lp->yy_errstr == NULL) {
	if (pos == NULL)
	    pmsprintf(errmsg, sizeof(errmsg), "%s '%s'", phrase, arg);
	else
	    pmsprintf(errmsg, sizeof(errmsg), "%s expected to %s %s", phrase, pos, arg);
	lp->yy_errstr = sdsnew(errmsg);
   }
}

/* Construct error message buffer for syntactic error */
static char *
parseError(const char *spec, const char *point, const char *msg)
{
    size_t	need = 2 * strlen(spec) + strlen(msg) + 8;
    const char	*p;
    char	*q, *result;

    if ((result = malloc(need)) == NULL)
	return NULL;
    q = result;

    for (p = spec; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    for (p = spec; p != point; p++)
	*q++ = isgraph((int)*p) ? ' ' : *p;
    snprintf(q, need - (q - result), "^ -- ");
    q += 5;
    for (p = msg; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    *q = '\0';
    return result;
}

static int
series_error(PARSER *lp, const char *s)
{
    char	*msg;

    msg = parseError(lp->yy_base, lp->yy_lexicon, s ? s : "syntax error");
    lp->yy_errstr = sdscatfmt(sdsempty(), "cannot parse given string\n\n%s", msg);
    free(msg);

    lp->yy_series.expr = NULL;
    lp->yy_error = -EINVAL;
    return 0;
}

static int
yyerror(PARSER *lp, const char *s)
{
    return series_error(lp, s);
}

static void
unget(PARSER *lp, int c)
{
    lp->yy_lexpeek = c;
}

static int
get(PARSER *lp)
{
    int		c;

    if (lp->yy_lexpeek != 0) {
	c = lp->yy_lexpeek;
	lp->yy_lexpeek = 0;
	return c;
    }
    c = *lp->yy_input;
    if (c == '\0')
	return EOF;
    lp->yy_input++;
    return c;
}

static int
series_lex(YYSTYPE *lvalp, PARSER *lp)
{
    char	*p = lp->yy_tokbuf;
    int		ltype = L_UNDEF;
    int		c, i, len;
    int		firstch = 1;
    int		ret = L_UNDEF;

    for ( ; ret == L_UNDEF; ) {
	c = get(lp);
	if (firstch) {
	    if (isspace((int)c)) continue;
	    lp->yy_lexicon = &lp->yy_input[-1];
	    firstch = 0;
	}
	if (c == EOF) {
	    if (ltype != L_UNDEF) {
		/* force end of last token */
		c = 0;
	    }
	    else {
		/* really the end of the input buffer */
		ret = L_EOS;
		break;
	    }
	}
	if (p == NULL) {
	    lp->yy_tokbuflen = 128;
	    if ((p = (char *)malloc(lp->yy_tokbuflen+1)) == NULL) { /* CID287946 */
		lp->yy_errstr = sdscatfmt(sdsempty(),
				"cannot allocate token buffer (length=%lld)",
				(long long)lp->yy_tokbuflen);
		lp->yy_error = -ENOMEM;
		ret = L_ERROR;
		break;
	    }
	    lp->yy_tokbuf = p;
	}
	else if (p >= &lp->yy_tokbuf[lp->yy_tokbuflen]) {
	    len = p - lp->yy_tokbuf;
	    lp->yy_tokbuflen *= 2;
	    if (!(p = (char *)realloc(lp->yy_tokbuf, lp->yy_tokbuflen+1))) { /* CID287946 */
		lp->yy_errstr = sdscatfmt(sdsempty(),
				"cannot reallocate token buffer (length=%lld)",
				(long long)lp->yy_tokbuflen);
		lp->yy_error = -ENOMEM;
		ret = L_ERROR;
		break;
	    }
	    lp->yy_tokbuf = p;
	    p = &lp->yy_tokbuf[len];
	}

	*p++ = (char)c;

	if (ltype == L_UNDEF) {
	    if (isdigit((int)c))
		ltype = L_INTEGER;
	    else if (c == '.')
		ltype = L_DOUBLE;
	    else if (isalpha((int)c))
		ltype = L_NAME;
	    else {
		switch (c) {
		    case '+':
			*p = '\0';
			ret = L_PLUS;
			break;

		    case '-':
			*p = '\0';
			ret = L_MINUS;
			break;

		    case '*':
			*p = '\0';
			ret = L_STAR;
			break;

		    case '/':
			*p = '\0';
			ret = L_SLASH;
			break;

		    case '(':
			*p = '\0';
			ret = L_LPAREN;
			break;

		    case ')':
			*p = '\0';
			ret = L_RPAREN;
			break;

		    case '{':
			*p = '\0';
			ret = L_LBRACE;
			break;

		    case '}':
			*p = '\0';
			ret = L_RBRACE;
			break;

		    case '[':
			*p = '\0';
			ret = L_LSQUARE;
			break;

		    case ']':
			*p = '\0';
			ret = L_RSQUARE;
			break;

		    case '<':
			ltype = L_LT;
			break;

		    case '=':
			ltype = L_EQ;
			break;

		    case '~':
			ltype = L_GLOB;
			break;

		    case '>':
			ltype = L_GT;
			break;

		    case '!':
			ltype = L_NEQ;
			break;

		    case '&':
			ltype = L_AND;
			break;

		    case '|':
			ltype = L_OR;
			break;

		    case ':':
			*p = '\0';
			ret = L_COLON;
			break;

		    case ',':
			*p = '\0';
			ret = L_COMMA;
			break;

		    case '"':
			ltype = L_STRING;
			break;

		    default:
			*p = '\0';
			lp->yy_errstr = sdsnew("Illegal character");
			lp->yy_error = -EINVAL;
			ret = L_ERROR;
			break;
		}
	    }
	}
	else {
	    if (ltype == L_INTEGER) {
		if (c == '.') {
		    ltype = L_DOUBLE;
		}
		else if (isalpha((int)c)) {
		    ltype = L_RANGE;
		}
		else if (!isdigit((int)c)) {
		    char	*endptr;
		    __uint64_t	check;

		    unget(lp, c);
		    p[-1] = '\0';
		    check = strtoull(lp->yy_tokbuf, &endptr, 10);
		    if (*endptr != '\0' || check > 0xffffffffUL) {
			lp->yy_errstr = sdsnew("Constant value too large");
			lp->yy_error = -EINVAL;
			ret = L_ERROR;
			break;
		    }
		    if ((lvalp->s = sdsnew(lp->yy_tokbuf)) == NULL) {
			lp->yy_errstr = sdsnew("dup() for INTEGER failed");
			lp->yy_error = -ENOMEM;
			ret = L_ERROR;
			break;
		    }
		    ret = L_INTEGER;
		    break;
		}
	    }
	    else if (ltype == L_RANGE) {
		if (!isalpha((int)c)) {
		    unget(lp, c);
		    p[-1] = '\0';
		    if ((lvalp->s = sdsnew(lp->yy_tokbuf)) == NULL) {
			lp->yy_errstr = sdsnew("dup() for RANGE failed");
			lp->yy_error = -ENOMEM;
			ret = L_ERROR;
			break;
		    }
		    ret = L_RANGE;
		    break;
		}
	    }
	    else if (ltype == L_DOUBLE) {
		if (!isdigit((int)c)) {
		    unget(lp, c);
		    p[-1] = '\0';
		    if ((lvalp->s = sdsnew(lp->yy_tokbuf)) == NULL) {
			lp->yy_errstr = sdsnew("dup() for DOUBLE failed");
			lp->yy_error = -ENOMEM;
			ret = L_ERROR;
			break;
		    }
		    ret = L_DOUBLE;
		    break;
		}
	    }
	    else if (ltype == L_NAME) {
		if (isalpha((int)c) || isdigit((int)c) ||
		    c == '_' || c == '.' || c == '*' || c == '%' || c == '\\')
		    continue;
		if (c == '(') {
		    /* check for functions ... */
		    len = p - lp->yy_tokbuf - 1;
		    for (i = 0; func[i].f_name != NULL; i++) {
			if (len == func[i].f_namelen &&
			    strncmp(lp->yy_tokbuf, func[i].f_name, len) == 0) {
			    unget(lp, c);
			    p[-1] = '\0';
			    ret = func[i].f_type;
			    break;
			}
		    }
		    if (func[i].f_name != NULL) {
			/* match func name */
			break;
		    }
		}

		/* current character is end of name */
		unget(lp, c);
		p[-1] = '\0';
		if (strcmp(lp->yy_tokbuf, "type") == 0) {
		    ret = L_TYPE;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "semantics") == 0) {
		    ret = L_SEMANTICS;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "units") == 0) {
		    ret = L_UNITS;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "interval") == 0 ||
			 strcmp(lp->yy_tokbuf, "delta") == 0) {
		    ret = L_INTERVAL;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "timezone") == 0 ||
			 strcmp(lp->yy_tokbuf, "tz") == 0) {
		    ret = L_TIMEZONE;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "start") == 0 ||
			 strcmp(lp->yy_tokbuf, "begin") == 0) {
		    ret = L_START;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "finish") == 0 ||
			 strcmp(lp->yy_tokbuf, "end") == 0) {
		    ret = L_FINISH;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "samples") == 0 ||
			 strcmp(lp->yy_tokbuf, "count") == 0) {
		    ret = L_SAMPLES;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "offset") == 0 ||
			 strcmp(lp->yy_tokbuf, "from") == 0) {
		    ret = L_OFFSET;
		    break;
		}
		else if (strcmp(lp->yy_tokbuf, "align") == 0 ||
			 strcmp(lp->yy_tokbuf, "alignment") == 0) {
		    ret = L_ALIGN;
		    break;
		}
		if ((lvalp->s = sdsnew(lp->yy_tokbuf)) == NULL) {
		    lp->yy_errstr = sdsnew("dup() for NAME failed");
		    lp->yy_error = -ENOMEM;
		    ret = L_ERROR;
		    break;
		}
		ret = L_NAME;
		break;
	    }
	    else if (ltype == L_STRING) {
		if (c != '"')
		    continue;
		/* [-1] and [1] to strip quotes */
		p[-1] = '\0';
		if ((lvalp->s = sdsnew(&lp->yy_tokbuf[1])) == NULL) {
		    lp->yy_errstr = sdsnew("dup() for STRING failed");
		    lp->yy_error = -ENOMEM;
		    ret = L_ERROR;
		    break;
		}
		ret = L_STRING;
		break;
	    }
	    else if (ltype == L_LT) {
		if (c == '=') {
		    *p = '\0';
		    ret = L_LEQ;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    ret = L_LT;
		    break;
		}
	    }
	    else if (ltype == L_GT) {
		if (c == '=') {
		    *p = '\0';
		    ret = L_GEQ;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    ret = L_GT;
		    break;
		}
	    }
	    else if (ltype == L_GLOB) {
		if (c == '~') {
		    *p = '\0';
		    ret = L_GLOB;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    lp->yy_errstr = sdsnew("Illegal character");
		    lp->yy_error = -EINVAL;
		    ret = L_ERROR;
		    break;
		}
	    }
	    else if (ltype == L_EQ) {
		if (c == '=') {
		    *p = '\0';
		    ret = L_EQ;
		    break;
		}
		else if (c == '~') {
		    *p = '\0';
		    ret = L_REQ;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    ret = L_ASSIGN;
		    break;
		}
	    }
	    else if (ltype == L_NEQ) {
		if (c == '=') {
		    *p = '\0';
		    ret = L_NEQ;
		    break;
		}
		else if (c == '~') {
		    *p = '\0';
		    ret = L_RNE;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    lp->yy_errstr = sdsnew("Illegal character");
		    lp->yy_error = -EINVAL;
		    ret = L_ERROR;
		    break;
		}
	    }
	    else if (ltype == L_AND) {
		if (c == '&') {
		    *p = '\0';
		    ret = L_AND;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    lp->yy_errstr = sdsnew("Illegal character");
		    lp->yy_error = -EINVAL;
		    ret = L_ERROR;
		    break;
		}
	    }
	    else if (ltype == L_OR) {
		if (c == '|') {
		    *p = '\0';
		    ret = L_OR;
		    break;
		}
		else {
		    unget(lp, c);
		    p[-1] = '\0';
		    lp->yy_errstr = sdsnew("Illegal character");
		    lp->yy_error = -EINVAL;
		    ret = L_ERROR;
		    break;
		}
	    }
	}
    }

    if (pmDebugOptions.series && pmDebugOptions.appl0)
	fprintf(stderr, "series_lex() -> type=L_%s \"%s\"\n",
		l_type_str(ret), ret == L_EOS ? "" : lp->yy_tokbuf);

    return ret;
}

static int
yylex(YYSTYPE *lvalp, PARSER *lp)
{
    return series_lex(lvalp, lp);
}

static void
series_dumpexpr(node_t *np, int level)
{
    if (np == NULL)
	return;

    series_dumpexpr(np->left, level+1);

    switch (np->type) {
    case N_NAME: case N_INTEGER: case N_DOUBLE:
	fprintf(stderr, "%*s%s", level*4, "", np->value);
	break;
    case N_STRING:
	fprintf(stderr, "%*s\"%s\"", level*4, "", np->value);
	break;
    case N_LT:  case N_LEQ: case N_EQ:  case N_GEQ: case N_GT:  case N_NEQ:
    case N_AND: case N_OR:  case N_RNE: case N_REQ: case N_NEG: case N_GLOB:
    case N_PLUS: case N_MINUS: case N_STAR: case N_SLASH:
	fprintf(stderr, "%*s%s", level*4, "", n_type_c(np->type));
	break;
    case N_AVG: case N_COUNT:   case N_DELTA:   case N_MAX:     case N_MIN:
    case N_SUM: case N_ANON:    case N_RATE:    case N_INSTANT: case N_RESCALE:
    case N_MAX_INST: case N_MAX_SAMPLE: case N_MIN_INST: case N_MIN_SAMPLE:
    case N_AVG_INST: case N_AVG_SAMPLE: case N_SUM_INST: case N_SUM_SAMPLE:
    case N_STDEV_INST: case N_STDEV_SAMPLE: case N_NTH_PERCENTILE_INST:
    case N_NTH_PERCENTILE_SAMPLE: case N_TOPK_INST: case N_TOPK_SAMPLE: 
	fprintf(stderr, "%*s%s()", level*4, "", n_type_str(np->type));
	break;
    case N_SCALE: {
	char	strbuf[60];

	if (pmUnitsStr_r(&np->meta.units, strbuf, sizeof(strbuf)) == NULL)
	    fprintf(stderr, " [bad units: %d %d %d %d %d %d]",
			np->meta.units.scaleCount, np->meta.units.scaleTime,
			np->meta.units.scaleSpace,
			np->meta.units.dimCount, np->meta.units.dimTime,
			np->meta.units.dimSpace);
	else
	    fprintf(stderr, " [%s]", strbuf);
	break;
	}
    default:
	break;
    }
    fputc('\n', stderr);

    series_dumpexpr(np->right, level+1);
}

int
series_parse(sds query, series_t *sp, char **err, void *arg)
{
    PARSER	yp = { .yy_base = query, .yy_input = (char *)query };
    series_t	*ypsp = &yp.yy_series;
    int		sts;

    if ((sts = yyparse(&yp)) != 0) {
	*err = yp.yy_errstr;
	return yp.yy_error;
    }

    if (ypsp->expr == NULL) {
	if (pmDebugOptions.series || pmDebugOptions.query)
	    fprintf(stderr, "Error: parsing query '%s'\n", query);
	return PM_ERR_NYI;
    }

    if (pmDebugOptions.query) {
	fprintf(stderr, "parsed query: %s\n", query);
	series_dumpexpr(ypsp->expr, 0);
	fputc('\n', stderr);
    }

    *sp = *ypsp;
    return sts;
}

int
pmSeriesQuery(pmSeriesSettings *settings, sds query, pmSeriesFlags flags, void *arg)
{
    series_t	sp = {0};
    char	*errstr;
    int		sts;

    series_stats_inc(settings, SERIES_QUERY_CALLS);

    if ((sts = series_parse(query, &sp, &errstr, arg)) != 0) {
	moduleinfo(&settings->module, PMLOG_ERROR, errstr, arg);
    	return sts;
    }

    if (sp.expr == NULL) {
	if (pmDebugOptions.series || pmDebugOptions.query)
	    fprintf(stderr, "Error: parsing query '%s'\n", query);
	return PM_ERR_NYI;
    }

    return series_solve(settings, sp.expr, &sp.time, flags, arg);
}

int
pmSeriesLoad(pmSeriesSettings *settings, sds source, pmSeriesFlags flags, void *arg)
{
    series_t	sp = {0};
    char	*errstr;
    int		sts;

    series_stats_inc(settings, SERIES_LOAD_CALLS);

    if ((sts = series_parse(source, &sp, &errstr, arg)) != 0) {
	moduleinfo(&settings->module, PMLOG_ERROR, errstr, arg);
    	return sts;
    }

    if (sp.expr == NULL) {
	if (pmDebugOptions.series || pmDebugOptions.query)
	    fprintf(stderr, "Error: parsing query '%s'\n", source);
	return PM_ERR_NYI;
    }

    if (pmDebugOptions.query) {
	fprintf(stderr, "pmSeriesLoad: %s\n", source);
	series_dumpexpr(sp.expr, 0);
	fputc('\n', stderr);
    }

    return series_load(settings, sp.expr, &sp.time, flags, arg);
}
