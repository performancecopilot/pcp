/***********************************************************************
 * pragmatics.h - inference engine pragmatics analysis
 *
 * The analysis of how to organize the fetching of metrics (pragmatics)
 * and other parts of the inference engine that are particularly
 * sensitive to details of the performance metrics API are kept here.
 ***********************************************************************
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Red Hat
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
#ifndef PRAG_H
#define PRAG_H

#include "pmapi.h"

/* report where PCP data is coming from */
char *findsource(const char *, const char *);

/* initialize performance metrics API */
void pmcsInit(void);

/* juggle contexts */
int newContext(Symbol *, const char *, int);

/* initialize access to archive */
int initArchive(Archive *);

/* initialize timezone */
void zoneInit(void);

/* convert to canonical units */
pmUnits canon(pmUnits);

/* scale factor to canonical pmUnits */
double scale(pmUnits);

/* initialize Metric */
int initMetric(Metric *);

/* reinitialize Metric */
int reinitMetric(Metric *);

/* put initialiaed Metric onto fetch list */
void bundleMetric(Host *, Metric *);

/* reconnect attempt to host */
int reconnect(Host *);

/* pragmatics analysis */
void pragmatics(Symbol, RealTime);

/* execute fetches for given Task */
void taskFetch(Task *);

/* convert Expr value to pmValueSet value */
void fillVSet(Expr *, pmValueSet *);

/* send pmDescriptors for all expressions in given task */
void sendDescs(Task *);

/* put Metric onto wait list */
void waitMetric(Metric *);

/* remove Metric from wait list */
void unwaitMetric(Metric *);

/* check that pmUnits dimensions are equal */
#define dimeq(x, y)	(((x).dimSpace == (y).dimSpace) && \
			 ((x).dimTime == (y).dimTime) && \
			 ((x).dimCount == (y).dimCount))

/* check equality of two pmUnits */
#define unieq(x, y)	(((x).dimSpace == (y).dimSpace) && \
			 ((x).dimTime == (y).dimTime) && \
			 ((x).dimCount == (y).dimCount) && \
			 ((x).scaleSpace == (y).scaleSpace) && \
			 ((x).scaleTime == (y).scaleTime) && \
			 ((x).scaleCount == (y).scaleCount))

/* for initialization of pmUnits struct */
extern pmUnits	noUnits;
extern pmUnits	countUnits;

/* flag processes spawned */
extern int	need_wait;

#endif /* PRAG_H */


