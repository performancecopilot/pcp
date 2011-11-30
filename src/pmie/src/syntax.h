/***********************************************************************
 * syntax.h - inference rule language parser
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
#ifndef SYNTAX_H
#define SYNTAX_H


/***********************************************************************
 * ONLY FOR USE BY: lexicon.c grammar.y
 ***********************************************************************/

typedef struct {
        int     n;              /* number of elements */
        char    **ss;           /* dynamically allocated array */
} StringArray;

typedef struct {
        int     t1;             /* start of interval */
        int     t2;             /* end of interval */
} Interval;

/* parser stack entry */
typedef union {
    int                  i;
    char                *s;
    double               d;
    StringArray          sa;
    Interval             t;
    pmUnits              u;
    Metric              *m;
    Expr                *x;
    Symbol              *z;
} YYSTYPE;
#define YYSTYPE_IS_DECLARED 1

extern YYSTYPE yylval;

/* error reporting */
extern int      errs;                   /* error count */
void yyerror(char *);
void synerr(void);
void synwarn(void);

/* parser actions */
Symbol statement(char *, Expr *);
Expr *ruleExpr(Expr *, Expr *);
Expr *relExpr(int, Expr *, Expr *);
Expr *binaryExpr(int, Expr *, Expr *);
Expr *unaryExpr(int, Expr *);
Expr *domainExpr(int, int, Expr *);
Expr *percentExpr(double, int, Expr *);
Expr *numMergeExpr(int, Expr *);
Expr *boolMergeExpr(int, Expr *);
Expr *fetchExpr(char *, StringArray, StringArray, Interval);
Expr *numConst(double, pmUnits);
Expr *strConst(char *);
Expr *boolConst(Truth);
Expr *numVar(Expr *);
Expr *boolVar(Expr *);
Expr *actExpr(int, Expr *, Expr *);
Expr *actArgExpr(Expr *, Expr *);
Expr *actArgList(Expr *, char *);

/* parse tree */
extern Symbol parse;



/***********************************************************************
 * public
 ***********************************************************************/

/* Initialization to be called at the start of new input file. */
int synInit(char *);

/* parse single statement */
Symbol syntax(void);

#endif /* SYNTAX_H */

