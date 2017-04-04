/*
 * Copyright (c) 2013, Red Hat.
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
#include "pmgadgets.h"
#include "qed_led.h"
#include "qed_line.h"

PmGadgets::PmGadgets(QWidget *parent) : QDialog(parent)
{
    QedRoundLED *redLED = new QedRoundLED(this, Qt::red);
    QedSquareLED *blueLED = new QedSquareLED(this, Qt::blue);
    QedRoundLED *greenLED = new QedRoundLED(this, Qt::green);
    QedSquareLED *grayLED = new QedSquareLED(this, Qt::darkGray);
    QedLine *horizontal = new QedLine(this, 2, Qt::Horizontal);

    my.mainLayout = new QGridLayout;
    my.mainLayout->addWidget(redLED, 0, 0);
    my.mainLayout->addWidget(blueLED, 0, 1);
    my.mainLayout->addWidget(greenLED, 0, 2);
    my.mainLayout->addWidget(grayLED, 1, 0);
    my.mainLayout->addWidget(horizontal, 1, 1);

    setLayout(my.mainLayout);
    setWindowsFlags();
}

void PmGadgets::setWindowsFlags(void)
{
    Qt::WindowFlags flags = windowFlags();
    // removal
    flags &= ~Qt::WindowMinimizeButtonHint;
    flags &= ~Qt::WindowCloseButtonHint;
    // addition
    flags |= Qt::WindowStaysOnTopHint;
    //flags |= Qt::FramelessWindowHint;
    setWindowFlags(flags);
}

void PmGadgets::help()
{
    QMessageBox::information(this, tr("PmGadgets Help"),
	tr("Performance Co-Pilot graphical gadgets and great gizmos."));
}
