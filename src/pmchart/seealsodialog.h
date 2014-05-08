/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef SEEALSODIALOG_H
#define SEEALSODIALOG_H

#include "ui_seealsodialog.h"

class SeeAlsoDialog : public QDialog, public Ui::SeeAlsoDialog
{
    Q_OBJECT

public:
    SeeAlsoDialog(QWidget* parent);

public slots:
    virtual void seeAlsoOKButton_clicked();
};

#endif // SEEALSODIALOG_H
