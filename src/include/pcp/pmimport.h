/*
 * NOTE
 *	This is the OLD pmimport API which has been retired.
 *	This header and the associated pmiport driver and plugins
 *	are not used in the PCP build and are not part of any
 *	current PCP package or installation.
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _PMIMPORT_H
#define _PMIMPORT_H

#include <pcp/pmapi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMIMPORT_API_VERSION 1

typedef enum { RS_Error, RS_Ok, RS_Reset } ResultStatus;
typedef enum { IS_Acknowledged, IS_Set } IndomState;

typedef struct {
    pmInDom    id;
    IndomState state;
    int        numinst;
    int        *instlist;
    char       **namelist;
} IndomEntry;

/* Plugin must supply the following functions */
extern int primeImportFile(const char *, int *, char **, char **);
extern ResultStatus getPmResult(const int, pmResult **);
extern int getPmDesc(const pmID, pmDesc **, char **);
extern int getIndom(const pmInDom, IndomEntry **);

#ifdef __cplusplus
}
#endif

#endif /* _PMIMPORT_H */
