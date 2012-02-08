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

#ifndef _TRACE_HASH_H
#define _TRACE_HASH_H

#ifdef __cplusplus      
extern "C" {
#endif

typedef int  (*__pmHashCmpFunc)(void *, void *);
typedef int  (*__pmHashKeyCmpFunc)(void *, const char *);
typedef void (*__pmHashDelFunc)(void *);

struct __pmHashEl {
    void		*ent;
    struct __pmHashEl	*next;
};
typedef struct __pmHashEl __pmHashEnt;

typedef struct {
    size_t		tsize;
    size_t		esize;
    unsigned int	entries;
    __pmHashCmpFunc	cmp;
    __pmHashDelFunc	del;
    __pmHashEnt		**rows;
    /*void		**pool;*/
} __pmHashTable;

extern int __pmhashinit(__pmHashTable *, size_t, size_t, __pmHashCmpFunc, __pmHashDelFunc);

extern void __pmhashtrunc(__pmHashTable *);

extern int __pmhashinsert(__pmHashTable *, const char *, void *);

extern void *__pmhashlookup(__pmHashTable *, const char *, void *);

typedef void (*__pmHashIterFunc)(__pmHashTable *, void *);
extern void __pmhashtraverse(__pmHashTable *, __pmHashIterFunc);

#ifndef PM_HASH_TUNE
#define PM_HASH_SHFT	5
#define PM_HASH_SIZE	31
#endif

#ifdef __cplusplus      
}
#endif

#endif /* _TRACE_HASH_H */
