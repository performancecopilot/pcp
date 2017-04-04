/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1996-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _GLOBAL_H
#define _GLOBAL_H

// Global data from resources/command line options
struct AppData {
    int		zoom;			// Zoom factor
    double	delta;			// Update interval (seconds)
    char *	defaultFont;		// Default font for label gadgets
};

extern AppData	appData;		// Global options/resources

#endif	/* _GLOBAL_H */
