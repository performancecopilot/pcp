/*
 * Copyright (c) 1996 Silicon Graphics, Inc.  All Rights Reserved.
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

/* gadget tokens */
#define TOK_LINE		1
#define TOK_LABEL		2
#define TOK_BAR			3
#define TOK_MULTIBAR		4
#define TOK_BARGRAPH		5
#define TOK_LED			6

/* gadget building block tokens */
#define TOK_LEGEND		100
#define TOK_COLOURLIST		101
#define TOK_ACTIONLIST		102

/* other reserved words' tokens */
#define TOK_BAD_RES_WORD	200
#define TOK_UPDATE		201
#define TOK_METRIC		202
#define TOK_HORIZONTAL		203
#define TOK_VERTICAL		204
#define TOK_METRICS		205
#define TOK_MIN			206
#define TOK_MAX			207
#define TOK_DEFAULT		208
#define TOK_FIXED		209
#define TOK_COLOUR		210
#define TOK_HISTORY		211
#define TOK_NOBORDER		212

/* other lexical symbols' tokens */
#define TOK_IDENTIFIER		300
#define TOK_INTEGER		301
#define TOK_REAL		302
#define TOK_STRING		303
#define TOK_LPAREN		304
#define TOK_RPAREN		305
#define TOK_LBRACKET		306
#define TOK_RBRACKET		307
#define TOK_COLON		308

/* end of file */
#define TOK_EOF			666

extern unsigned nLines;
extern int tokenIntVal;
extern double tokenRealVal;
extern char* tokenStringVal;
