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
#include "metricdetails.h"
#include <QTableWidget>

MetricDetailsWindow::MetricDetailsWindow(QWidget* parent) :
    QMainWindow(parent)
{
    setupUi(this);

    // Set up the metrics table and headers.
    tableWidget->setColumnCount(numColumns());
    tableWidget->setRowCount(1);
    QTableWidgetItem *item = new QTableWidgetItem("Time");
    tableWidget->setHorizontalHeaderItem(timeColumn(), item);
    item = new QTableWidgetItem("Hostname");
    tableWidget->setHorizontalHeaderItem(hostNameColumn(), item);
    item = new QTableWidgetItem("Metric");
    tableWidget->setHorizontalHeaderItem(metricColumn(), item);
    item = new QTableWidgetItem("Instance");
    tableWidget->setHorizontalHeaderItem(instanceColumn(), item);
    item = new QTableWidgetItem("Value");
    tableWidget->setHorizontalHeaderItem(valueColumn(), item);
}
