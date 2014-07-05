/* -*- C++ -*- */

#ifndef _INV_METRICLIST_H_
#define _INV_METRICLIST_H_

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


#include <sys/types.h>
#include <Inventor/SbColor.h>
#include "List.h"
#include "Metric.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

typedef OMC_List<OMC_Metric *> INV_MetricsList;
typedef OMC_List<SbColor *> INV_ColorList;

class INV_MetricList
{
public:

    enum AlignColor { noColors, perMetric, perValue };

private:

    INV_MetricsList	_metrics;
    INV_ColorList	_colors;
    uint_t		_values;

public:

    ~INV_MetricList();

    INV_MetricList();

    uint_t numMetrics() const
	{ return _metrics.length(); }
    uint_t numValues() const
	{ return _values; }
    int numColors() const
        { return _colors.length(); }

    const OMC_Metric &metric(uint_t i) const
	{ return *(_metrics[i]); }
    OMC_Metric &metric(uint_t i)
	{ return *(_metrics[i]); }

    const SbColor &color(uint_t i) const
	{ return *(_colors[i]); }
    SbColor &color(uint_t i)
	{ return *(_colors[i]); }    
    void color(uint_t i, OMC_String &str) const;

    int add(char const* metric, double scale);
    int add(char const* metric, double scale, int history);
    int add(pmMetricSpec *metric, double scale);
    int add(pmMetricSpec *metric, double scale, int history);
    int traverse(const char *metric);

    void add(SbColor const& color);
    void add(uint32_t packedcol);

    void resolveColors(AlignColor align = perMetric);

    static void toString(const SbColor &color, OMC_String &str);

    friend ostream& operator<<(ostream&, INV_MetricList const &);

private:

    INV_MetricList(const INV_MetricList &);
    const INV_MetricList &operator=(const INV_MetricList &);
    // Never defined
};

#endif /* _INV_METRICLIST_H_ */

