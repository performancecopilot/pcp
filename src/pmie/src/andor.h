/***********************************************************************
 * andor.h - Logical AND/OR expression evaluator functions
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
#ifndef ANDOR_H
#define ANDOR_H

/* expression evaluator function prototypes */
void cndOr_n_n(Expr *);
void cndOr_1_n(Expr *);
void cndOr_n_1(Expr *);
void cndOr_1_1(Expr *);
void cndAnd_n_n(Expr *);
void cndAnd_1_n(Expr *);
void cndAnd_n_1(Expr *);
void cndAnd_1_1(Expr *);

#endif /* ANDOR_H */
