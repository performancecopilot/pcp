/***********************************************************************
 * fun.h - expression evaluator functions
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
#ifndef FUN_H
#define FUN_H

#include "dstruct.h"
#include "andor.h"

#define ROTATE(x)  if ((x)->nsmpls > 1) rotate(x);
#define EVALARG(x) if ((x)->op < NOP) ((x)->eval)(x);

/* expression evaluator function prototypes */
void rule(Expr *);
void ruleset(Expr *);
void cndFetch_all(Expr *);
void cndFetch_n(Expr *);
void cndFetch_1(Expr *);
void cndDelay_n(Expr *);
void cndDelay_1(Expr *);
void cndRate_n(Expr *);
void cndRate_1(Expr *);
void cndInstant_n(Expr *);
void cndInstant_1(Expr *);
void cndSum_host(Expr *);
void cndSum_inst(Expr *);
void cndSum_time(Expr *);
void cndAvg_host(Expr *);
void cndAvg_inst(Expr *);
void cndAvg_time(Expr *);
void cndMax_host(Expr *);
void cndMax_inst(Expr *);
void cndMax_time(Expr *);
void cndMin_host(Expr *);
void cndMin_inst(Expr *);
void cndMin_time(Expr *);
void cndNeg_n(Expr *);
void cndNeg_1(Expr *);
void cndAdd_n_n(Expr *);
void cndAdd_1_n(Expr *);
void cndAdd_n_1(Expr *);
void cndAdd_1_1(Expr *);
void cndSub_n_n(Expr *);
void cndSub_1_n(Expr *);
void cndSub_n_1(Expr *);
void cndSub_1_1(Expr *);
void cndMul_n_n(Expr *);
void cndMul_1_n(Expr *);
void cndMul_n_1(Expr *);
void cndMul_1_1(Expr *);
void cndDiv_n_n(Expr *);
void cndDiv_1_n(Expr *);
void cndDiv_n_1(Expr *);
void cndDiv_1_1(Expr *);
void cndEq_n_n(Expr *);
void cndEq_1_n(Expr *);
void cndEq_n_1(Expr *);
void cndEq_1_1(Expr *);
void cndNeq_n_n(Expr *);
void cndNeq_1_n(Expr *);
void cndNeq_n_1(Expr *);
void cndNeq_1_1(Expr *);
void cndLt_n_n(Expr *);
void cndLt_1_n(Expr *);
void cndLt_n_1(Expr *);
void cndLt_1_1(Expr *);
void cndLte_n_n(Expr *);
void cndLte_1_n(Expr *);
void cndLte_n_1(Expr *);
void cndLte_1_1(Expr *);
void cndGt_n_n(Expr *);
void cndGt_1_n(Expr *);
void cndGt_n_1(Expr *);
void cndGt_1_1(Expr *);
void cndGte_n_n(Expr *);
void cndGte_1_n(Expr *);
void cndGte_n_1(Expr *);
void cndGte_1_1(Expr *);
void cndNot_n(Expr *);
void cndNot_1(Expr *);
void cndRise_n(Expr *);
void cndRise_1(Expr *);
void cndFall_n(Expr *);
void cndFall_1(Expr *);
void cndMatch_inst(Expr *);
void cndAll_host(Expr *);
void cndAll_inst(Expr *);
void cndAll_time(Expr *);
void cndSome_host(Expr *);
void cndSome_inst(Expr *);
void cndSome_time(Expr *);
void cndPcnt_host(Expr *);
void cndPcnt_inst(Expr *);
void cndPcnt_time(Expr *);
void cndCount_host(Expr *);
void cndCount_inst(Expr *);
void cndCount_time(Expr *);
void actAnd(Expr *);
void actOr(Expr *);
void actShell(Expr *);
void actAlarm(Expr *);
void actSyslog(Expr *);
void actPrint(Expr *);
void actStomp(Expr *);
void actArg(Expr *);
void actFake(Expr *);

#endif /* FUN_H */

