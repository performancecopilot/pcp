/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "data.h"

int
instcmp(void *a, void *b)
{
    instdata_t  *aa = (instdata_t *)a;
    instdata_t  *bb = (instdata_t *)b;

    if (aa == NULL || bb == NULL)
	return 0;
    if (aa->type != bb->type)
	return 0;
    return !strcmp(aa->tag, bb->tag);
}

void
instdel(void *a)
{
    instdata_t	*k = (instdata_t *)a;

    if (k != NULL) {
	if (k->tag != NULL)
	    free(k->tag);
	free(k);
    }
}

void
instprint(__pmHashTable *t, void *e)
{
    instdata_t	*i = (instdata_t *)e;

    __pmNotifyErr(LOG_DEBUG, "Instance history table entry\n"
	"Name:      '%s'\n type:     %d\n inst:     %d\n",
	i->tag, i->type, i->instid);
}


int
datacmp(void *a, void *b)
{
    hashdata_t	*aa = (hashdata_t *)a;
    hashdata_t	*bb = (hashdata_t *)b;

    if (aa == NULL || bb == NULL)
	return 0;
    if (aa->tracetype != bb->tracetype)
	return 0;
    return !strcmp(aa->tag, bb->tag);
}

void
datadel(void *a)
{
    hashdata_t	*k = (hashdata_t *)a;

    if (k != NULL) {
	if (k->tag != NULL)
	    free(k->tag);
	free(k);
    }
}

void
dataprint(__pmHashTable *t, void *e)
{
    hashdata_t	*h = (hashdata_t *)e;

    __pmNotifyErr(LOG_DEBUG, "PMDA hash table entry\n"
	"Name:      '%s'\n filedes:  %d\n"
	" type:     %d\n length:   %d\n"
	" padding:  %d\n count:    %d\n"
	" txmin:    %f\n txmax:    %f\n"
	" txsum:    %f\nSize:      %d\n"
	"-----------\n",
	h->tag, h->fd, h->tracetype, h->taglength, h->padding,
	(int)h->txcount, h->txmin, h->txmax, h->txsum, (int)sizeof(*h));
}


void
debuglibrary(int flag)
{
    extern int	__pmstate;
    int		state;

    state = pmtracestate(0);
    if (flag & DBG_TRACE_APPL0)
	state |= PMTRACE_STATE_COMMS;
    if (flag & DBG_TRACE_PDU)
	state |= PMTRACE_STATE_PDU;
    if (flag & DBG_TRACE_PDUBUF)
	state |= PMTRACE_STATE_PDUBUF;
    if (flag == 0)
	__pmstate = 0;
    else
	pmtracestate(state);
}


