/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
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
#include <limits>
#include "sampling.h"
#include "main.h"
#include <qnumeric.h>
#include <qwt_picker_machine.h>

SamplingItem::SamplingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp,
	const QString &legend, Chart::Style style, int samples, int index)
	: ChartItem(mp, msp, dp, legend)
{
    pmDesc desc = mp->desc().desc();

    my.chart = parent;
    my.info = QString::null;

    // initialize the pcp data and item data arrays
    my.dataCount = 0;
    my.data = NULL;
    my.itemData = NULL;
    resetValues(samples, 0.0, 0.0);

    // set base scale, then tweak if value to plot is time / time
    my.scale = 1;
    if (style != Chart::UtilisationStyle &&
	desc.sem == PM_SEM_COUNTER && desc.units.dimTime == 0) {
	if (desc.units.scaleTime == PM_TIME_USEC)
	    my.scale = 0.000001;
	else if (desc.units.scaleTime == PM_TIME_MSEC)
	    my.scale = 0.001;
    }

    // create and attach the plot right here
    my.curve = new SamplingCurve(label());
    my.curve->attach(parent);

    // the 1000 is arbitrary ... just want numbers to be monotonic
    // decreasing as plots are added
    my.curve->setZ(1000 - index);
}

SamplingItem::~SamplingItem(void)
{
    if (my.data != NULL)
	free(my.data);
    if (my.itemData != NULL)
	free(my.itemData);
}

QwtPlotItem *
SamplingItem::item(void)
{
    return my.curve;
}

QwtPlotCurve *
SamplingItem::curve(void)
{
    return my.curve;
}

void
SamplingItem::resetValues(int values, double, double)
{
    size_t size;

    // Reset sizes of pcp data array and the plot data array
    size = values * sizeof(my.data[0]);
    if ((my.data = (double *)realloc(my.data, size)) == NULL)
	nomem();
    size = values * sizeof(my.itemData[0]);
    if ((my.itemData = (double *)realloc(my.itemData, size)) == NULL)
	nomem();
    if (my.dataCount > values)
	my.dataCount = values;
}

void
SamplingItem::preserveSample(int index, int oldindex)
{
    if (my.dataCount > oldindex)
	my.itemData[index] = my.data[index] = my.data[oldindex];
    else
	my.itemData[index] = my.data[index] = qQNaN();
}

void
SamplingItem::punchoutSample(int index)
{
    my.data[index] = my.itemData[index] = qQNaN();
}

void
SamplingItem::updateValues(bool forward,
		bool rateConvert, pmUnits *units, int sampleHistory, int,
		double, double, double)
{
    pmAtomValue	scaled, raw;
    QmcMetric	*metric = ChartItem::my.metric;
    double	value;
    int		sz;

    if (metric->numValues() < 1 || metric->error(0)) {
	value = qQNaN();
    } else {
	// convert raw value to current chart scale
	raw.d = rateConvert ? metric->value(0) : metric->currentValue(0);
	pmConvScale(PM_TYPE_DOUBLE, &raw, &ChartItem::my.units, &scaled, units);
	value = scaled.d * my.scale;
    }

    if (my.dataCount < sampleHistory)
	sz = qMax(0, (int)(my.dataCount * sizeof(double)));
    else
	sz = qMax(0, (int)((my.dataCount - 1) * sizeof(double)));

    if (forward) {
	memmove(&my.data[1], &my.data[0], sz);
	memmove(&my.itemData[1], &my.itemData[0], sz);
	my.data[0] = value;
    } else {
	memmove(&my.data[0], &my.data[1], sz);
	memmove(&my.itemData[0], &my.itemData[1], sz);
	my.data[my.dataCount - 1] = value;
    }

    if (my.dataCount < sampleHistory)
	my.dataCount++;
}

void
SamplingItem::rescaleValues(pmUnits *new_units)
{
    pmUnits	*old_units = &ChartItem::my.units;
    pmAtomValue	old_av, new_av;

    console->post("Chart::update change units from %s to %s",
			pmUnitsStr(old_units), pmUnitsStr(new_units));

    for (int i = my.dataCount - 1; i >= 0; i--) {
	if (my.data[i] != qQNaN()) {
	    old_av.d = my.data[i];
	    pmConvScale(PM_TYPE_DOUBLE, &old_av, old_units, &new_av, new_units);
	    my.data[i] = new_av.d;
	}
	if (my.itemData[i] != qQNaN()) {
	    old_av.d = my.itemData[i];
	    pmConvScale(PM_TYPE_DOUBLE, &old_av, old_units, &new_av, new_units);
	    my.itemData[i] = new_av.d;
	}
    }
}

void
SamplingItem::replot(int history, double *timeData)
{
    int count = qMin(history, my.dataCount);
    my.curve->setRawSamples(timeData, my.itemData, count);
}

void
SamplingItem::revive(void)
{
    if (removed()) {
	setRemoved(false);
	my.curve->attach(my.chart);
    }
}

void
SamplingItem::remove(void)
{
    setRemoved(true);
    my.curve->detach();

    // We can't really do this properly (free memory, etc) - working around
    // metrics class limit (its using an ordinal index for metrics, remove any
    // and we'll get problems.  Which means the plots array must also remain
    // unchanged, as we drive things via the metriclist at times.  D'oh.
    // This blows - it means we have to continue to fetch metrics for those
    // metrics that have been removed from the chart, which may be remote
    // hosts, hosts which are down (introducing retry issues...).  Bother.

    //delete my.curve;
    //free(my.legend);
}

void
SamplingItem::setStroke(Chart::Style style, QColor color, bool antiAlias)
{
    int  sem = metricPtr()->desc().desc().sem;
    bool step = (sem == PM_SEM_INSTANT || sem == PM_SEM_DISCRETE);

    my.curve->setLegendColor(color);
    my.curve->setRenderHint(QwtPlotItem::RenderAntialiased, antiAlias);

    switch (style) {
	case Chart::BarStyle:
	    my.curve->setPen(color);
	    my.curve->setBrush(QBrush(color, Qt::SolidPattern));
	    my.curve->setStyle(QwtPlotCurve::Sticks);
	    break;

	case Chart::AreaStyle:
	    my.curve->setPen(color);
	    my.curve->setBrush(QBrush(color, Qt::SolidPattern));
	    my.curve->setStyle(step? QwtPlotCurve::Steps : QwtPlotCurve::Lines);
	    break;

	case Chart::UtilisationStyle:
	    my.curve->setPen(QColor(Qt::black));
	    my.curve->setStyle(QwtPlotCurve::Steps);
	    my.curve->setBrush(QBrush(color, Qt::SolidPattern));
	    break;

	case Chart::LineStyle:
	    my.curve->setPen(color);
	    my.curve->setBrush(QBrush(Qt::NoBrush));
	    my.curve->setStyle(step? QwtPlotCurve::Steps : QwtPlotCurve::Lines);
	    break;

	case Chart::StackStyle:
	    my.curve->setPen(QColor(Qt::black));
	    my.curve->setBrush(QBrush(color, Qt::SolidPattern));
	    my.curve->setStyle(QwtPlotCurve::Steps);
	    break;

	default:
	    break;
    }
}

void
SamplingItem::clearCursor()
{
    // nothing to do here.
}

bool
SamplingItem::containsPoint(const QRectF &, int)
{
    return false;
}

void
SamplingItem::updateCursor(const QPointF &p, int)
{
    QString title = my.chart->YAxisTitle();

    my.info.sprintf("[%.2f", (float)p.y());
    if (title != QString::null) {
	my.info.append(" ");
	my.info.append(title);
    }
    my.info.append(" at ");
    my.info.append(timeHiResString(p.x()));
    my.info.append("]");

    pmchart->setValueText(my.info);
}

const QString &
SamplingItem::cursorInfo()
{
    return my.info;
}

void
SamplingItem::copyRawDataPoint(int index)
{
    if (index < 0)
	index = my.dataCount - 1;
    my.itemData[index] = my.data[index];
}

int
SamplingItem::maximumDataCount(int maximum)
{
    return qMax(maximum, my.dataCount);
}

void
SamplingItem::truncateData(int offset)
{
    for (int index = my.dataCount + 1; index < offset; index++) {
	my.data[index] = 0;
	// don't re-set dataCount ... so we don't plot these values,
	// we just want them to count 0 towards any Stack aggregation
    }
}

double
SamplingItem::sumData(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (index < my.dataCount && !qIsNaN(my.data[index]))
	sum += my.data[index];
    return sum;
}

void
SamplingItem::copyRawDataArray(void)
{
    for (int index = 0; index < my.dataCount; index++)
	my.itemData[index] = my.data[index];
}

void
SamplingItem::copyDataPoint(int index)
{
    if (hidden() || index >= my.dataCount)
	my.itemData[index] = qQNaN();
    else
	my.itemData[index] = my.data[index];
}

void
SamplingItem::setPlotUtil(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (hidden() || sum == 0.0 ||
	index >= my.dataCount || qIsNaN(my.data[index]))
	my.itemData[index] = qQNaN();
    else
	my.itemData[index] = 100.0 * my.data[index] / sum;
}

double
SamplingItem::setPlotStack(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (!hidden() && !qIsNaN(my.itemData[index])) {
	sum += my.itemData[index];
	my.itemData[index] = sum;
    }
    return sum;
}

double
SamplingItem::setDataStack(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (hidden() || qIsNaN(my.data[index])) {
	my.itemData[index] = qQNaN();
    } else {
	sum += my.data[index];
	my.itemData[index] = sum;
    }
    return sum;
}


//
// SamplingCurve deals with overriding some QwtPlotCurve defaults;
// particularly around dealing with empty sections of chart (NaN),
// and the way the legend is rendered.
//

void
SamplingCurve::drawSeries(QPainter *p,
		const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		const QRectF &canvasRect, int from, int to) const
{
    int okFrom, okTo = from;
    int size = (to > 0) ? to : dataSize();

    while (okTo < size) {
	okFrom = okTo;
	while (qIsNaN(sample(okFrom).y()) && okFrom < size)
	    ++okFrom;
	okTo = okFrom;
	while (!qIsNaN(sample(okTo).y()) && okTo < size)
	    ++okTo;
	if (okFrom < size)
	    QwtPlotCurve::drawSeries(p, xMap, yMap, canvasRect, okFrom, okTo-1);
    }
}


//
// SamplingScaleEngine deals with rendering the vertical Y-Axis
//

SamplingScaleEngine::SamplingScaleEngine() : QwtLinearScaleEngine()
{
    my.autoScale = true;
    my.minimum = 0.0;
    my.maximum = 1.0;
}

void
SamplingScaleEngine::setScale(bool autoScale,
			double minValue, double maxValue)
{
    my.autoScale = autoScale;
    my.minimum = minValue;
    my.maximum = maxValue;
}

void
SamplingScaleEngine::autoScale(int maxSteps, double &minValue,
			double &maxValue, double &stepSize) const
{
    if (my.autoScale) {
       if (minValue > 0)
           minValue = 0.0;
    } else {
       minValue = my.minimum;
       maxValue = my.maximum;
    }
    QwtLinearScaleEngine::autoScale(maxSteps, minValue, maxValue, stepSize);
}


//
// The SamplingEngine implements all sampling-specific Chart behaviour
//
SamplingEngine::SamplingEngine(Chart *chart, pmDesc &desc)
{
    QwtPlotPicker *picker = chart->my.picker;
    ChartEngine *engine = chart->my.engine;

    my.chart = chart;
    my.rateConvert = engine->rateConvert();
    my.antiAliasing = engine->antiAliasing();

    normaliseUnits(desc);
    my.units = desc.units;

    my.scaleEngine = new SamplingScaleEngine();
    chart->setAxisScaleEngine(QwtPlot::yLeft, my.scaleEngine);
    chart->setAxisScaleDraw(QwtPlot::yLeft, new QwtScaleDraw());

    // use an individual point picker for sampled data
    picker->setStateMachine(new QwtPickerDragPointMachine());
    picker->setRubberBand(QwtPicker::CrossRubberBand);
    picker->setRubberBandPen(QColor(Qt::green));
}

SamplingItem *
SamplingEngine::samplingItem(int index)
{
    return (SamplingItem *)my.chart->my.items[index];
}

ChartItem *
SamplingEngine::addItem(QmcMetric *mp, pmMetricSpec *msp, pmDesc *desc, const QString &legend)
{
    int sampleHistory = my.chart->my.tab->group()->sampleHistory();
    int existingItemCount = my.chart->metricCount();
    SamplingItem *item = new SamplingItem(my.chart, mp, msp, desc, legend,
				my.chart->my.style,
				sampleHistory, existingItemCount);

    // Find current max count for all plot items
    int i, size = 0;
    for (i = 0; i < existingItemCount; i++)
	size = samplingItem(i)->maximumDataCount(size);
    // Zero any plot from there to end, so Stack<->Line transitions work
    for (i = 0; i < existingItemCount; i++)
	samplingItem(i)->truncateData(size);

    return item;
}

void
SamplingEngine::normaliseUnits(pmDesc &desc)
{
    if (my.rateConvert && desc.sem == PM_SEM_COUNTER) {
	if (desc.units.dimTime == 0) {
	    desc.units.dimTime = -1;
	    desc.units.scaleTime = PM_TIME_SEC;
	}
	else if (desc.units.dimTime == 1) {
	    desc.units.dimTime = 0;
	    // don't play with scaleTime, need native per item scaleTime
	    // so we can apply correct scaling via item->scale, e.g. in
	    // the msec -> msec/sec after rate conversion ... see the
	    // calculation for item->scale below
	}
    }
}

bool
SamplingEngine::isCompatible(pmDesc &desc)
{
    console->post("SamplingEngine::isCompatible"
		  " type=%d, units=%s", desc.type, pmUnitsStr(&desc.units));

    if (desc.type == PM_TYPE_EVENT || desc.type == PM_TYPE_HIGHRES_EVENT)
	return false;
    normaliseUnits(desc);
    if (my.units.dimSpace != desc.units.dimSpace ||
	my.units.dimTime != desc.units.dimTime ||
	my.units.dimCount != desc.units.dimCount)
	return false;
    return true;
}

void
SamplingEngine::updateValues(bool forward,
	int size, int points, double left, double right, double delta)
{
    int	i, index = forward ? 0 : -1;	/* first or last data point */
    int itemCount = my.chart->metricCount();
    Chart::Style style = my.chart->my.style;

    // Drive new values into each chart item
    for (int i = 0; i < itemCount; i++) {
	samplingItem(i)->updateValues(forward, my.rateConvert, &my.units,
					size, points, left, right, delta);
    }

    if (style == Chart::BarStyle || style == Chart::AreaStyle || style == Chart::LineStyle) {
	for (i = 0; i < itemCount; i++)
	    samplingItem(i)->copyRawDataPoint(index);
    }
    // Utilisation: like Stack, but normalize value to a percentage (0,100)
    else if (style == Chart::UtilisationStyle) {
	double	sum = 0.0;
	// compute sum
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->sumData(index, sum);
	// scale all components
	for (i = 0; i < itemCount; i++)
	    samplingItem(i)->setPlotUtil(index, sum);
	// stack components
	sum = 0.0;
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->setPlotStack(index, sum);
    }
    else if (style == Chart::StackStyle) {
	double	sum = 0.0;
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->setDataStack(index, sum);
    }

#if DESPERATE
    for (i = 0; i < my.chart->metricCount(); i++) {
	console->post(PmChart::DebugForce, "metric[%d] value %f", i,
			samplingItem(i)->metricPtr()->currentValue(0));
    }
#endif
}

void
SamplingEngine::redoScale(void)
{
    bool	rescale = false;

    // The 1,000 and 0.1 thresholds are just a heuristic guess.
    //
    // We're assuming lBound() plays no part in this, which is OK as
    // the upper bound of the y-axis range (hBound()) drives the choice
    // of appropriate units scaling.
    //
    if (my.scaleEngine->autoScale() &&
	my.chart->axisScaleDiv(QwtPlot::yLeft)->upperBound() > 1000) {
	double scaled_max = my.chart->axisScaleDiv(QwtPlot::yLeft)->upperBound();
	if (my.units.dimSpace == 1) {
	    switch (my.units.scaleSpace) {
		case PM_SPACE_BYTE:
		    my.units.scaleSpace = PM_SPACE_KBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_KBYTE:
		    my.units.scaleSpace = PM_SPACE_MBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_MBYTE:
		    my.units.scaleSpace = PM_SPACE_GBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_GBYTE:
		    my.units.scaleSpace = PM_SPACE_TBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_TBYTE:
		    my.units.scaleSpace = PM_SPACE_PBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_PBYTE:
		    my.units.scaleSpace = PM_SPACE_EBYTE;
		    rescale = true;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_SPACE_* values being consecutive
		// integer values as the scale increases
		scaled_max /= 1024;
		while (scaled_max > 1000) {
		    my.units.scaleSpace++;
		    scaled_max /= 1024;
		    if (my.units.scaleSpace == PM_SPACE_EBYTE) break;
		}
	    }
	}
	else if (my.units.dimTime == 1) {
	    switch (my.units.scaleTime) {
		case PM_TIME_NSEC:
		    my.units.scaleTime = PM_TIME_USEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_USEC:
		    my.units.scaleTime = PM_TIME_MSEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_MSEC:
		    my.units.scaleTime = PM_TIME_SEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_SEC:
		    my.units.scaleTime = PM_TIME_MIN;
		    rescale = true;
		    scaled_max /= 60;
		    break;
		case PM_TIME_MIN:
		    my.units.scaleTime = PM_TIME_HOUR;
		    rescale = true;
		    scaled_max /= 60;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_TIME* values being consecutive
		// integer values as the scale increases
		while (scaled_max > 1000) {
		    my.units.scaleTime++;
		    if (my.units.scaleTime <= PM_TIME_SEC)
			scaled_max /= 1000;
		    else
			scaled_max /= 60;
		    if (my.units.scaleTime == PM_TIME_HOUR) break;
		}
	    }
	}
    }

    if (rescale == false &&
	my.scaleEngine->autoScale() &&
	my.chart->axisScaleDiv(QwtPlot::yLeft)->upperBound() < 0.1) {
	double scaled_max = my.chart->axisScaleDiv(QwtPlot::yLeft)->upperBound();
	if (my.units.dimSpace == 1) {
	    switch (my.units.scaleSpace) {
		case PM_SPACE_KBYTE:
		    my.units.scaleSpace = PM_SPACE_BYTE;
		    rescale = true;
		    break;
		case PM_SPACE_MBYTE:
		    my.units.scaleSpace = PM_SPACE_KBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_GBYTE:
		    my.units.scaleSpace = PM_SPACE_MBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_TBYTE:
		    my.units.scaleSpace = PM_SPACE_GBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_PBYTE:
		    my.units.scaleSpace = PM_SPACE_TBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_EBYTE:
		    my.units.scaleSpace = PM_SPACE_PBYTE;
		    rescale = true;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_SPACE_* values being consecutive
		// integer values (in reverse) as the scale decreases
		scaled_max *= 1024;
		while (scaled_max < 0.1) {
		    my.units.scaleSpace--;
		    scaled_max *= 1024;
		    if (my.units.scaleSpace == PM_SPACE_BYTE) break;
		}
	    }
	}
	else if (my.units.dimTime == 1) {
	    switch (my.units.scaleTime) {
		case PM_TIME_USEC:
		    my.units.scaleTime = PM_TIME_NSEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_MSEC:
		    my.units.scaleTime = PM_TIME_USEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_SEC:
		    my.units.scaleTime = PM_TIME_MSEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_MIN:
		    my.units.scaleTime = PM_TIME_SEC;
		    rescale = true;
		    scaled_max *= 60;
		    break;
		case PM_TIME_HOUR:
		    my.units.scaleTime = PM_TIME_MIN;
		    rescale = true;
		    scaled_max *= 60;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_TIME* values being consecutive
		// integer values (in reverse) as the scale decreases
		while (scaled_max < 0.1) {
		    my.units.scaleTime--;
		    if (my.units.scaleTime < PM_TIME_SEC)
			scaled_max *= 1000;
		    else
			scaled_max *= 60;
		    if (my.units.scaleTime == PM_TIME_NSEC) break;
		}
	    }
	}
    }

    if (rescale) {
	//
	// need to rescale ... we transform all of the historical (raw)
	// data, new data will be taken care of by changing my.units.
	//
	for (int i = 0; i < my.chart->metricCount(); i++)
	    samplingItem(i)->rescaleValues(&my.units);

	if (my.chart->my.style == Chart::UtilisationStyle)
	    my.chart->setYAxisTitle("% utilization");
	else
	    my.chart->setYAxisTitle(pmUnitsStr(&my.units));
	my.chart->replot();
    }
}

void
SamplingEngine::replot(void)
{
    GroupControl *group = my.chart->my.tab->group();
    int		vh = group->visibleHistory();
    double	*vp = group->timeAxisData();
    int		itemCount = my.chart->metricCount();
    int		maxCount = 0;
    int		i, m;
    double	sum;

#if DESPERATE
    console->post(PmChart::DebugForce, "SamplingEngine::replot %d items)", itemCount);
#endif

    for (i = 0; i < itemCount; i++)
	samplingItem(i)->replot(vh, vp);

    switch (my.chart->style()) {
	case Chart::BarStyle:
	case Chart::AreaStyle:
	case Chart::LineStyle:
	    for (i = 0; i < itemCount; i++)
		samplingItem(i)->copyRawDataArray();
	    break;

	case Chart::UtilisationStyle:
	    for (i = 0; i < itemCount; i++)
		maxCount = samplingItem(i)->maximumDataCount(maxCount);
	    for (m = 0; m < maxCount; m++) {
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->sumData(m, sum);
		for (i = 0; i < itemCount; i++)
		    samplingItem(i)->setPlotUtil(m, sum);
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->setPlotStack(m, sum);
	    }
	    break;

	case Chart::StackStyle:
	    for (i = 0; i < itemCount; i++)
		maxCount = samplingItem(i)->maximumDataCount(maxCount);
	    for (m = 0; m < maxCount; m++) {
		for (i = 0; i < itemCount; i++)
		    samplingItem(i)->copyDataPoint(m);
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->setPlotStack(m, sum);
	    }
	    break;

	default:
	    break;
    }
}

void
SamplingEngine::scale(bool *autoScale, double *yMin, double *yMax)
{
    *autoScale = my.scaleEngine->autoScale();
    *yMin = my.scaleEngine->minimum();
    *yMax = my.scaleEngine->maximum();
}

void
SamplingEngine::setScale(bool autoScale, double yMin, double yMax)
{
    my.scaleEngine->setScale(autoScale, yMin, yMax);

    if (autoScale)
	my.chart->setAxisAutoScale(QwtPlot::yLeft);
    else
	my.chart->setAxisScale(QwtPlot::yLeft, yMin, yMax);
}

void
SamplingEngine::selected(const QPolygon &)
{
    // Nothing to do here.
}

void
SamplingEngine::moved(const QPointF &p)
{
    my.chart->showPoint(p);
}

void
SamplingEngine::setStyle(Chart::Style style)
{
    // Y-Axis title choice is difficult.  A Utilisation plot by definition
    // is dimensionless and scaled to a percentage, so a label of just
    // "% utilization" makes sense ... there has been some argument in
    // support of "% time utilization" as a special case when the metrics
    // involve some aspect of time, but the base metrics in the common case
    // are counters in units of time (e.g. the CPU view), which after rate
    // conversion is indistinguishable from instantaneous or discrete
    // metrics of dimension time^0 which are units compatible ... so we're
    // opting for the simplest possible interpretation of utilization or
    // everything else.
    //
    switch (style) {
	case Chart::BarStyle:
	case Chart::AreaStyle:
	case Chart::LineStyle:
	case Chart::StackStyle:
	    if (my.chart->style() == Chart::UtilisationStyle)
		my.scaleEngine->setAutoScale(true);
	    my.chart->setYAxisTitle(pmUnitsStr(&my.units));
	    break;
	case Chart::UtilisationStyle:
	    my.scaleEngine->setScale(false, 0.0, 100.0);
	    my.chart->setYAxisTitle("% utilization");
	    break;
	default:
	    break;
    }
}
