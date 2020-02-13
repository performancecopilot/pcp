/*
 * Copyright (c) 2017-2018 Red Hat.
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
#ifndef SERIES_BATONS_H
#define SERIES_BATONS_H

typedef enum {
    MAGIC_SLOTS = 1,
    MAGIC_MAPPING,
    MAGIC_CONTEXT,
    MAGIC_LOAD,
    MAGIC_STREAM,
    MAGIC_QUERY,
    MAGIC_SID,
    MAGIC_NAMES,
    MAGIC_LABELMAP,

    MAGIC_COUNT
} series_baton_magic;

typedef struct seriesBatonMagic {
    series_baton_magic		magic  : 16;
    unsigned int		unused : 15;
    unsigned int		traced : 1;
    unsigned int		refcount;
} seriesBatonMagic;

extern void initSeriesBatonMagic(void *, series_baton_magic);
extern void seriesBatonCheckMagic(void *, series_baton_magic, const char *);
extern void seriesBatonCheckCount(void *, const char *);
extern void seriesBatonSetTraced(void *, int);
extern void seriesBatonReferences(void *, unsigned int, const char *);
extern void seriesBatonReference(void *, const char *);
extern int seriesBatonDereference(void *, const char *);

/*
 * General asynchronous response helper routines
 */
typedef void (*seriesBatonCallBack)(void *);

typedef struct seriesBatonPhase {
    unsigned int		refcount;
    seriesBatonCallBack		func;
    struct seriesBatonPhase	*next;
} seriesBatonPhase;

extern void seriesBatonPhases(seriesBatonPhase *, unsigned int, void *);
extern void seriesPassBaton(seriesBatonPhase **, void *, const char *);

#endif	/* SERIES_BATONS_H */
