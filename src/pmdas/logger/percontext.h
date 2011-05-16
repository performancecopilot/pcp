/*
 * Some functions become per-client (of pmcd) with the introduction
 * of PMDA_INTERFACE_5.
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
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

extern int ctx_start(int ctx);
extern void ctx_end(int ctx);
extern int ctx_get_num(void);

/*
 * Context callbacks:
 *
 * ctxStartContextCallBack
 * 	Called the first time a new client context is seen.  Returns a
 * 	'void *' to user-created data that can be retrieved later with
 * 	ctx_get_user_data().
 *
 * ctxEndContextCallBack
 * 	Called when a client context is closed.  Can be used to clean
 * 	up user data created by ctxStartContextCallBack.
 */
typedef void *(*ctxStartContextCallBack)(int ctx);
typedef void (*ctxEndContextCallBack)(int ctx, void *user_data);

extern void ctx_register_callbacks(ctxStartContextCallBack start,
				   ctxEndContextCallBack end);

/* Returns the user data associated with the current client context. */
extern void *ctx_get_user_data(void);

/* Get and set access level for current client context event streams */
extern int ctx_get_user_access(void);
extern void ctx_set_user_access(int enable);

#endif /* _PERCONTEXT_H */
