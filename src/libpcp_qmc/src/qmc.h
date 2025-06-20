/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#ifndef QMC_H
#define QMC_H

#include <pcp/pmapi.h>
#include <pcp/libpcp.h>

/*
 * PMAPI_VERSION note
 * 	As of PMAPI_VERSION_4, pmResult was changed to a "HighRes"
 * 	but the libpcp_qmc routines are still using the older
 * 	pmResult_v2 structure (aka qmcResult)
 */
#define qmcFetch pmFetch_v2
#define qmcResult pmResult_v2
#define qmcFreeResult pmFreeResult_v2
#define qmcFreeResultValues __pmFreeResultValues_v2

//
// Classes
//
class QmcContext;
class QmcDesc;
class QmcGroup;
class QmcIndom;
class QmcMetric;
class QmcSource;

#endif // QMC_H
