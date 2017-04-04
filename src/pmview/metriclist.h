/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#ifndef _METRICLIST_H_
#define _METRICLIST_H_

#include <sys/types.h>
#include <Inventor/SbColor.h>
#include "qmc_metric.h"
#include "qmc_source.h"

typedef QList<QmcMetric *> MetricsList;
typedef QList<SbColor *> SbColorList;

class MetricList
{
public:

    enum AlignColor { noColors, perMetric, perValue };

private:

    MetricsList		_metrics;
    SbColorList		_colors;
    int			_values;

public:

    ~MetricList();

    MetricList();

    int numMetrics() const
	{ return _metrics.size(); }
    int numValues() const
	{ return _values; }
    int numColors() const
        { return _colors.size(); }

    const QmcMetric &metric(int i) const
	{ return *(_metrics[i]); }
    QmcMetric &metric(int i)
	{ return *(_metrics[i]); }

    const SbColor &color(int i) const
	{ return *(_colors[i]); }
    SbColor &color(int i)
	{ return *(_colors[i]); }    
    void color(int i, QString &str) const;

    int add(char const* metric, double scale);
    int add(char const* metric, double scale, int history);
    int add(pmMetricSpec *metric, double scale);
    int add(pmMetricSpec *metric, double scale, int history);
    int traverse(const char *metric);

    void add(SbColor const& color);
    void add(int packedcol);

    void resolveColors(AlignColor align = perMetric);

    static void toString(const SbColor &color, QString &str);

    friend QTextStream& operator<<(QTextStream&, MetricList const &);

private:

    MetricList(const MetricList &);
    const MetricList &operator=(const MetricList &);
    // Never defined
};

#endif /* _METRICLIST_H_ */
