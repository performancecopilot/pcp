/* -*- C++ -*- */

#ifndef _PMC_H_
#define _PMC_H_

/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: PMC.h,v 1.1 2007/09/11 01:38:10 kimbrr Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <sys/types.h>
#include <assert.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

//
// Classes
//
class PMC_Context;
class PMC_Desc;
class PMC_Group;
class PMC_Indom;
class PMC_Metric;
class PMC_Source;

#endif /* _PMC_H_ */
