/*
 * Copyright (c) 1999 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "pmapi.h"	/* _pmCtime only */
#include "impl.h"	/* _pmCtime only */
#include "dstruct.h"
#include "fun.h"
#include "show.h"

/*
 * x-arg1 is the bexp, x->arg2 is the regex
 */
void
cndMatch_inst(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Boolean	*ip1;
    Boolean	*op;
    int		n;
    int         i;
    int		sts;
    int		mi;
    Metric	*m;

    EVALARG(arg1)
    ROTATE(x)


#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "match_inst(" PRINTF_P_PFX "%p): regex handle=" PRINTF_P_PFX "%p desire %s\n",
	    x, arg2->ring, x->op == CND_MATCH ? "match" : "nomatch");
	dumpExpr(x);
    }
#endif

    if (arg2->sem != SEM_REGEX) {
	fprintf(stderr, "cndMatch_inst: internal botch arg2 not SEM_REGEX?\n");
	dumpExpr(arg2);
	exit(1);
    }

    if (arg1->tspan > 0) {

	mi = 0;
	m = &arg1->metrics[mi++];
	i = 0;
	ip1 = (Boolean *)(&arg1->smpls[0])->ptr;
	op = (Boolean *)(&x->smpls[0])->ptr;

	for (n = 0; n < arg1->tspan; n++) {

	    if (!arg2->valid || !arg1->valid) {
		*op++ = B_UNKNOWN;
	    }
	    else if (x->e_idom <= 0) {
		*op++ = B_FALSE;
	    }
	    else {
		while (i >= m->m_idom) {
		    /*
		     * no more values, next metric
		     */
		    m = &arg1->metrics[mi++];
		    i = 0;
		}

		if (m->inames == NULL) {
		    *op++ = B_FALSE;
		}
		else {
		    sts = regexec((regex_t *)arg2->ring, m->inames[i], 0, NULL, 0);
#if PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL2) {
			if (x->op == CND_MATCH && sts != REG_NOMATCH) {
			    fprintf(stderr, "match_inst: inst=\"%s\" match && %s\n",
				m->inames[i],
				*ip1 == B_TRUE ? "true" :
				    (*ip1 == B_FALSE ? "false" :
					(*ip1 == B_UNKNOWN ? "unknown" : "bogus" )));

			}
			else if (x->op == CND_NOMATCH && sts == REG_NOMATCH) {
			    fprintf(stderr, "match_inst: inst=\"%s\" nomatch && %s\n",
				m->inames[i],
				*ip1 == B_TRUE ? "true" :
				    (*ip1 == B_FALSE ? "false" :
					(*ip1 == B_UNKNOWN ? "unknown" : "bogus" )));
			}
		    }
#endif
		    if ((x->op == CND_MATCH && sts != REG_NOMATCH) ||
			(x->op == CND_NOMATCH && sts == REG_NOMATCH))
			*op++ = *ip1 && B_TRUE;
		    else
			*op++ = *ip1 && B_FALSE;
		}
		i++;
	    }
	    ip1++;
	}
	x->valid++;
    }
    else
	x->valid = 0;

    x->smpls[0].stamp = arg1->smpls[0].stamp;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndMatch_inst(" PRINTF_P_PFX "%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

