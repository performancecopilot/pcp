/*
 * Dynamic namespace metrics for the GFS2 PMDA (based on Linux PMDA)
 *
 * Copyright (c) 2013 Red Hat.
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
#ifndef DYNAMIC_H
#define DYNAMIC_H

/* function to refresh a specific subtree */
typedef int  (*pmnsUpdate)(pmdaExt *, __pmnsTree **);
typedef int  (*textUpdate)(pmdaExt *, pmID, int, char **);
typedef void (*mtabUpdate)(pmdaMetric *, pmdaMetric *, int);
typedef void (*mtabCounts)(int *, int *);

extern void gfs2_dynamic_pmns(const char *, int *, int,
			       pmnsUpdate, textUpdate, mtabUpdate, mtabCounts);
extern __pmnsTree *gfs2_dynamic_lookup_name(pmdaExt *, const char *);
extern __pmnsTree *gfs2_dynamic_lookup_pmid(pmdaExt *, pmID);
extern int gfs2_dynamic_lookup_text(pmID, int, char **, pmdaExt *);
extern void gfs2_dynamic_metrictable(pmdaExt *);

#endif /*DYNAMIC_H*/
