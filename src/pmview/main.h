/*
 * Copyright (c) 2009, Aconex.  All Rights Reserved.
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
#ifndef MAIN_H
#define MAIN_H

#include "pcp/pmapi.h"
#include "pcp/impl.h"

#include "qed_app.h"
#include "main.h"
#include "viewobj.h"
#include "colorlist.h"
#include "pmview.h"

class QedApp;
class View;
class ModList;
class SoQtExaminerViewer;

typedef void (*TermCB)(int);

typedef struct {
	// Sampling
	double viewDelta;
	bool viewDeltaModified;
	double loggerDelta;
	bool loggerDeltaModified;

	// Colors
	QColor viewBackground;
	QString viewBackgroundName;
	bool viewBackgroundModified;
	QColor viewHighlight;
	QString viewHighlightName;
	bool viewHighlightModified;
	QColor gridBackground;
	QString gridBackgroundName;
	bool gridBackgroundModified;

	// Toolbar
	int initialToolbar;
	bool initialToolbarModified;
	int nativeToolbar;
	bool nativeToolbarModified;
	int toolbarLocation;
	int toolbarLocationModified;
	QStringList toolbarActions;
	bool toolbarActionsModified;
} Settings;

// TODO: old X-resources...
// ! Background color of read-only labels
// !PmView+*readOnlyBackground: Black
// ! Maximum value before saturation
// ! The default of 1.05 allows for 5% error in the time delta when
// ! determining rates, before values are deemed saturated.
// PmView+*saturation: 1.05
// ! Use fast anti-aliasing
// PmView+*antiAliasSmooth: tree
// ! Number of anti-aliasing passes: 1-255. Only 1 pass disables antialiasing.
// PmView+*antiAliasPasses: 1
// ! Grid, Bar and Stack object base borders
// PmView+*baseBorderWidth: 8
// PmView+*baseBorderDepth: 8
// ! Height of Grid, Bar and Stack bases
// PmView+*baseHeight: 2
// ! Color of base plane
// PmView+*baseColor: rgbi:0.15/0.15/0.15
// ! Spacing between Bar blocks
// PmView+*barSpaceWidth: 8
// PmView+*barSpaceDepth: 8
// ! Spacing between Bar base and labels
// PmView+*barSpaceLabel: 6
// ! Width and depth of Bar blocks
// PmView+*barLength: 28
// PmView+*barHeight: 80
// ! Margin around a Label
// PmView+*labelMargin: 5
// ! Color of labels
// PmView+*labelColor: rgbi:1.0/1.0/1.0
// ! Width and depth of Grid columns and rows
// PmView+*gridMinWidth: 20
// PmView+*gridMinDepth: 20

extern Settings globalSettings;
extern void readSettings();
extern void writeSettings();
extern QColor nextColor(const QString &, int *);

extern int Cflag;
extern int Lflag;
extern char *outgeometry;

extern QString	theConfigName;	// Configuration file name
extern FILE	*theConfigFile;	// Configuration file
extern ColorList theColorLists;	// ColorLists generated while parsing config
extern float	theGlobalScale;	// Scale applied to entire scene
extern FILE	*theAltConfig;	// Save the config file here
extern bool	theAltConfigFlag; // True when config is saved to temporary file
extern QString	theAltConfigName; // Name of the saved configuration file

class SceneGroup;
extern SceneGroup *liveGroup;
extern SceneGroup *archiveGroup;
extern SceneGroup *activeGroup;

class PmView;
extern PmView *pmview;

class QedTimeControl;
extern QedTimeControl *pmtime;

extern int genInventor();
extern char lastinput();
extern char input();
extern int markpos();
extern int locateError();

extern ViewObj *rootObj;
extern int errorCount;
extern int yyparse(void);
extern FILE *yyin;

extern float			theScale;	// The scale controls multiplier
extern ModList			*theModList;	// List of modulated objects
extern View			*theView;	// Viewer coordinator
extern QedApp			*theApp;	// Our application object
extern const int		theBufferLen;	// Length of theBuffer
extern char			theBuffer[];	// String buffer for anything
extern const QString		theDefaultFlags;

int setup(const char *appname, int *argc, char **argv, 
	      void *cmdopts, int numOpts, TermCB termCB);

#define _POS_	__FILE__, __LINE__
 
int warningMsg(const char *fileName, int line, const char *msg, ...);
int errorMsg(const char *fileName, int line, const char *msg, ...);
int fatalMsg(const char *fileName, int line, const char *msg, ...);

#endif	// MAIN_H
