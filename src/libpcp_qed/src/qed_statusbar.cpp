/*
 * Copyright (c) 2014-2015, Red Hat.
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
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
 */

#include <QtGui>
#include <QStyle>
#include <QStyleOption>
#include <QHBoxLayout>
#include "qed_statusbar.h"
#include "qed_app.h"

QedStatusBar::QedStatusBar()
{
    QFont *font = QedApp::globalFont();

    setFont(*font);
    setFixedHeight(buttonSize());
    setSizeGripEnabled(false);

    my.timeButton = new QedTimeButton(this);
    my.timeButton->setFixedSize(QSize(buttonSize(), buttonSize()));
    my.timeButton->setWhatsThis(
	"VCR state button, also used to display the time control window.");
    my.timeFrame = new QToolButton(this);
    my.timeFrame->setMinimumSize(QSize(buttonSize(), buttonSize()));
    my.timeFrame->setSizePolicy(
			QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    my.timeFrame->setWhatsThis(
	"Unified time axis, displaying the current time position at the "
	"rightmost point, and either status information or the timeframe "
	"covering all Visible Points to the left");

    delete layout();
    QHBoxLayout *box = new QHBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(1);
    box->addWidget(my.timeButton);
    box->addWidget(my.timeFrame);
    setLayout(box);

    my.dateLabel = new QLabel(my.timeFrame);
    my.dateLabel->setFont(*font);
    my.dateLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    my.labelSpacer = new QSpacerItem(10, 0,
				QSizePolicy::Fixed, QSizePolicy::Minimum);

    my.valueLabel = new QLabel(my.timeFrame);
    my.valueLabel->setFont(*font);
    my.valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    my.grid = new QGridLayout;	// Grid of [3 x 1] cells
    my.grid->setContentsMargins(4, 4, 4, 4);
    my.grid->setSpacing(1);
    my.grid->addWidget(my.dateLabel, 0, 0, 1, 1);  // date & time on top row
    my.grid->addItem(my.labelSpacer, 1, 0, 1, 1);  // spacer
    my.grid->addWidget(my.valueLabel, 2, 0, 1, 1); // metric-instance value on bottom row
    my.timeFrame->setLayout(my.grid);
}

void QedStatusBar::init()
{
}

bool QedStatusBar::event(QEvent *e)
{
    if (e->type() == QEvent::Show)
	my.grid->update();
    return QStatusBar::event(e);
}

void QedStatusBar::resizeEvent(QResizeEvent *e)
{
    my.timeFrame->resize(e->size().width()-1 - buttonSize(), buttonSize());
}

void QedStatusBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt(0);

    opt.rect.setRect(buttonSize()+2, 0, width()-buttonSize()-2, buttonSize());
    opt.palette = palette();
    opt.state = QStyle::State_None;
    style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &p, my.timeFrame);
}
