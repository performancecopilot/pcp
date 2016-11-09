/*
 * Copyright (c) 2016, Red Hat.
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
#ifndef METRICDETAILS_H
#define METRICDETAILS_H

#include "ui_metricdetails.h"

class Chart;

class MetricDetailsWindow : public QDialog, public Ui::MetricDetailsWindow
{
    Q_OBJECT

 public:
    MetricDetailsWindow(QWidget*);

    void setupTable(Chart *);

 private:
    // Define the column layout
    int timeColumn()     const { return 0; }
    int hostNameColumn() const { return 1; }
    int metricColumn()   const { return 2; }
    int instanceColumn() const { return 3; }
    int valueColumn()    const { return 4; }
    int numColumns()     const { return 5; }
};

#endif	// METRICDETAILS_H
