/*
 * Copyright (c) 2020 Red Hat.
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
#ifndef QMC_CONFIG_H
#define QMC_CONFIG_H

#include "qmc.h"
#include <qlist.h>
#include <qstring.h>
#include <qtextstream.h>

#if QT_VERSION >= 0x050104
# define QT_ENDL Qt::endl
#else
# define QT_ENDL endl
#endif

#endif	// QMC_CONFIG_H
