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
#ifndef PMGADGETS_H
#define PMGADGETS_H

#include <QGridLayout>
#include <QDialog>
#include <QList>

class PmGadgets : public QDialog
{
    Q_OBJECT

public:
    PmGadgets(QWidget *parent = 0);

private slots:
    void help();

private:
    void setWindowsFlags();

    struct {
	QList<QWidget *> widgets;
	QGridLayout *mainLayout;
    } my;
};

#endif // PMGADGETS_H
