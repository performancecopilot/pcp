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

void SamplingCurve::drawSeries(QPainter *p, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		const QRectF &canvasRect, int from, int to) const
{
    int okFrom, okTo = from;
    int size = (to > 0) ? to : dataSize();

    while (okTo < size) {
	okFrom = okTo;
	while (isNaN(sample(okFrom).y()) && okFrom < size)
	    ++okFrom;
	okTo = okFrom;
	while (!isNaN(sample(okTo).y()) && okTo < size)
	    ++okTo;
	if (okFrom < size)
	    QwtPlotCurve::drawSeries(p, xMap, yMap, canvasRect, okFrom, okTo-1);
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

SamplingItem::SamplingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp,
	const char *legend, Chart::Style style, int samples, int index)
	: ChartItem(mp, msp, dp, legend)
{
    pmDesc desc = mp->desc().desc();

    // initialize the pcp data and item data arrays
    my.dataCount = 0;
    my.data = NULL;
    my.itemData = NULL;
    resetValues(samples);

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

QwtPlotItem *SamplingItem::item(void)
{
    return my.curve;
}

QwtPlotCurve *SamplingItem::curve(void)
{
    return my.curve;
}

void SamplingItem::resetValues(int values)
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

void SamplingItem::preserveLiveData(int index, int oldindex)
{
    if (my.dataCount > oldindex)
	my.itemData[index] = my.data[index] = my.data[oldindex];
    else
	my.itemData[index] = my.data[index] = SamplingCurve::NaN();
}

void SamplingItem::punchoutLiveData(int i)
{
    my.data[i] = my.itemData[i] = SamplingCurve::NaN();
}

void SamplingItem::updateValues(bool forward,
		bool rateConvert, pmUnits *units, int sampleHistory,
		double, double, double)
{
    pmAtomValue	scaled, raw;
    QmcMetric	*metric = ChartItem::my.metric;
    double	value;
    int		sz;

    if (metric->error(0)) {
	value = SamplingCurve::NaN();
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

void SamplingItem::rescaleValues(pmUnits *new_units)
{
    pmUnits	*old_units = &ChartItem::my.units;
    pmAtomValue	old_av, new_av;

    console->post("Chart::update change units from %s to %s",
			pmUnitsStr(old_units), pmUnitsStr(new_units));

    for (int i = my.dataCount - 1; i >= 0; i--) {
	if (my.data[i] != SamplingCurve::NaN()) {
	    old_av.d = my.data[i];
	    pmConvScale(PM_TYPE_DOUBLE, &old_av, old_units, &new_av, new_units);
	    my.data[i] = new_av.d;
	}
	if (my.itemData[i] != SamplingCurve::NaN()) {
	    old_av.d = my.itemData[i];
	    pmConvScale(PM_TYPE_DOUBLE, &old_av, old_units, &new_av, new_units);
	    my.itemData[i] = new_av.d;
	}
    }
}

void SamplingItem::replot(int history, double *timeData)
{
    int count = qMin(history, my.dataCount);
    my.curve->setRawSamples(timeData, my.itemData, count);
}

void SamplingItem::revive(Chart *parent) // TODO: inheritance, move to ChartItem
{
    if (removed()) {
	setRemoved(false);
	my.curve->attach(parent);
    }
}

void SamplingItem::remove() // TODO: inheritance, move to ChartItem?
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

void SamplingItem::setStroke(Chart::Style style, QColor color, bool antiAlias)
{
    int  sem = metric()->desc().desc().sem;
    bool step = (sem == PM_SEM_INSTANT || sem == PM_SEM_DISCRETE);
    QPen p(color);

    p.setWidth(8);
    my.curve->setLegendPen(p);
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

void SamplingItem::clearCursor()
{
    // nothing to do here.
}

bool SamplingItem::containsPoint(const QRectF &, int)
{
    return false;
}

void SamplingItem::updateCursor(const QPointF &p, int)
{
    QString string;

    string.sprintf("[%.2f %s at %s]",
		(float)p.y(),
		pmUnitsStr(&ChartItem::my.units),
		timeHiResString(p.x()));
    pmchart->setValueText(string);
}

void SamplingItem::showCursor()
{
    // nothing to do here.
}

const QString &SamplingItem::cursorInfo()
{
    return ChartItem::my.name;
}

void SamplingItem::copyRawDataPoint(int index)
{
    if (index < 0)
	index = my.dataCount - 1;
    my.itemData[index] = my.data[index];
}

int SamplingItem::maximumDataCount(int maximum)
{
    return qMax(maximum, my.dataCount);
}

void SamplingItem::truncateData(int offset)
{
    for (int index = my.dataCount + 1; index < offset; index++) {
	my.data[index] = 0;
	// don't re-set dataCount ... so we don't plot these values,
	// we just want them to count 0 towards any Stack aggregation
    }
}

double SamplingItem::sumData(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (index < my.dataCount && !SamplingCurve::isNaN(my.data[index]))
	sum += my.data[index];
    return sum;
}

void SamplingItem::copyRawDataArray(void)
{
    for (int index = 0; index < my.dataCount; index++)
	my.itemData[index] = my.data[index];
}

void SamplingItem::copyDataPoint(int index)
{
    if (hidden() || index >= my.dataCount)
	my.itemData[index] = SamplingCurve::NaN();
    else
	my.itemData[index] = my.data[index];
}

void SamplingItem::setPlotUtil(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (hidden() || sum == 0.0 ||
	index >= my.dataCount || SamplingCurve::isNaN(my.data[index]))
	my.itemData[index] = SamplingCurve::NaN();
    else
	my.itemData[index] = 100.0 * my.data[index] / sum;
}

double SamplingItem::setPlotStack(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (!hidden() && !SamplingCurve::isNaN(my.itemData[index])) {
	sum += my.itemData[index];
	my.itemData[index] = sum;
    }
    return sum;
}

double SamplingItem::setDataStack(int index, double sum)
{
    if (index < 0)
	index = my.dataCount - 1;
    if (hidden() || SamplingCurve::isNaN(my.data[index])) {
	my.itemData[index] = SamplingCurve::NaN();
    } else {
	sum += my.data[index];
	my.itemData[index] = sum;
    }
    return sum;
}

SamplingScaleEngine::SamplingScaleEngine() : QwtLinearScaleEngine()
{
    my.autoScale = true;
    my.minimum = 0.0;
    my.maximum = 1.0;
}

void SamplingScaleEngine::setScale(bool autoScale,
			double minValue, double maxValue)
{
    my.autoScale = autoScale;
    my.minimum = minValue;
    my.maximum = maxValue;
}

void SamplingScaleEngine::autoScale(int maxSteps, double &minValue,
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
