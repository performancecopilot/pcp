/*
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
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
#include <QtGui>
#include <QHBoxLayout>
#include "statusbar.h"
#include "main.h"

StatusBar::StatusBar()
{
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

    // Add the time button and the time frame to the status bar.
    addWidget(my.timeButton);
    addWidget(my.timeFrame, 1);
    clearSpacing(layout());

    my.timeAxis = new TimeAxis(my.timeFrame);
    my.timeAxis->setFixedHeight(timeAxisHeight());
    my.gadgetLabel = new QLabel(my.timeFrame);
    my.gadgetLabel->hide();	// shown with gadget Tabs

    my.dateLabel = new QLabel(my.timeFrame);
    my.dateLabel->setIndent(8);
    my.dateLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    my.labelSpacer = new QSpacerItem(10, 0,
				QSizePolicy::Fixed, QSizePolicy::Minimum);
    my.rightSpacer = new QSpacerItem(0, 0,
				QSizePolicy::Fixed, QSizePolicy::Minimum);

    my.valueLabel = new QLabel(my.timeFrame);
    my.valueLabel->setIndent(8);
    my.valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    my.grid = new QGridLayout;	// Grid of [5 x 3] cells
    my.grid->setMargin(0);
    my.grid->setSpacing(0);
    my.grid->addWidget(my.gadgetLabel, 0, 0, 1, 3);
    my.grid->addWidget(my.timeAxis, 0, 0, 1, 3);   // top two rows, all columns
    my.grid->addWidget(my.dateLabel, 2, 2, 1, 1);  // bottom row, last two cols
    my.grid->addItem(my.labelSpacer, 2, 1, 1, 1);  // bottom row, second column
    my.grid->addWidget(my.valueLabel, 2, 0, 1, 1); // bottom row, first column.
    my.grid->addItem(my.rightSpacer, 0, 4, 2, 1);  // all rows, in final column
    my.timeFrame->setLayout(my.grid);

    resetFont();
}

void StatusBar::resetFont()
{
    setFont(*globalFont);
    my.dateLabel->setFont(*globalFont);
    my.valueLabel->setFont(*globalFont);
    my.gadgetLabel->setFont(*globalFont);
    my.timeAxis->resetFont();
}

void StatusBar::setTimeAxisRightAlignment(int width)
{
    my.rightSpacer->changeSize(width, buttonSize(),
				QSizePolicy::Fixed, QSizePolicy::Fixed);
    my.grid->invalidate();
}

void StatusBar::init()
{
    my.timeAxis->init();
}

bool StatusBar::event(QEvent *e)
{
    if (e->type() == QEvent::Show)
	my.grid->update();
    return QStatusBar::event(e);
}

void StatusBar::resizeEvent(QResizeEvent *e)
{
    my.timeFrame->resize(e->size().width()-1 - buttonSize(), buttonSize());
}

void StatusBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt(0);

    opt.rect.setRect(buttonSize()+2, 0, width()-buttonSize()-2, buttonSize());
    opt.palette = palette();
    opt.state = QStyle::State_None;
    style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &p, my.timeFrame);
}

void StatusBar::clearSpacing(QLayout *lp)
{
    // Traverse the structure of the given layout ensuring that each object takes
    // no extra space. Start with the layout itself.
    lp->setMargin(0);
    lp->setSpacing(0);

    // Now examine its children, if any.
    int numItems = lp->count();
    for (int i = 0; i < numItems; ++i) {
	QLayoutItem *child = lp->itemAt(i);

	// Recursively examine any nested layouts.
	QLayout *l = child->layout();
	if (l) {
	    clearSpacing(l);
	    continue;
	}

	// Remove any spacer items.
	// Careful!! The list will shrink!
	QSpacerItem *s = child->spacerItem();
	if (s) {
	    lp->removeItem(s);
	    delete s;
	    --numItems;
	    --i;
	}
    }
}
