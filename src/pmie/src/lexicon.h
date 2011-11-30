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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/***********************************************************************
 * lexicon.h - lexical scanner
 ***********************************************************************/

#ifndef LEXICON_H
#define LEXICON_H

/***********************************************************************
 * ONLY FOR USE BY: lexicon.c and syntax.c
 ***********************************************************************/

#define LEX_MAX 254			/* max length of token */

/* scanner input context stack entry */
typedef struct lexin {
    struct lexin *prev;                 /* calling context on stack */
    FILE         *stream;               /* rule input stream */
    char         *macro;                /* input from macro definition */
    char         *name;                 /* file/macro name */
    int          lno;                   /* current line number */
    int          cno;                   /* current column number */
    int          lookin;                /* lookahead buffer input index */
    int          lookout;               /* lookahead buffer output index */
    signed char  look[LEX_MAX + 2];     /* lookahead ring buffer */
} LexIn;

extern LexIn    *lin;                   /* current input context */


/***********************************************************************
 * public
 ***********************************************************************/

/* initialize scan of new input file */
int lexInit(char *);

/* finalize scan of input stream */
void lexFinal(void);

/* not end of input stream? */
int lexMore(void);

/* discard input until ';' or EOF */
void lexSync(void);

/* scanner main function */
int yylex(void);

/* yacc parser */
int yyparse(void);

#endif /* LEXICON_H */

