/*
 * Some functions become per-client (of pmcd) with the introduction
 * of PMDA_INTERFACE_5.
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
#ifndef _PERCONTEXT_H
#define _PERCONTEXT_H

extern int sample_get_recv(int);
extern int sample_get_xmit(int);
extern void sample_clr_recv(int);
extern void sample_clr_xmit(int);
extern void sample_inc_recv(int);
extern void sample_inc_xmit(int);
extern int sample_ctx_fetch(int, int);
extern void sample_ctx_end(int);

#define CTX_ALL	-1

#endif /* _PERCONTEXT_H */
