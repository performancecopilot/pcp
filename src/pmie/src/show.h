/***********************************************************************
 * show.h - output syntax and values
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
#include "dstruct.h"

char *opStrings(int);

void showSyntax(FILE *,Symbol);
void showSubsyntax(FILE *, Symbol);
void showValue(FILE *, Expr *);
void showAnnotatedValue(FILE *, Expr *);
void showSatisfyingValue(FILE *, Expr *);
void showTime(FILE *, RealTime);
void showFullTime(FILE *, RealTime);
size_t formatSatisfyingValue(char *, size_t, char **);
