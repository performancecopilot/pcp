/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 */
#ifndef PCP_FAULT_H
#define PCP_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Routines to support fault injection infrastructure
 *
 * Build libpcp with -DPM_FAULT_INJECTION to enable all of this.
 */
extern void __pmFaultInject(const char *, int);
extern void __pmFaultSummary(FILE *f);

#ifdef PM_FAULT_INJECTION
extern int __pmFault_arm;
#define PM_FAULT_POINT(ident, class) __pmFaultInject(ident, class)
#ifdef malloc
#undef malloc
#endif
#define malloc(x) __pmFault_malloc(x)
extern void *__pmFault_malloc(size_t);
#ifdef realloc
#undef realloc
#endif
#define realloc(x,y) __pmFault_realloc(x,y)
extern void *__pmFault_realloc(void *, size_t);
#ifdef strdup
#undef strdup
#endif
#define strdup(x) __pmFault_strdup(x)
extern char *__pmFault_strdup(const char *);
#define PM_FAULT_CHECK(class) if (__pmFault_arm == PM_FAULT_PMAPI) { __pmFault_arm = 0; return PM_ERR_FAULT; }
#else
#define PM_FAULT_POINT(ident, class)
#define PM_FAULT_CHECK(class)
#endif

/*
 * Classes of fault types (second arg to __pmFaultInject())
 */
#define PM_FAULT_ALLOC	100
#define PM_FAULT_PMAPI	101

#ifdef __cplusplus
}
#endif

#endif /* PCP_FAULT_H */
