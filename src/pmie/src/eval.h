/***********************************************************************
 * eval.h
 ***********************************************************************
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Red Hat
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
#ifndef EVAL_H
#define EVAL_H

#include "dstruct.h"

/* does function operator return a scalar result? */
int isScalarResult(Expr *);

/* fill in apprpriate evaluator function for given Expr */
void findEval(Expr *);

/* run evaluator until specified time reached */
void run(void);

/* invalidate one expression (and descendents) */
void clobber(Expr *);

/* invalidate all expressions being evaluated */
void invalidate(void);

/* report changes in pmcd connection state */
#define STATE_INIT	0
#define STATE_FAILINIT	1
#define STATE_RECONN	2
#define STATE_LOSTCONN	3
int host_state_changed(const char *, int);

#endif /* EVAL_H */

