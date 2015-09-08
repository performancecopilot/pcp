/*
 * Copyright (c) 2014, Red Hat.
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
    my.timeButton->setWhatsThis(QApplication::translate("PmChart",
	"VCR state button, also used to display the time control window.",
	0, QApplication::UnicodeUTF8));
    my.timeFrame = new QToolButton(this);
    my.timeFrame->setMinimumSize(QSize(buttonSize(), buttonSize()));
    my.timeFrame->setSizePolicy(
			QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    my.timeFrame->setWhatsThis(QApplication::translate("PmChart",
	"Unified time axis, displaying the current time position at the "
	"rightmost point, and either status information or the timeframe "
	"covering all Visible Points to the left",
	0, QApplication::UnicodeUTF8));

    delete layout();
    QHBoxLayout *box = new QHBoxLayout;
    box->setMargin(0);
    box->setSpacing(1);
    box->addWidget(my.timeButton);
    box->addWidget(my.timeFrame);
    setLayout(box);

    my.gadgetLabel = new QLabel(my.timeFrame);
    my.gadgetLabel->setFont(*font);
    my.gadgetLabel->hide();	// shown with gadget Views

    my.dateLabel = new QLabel(my.timeFrame);
    my.dateLabel->setIndent(8);
    my.dateLabel->setFont(*font);
    my.dateLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    my.labelSpacer = new QSpacerItem(10, 0,
				QSizePolicy::Fixed, QSizePolicy::Minimum);
    my.rightSpacer = new QSpacerItem(0, 0,
				QSizePolicy::Fixed, QSizePolicy::Minimum);

    my.valueLabel = new QLabel(my.timeFrame);
    my.valueLabel->setIndent(8);
    my.valueLabel->setFont(*font);
    my.valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    my.grid = new QGridLayout;	// Grid of [5 x 3] cells
    my.grid->setMargin(0);
    my.grid->setSpacing(0);
    my.grid->addWidget(my.gadgetLabel, 0, 0, 1, 3);
    my.grid->addWidget(my.dateLabel, 2, 2, 1, 1);  // bottom row, last two cols
    my.grid->addItem(my.labelSpacer, 2, 1, 1, 1);  // bottom row, second column
    my.grid->addWidget(my.valueLabel, 2, 0, 1, 1); // bottom row, first column.
    my.grid->addItem(my.rightSpacer, 0, 4, 2, 1);  // all rows, in final column
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
