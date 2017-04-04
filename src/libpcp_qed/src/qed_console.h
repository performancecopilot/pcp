/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#ifndef QED_CONSOLE_H
#define QED_CONSOLE_H

#include "ui_qed_console.h"
#include "qed_app.h"

class QedConsole : public QDialog, public Ui::QedConsole
{
    Q_OBJECT

public:
    QedConsole(struct timeval);
    void post(const char *p, ...);
    void post(int level, const char *p, ...);
    bool logLevel(int level = QedApp::DebugApp);

private:
    struct {
	int level;
	double origin;
    } my;
};

extern QedConsole *console;

#endif	// QED_CONSOLE_H
