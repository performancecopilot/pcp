/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
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
#ifndef TRACING_H
#define TRACING_H

#include "chart.h"
#include <qwt_scale_engine.h>
//#include <qwt_interval_symbol.h>

//
// Place-holder class for future work on vertical scale
// characteristics of event traces.  This will need to
// work in with the vertical arrangement of trace bars
// such that identifiers make sense.  I think.
//
class TracingScaleEngine : public QwtLinearScaleEngine
{
public:
    TracingScaleEngine();

    void setScale(bool autoScale, double minValue, double maxValue);
    virtual void autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const;
};

#endif	// TRACING_H
