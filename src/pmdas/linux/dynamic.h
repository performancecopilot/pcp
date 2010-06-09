/*
 * Dynamic namespace metrics for the Linux kernel PMDA
 *
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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

/* function to refresh a specific subtree */
typedef void (*pmnsUpdate)(pmdaExt *, __pmnsTree **);

extern void linux_dynamic_pmns(const char *, int *, int, pmnsUpdate);
extern __pmnsTree *linux_dynamic_lookup_name(pmdaExt *, const char *);
extern __pmnsTree *linux_dynamic_lookup_pmid(pmdaExt *, pmID);

