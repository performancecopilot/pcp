/***********************************************************************
 * grammar.y - yacc grammar for rule language
 ***********************************************************************
 *
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

%{
#include "dstruct.h"
#include "syntax.h"
#include "lexicon.h"
#include "pragmatics.h"
#include "systemlog.h"
#include "stomp.h"
#include "show.h"

/* strings for error reporting */
char precede[]	 = "precede";
char follow[]	 = "follow";
char act_str[]	 = "action";
char bexp_str[]	 = "logical expression";
char aexp_str[]	 = "arithmetic expression";
char quant_str[] = "quantifier";
char aggr_str[]  = "aggregation operator";
char pcnt_str[]  = "percentage quantifier";
char host_str[]  = "host name";
char inst_str[]  = "instance name";
char sample_str[]  = "sample number(s)";
char tstr_str[]	 = "(time interval optional) and string";
char num_str[]	 = "number";

/* report grammatical error */
static void
gramerr(char *phrase, char *pos, char *op)
{
    fprintf(stderr, "%s expected to %s %s\n", phrase, pos, op);
    lexSync();
}


%}

/***********************************************************************
 * yacc token and operator declarations
 ***********************************************************************/

%expect     188
%start      stmnt

%token      ARROW
%token      SHELL
%token      ALARM
%token      SYSLOG
%token      PRINT
%token      STOMP
%token      SOME_QUANT
%token      ALL_QUANT
%token      PCNT_QUANT
%token      LEQ_REL
%token      GEQ_REL
%token      NEQ_REL
%token      EQ_REL
%token	    AND
%token	    SEQ
%token	    OR
%token	    ALT
%token	    NOT
%token	    RISE
%token	    FALL
%token	    MATCH
%token	    NOMATCH
%token	    RULESET
%token      ELSE
%token      UNKNOWN
%token      OTHERWISE
%token      MIN_AGGR
%token      MAX_AGGR
%token      AVG_AGGR
%token      SUM_AGGR
%token	    COUNT_AGGR
%token      TIME_DOM
%token      INST_DOM
%token      HOST_DOM
%token      UNIT_SLASH
%token      INTERVAL
%token <u>  EVENT_UNIT
%token <u>  TIME_UNIT
%token <u>  SPACE_UNIT
%token <d>  NUMBER
%token <s>  IDENT
%token <s>  STRING
%token      TRU
%token      FALS
%token <x>  VAR

%type  <x>  exp
%type  <x>  rule
%type  <x>  ruleset
%type  <x>  rulelist
%type  <x>  rulesetopt
%type  <x>  act
%type  <x>  bexp
%type  <x>  rexp
%type  <x>  actarg
%type  <x>  arglist
%type  <x>  aexp
%type  <x>  quant
%type  <x>  aggr
%type  <x>  num
%type  <x>  str
%type  <i>  dom
%type  <x>  fetch
%type  <s>  metric
%type  <sa> hosts
%type  <sa> insts
%type  <t>  times
%type  <u>  units
%type  <u>  unit

%left  NAME_DELIM
%left  ARROW
%left  AND OR SEQ ALT
%left  NOT RISE FALL
%left  ALL_QUANT SOME_QUANT PCNT_QUANT
%left  MATCH NOMATCH
%left  '>' '<' EQ_REL NEQ_REL GEQ_REL LEQ_REL
%left  '+' '-'
%left  '*' '/'
%left  UMINUS RATE INSTANT
%left  SUM_AGGR AVG_AGGR MAX_AGGR MIN_AGGR COUNT_AGGR
%left  SHELL ALARM SYSLOG PRINT STOMP
%left  ':' '#' '@'
%left  UNIT_SLASH INTERVAL

%%

/***********************************************************************
 * yacc productions
 ***********************************************************************/

stmnt	: /* empty */
		{    parse = NULL; }
	| IDENT '=' exp
		{   parse = statement($1, $3);
		    if ((agent || applet) && $3 != NULL &&
			($3->op == RULE || $3->op == ACT_SEQ ||
			 $3->op == ACT_ALT || $3->op == ACT_SHELL ||
			 $3->op == ACT_ALARM || $3->op == ACT_SYSLOG ||
			 $3->op == ACT_PRINT || $3->op == ACT_STOMP)) {
			synerr();
			fprintf(stderr, "operator %s not allowed in agent "
					"mode\n", opStrings($3->op));
			parse = NULL;
		    }
		}
	| exp
		{   parse = statement(NULL, $1);
		    if (agent) {
			synerr();
			fprintf(stderr, "expressions must be named in agent "
					"mode\n");
			parse = NULL;
		    }
		}
	;

exp	: rule
		{   $$ = $1; }
	| ruleset
		{   $$ = $1; }
	| bexp
		{   $$ = $1; }
	| aexp
		{   $$ = $1; }
	| act
		{   $$ = $1; }
	| str
		{   $$ = $1; }
	;

rule	: '(' rule ')'
		{   $$ = $2; }
	| bexp ARROW act
		{    $$ = ruleExpr($1, $3); }

	/* error reporting */
	| error ARROW
		{   gramerr(bexp_str, precede, opStrings(RULE));
		    $$ = NULL; }
	| bexp ARROW error
		{   gramerr(act_str, follow, opStrings(RULE));
		    $$ = NULL; }
	;

ruleset : RULESET rulelist rulesetopt
		{   $$ = binaryExpr(CND_RULESET, $2, $3); }
	;

rulelist : rule
		{   $$ = $1; }
	| rule ELSE rulelist
		/*
		 * use right recursion here so rules appear l-to-r in
		 * the expression tree to match the required order of
		 * evaluation
		 */
		{   $$ = binaryExpr(CND_OR, $1, $3); }
	;

rulesetopt : UNKNOWN ARROW act
		{    $$ = binaryExpr(CND_OTHER, ruleExpr(boolConst(B_TRUE), $3), boolConst(B_FALSE)); }
	| OTHERWISE ARROW act
		{    $$ = binaryExpr(CND_OTHER, boolConst(B_FALSE), ruleExpr(boolConst(B_TRUE), $3)); }
	| UNKNOWN ARROW act OTHERWISE ARROW act
		{    $$ = binaryExpr(CND_OTHER, ruleExpr(boolConst(B_TRUE), $3), ruleExpr(boolConst(B_TRUE), $6)); }
	| /* empty */
		{    $$ = NULL; }
	;

act	: '(' act ')'
		{   $$ = $2; }
	| act SEQ act
		{   $$ = actExpr(ACT_SEQ, $1, $3); }
	| act ALT act
		{   $$ = actExpr(ACT_ALT, $1, $3); }
	| SHELL actarg
		{   $$ = actExpr(ACT_SHELL, $2, NULL); }
	| SHELL num actarg		/* holdoff format */
		{   $$ = actExpr(ACT_SHELL, $3, $2); }
	| ALARM actarg
		{   $$ = actExpr(ACT_ALARM, $2, NULL); }
	| ALARM num actarg		/* holdoff format */
		{   $$ = actExpr(ACT_ALARM, $3, $2); }
	| SYSLOG actarg
		{   do_syslog_args($2);
		    $$ = actExpr(ACT_SYSLOG, $2, NULL);
		}
	| SYSLOG num actarg		/* holdoff format */
		{
		    do_syslog_args($3);
		    $$ = actExpr(ACT_SYSLOG, $3, $2);
		}
	| PRINT actarg
		{   $$ = actExpr(ACT_PRINT, $2, NULL); }
	| PRINT num actarg		/* holdoff format */
		{   $$ = actExpr(ACT_PRINT, $3, $2); }
	| STOMP actarg
		{
		    stomping = 1;
		    $$ = actExpr(ACT_STOMP, $2, NULL);
		}
	| STOMP num actarg		/* holdoff format */
		{
		    stomping = 1;
		    $$ = actExpr(ACT_STOMP, $3, $2);
		}

	/* error reporting */
	| error SEQ
		{   gramerr(act_str, precede, opStrings(ACT_SEQ));
		    $$ = NULL; }
/*** following cause harmless shift/reduce conflicts ***/
	| act SEQ error
		{   gramerr(act_str, follow, opStrings(ACT_SEQ));
		    $$ = NULL; }
	| error ALT
		{   gramerr(act_str, precede, opStrings(ACT_ALT));
		    $$ = NULL; }
	| act ALT error
		{   gramerr(act_str, follow, opStrings(ACT_ALT));
		    $$ = NULL; }
/*** preceding cause harmless shift/reduce conflicts ***/
	| SHELL error
		{   gramerr(tstr_str, follow, opStrings(ACT_SHELL));
		    $$ = NULL; }
	| ALARM error
		{   gramerr(tstr_str, follow, opStrings(ACT_ALARM));
		    $$ = NULL; }
	| SYSLOG error
		{   gramerr(tstr_str, follow, opStrings(ACT_SYSLOG));
		    $$ = NULL; }
	| PRINT error
		{   gramerr(tstr_str, follow, opStrings(ACT_PRINT));
		    $$ = NULL; }
	| STOMP error
		{   gramerr(tstr_str, follow, opStrings(ACT_STOMP));
		    $$ = NULL; }
	;

actarg	: arglist
		{   $$ = actArgExpr($1, NULL); }
	;

arglist	: STRING
		{   $$ = actArgList(NULL, $1); }
	| STRING arglist
		{   $$ = actArgList($2, $1); }
	;

bexp	: '(' bexp ')'
		{   $$ = $2; }
	| rexp
		{   $$ = $1; }
	| quant
		{   $$ = $1; }
	| TRU
		{   $$ = boolConst(B_TRUE); }
	| FALS
		{   $$ = boolConst(B_FALSE); }
	| NOT bexp
		{   $$ = unaryExpr(CND_NOT, $2); }
	| RISE bexp
		{   $$ = boolMergeExpr(CND_RISE, $2); }
	| FALL bexp
		{   $$ = boolMergeExpr(CND_FALL, $2); }
	| bexp AND bexp
		{   $$ = binaryExpr(CND_AND, $1, $3); }
	| bexp OR bexp
		{   $$ = binaryExpr(CND_OR, $1, $3); }
	| MATCH str bexp
		{   /*
		     * note args are reversed so bexp is to the "left"
		     * of the operand node in the expr tree
		     */
		    $$ = binaryExpr(CND_MATCH, $3, $2); }
	| NOMATCH str bexp
		{   $$ = binaryExpr(CND_NOMATCH, $3, $2); }

	/* error reporting */
	| NOT error
		{   gramerr(bexp_str, follow, opStrings(CND_NOT));
		    $$ = NULL; }
	| RISE error
		{   gramerr(bexp_str, follow, opStrings(CND_RISE));
		    $$ = NULL; }
	| FALL error
		{   gramerr(bexp_str, follow, opStrings(CND_FALL));
		    $$ = NULL; }
	| MATCH error
		{   gramerr("regular expression", follow, opStrings(CND_MATCH));
		    $$ = NULL; }
	| MATCH str error
		{   gramerr(bexp_str, follow, "regular expression");
		    $$ = NULL; }
	| NOMATCH error
		{   gramerr("regular expression", follow, opStrings(CND_NOMATCH));
		    $$ = NULL; }
	| NOMATCH str error
		{   gramerr(bexp_str, follow, "regular expression");
		    $$ = NULL; }
/*** following cause harmless shift/reduce conflicts ***/
	| error AND
		{   gramerr(bexp_str, precede, opStrings(CND_AND));
		    $$ = NULL; }
	| bexp AND error
		{   gramerr(bexp_str, follow, opStrings(CND_AND));
		    $$ = NULL; }
	| error OR
		{   gramerr(bexp_str, precede, opStrings(CND_OR));
		    $$ = NULL; }
	| bexp OR error
		{   gramerr(bexp_str, follow, opStrings(CND_OR));
		    $$ = NULL; }
/*** preceding cause harmless shift/reduce conflicts ***/
	;

quant	: ALL_QUANT dom bexp
		{   $$ = domainExpr(CND_ALL_HOST, $2, $3); }
	| SOME_QUANT dom bexp
		{   $$ = domainExpr(CND_SOME_HOST, $2, $3); }
	| NUMBER PCNT_QUANT dom bexp
		{   $$ = percentExpr($1, $3, $4); }

	/* error reporting */
	| ALL_QUANT dom error
		{   gramerr(bexp_str, follow, quant_str);
		    $$ = NULL; }
	| SOME_QUANT dom error
		{   gramerr(bexp_str, follow, quant_str);
		    $$ = NULL; }
	| NUMBER PCNT_QUANT dom error
		{   gramerr(bexp_str, follow, quant_str);
		    $$ = NULL; }
	| error PCNT_QUANT
		{   gramerr(num_str, precede, pcnt_str);
		    $$ = NULL; }
	;

rexp	: aexp EQ_REL aexp
		{   $$ = relExpr(CND_EQ, $1, $3); }
	| aexp NEQ_REL aexp
		{   $$ = relExpr(CND_NEQ, $1, $3); }
	| aexp '<' aexp
		{   $$ = relExpr(CND_LT, $1, $3); }
	| aexp '>' aexp
		{   $$ = relExpr(CND_GT, $1, $3); }
	| aexp LEQ_REL aexp
		{   $$ = relExpr(CND_LTE, $1, $3); }
	| aexp GEQ_REL aexp
		{   $$ = relExpr(CND_GTE, $1, $3); }

	/* error reporting */
	| error EQ_REL
		{   gramerr(aexp_str, precede, opStrings(CND_EQ));
		    $$ = NULL; }
	| aexp EQ_REL error
		{   gramerr(aexp_str, follow, opStrings(CND_EQ));
		    $$ = NULL; }
	| error NEQ_REL
		{   gramerr(aexp_str, precede, opStrings(CND_NEQ));
		    $$ = NULL; }
	| aexp NEQ_REL error
		{   gramerr(aexp_str, follow, opStrings(CND_NEQ));
		    $$ = NULL; }
	| error '<'
		{   gramerr(aexp_str, precede, opStrings(CND_LT));
		    $$ = NULL; }
	| aexp '<' error
		{   gramerr(aexp_str, follow, opStrings(CND_LT));
		    $$ = NULL; }
	| error '>'
		{   gramerr(aexp_str, precede, opStrings(CND_GT));
		    $$ = NULL; }
	| aexp '>' error
		{   gramerr(aexp_str, follow, opStrings(CND_GT));
		    $$ = NULL; }
	| error LEQ_REL
		{   gramerr(aexp_str, precede, opStrings(CND_LTE));
		    $$ = NULL; }
	| aexp LEQ_REL error
		{   gramerr(aexp_str, follow, opStrings(CND_LTE));
		    $$ = NULL; }
	| error GEQ_REL
		{   gramerr(aexp_str, precede, opStrings(CND_GTE));
		    $$ = NULL; }
	| aexp GEQ_REL error
		{   gramerr(aexp_str, follow, opStrings(CND_GTE));
		    $$ = NULL; }
	;

aexp	: '(' aexp ')'
		{   $$ = $2; }
	| fetch
		{   $$ = $1; }
	| num
		{   $$ = $1; }
	| VAR
		{   $$ = $1; }
	| aggr
		{   $$ = $1; }
	| RATE aexp
		{   $$ = numMergeExpr(CND_RATE, $2); }
	| INSTANT aexp
		{   $2->sem = PM_SEM_INSTANT;
		    $$ = unaryExpr(CND_INSTANT, $2); }
	| '-' aexp		%prec UMINUS
		{   $$ = unaryExpr(CND_NEG, $2); }
	| aexp '+' aexp
		{   $$ = binaryExpr(CND_ADD, $1, $3); }
	| aexp '-' aexp
		{   $$ = binaryExpr(CND_SUB, $1, $3); }
	| aexp '*' aexp
		{   $$ = binaryExpr(CND_MUL, $1, $3); }
	| aexp '/' aexp
		{   $$ = binaryExpr(CND_DIV, $1, $3); }

	/* error reporting */
	| RATE error
		{   gramerr(aexp_str, follow, opStrings(CND_RATE));
		    $$ = NULL; }
	| INSTANT error
		{   gramerr(aexp_str, follow, opStrings(CND_INSTANT));
		    $$ = NULL; }
	| '-' error		%prec UMINUS
		{   gramerr(aexp_str, follow, opStrings(CND_NEG));
		    $$ = NULL; }
/*** following cause harmless shift/reduce conflicts ***/
	| error '+'
		{   gramerr(aexp_str, precede, opStrings(CND_ADD));
		    $$ = NULL; }
	| aexp '+' error
		{   gramerr(aexp_str, follow, opStrings(CND_ADD));
		    $$ = NULL; }
	| error '-'
		{   gramerr(aexp_str, precede, opStrings(CND_SUB));
		    $$ = NULL; }
	| aexp '-' error
		{   gramerr(aexp_str, follow, opStrings(CND_SUB));
		    $$ = NULL; }
	| error '*'
		{   gramerr(aexp_str, precede, opStrings(CND_MUL));
		    $$ = NULL; }
	| aexp '*' error
		{   gramerr(aexp_str, follow, opStrings(CND_MUL));
		    $$ = NULL; }
	| error '/'
		{   gramerr(aexp_str, precede, opStrings(CND_DIV));
		    $$ = NULL; }
	| aexp '/' error
		{   gramerr(aexp_str, follow, opStrings(CND_DIV));
		    $$ = NULL; }
/*** preceding cause harmless shift/reduce conflicts ***/
	;

aggr	: SUM_AGGR dom aexp
		{   $$ = domainExpr(CND_SUM_HOST, $2, $3); }
	| AVG_AGGR dom aexp
		{   $$ = domainExpr(CND_AVG_HOST, $2, $3); }
	| MAX_AGGR dom aexp
		{   $$ = domainExpr(CND_MAX_HOST, $2, $3); }
	| MIN_AGGR dom aexp
		{   $$ = domainExpr(CND_MIN_HOST, $2, $3); }
	| COUNT_AGGR dom bexp
		{   $$ = domainExpr(CND_COUNT_HOST, $2, $3); }

	/* error reporting */
	| SUM_AGGR dom error
		{   gramerr(aexp_str, follow, aggr_str);
		    $$ = NULL; }
	| AVG_AGGR dom error
		{   gramerr(aexp_str, follow, aggr_str);
		    $$ = NULL; }
	| MAX_AGGR dom error
		{   gramerr(aexp_str, follow, aggr_str);
		    $$ = NULL; }
	| MIN_AGGR dom error
		{   gramerr(aexp_str, follow, aggr_str);
		    $$ = NULL; }
	;

dom	: HOST_DOM
		{   $$ = HOST_DOM; }
	| INST_DOM
		{   $$ = INST_DOM; }
	| TIME_DOM
		{   $$ = TIME_DOM; }
	;

fetch   : metric hosts insts times
		{   $$ = fetchExpr($1, $2, $3, $4); }
	;

metric	: IDENT
		{   $$ = $1; }
	;

hosts	: /* empty */
                {   $$.n = 0;
		    $$.ss = NULL; }
	| hosts ':' IDENT
		{   $$.n = $1.n + 1;
		    $$.ss = (char **) ralloc($1.ss, $$.n * sizeof(char *));
		    $$.ss[$1.n] = $3; }

	/* error reporting */
	| hosts ':' error 
		{   gramerr(host_str, follow, ":");
                    $$.n = 0;
		    $$.ss = NULL; }
	;

insts	: /* empty */
                {   $$.n = 0;
		    $$.ss = NULL; }
	| insts '#' IDENT
		{   $$.n = $1.n + 1;
		    $$.ss = (char **) ralloc($1.ss, $$.n * sizeof(char *));
		    $$.ss[$1.n] = $3; }

	/* error reporting */
	| insts '#' error 
		{   gramerr(inst_str, follow, "#");
                    $$.n = 0;
		    $$.ss = NULL; }
	;

times	: /* empty */
		{   $$.t1 = 0;
		    $$.t2 = 0; }
	| '@' NUMBER
		{   $$.t1 = $2;
		    $$.t2 = $2; }
	| '@' NUMBER INTERVAL NUMBER
		{   if ($2 <= $4) {
			$$.t1 = $2;
			$$.t2 = $4;
		    }
		    else {
			$$.t1 = $4;
			$$.t2 = $2;
		    } }

	/* error reporting */
	| '@' error 
		{   gramerr(sample_str, follow, "@");
                    $$.t1 = 0;
                    $$.t2 = 0; }
	;

num	: NUMBER units
		{   $$ = numConst($1, $2); }
	;

units	: /* empty */
		{   $$ = noUnits; }
	| units unit
		{   $$ = $1;
		    if ($2.dimSpace) {
			$$.dimSpace = $2.dimSpace;
			$$.scaleSpace = $2.scaleSpace;
		    }
		    else if ($2.dimTime) {
			$$.dimTime = $2.dimTime;
			$$.scaleTime = $2.scaleTime;
		    }
		    else {
			$$.dimCount = $2.dimCount;
			$$.scaleCount = $2.scaleCount;
		    } }
	| units UNIT_SLASH unit
		{   $$ = $1;
		    if ($3.dimSpace) {
			$$.dimSpace = -$3.dimSpace;
			$$.scaleSpace = $3.scaleSpace;
		    }
		    else if ($3.dimTime) {
			$$.dimTime = -$3.dimTime;
			$$.scaleTime = $3.scaleTime;
		    }
		    else {
			$$.dimCount = -$3.dimCount;
			$$.scaleCount = $3.scaleCount;
		    } }
	;

unit	: SPACE_UNIT
		{   $$ = $1; }
	| SPACE_UNIT '^' NUMBER
		{   $$ = $1;
		    $$.dimSpace = $3; }
	| TIME_UNIT
		{   $$ = $1; }
	| TIME_UNIT '^' NUMBER
		{   $$ = $1;
		    $$.dimTime = $3; }
	| EVENT_UNIT
		{   $$ = $1; }
	| EVENT_UNIT '^' NUMBER
		{   $$ = $1;
		    $$.dimCount = $3; }
	;

str	: STRING
		{   $$ = strConst($1); }
	;

%%

