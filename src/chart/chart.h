/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef CHART_H
#define CHART_H

//
// Chart class ... multiple plots per chart, multiple charts per tab
//

#include <qdatetime.h>
#include <qlistview.h>
#include <qcolor.h>
#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_picker.h>
#include <qwt/qwt_double_rect.h>
#include <pcp/pmc/Metric.h>
#include <pcp/pmc/String.h>

typedef enum { None, Line, Bar, Stack, Area, Util } chartStyle;

class Tab;

class Chart : public QwtPlot 
{
    Q_OBJECT
public:
    Chart(Tab *, QWidget * = 0);
    ~Chart(void);
    int		addPlot(pmMetricSpec *, char *);
    int		numPlot(void);
    void	delPlot(int);
    char	*title(void);			// return chart title
    void	changeTitle(char *, int);	// NULL to clear
    chartStyle	style(void);			// return chart style
    int		setStyle(chartStyle);		// set chart style
    QColor	color(int);			// return color for ith plot
    int		setColor(int, QColor);		// set plot color
    void	scale(bool *, double *, double *);
			// return autoscale state and fixed scale parameters
    void	setScale(bool, double, double);
			// set autoscale state and fixed scale parameters
    void	setYAxisTitle(char *);
    bool	legendVisible();
    void	setLegendVisible(bool);

    void	update(bool, bool);

    PMC_String	*name(int);
    char	*legend_spec(int);
    PMC_Desc	*metricDesc(int);
    PMC_String	*metricName(int);
    PMC_Context	*metricContext(int);

    QString	pmloggerMetricSyntax(int);

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;
    void	fixLegendPen(void);

    void	setupListView(QListView *);
    void	addToList(QListView *, QString, const PMC_Context *,
			  bool, bool, QColor&);

    static QColor defaultColor(int);

private slots:
    void	selected(const QwtDoublePoint &);
    void	moved(const QwtDoublePoint &);
    void	showCurve(QwtPlotItem *, bool);

private:
    typedef struct plot {
	QwtPlotCurve	*curve;
	PMC_String	*name;
	char		*legend;	// from config
	PMC_String	*legend_label;	// as appears in plot
	QColor		color;
	double		scale;
	double		*data;
	double		*plot_data;
	int		dataCount;
	bool		removed;
    } plot_t;
    plot_t		*_plots;
    PMC_MetricList	_metrics;
    char		*_title;
    chartStyle		_style;
    bool		_autoscale;
    double		_ymin;
    double		_ymax;
    QwtPlotPicker	*_picker;
    Tab			*_tab;
};

#endif	/* CHART_H */
