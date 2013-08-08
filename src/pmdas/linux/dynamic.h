/*
 * Dynamic namespace metrics, PMDA helper routines.
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

/* function to refresh a specific subtree */
typedef int  (*pmdaUpdatePMNS)(pmdaExt *, __pmnsTree **);
typedef int  (*pmdaUpdateText)(pmdaExt *, pmID, int, char **);
typedef void (*pmdaUpdateMetric)(pmdaMetric *, pmdaMetric *, int);
typedef void (*pmdaCountMetrics)(int *, int *);

extern void pmdaDynamicPMNS(const char *, int *, int,
			    pmdaUpdatePMNS, pmdaUpdateText,
			    pmdaUpdateMetric, pmdaCountMetrics,
			    pmdaMetric *, int);

extern pmdaNameSpace *pmdaDynamicLookupName(pmdaExt *, const char *);
extern pmdaNameSpace *pmdaDynamicLookupPMID(pmdaExt *, pmID);
extern int pmdaDynamicLookupText(pmID, int, char **, pmdaExt *);
extern void pmdaDynamicMetricTable(pmdaExt *);
