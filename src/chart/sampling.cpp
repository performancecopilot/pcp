/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include "sampling.h"
#include <limits>

void SamplingCurve::draw(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		 int, int) const
{
    unsigned int okFrom, okTo = 0;

    while (okTo < data().size()) {
	okFrom = okTo;
	while (isNaN(data().y(okFrom)) && okFrom < data().size())
	    ++okFrom;
	okTo = okFrom;
	while (!isNaN(data().y(okTo)) && okTo < data().size())
	    ++okTo;
	if (okFrom < data().size())
	    QwtPlotCurve::draw(p, xMap, yMap, (int)okFrom, (int)okTo-1);
    }
}

double SamplingCurve::NaN()
{
    return std::numeric_limits<double>::quiet_NaN();
}

bool SamplingCurve::isNaN(double v)
{
    return v != v;
}
