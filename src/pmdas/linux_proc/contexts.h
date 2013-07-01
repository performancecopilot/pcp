/*
 * Copyright (c) 2013 Red Hat.
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

#ifndef _CONTEXTS_H
#define _CONTEXTS_H

/*
 * Handle newly arriving clients, security attributes being set on 'em,
 * switching to alternative accounts (temporarily) and back, and client
 * termination.  State maintained in a global table, with a high-water
 * allocator and active/inactive entry tracking.
 */

enum {
    CTX_INACTIVE = 0x0,
    CTX_ACTIVE = 0x1,
    CTX_USERID = 0x2,
    CTX_GROUPID = 0x4
};

typedef struct {
    unsigned int	state;
    uid_t		uid;
    gid_t		gid;
} proc_perctx_t;

extern void proc_ctx_init(void);
extern int proc_ctx_attrs(int, int, const char *, int, pmdaExt *);
extern void proc_ctx_access(int);
extern void proc_ctx_revert(int);
extern void proc_ctx_end(int);

#endif	/* _CONTEXTS_H */
