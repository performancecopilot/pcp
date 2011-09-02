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

#ifndef _FAULT_H
#define _FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Routines to support fault injection infrastructure
 *
 * Build libpcp with -DPM_FAULT_INJECTION to enable all of this.
 */
extern void __pmFaultInject(char *, int);
extern void __pmFaultSummary(void);

#ifdef PM_FAULT_INJECTION
extern int __pmFault_arm;
#define PM_FAULT_POINT(ident, class) __pmFaultInject(ident, class)
#define malloc(x) __pmFault_malloc(x)
extern void *__pmFault_malloc(size_t);
#define strdup(x) __pmFault_strdup(x)
extern char *__pmFault_strdup(const char *);
#define PM_FAULT_CHECK(class) if (__pmFault_arm == class) { __pmFault_arm = 0; return PM_ERR_FAULT; }
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

#endif /* _FAULT_H */
