/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"

#define PDU_CHUNK	1024	/* unit of space allocation for PDU buffer */

typedef struct bufctl {
    struct bufctl	*bc_next;
    int			bc_size;
    int			bc_pincnt;
    char		*bc_buf;
    char		*bc_bufend;
} bufctl_t;

static bufctl_t	*buf_free;
static bufctl_t	*buf_pin;
static bufctl_t	*buf_pin_tail;

#ifdef PCP_DEBUG
static void
pdubufdump(void)
{
    bufctl_t	*pcp;

    if (buf_free != NULL) {
	fprintf(stderr, "   free pdubuf[size]:");
	for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next)
	    fprintf(stderr, " " PRINTF_P_PFX "%p[%d]", pcp->bc_buf, pcp->bc_size);
	fputc('\n', stderr);
    }

    if (buf_pin != NULL) {
	fprintf(stderr, "   pinned pdubuf[pincnt]:");
	for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next)
	    fprintf(stderr, " " PRINTF_P_PFX "%p[%d]", pcp->bc_buf, pcp->bc_pincnt);
	fputc('\n', stderr);
    }
}
#endif

__pmPDU *
__pmFindPDUBuf(int need)
{
    bufctl_t	*pcp;

    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    break;
    }
    if (pcp == NULL) {
	if ((pcp = (bufctl_t *)malloc(sizeof(*pcp))) == NULL)
	    return NULL;
	pcp->bc_pincnt = 0;
	pcp->bc_size = PDU_CHUNK * (1 + need/PDU_CHUNK);
	if ((pcp->bc_buf = (char *)valloc(pcp->bc_size)) == NULL) {
	    free(pcp);
	    return NULL;
	}
	pcp->bc_next = buf_free;
	pcp->bc_bufend = &pcp->bc_buf[pcp->bc_size];
	buf_free = pcp;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "__pmFindPDUBuf(%d) -> " PRINTF_P_PFX "%p\n", need, pcp->bc_buf);
	pdubufdump();
    }
#endif

    return (__pmPDU *)pcp->bc_buf;
}

void
__pmPinPDUBuf(void *handle)
{
    bufctl_t	*pcp;
    bufctl_t	*prior = NULL;

    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_buf <= (char *)handle && (char *)handle < pcp->bc_bufend)
	    break;
	prior = pcp;
    }

    if (pcp != NULL) {
	/* first pin for this buffer, move between lists */
	if (prior == NULL)
	    buf_free = pcp->bc_next;
	else
	    prior->bc_next = pcp->bc_next;
	pcp->bc_next = NULL;
	if (buf_pin_tail != NULL)
	    buf_pin_tail->bc_next = pcp;
	buf_pin_tail = pcp;
	if (buf_pin == NULL)
	    buf_pin = pcp;
	pcp->bc_pincnt = 1;
    }
    else {
	for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	    if (pcp->bc_buf <= (char *)handle && (char *)handle < pcp->bc_bufend)
		break;
	}
	if (pcp != NULL)
	    pcp->bc_pincnt++;
	else {
	    __pmNotifyErr(LOG_WARNING, "__pmPinPDUBuf: 0x%x not in pool!", handle);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF)
		pdubufdump();
#endif
	    return;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPinPDUBuf(" PRINTF_P_PFX "%p) -> pdubuf=" PRINTF_P_PFX "%p, cnt=%d\n",
	    handle, pcp->bc_buf, pcp->bc_pincnt);
#endif
    return;
}

int
__pmUnpinPDUBuf(void *handle)
{
    bufctl_t	*pcp;
    bufctl_t	*prior = NULL;

    for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_buf <= (char *)handle && (char *)handle < &pcp->bc_buf[pcp->bc_size])
	    break;
	prior = pcp;
    }
    if (pcp == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDUBUF) {
	    fprintf(stderr, "__pmUnpinPDUBuf(" PRINTF_P_PFX "%p) -> fails\n", handle);
	    pdubufdump();
	}
#endif
	return 0;
    }

    if (--pcp->bc_pincnt == 0) {
	if (prior == NULL)
	    buf_pin = pcp->bc_next;
	else
	    prior->bc_next = pcp->bc_next;
	if (buf_pin_tail == pcp)
	    buf_pin_tail = prior;
	
	pcp->bc_next = buf_free;
	buf_free = pcp;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmUnpinPDUBuf(" PRINTF_P_PFX "%p) -> pdubuf=" PRINTF_P_PFX "%p, pincnt=%d\n",
		handle, pcp->bc_buf, pcp->bc_pincnt);
#endif

    return 1;
}

void
__pmCountPDUBuf(int need, int *alloc, int *free)
{
    bufctl_t	*pcp;
    int		count;

    count = 0;
    for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    count++;
    }
    *alloc = count;

    count = 0;
    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    count++;
    }
    *free = count;
    *alloc += count;

    return;
}
