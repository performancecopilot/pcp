/*
 * Copyright (c) 2012-2015, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
 * Copyright (c) 2006-2010, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include "main.h"
#include "tracing.h"
#include "sampling.h"
#include "saveviewdialog.h"

#include <QtCore/QPoint>
#include <QtCore/QRegExp>
#include <QtGui/QApplication>
#include <QtGui/QWhatsThis>
#include <QtGui/QCursor>
#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qwt_plot_renderer.h>
#include <qwt_legend_item.h>
#include <qwt_scale_widget.h>

#define DESPERATE 0

Chart::Chart(Tab *chartTab, QWidget *parent) : QwtPlot(parent), Gadget(this)
{
    my.tab = chartTab;
    my.title = QString::null;
    my.style = NoStyle;
    my.scheme = QString::null;
    my.sequence = 0;

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    plotLayout()->setCanvasMargin(0);
    plotLayout()->setAlignCanvasToScales(true);
    plotLayout()->setFixedAxisOffset(54, QwtPlot::yLeft);
    enableAxis(xBottom, false);

    // prepare initial engine dealing with (no) chart type
    my.engine = new ChartEngine(this);

    // setup the legend (all charts must have one)
    setLegendVisible(true);
    QwtPlot::legend()->contentsWidget()->setFont(*globalFont);
    connect(this, SIGNAL(legendChecked(QwtPlotItem *, bool)),
		    SLOT(legendChecked(QwtPlotItem *, bool)));

    // setup a picker (all charts must have one)
    my.picker = new ChartPicker(canvas());
    connect(my.picker, SIGNAL(activated(bool)),
			 SLOT(activated(bool)));
    connect(my.picker, SIGNAL(selected(const QPolygon &)),
			 SLOT(selected(const QPolygon &)));
    connect(my.picker, SIGNAL(selected(const QPointF &)),
			 SLOT(selected(const QPointF &)));
    connect(my.picker, SIGNAL(moved(const QPointF &)),
			 SLOT(moved(const QPointF &)));

    // feedback into the group about any selection
    connect(this, SIGNAL(timeSelectionActive(Gadget *, int)),
	    my.tab->group(), SLOT(timeSelectionActive(Gadget *, int)));
    connect(this, SIGNAL(timeSelectionReactive(Gadget *, int)),
	    my.tab->group(), SLOT(timeSelectionReactive(Gadget *, int)));
    connect(this, SIGNAL(timeSelectionInactive(Gadget *)),
	    my.tab->group(), SLOT(timeSelectionInactive(Gadget *)));

    console->post("Chart::ctor complete(%p)", this);
}

Chart::~Chart()
{
    console->post("Chart::~Chart() for chart %p", this);

    for (int i = 0; i < my.items.size(); i++)
	delete my.items[i];
    delete my.engine;
    delete my.picker;
}

ChartEngine::ChartEngine(Chart *chart)
{
    my.rateConvert = true;
    my.antiAliasing = true;
    chart->setAutoReplot(false);
    chart->setCanvasBackground(globalSettings.chartBackground);
    chart->setAxisFont(QwtPlot::yLeft, *globalFont);
    chart->setAxisAutoScale(QwtPlot::yLeft);
}

ChartItem::ChartItem(QmcMetric *mp,
		pmMetricSpec *msp, pmDesc *dp, const QString &legend)
{
    my.metric = mp;
    my.units = dp->units;
    my.name = shortHostName().append(':').append(QString(msp->metric));
    if (msp->ninst == 1) {
	my.name.append("[").append(msp->inst[0]).append("]");
	my.inst = QString(msp->inst[0]);
    } else {
	my.inst = QString::null;
    }
    setLegend(legend);
    my.removed = false;
    my.hidden = false;
}

//
// Build the legend label string, even if the chart is declared
// "legend off" so that subsequent Edit->Chart Title and Legend
// changes can turn the legend on and off dynamically.
//
// NB: "legend" is not expanded (wrt %h, %i, etc), "label" is.
//
void
ChartItem::setLegend(const QString &legend)
{
    my.legend = legend;
    if (legend != QString::null)
	expandLegendLabel(legend);
    else
	clearLegendLabel();
}

void
ChartItem::updateLegend()
{
    // drive any change to label into QwtLegendItem
    item()->setTitle(my.label);
}

void
ChartItem::expandLegendLabel(const QString &legend)
{
    bool expandMetricShort = legend.contains("%m");
    bool expandMetricLong = legend.contains("%M");
    bool expandInstShort = legend.contains("%i");
    bool expandInstLong = legend.contains("%I");
    bool expandHostShort = legend.contains("%h");
    bool expandHostLong = legend.contains("%H");

    my.label = legend;
    if (expandMetricShort)
	my.label.replace(QRegExp("%m"), shortMetricName());
    if (expandMetricLong)
	my.label.replace(QRegExp("%M"), my.metric->name());
    if (expandInstShort)
	my.label.replace(QRegExp("%i"), shortInstName());
    if (expandInstLong)
	my.label.replace(QRegExp("%I"), my.inst);
    if (expandHostShort)
	my.label.replace(QRegExp("%h"), shortHostName());
    if (expandHostLong)
	my.label.replace(QRegExp("%H"), hostname());
}

QString
ChartItem::hostname(void) const
{
    return my.metric->context()->source().hostLabel();
}

QString
ChartItem::shortHostName(void) const
{
    QString hostName = hostname();
    int index;

    if ((index = hostName.indexOf(QChar('.'))) != -1) {
	// no change if it looks even vaguely like an IP address or container
	if (!hostName.contains(QRegExp("^\\d+\\.")) &&	// IPv4
	    !hostName.contains(QChar(':')))		// IPv6 or container
	    hostName.truncate(index);
    }
    return hostName;
}

QString
ChartItem::shortMetricName(void) const
{
    QString shortName = my.metric->name();
    int count, index;

    // heuristic: use final two PMNS components (e.g. dev.bytes)
    // reason: short, often this is enough to differentiate well
    while ((count = shortName.count(QChar('.'))) > 1) {
	index = shortName.indexOf(QChar('.'));
	shortName.remove(0, index + 1);
    }
    return shortName;
}

QString
ChartItem::shortInstName(void) const
{
    QString shortName = my.inst;
    int index;

    // heuristic: cull from the first space onward
    // reason: this is required to be unique from PMDAs
    if ((index = shortName.indexOf(QChar(' '))) != -1) {
	shortName.truncate(index);
    }
    return shortName;
}

void
ChartItem::clearLegendLabel(void)
{
    //
    // No legend has been explicitly requested - but something
    // must be displayed.  Use metric name with optional [inst].
    // If that goes beyond a limit, truncate from the front and
    // use ellipses to indicate this has happened.
    //
    if (my.name.size() > PmChart::maximumLegendLength()) {
	int size = PmChart::maximumLegendLength() - 3;
	my.label = QString("...");
	my.label.append(my.name.right(size));
    } else {
	my.label = my.name;
    }
}


void
Chart::setCurrent(bool enable)
{
    QwtScaleWidget *sp;
    QPalette palette;
    QwtText t;

    console->post("Chart::setCurrent(%p) %s", this, enable ? "true" : "false");

    // (Re)set title and y-axis highlight for new/old current chart.
    // For title, have to set both QwtText and QwtTextLabel because of
    // the way attributes are cached and restored when printing charts

    t = titleLabel()->text();
    t.setColor(enable ? globalSettings.chartHighlight : "black");
    setTitle(t);
    palette = titleLabel()->palette();
    palette.setColor(QPalette::Active, QPalette::Text,
		enable ? globalSettings.chartHighlight : QColor("black"));
    titleLabel()->setPalette(palette);

    sp = axisWidget(QwtPlot::yLeft);
    t = sp->title();
    t.setColor(enable ? globalSettings.chartHighlight : "black");
    sp->setTitle(t);
}

void
Chart::preserveSample(int index, int oldindex)
{
#if DESPERATE
    console->post("Chart::preserveSample %d <- %d (%d items)",
			index, oldindex, my.items.size());
#endif
    for (int i = 0; i < my.items.size(); i++)
	my.items[i]->preserveSample(index, oldindex);
}

void
Chart::punchoutSample(int index)
{
#if DESPERATE
    console->post("Chart::punchoutSample %d (%d items)",
			index, my.items.size());
#endif
    for (int i = 0; i < my.items.size(); i++)
	my.items[i]->punchoutSample(index);
}

void
Chart::adjustValues(void)
{
    my.engine->replot();
    replot();
}

void
Chart::updateValues(bool forward, bool visible, int size, int points,
			 double left, double right, double delta)
{
#if DESPERATE
    console->post(PmChart::DebugForce,
		  "Chart::updateValues(forward=%d,visible=%d) sz=%d pts=%d (%d items)",
		  forward, visible, size, points, my.items.size());
#endif

    if (visible) {
	double scale = pmchart->timeAxis()->scaleValue(delta, points);
	setAxisScale(QwtPlot::xBottom, left, right, scale);
    }

    if (my.items.size() > 0)
	my.engine->updateValues(forward, size, points, left, right, delta);

    if (visible) {
	replot();	// done first so Value Axis range is updated
	my.engine->redoScale();
    }
}

void
Chart::replot()
{
    my.engine->replot();
    QwtPlot::replot();
}

void
Chart::legendChecked(QwtPlotItem *item, bool down)
{
#if DESPERATE
    console->post(PmChart::DebugForce, "Chart::legendChecked %s for item %p",
		down? "down":"up", item);
#endif

    // find matching item and update hidden status if required
    bool changed = false;
    for (int i = 0; i < my.items.size(); i++) {
	if (my.items[i]->item() != item)
	    continue;
	// if the state is changing, note it and update
	if (my.items[i]->hidden() != down) {
	    my.items[i]->setHidden(down);
	    changed = true;
	}
	break;
    }

    if (changed) {
	item->setVisible(down == false);
	replot();
    }
}

//
// Add a new chart item (metric, usually with a specific instance)
//
int
Chart::addItem(pmMetricSpec *msp, const QString &legend)
{
    console->post("Chart::addItem src=%s", msp->source);
    if (msp->ninst == 0)
	console->post("addItem metric=%s", msp->metric);
    else
	console->post("addItem instance %s[%s]", msp->metric, msp->inst[0]);

    QmcMetric *mp = my.tab->group()->addMetric(msp, 0.0, true);
    if (mp->status() < 0)
	return mp->status();

    pmDesc desc = mp->desc().desc();
    if (my.items.size() == 0) {
	// first plot item, setup a new ChartEngine
	ChartEngine *engine =
		(desc.type == PM_TYPE_EVENT ||
		 desc.type == PM_TYPE_HIGHRES_EVENT) ?
			(ChartEngine *) new TracingEngine(this) :
			(ChartEngine *) new SamplingEngine(this, desc);
	delete my.engine;
	my.engine = engine;
    }
    else if (!my.engine->isCompatible(desc)) {
	// not compatible with existing metrics, fail
	return PM_ERR_CONV;
    }

    // Finally, request the engine allocate a new chart item,
    // set prevailing chart style and default color, show it.
    //
    ChartItem *item = my.engine->addItem(mp, msp, &desc, legend);
    setStroke(item, my.style, nextColor(my.scheme, &my.sequence));
    item->setVisible(true);
    replot();

    my.items.append(item);
    console->post("addItem %p nitems=%d", item, my.items.size());

    changeTitle(title(), true); // regenerate %h and/or %H expansion
    return my.items.size() - 1;
}

bool
Chart::activeMetric(int index) const
{
    return activeItem(index);
}

bool
Chart::activeItem(int index) const
{
    return (my.items[index]->removed() == false);
}

void
Chart::removeItem(int index)
{
    my.items[index]->remove();
    changeTitle(title(), true); // regenerate %h and/or %H expansion
}

void
Chart::reviveItem(int index)
{
    my.items[index]->revive();
    changeTitle(title(), true); // regenerate %h and/or %H expansion
}

void
Chart::resetValues(int samples, double left, double right)
{
    for (int i = 0; i < my.items.size(); i++)
	my.items[i]->resetValues(samples, left, right);
    replot();
}

int
Chart::metricCount() const
{
    return my.items.size();
}

QString
Chart::title()
{
    return my.title;
}

void
Chart::resetTitleFont(void)
{
    QwtText t = titleLabel()->text();
    t.setFont(*globalFont);
    setTitle(t);
    // have to set font for both QwtText and QwtTextLabel because of
    // the way attributes are cached and restored when printing charts
    QFont titleFont = *globalFont;
    titleFont.setBold(true);
    titleLabel()->setFont(titleFont);
}

void
Chart::resetFont(void)
{
    QwtPlot::legend()->contentsWidget()->setFont(*globalFont);
    setAxisFont(QwtPlot::yLeft, *globalFont);
    resetTitleFont();
}

QString
Chart::hostNameString(bool shortened)
{
    int dot = -1;
    QSet<QString> hostNameSet;
    QList<ChartItem*>::Iterator item;

    // iterate across this chart's items, not activeGroup
    for (item = my.items.begin(); item != my.items.end(); ++item) {
        // NB: ... but we don't get notified of direct calls to
        // ChartItem::setRemoved().
        if ((*item)->removed())
	    continue;

	QString hostName = (*item)->metricContext()->source().hostLabel();

	// decide whether or not to truncate this hostname
	if (shortened)
	    dot = hostName.indexOf(QChar('.'));
	if (dot != -1) {
	    // no change if it looks even vaguely like an IP address or container
	    if (!hostName.contains(QRegExp("^\\d+\\.")) &&	// IPv4
		!hostName.contains(QChar(':')))		// IPv6 or container
		hostName.truncate(dot);
	}
	hostNameSet.insert(hostName);
    }

    // extract the duplicate-eliminated host names
    QSet<QString>::Iterator qsi;
    QString names;
    for (qsi = hostNameSet.begin(); qsi != hostNameSet.end(); qsi++) {
	if (names != "")
	    names += ",";
	names += (*qsi);
    }
    return names;
}

//
// If expand is true then expand %h or %H to host name in rendered title label
//
void
Chart::changeTitle(QString title, bool expand)
{
    bool hadTitle = (my.title != QString::null);
    bool expandHostShort = title.contains("%h");
    bool expandHostLong = title.contains("%H");

    my.title = title;	// copy into QString

    if (my.title != QString::null) {
	if (!hadTitle)
	    pmchart->updateHeight(titleLabel()->height());
	resetTitleFont();
	if (expand && (expandHostShort || expandHostLong)) {
	    if (expandHostShort)
		title.replace(QRegExp("%h"), hostNameString(true));
	    if (expandHostLong)
		title.replace(QRegExp("%H"), hostNameString(false));
	    setTitle(title);
	    // NB: my.title retains the %h and/or %H
	}
	else 
	    setTitle(my.title);
    }
    else {
	if (hadTitle)
	    pmchart->updateHeight(-(titleLabel()->height()));
	setTitle(NULL);
    }
}

QString
Chart::scheme() const
{
    return my.scheme;
}

void
Chart::setScheme(const QString &scheme)
{
    my.sequence = 0;
    my.scheme = scheme;
}

void
Chart::setScheme(const QString &scheme, int sequence)
{
    my.sequence = sequence;
    my.scheme = scheme;
}

Chart::Style
Chart::style()
{
    return my.style;
}

void
Chart::setStyle(Style style)
{
    my.style = style;
}

void
Chart::setStroke(int index, Style style, QColor color)
{
    setStroke(my.items[index], style, color);
}

void
Chart::setStroke(ChartItem *item, Style style, QColor color)
{
    item->setColor(color);
    item->setStroke(style, color, my.engine->antiAliasing());

    if (style != my.style) {
	my.engine->setStyle(my.style);
	my.style = style;
	adjustValues();
    }
}

QColor
Chart::color(int index)
{
    if (index >= 0 && index < my.items.size())
	return my.items[index]->color();
    return QColor("white");
}

void
Chart::setLabel(int index, const QString &s)
{
    if (index >= 0 && index < my.items.size()) {
	ChartItem *item = my.items[index];
	item->setLegend(s);
	item->updateLegend();
    }
}

void
Chart::scale(bool *autoScale, double *yMin, double *yMax)
{
    my.engine->scale(autoScale, yMin, yMax);
}

bool
Chart::autoScale(void)
{
    return my.engine->autoScale();
}

void
Chart::setScale(bool autoScale, double yMin, double yMax)
{
    my.engine->setScale(autoScale, yMin, yMax);
    replot();
    my.engine->redoScale();
}

bool
Chart::rateConvert()
{
    return my.engine->rateConvert();
}

void
Chart::setRateConvert(bool enabled)
{
    my.engine->setRateConvert(enabled);
}

bool
Chart::antiAliasing()
{
    return my.engine->antiAliasing();
}

void
Chart::setAntiAliasing(bool enabled)
{
    my.engine->setAntiAliasing(enabled);
}

QString
Chart::YAxisTitle(void) const
{
    return axisTitle(QwtPlot::yLeft).text();
}

void
Chart::setYAxisTitle(const char *p)
{
    QwtText t;
    bool enable = (my.tab->currentGadget() == this);

    if (!p || *p == '\0')
	t = QwtText(" ");	// for y-axis alignment (space is invisible)
    else
	t = QwtText(p);
    t.setFont(*globalFont);
    t.setColor(enable ? globalSettings.chartHighlight : "black");
    setAxisTitle(QwtPlot::yLeft, t);
}

void
Chart::activated(bool on)
{
    if (on)
	Q_EMIT timeSelectionActive(this,
		canvas()->mapFromGlobal(QCursor::pos()).x());
    else
	Q_EMIT timeSelectionInactive(this);
}

void
Chart::selected(const QPolygon &poly)
{
    my.engine->selected(poly);
    my.tab->setCurrent(this);
}

void
Chart::selected(const QPointF &p)
{
    showPoint(p);
    my.tab->setCurrent(this);
}

void
Chart::moved(const QPointF &p)
{
    Q_EMIT timeSelectionReactive(this,
		canvas()->mapFromGlobal(QCursor::pos()).x());
    my.engine->moved(p);
}


void
Chart::showPoint(const QPointF &p)
{
    ChartItem *selected = NULL;
    double dist, distance = 10e10;
    int index = -1;

    // pixel point
    QPoint pp = my.picker->transform(p);

    console->post("Chart::showPoint p=%.2f,%.2f pixel=%d,%d",
		p.x(), p.y(), pp.x(), pp.y());

    // seek the closest curve to the point selected
    for (int i = 0; i < my.items.size(); i++) {
	QwtPlotCurve *curve = my.items[i]->curve();
	int point = curve->closestPoint(pp, &dist);

	if (dist < distance) {
	    index = point;
	    distance = dist;
	    selected = my.items[i];
	}
    }

    // clear existing selections then show this one
    bool update = (index >= 0 && pp.y() >= 0);
    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];

	item->clearCursor();
	if (update && item == selected)
	    item->updateCursor(p, index);
    }
}

void
Chart::activateTime(QMouseEvent *event)
{
    bool block = signalsBlocked();
    blockSignals(true);
    my.picker->widgetMousePressEvent(event);
    blockSignals(block);
}

void
Chart::reactivateTime(QMouseEvent *event)
{
    bool block = signalsBlocked();
    blockSignals(true);
    my.picker->widgetMouseMoveEvent(event);
    blockSignals(block);
}

void
Chart::deactivateTime(QMouseEvent *event)
{
    bool block = signalsBlocked();
    blockSignals(true);
    my.picker->widgetMouseReleaseEvent(event);
    blockSignals(block);
}

void
Chart::showPoints(const QPolygon &poly)
{
    Q_ASSERT(poly.size() == 2);

    console->post("Chart::showPoints: %d,%d -> %d,%d",
		poly.at(0).x(), poly.at(0).y(), poly.at(1).x(), poly.at(1).y());

    // Transform selected (pixel) points to our coordinate system
    QRectF cp = my.picker->invTransform(poly.boundingRect());
    const QPointF &p = cp.topLeft();

    //
    // If a single-point selected, use showPoint instead
    // (this uses proximity checking, not bounding box).
    //
    if (cp.width() == 0 && cp.height() == 0) {
	showPoint(p);
    }
    else {
	// clear existing selections, find and show new ones
	for (int i = 0; i < my.items.size(); i++) {
	    ChartItem *item = my.items[i];
	    int itemDataSize = item->curve()->dataSize();

	    item->clearCursor();
	    for (int index = 0; index < itemDataSize; index++)
		if (item->containsPoint(cp, index))
		    item->updateCursor(p, index);
	}
    }

    showInfo();
}

//
// give feedback (popup) about the selection
//
void
Chart::showInfo(void)
{
    QString info = QString::null;

    pmchart->timeout();	// clear status bar
    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];
	if (info != QString::null)
	    info.append("\n");
	info.append(item->cursorInfo());
    }

    while (!info.isEmpty() && (info.at(info.length()-1) == '\n'))
	info.chop(1);

    if (!info.isEmpty())
	QWhatsThis::showText(QCursor::pos(), info, this);
    else
	QWhatsThis::hideText();
}

bool
Chart::legendVisible()
{
    // Legend is on or off for all items, only need to test the first item
    if (my.items.size() > 0)
	return QwtPlot::legend() != NULL;
    return false;
}

// Use Edit->Chart Title and Legend to enable/disable the legend.
// Clicking on individual legend buttons will hide/show the
// corresponding item.
//
void
Chart::setLegendVisible(bool on)
{
    QwtLegend *legend = QwtPlot::legend();

    if (on) {
	if (legend == NULL) { // currently disabled, enable it
	    legend = new QwtLegend;

	    legend->setItemMode(QwtLegend::CheckableItem);
	    insertLegend(legend, QwtPlot::BottomLegend);
	    // force each Legend item to "checked" state matching
	    // the initial plotting state
	    for (int i = 0; i < my.items.size(); i++)
		my.items[i]->item()->setVisible(my.items[i]->removed());
	    replot();
	}
    }
    else {
	if (legend != NULL) {
	    // currently enabled, disable it
	    insertLegend(NULL, QwtPlot::BottomLegend);

	    // delete legend;
	    // WISHLIST: this can cause a core dump - needs investigating
	    // [memleak].  Really, all of the legend code needs reworking.
	}
    }
}

void
Chart::save(FILE *f, bool hostDynamic)
{
    SaveViewDialog::saveChart(f, this, hostDynamic);
}

void
Chart::print(QPainter *qp, QRect &rect, bool transparent)
{
    QwtPlotRenderer renderer;

    if (transparent)
	renderer.setDiscardFlag(QwtPlotRenderer::DiscardBackground);
    renderer.render(this, qp, rect);
}

QString Chart::name(int index) const
{
    return my.items[index]->name();
}

QString Chart::legend(int index) const
{
    return my.items[index]->legend();
}

QmcDesc *Chart::metricDesc(int index) const
{
    return my.items[index]->metricDesc();
}

QString Chart::metricName(int index) const
{
    return my.items[index]->metricName();
}

QString Chart::metricInstance(int index) const
{
    return my.items[index]->metricInstance();
}

QmcContext *Chart::metricContext(int index) const
{
    return my.items[index]->metricContext();
}

QmcMetric *Chart::metricPtr(int index) const
{
    return my.items[index]->metricPtr();
}

QSize Chart::minimumSizeHint() const
{
    return QSize(10,10);
}

QSize Chart::sizeHint() const
{
    return QSize(150,100);
}

void
Chart::setupTree(QTreeWidget *tree)
{
    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];

	if (item->removed())
	    continue;
	addToTree(tree, item->name(),
		  item->metricContext(), item->metricHasInstances(), 
		  item->color(), item->legend());
    }
}

void
Chart::addToTree(QTreeWidget *treeview, const QString &metric,
	const QmcContext *context, bool isInst, QColor color,
	const QString &label)
{
    QRegExp regexInstance("\\[(.*)\\]$");
    QRegExp regexNameNode("\\.");
    QString source = context->source().source();
    QString inst, name = metric;
    QStringList	namelist;
    int depth, index;

    console->post("Chart::addToTree src=%s metric=%s, isInst=%d",
		(const char *)source.toAscii(), (const char *)metric.toAscii(),
		isInst);

    depth = name.indexOf(regexInstance);
    if (depth > 0) {
	inst = name.mid(depth+1);	// after '['
	inst.chop(1);			// final ']'
	name = name.mid(0, depth);	// prior '['
    }
    // note: hostname removal must be done *after* instance removal
    // and must take into consideration IPv4/IPv6 address types too
    index = name.lastIndexOf(QChar(':'));
    if (index > 0)
	name = name.remove(0, index+1);

    namelist = name.split(regexNameNode);
    namelist.prepend(source);	// add the host/archive root as well.
    if (depth > 0)
	namelist.append(inst);
    depth = namelist.size();

    // Walk through each component of this name, creating them in the
    // target tree (if not there already), right down to the leaf.

    NameSpace *tree = (NameSpace *)treeview->invisibleRootItem();
    NameSpace *item = NULL;

    for (int b = 0; b < depth; b++) {
	QString text = namelist.at(b);
	bool foundMatchingName = false;
	for (int i = 0; i < tree->childCount(); i++) {
	    item = (NameSpace *)tree->child(i);
	    if (text == item->text(0)) {
		// No insert at this level necessary, move down a level
		tree = item;
		foundMatchingName = true;
		break;
	    }
	}

	// When no more children and no match so far, we create & insert
	if (foundMatchingName == false) {
	    NameSpace *n;
	    if (b == 0) {
		n = new NameSpace(treeview, context);
		n->expand();
		n->setExpanded(true, true);
		n->setSelectable(false);
	    }
	    else {
		bool isLeaf = (b == depth-1);
		n = new NameSpace(tree, text, isLeaf && isInst);
		if (isLeaf) {
		    n->setLabel(label);
		    n->setOriginalColor(color);
		    n->setCurrentColor(color, NULL);
		}
		n->expand();
		n->setExpanded(!isLeaf, true);
		n->setSelectable(isLeaf);
		if (!isLeaf)
		    n->setType(NameSpace::NonLeafName);
		else if (isInst)	// Constructor sets Instance type
		    tree->setType(NameSpace::LeafWithIndom);
		else
		    n->setType(NameSpace::LeafNullIndom);
	    }
	    tree = n;
	}
    }
}


//
// Override behaviour from QwtPlotCurve legend rendering
// Gives us fine-grained control over the colour that we
// display in the legend boxes for each ChartItem.
//
void
ChartCurve::drawLegendIdentifier(QPainter *painter, const QRectF &rect) const
{
    if (rect.isEmpty())
        return;

    QRectF r(0, 0, rect.width()-1, rect.height()-1);
    r.moveCenter(rect.center());

    const QBrush brush(legendColor, Qt::SolidPattern);
    const QColor black(Qt::black);
    QPen pen(black);
    pen.setCapStyle(Qt::FlatCap);

    painter->setPen((const QPen &)pen);
    painter->setBrush(brush);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawRect(r.x(), r.y(), r.width(), r.height());
}
