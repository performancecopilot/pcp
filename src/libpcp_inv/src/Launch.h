/* -*- C++ -*- */

#ifndef _INV_LAUNCH_H_
#define _INV_LAUNCH_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "String.h"
#include "Bool.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SbColor;
class OMC_Metric;
class OMC_Desc;
class INV_ColorScale;

class INV_Launch
{
private:

    static const OMC_String	theVersion1Str;
    static const OMC_String	theVersion2Str;
    
    OMC_StrList		_strings;
    int			_groupMetric;
    int			_groupCount;
    OMC_String		_groupHint;
    int			_metricCount;
    int			_version;

public:

    ~INV_Launch();

    INV_Launch(const OMC_String &version = "");	// defaults to version 2.0
    INV_Launch(const INV_Launch &rhs);
    const INV_Launch &operator=(const INV_Launch &rhs);

    static const char *launchPath();

    void setDefaultOptions(int interval = 5,
			   int debug = 0,
			   const char *pmnsfile = NULL,
			   const char *timeport = NULL,
			   const char *starttime = NULL,
			   const char *endtime = NULL,
			   const char *offset = NULL,
			   const char *timezone = NULL,
			   const char *defsourcetype = NULL,
			   const char *defsourcename = NULL,
			   OMC_Bool selected = OMC_false);

    void addOption(const char *name, const char *value);
    void addOption(const char *name, int value);

    void addMetric(const OMC_Metric &metric, 
		   const SbColor &color,
		   int instance,
		   OMC_Bool useSocks = OMC_false);

    void addMetric(const OMC_Metric &metric, 
		   const INV_ColorScale &scale,
		   int instance,
		   OMC_Bool useSocks = OMC_false);

    void addMetric(int context,
		   const OMC_String &source,
		   const OMC_String &host,
		   const OMC_String &metric,
		   const char *instance,
		   const OMC_Desc &desc,
		   const SbColor &color,
		   double scale,
		   OMC_Bool useSocks = OMC_false);
    // Add metric with static color and scale

    void addMetric(int context,
		   const OMC_String &source,
		   const OMC_String &host,
		   const OMC_String &metric,
		   const char *instance,
		   const OMC_Desc &desc,
		   const INV_ColorScale &scale,
		   OMC_Bool useSocks = OMC_false);
    // Add metric with dynamic color range

    void startGroup(const char *hint = "none");
    void endGroup();

    void append(INV_Launch const& rhs);

    void output(int fd) const;

friend ostream& operator<<(ostream& os, INV_Launch const& rhs);

private:

    void preColor(int context, 
    		  const OMC_String &source, 
		  const OMC_String &host,
		  const OMC_String &metric,
		  OMC_Bool useSocks,
		  OMC_String &str);
    void postColor(const OMC_Desc &desc, const char *instance, OMC_String &str);
};

#endif /* _INV_LAUNCH_H_ */
