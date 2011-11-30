/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * andor.c
 *
 * These functions were originally generated from skeletons <file>.sk
 * by the shell-script './meta', then modified to support the semantics
 * of the boolean AND/OR operators correctly.  These are different to
 * every other operator in that they do not always require both sides
 * of the expression to be available in order to be evaluated, i.e.
 *   OR: if either side of the expression is true, expr is true
 *   AND: if either side of the expression is false, expr is false
 ***********************************************************************/

#include "pmapi.h"
#include "dstruct.h"
#include "pragmatics.h"
#include "fun.h"
#include "show.h"
#include "stomp.h"


/*
 *  operator: cndOr
 */

#define OR(x,y) (((x) == TRUE || (y) == TRUE) ? TRUE : (((x) == FALSE && (y) == FALSE) ? FALSE : DUNNO))
#define OR1(x) ((x) == TRUE ? TRUE : DUNNO)

void
cndOr_n_n(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	*ip1;
    Truth	*ip2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR(*ip1, *ip2);
	    ip1++;
	    ip2++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR1(*ip1);
	    ip1++;
	}
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid && x->tspan > 0) {
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR1(*ip2);
	    ip2++;
	}
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndOr_n_n(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndOr_n_1(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	*ip1;
    Truth	iv2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	iv2 = *(Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR(*ip1, iv2);
	    ip1++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR1(*ip1);
	    ip1++;
	}
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid && x->tspan > 0) {
	*(Truth *)os->ptr = OR1(*(Truth *)is2->ptr);
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndOr_n_1(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndOr_1_n(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	iv1;
    Truth	*ip2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid && x->tspan > 0) {
	iv1 = *(Truth *)is1->ptr;
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR(iv1, *ip2);
	    ip2++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid && x->tspan > 0) {
	*(Truth *)os->ptr = OR1(*(Truth *)is1->ptr);
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid && x->tspan > 0) {
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = OR1(*ip2);
	    ip2++;
	}
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndOr_1_n(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndOr_1_1(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid) {
	*(Truth *)os->ptr = OR(*(Truth *)is1->ptr, *(Truth *)is2->ptr);
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid) {
	*(Truth *)os->ptr = OR1(*(Truth *)is1->ptr);
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid) {
	*(Truth *)os->ptr = OR1(*(Truth *)is2->ptr);
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndOr_1_1(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

/*
 *  operator: cndAnd
 */

#define AND(x,y) (((x) == TRUE && (y) == TRUE) ? TRUE : (((x) == FALSE || (y) == FALSE) ? FALSE : DUNNO))
#define AND1(x)  (((x) == FALSE) ? FALSE : DUNNO)

void
cndAnd_n_n(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	*ip1;
    Truth	*ip2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid) {
	ip1 = (Truth *)is1->ptr;
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND(*ip1, *ip2);
	    ip1++;
	    ip2++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid) {
	ip1 = (Truth *)is1->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND1(*ip1);
	    ip1++;
	}
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid) {
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND1(*ip2);
	    ip2++;
	}
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndAnd_n_n(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndAnd_n_1(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	*ip1;
    Truth	iv2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	iv2 = *(Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND(*ip1, iv2);
	    ip1++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid && x->tspan > 0) {
	ip1 = (Truth *)is1->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND1(*ip1);
	    ip1++;
	}
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid && x->tspan > 0) {
	*(Truth *)os->ptr = AND1(*(Truth *)is2->ptr);
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndAnd_n_1(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndAnd_1_n(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];
    Truth	iv1;
    Truth	*ip2;
    Truth	*op;
    int         i;

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid && x->tspan > 0) {
	iv1 = *(Truth *)is1->ptr;
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND(iv1, *ip2);
	    ip2++;
	}
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid && x->tspan > 0) {
	*(Truth *)os->ptr = AND1(*(Truth *)is1->ptr);
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid && x->tspan > 0) {
	ip2 = (Truth *)is2->ptr;
	op = (Truth *)os->ptr;
	for (i = 0; i < x->tspan; i++) {
	    *op++ = AND1(*ip2);
	    ip2++;
	}
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndAnd_1_n(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

void
cndAnd_1_1(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Sample      *is1 = &arg1->smpls[0];
    Sample      *is2 = &arg2->smpls[0];
    Sample      *os = &x->smpls[0];

    EVALARG(arg1)
    EVALARG(arg2)
    ROTATE(x)

    if (arg1->valid && arg2->valid) {
	*(Truth *)os->ptr = AND(*(Truth *)is1->ptr, *(Truth *)is2->ptr);
	os->stamp = (is1->stamp > is2->stamp) ? is1->stamp : is2->stamp;
	x->valid++;
    }
    else if (arg1->valid) {
	*(Truth *)os->ptr = AND1(*(Truth *)is1->ptr);
	os->stamp = is1->stamp;
	x->valid++;
    }
    else if (arg2->valid) {
	*(Truth *)os->ptr = AND1(*(Truth *)is2->ptr);
	os->stamp = is2->stamp;
	x->valid++;
    }
    else x->valid = 0;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndAnd_1_1(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}
