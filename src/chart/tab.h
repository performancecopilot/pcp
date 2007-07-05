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
 *                      Nathan Scott, nathans At debian DoT orrg
 */
#ifndef TAB_H
#define TAB_H

//
// Top level control - one instance of this class for all visible charts
//

#include "view.h"
#include "chart.h"
#include "kmtime.h"
#include "timebutton.h"
#include <qobject.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qsplitter.h>
#include <qtabwidget.h>
#include <qpushbutton.h>
#include <qwt/qwt_plot.h>
#include <qwt/qwt_scale_draw.h>

enum TimeState {
    START_STATE,
    FORWARD_STATE,
    BACKWARD_STATE,
    ENDLOG_STATE,
    STANDBY_STATE,
};

class Tab : public QWidget
{
    Q_OBJECT
public:
    Tab(QWidget * = 0);
    void	init(QTabWidget *, int, int, PMC_Group *, km_tctl_source,
			const char *, struct timeval *, struct timeval *);
    bool	isArchiveMode(void);	// query if tab is for archives

    Chart	*chart(int);		// ith chart
    Chart	*currentChart(void);	// current chart (can be NULL)
    Chart	*addChart(void);	// append a new chart to tab area
    					// and becomes the current chart
    int		deleteChart(int);	// remove ith chart, return current
    int		deleteChart(Chart *);	// remove given chart, return current
    int		deleteCurrent(void);	// remove current chart, return current
    int		numChart(void);		// number of charts
    int		setCurrent(Chart *);	// set current chart based on choice

    bool	isRecording(void);
    int		startRecording(void);
    void	stopRecording(void);
    void	setFolio(QString);
    void	addLogger(PmLogger *);

    void	setVisibleHistory(int);
    int		visibleHistory(void);
    void	setSampleHistory(int);
    int		sampleHistory(void);

    void	setConfig(char *);
    char	*config(void);		// config filename from -c
    double	*timeData(void);	// base addr of time axis data
    // TODO: end nuke.

    PMC_Group	*group(void);		// metric fetchgroup

    void setupWorldView(void);
    void step(kmTime *);
    void vcrmode(kmTime *, bool);
    void setTimezone(char *);
    void showTimeControl(void);
    void updateTimeButton(void);
    void updateTimeAxis(void);
    void updateTimeAxis(time_t secs);

    enum TimeButtonState buttonState(void);
    km_tctl_state kmtimeState(void);
    void newButtonState(km_tctl_state s, km_tctl_mode m, int mode, bool record);

    QWidget	*splitter() { return _splitter; }

private:
    typedef struct chart {
	Chart		*cp;
    } chart_t;
    chart_t		*_charts;
    int			_num;		// total number of charts
    int			_current;	// currently selected chart
    bool		_recording;	// running any pmlogger's?
    bool		_showdate;	// display date in time axis?
    double		_interval;	// current update interval
    struct timeval	_lastkmdelta;	// last delta from kmtime
    struct timeval	_lastkmposition;// last position from kmtime
    int			_visible;	// -v visible points
    int			_samples;	// -s total number of samples
    double		*_timeData;	// time array (intervals)
    TimeState		_timestate;
    TimeButtonState	_buttonstate;
    km_tctl_source	_mode;		// reliable archive/host test
    km_tctl_state	_lastkmstate;
    PMC_Group		*_group;	// metric fetchgroup
    QString		_datelabel;
    QSplitter		*_splitter;
    QString		_folio;		// archive folio, if logging
    QPtrList<PmLogger>	_loglist;	// list of pmloggers for this Tab

    void refresh_charts(void);
    void adjustWorldView(kmTime *, bool);
    void adjustLiveWorldView(kmTime *);
    void adjustArchiveWorldView(kmTime *, bool);
    void adjustArchiveWorldViewForward(kmTime *, bool);
    void adjustArchiveWorldViewBackward(kmTime *, bool);
    void adjustArchiveWorldViewStop(kmTime *, bool);
};

#endif	/* TAB_H */
