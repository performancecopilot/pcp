/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1996 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <setjmp.h>
#include <values.h>
#include "global.h"
#include "tokens.h"

#include "qed.h"
#include "qed_bar.h"
#include "qed_led.h"
#include "qed_line.h"
#include "qed_label.h"
#include "qed_legend.h"
#include "qed_gadget.h"
#include "qed_colorlist.h"
#include "qed_actionlist.h"
#include <qnumeric.h>

extern "C" {
int yylex();
}

// Variables manipulated by the lexical analyser
//
unsigned nLines = 1;
int tokenIntVal;
double tokenRealVal;
char *tokenStringVal;

// Parser variables
//
static int token;
static int eofOK = 1;
static unsigned nErrors;
static unsigned nWarnings;

static QList<QedColorList*> colorLists;
static QList<QedLegend*> legends;
static QList<QedActionList*> actionLists;
static QList<QedGadget*> gadgets;
static QWidget *parent;

class MetricData
{
public:
    MetricData(char* h, char* m, char* i) { host = h; metric = m; inst = i; }
    char	*host;                   // NULL => localhost
    char	*metric;                 // metric name
    char	*inst;                   // instance name (may be NULL)
};

int AddMetrics(double a, QList<MetricData*>&b, QedGadget *c)
{
    /* TODO */
    (void)a;
    (void)b;
    (void)c;

    return 0;
}

typedef struct {
    int		id;
    const char*	string;
} TokenList;

static TokenList tokenStrings[] = {
    { TOK_LINE,		"line"			},
    { TOK_LABEL,	"label"			},
    { TOK_BAR,		"bar"			},
    { TOK_MULTIBAR,	"multibar"		},
    { TOK_BARGRAPH,	"bargraph"		},
    { TOK_LED,		"led"			},

    { TOK_LEGEND,	"legend"		},
    { TOK_COLOURLIST,	"colourlist"		},
    { TOK_ACTIONLIST,	"actionlist"		},

    { TOK_BAD_RES_WORD,	"<bad reserved word>"	},
    { TOK_UPDATE,	"update"		},
    { TOK_METRIC,	"metric"		},
    { TOK_HORIZONTAL,	"horizontal"		},
    { TOK_VERTICAL,	"vertical"		},
    { TOK_METRICS,	"metrics"		},
    { TOK_MIN,		"min"			},
    { TOK_MAX,		"max"			},
    { TOK_DEFAULT,	"default"		},
    { TOK_FIXED,	"fixed"			},
    { TOK_COLOUR,	"colour"		},
    { TOK_HISTORY,	"history"		},
    { TOK_NOBORDER,	"noborder"		},

    { TOK_IDENTIFIER,	"<identifier>"		},
    { TOK_INTEGER,	"<integer>"		},
    { TOK_REAL,		"<real>"		},
    { TOK_STRING,	"<string>"		},
    { TOK_LPAREN,	"("			},
    { TOK_RPAREN,	")"			},
    { TOK_LBRACKET,	"["			},
    { TOK_RBRACKET,	"]"			},
    { TOK_COLON,	":"			},

    { TOK_EOF,		"EOF"			},
    { 0,		"raw EOF"		},
};
const unsigned nTokenStrings = sizeof(tokenStrings) / sizeof(tokenStrings[0]);

static const char*
TokenToString(int tok)
{
    for (unsigned i = 0; i < nTokenStrings; i++)
	if (tokenStrings[i].id == tok)
	    return tokenStrings[i].string;
    return "???";
}

static jmp_buf scannerEofEnv;
static int stashedToken = -1;

static int
NextToken()
{
    static int atEOF = 0;

    if (atEOF)
	return token;

    if (stashedToken < 0) {
	if ((token = yylex()) == 0) {
	    atEOF = 1;
	    token = TOK_EOF;
	    if (!eofOK) {
		nErrors++;
		pmprintf("Error, line %d: unexpected EOF, giving up\n", nLines);
		longjmp(scannerEofEnv, 0);
	    }
	}
    }
    else {
	token = stashedToken;
	stashedToken = -1;
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "input: %s (%d)\n", TokenToString(token), token);
    return token;
}

static void
FindNewStatement()
{
    eofOK = 1;
    for(;;)
	switch (token) {
	    case TOK_EOF:
	    case TOK_LINE:
	    case TOK_LABEL:
	    case TOK_BAR:
	    case TOK_MULTIBAR:
	    case TOK_BARGRAPH:
	    case TOK_LED:
	    case TOK_LEGEND:
	    case TOK_COLOURLIST:
		return;

	    default:
		NextToken();
		break;
	}
}

static int
ParseUpdate(double& update, int eofMayFollow)
{
    eofOK = 0;
    NextToken();
    if (token == TOK_INTEGER)
	update = tokenIntVal;
    else if (token == TOK_REAL)
	update = tokenRealVal;
    else {
	nErrors++;
	pmprintf("Error, line %d: update interval must be a number\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParseHistory(int& history, int eofMayFollow)
{
    int err = 0;

    eofOK = 0;
    NextToken();
    if (token == TOK_INTEGER) {
	if ((history = tokenIntVal) < 0)
	    err = 1;
    }
    else
	err = 1;

    if (err) {
	nErrors++;
	pmprintf("Error, line %d: history must be a positive integer\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParsePosition(int& x, int& y, int eofMayFollow)
{
    eofOK = 0;
    if (token == TOK_INTEGER)
	x = tokenIntVal * appData.zoom;
    else {
	nErrors++;
	pmprintf("Error, line %d: X coordinate must be an integer\n", nLines);
	return -1;
    }
    NextToken();
    if (token == TOK_INTEGER)
	y = tokenIntVal * appData.zoom;
    else {
	nErrors++;
	pmprintf("Error, line %d: Y coordinate must be an integer\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParseSize(unsigned& w, unsigned& h, int eofMayFollow)
{
    eofOK = 0;
    if (token == TOK_INTEGER && tokenIntVal > 0)
	w = tokenIntVal * appData.zoom;
    else {
	nErrors++;
	pmprintf("Error, line %d: width must be a positive integer\n", nLines);
	return -1;
    }
    NextToken();
    if (token == TOK_INTEGER && tokenIntVal > 0)
	h = tokenIntVal * appData.zoom;
    else {
	nErrors++;
	pmprintf("Error, line %d: height must be a positive integer\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParseGeometry(int& x, int& y, unsigned& w, unsigned& h, int eofMayFollow)
{
    int sts;

    eofOK = 0;
    if ((sts = ParsePosition(x, y, 0)) < 0)
	return sts;
    sts = ParseSize(w, h, eofMayFollow);
    return sts;
}

static int
ParseActionListActions(QedActionList *alp, int eofMayFollow)
{
    unsigned int	n = 0;

    eofOK = 0;
    NextToken();
    while (token == TOK_IDENTIFIER || token == TOK_STRING) {
	if (token == TOK_IDENTIFIER || token == TOK_STRING) {
	    alp->addName(tokenStringVal);
	    NextToken();
	}
	else {
	    nErrors++;
	    pmprintf("Error, line %d: action name expected\n", nLines);
	    return -1;
	}
	if (token == TOK_IDENTIFIER || token == TOK_STRING) {
	    alp->addAction(tokenStringVal);
	    NextToken();
	}
	else {
	    nErrors++;
	    pmprintf("Error, line %d: action expected\n", nLines);
	    return -1;
	}
	if (token == TOK_DEFAULT) {
	    if (alp->defaultPos() == -1)
		alp->setDefaultPos(n);
	    else {
		nErrors++;
		pmprintf("Error, line %d: only one default action permitted in list\n", nLines);
	    }
	    NextToken();
	}
	n++;
    }
    eofOK = eofMayFollow;
    if (token == TOK_RPAREN)
	NextToken();
    else {
	nErrors++;
	pmprintf("Error, line %d: `)' expected at end of action list\n", nLines);
	return -1;
    }
    return 0;
}

typedef enum {
    AL_NEED_NAME = 1,
    AL_MAY_BE_REFERENCE = 2
} ActionListFlags;

// This parses 3 types of action list:
//
//	_actions name (...)	definition of action list
//	_actions name		reference to defined action list
//	_actions (...)		anonymous action list
//
static int
ParseActionList(
		int		flags,		// AL_* flags above
		int		eofMayFollow)
{
    char		*name;
    char		anon[64];
    int			ai, sts;
    int			existingActions = -1;
    QedActionList	*alp;
    static int		count;

    eofOK = 0;
    NextToken();
    if (token == TOK_IDENTIFIER) {
	name = tokenStringVal;
	for (ai = 0; ai < actionLists.size(); ai++) {
	    alp = actionLists.at(ai);
	    if (strcmp(alp->identity(), name) == 0)
		break;
	}
	if (ai != actionLists.size())
	    existingActions = ai;
	else
	    alp = new QedActionList(name);
	eofOK = eofMayFollow;
	NextToken();
    }
    else {
	if (flags & AL_NEED_NAME) {
	    nErrors++;
	    pmprintf("Error, line %d: name expected for actions list\n", nLines);
	    return -1;
	}
	pmsprintf(anon, sizeof(anon), "anonymous#%d", count++);
	alp = new QedActionList(anon);
	name = anon;
    }

    sts = 0;
    if (token == TOK_LPAREN) {
	// This is an action list definition (the name may be omitted)
	//
	eofOK = 0;
	if (existingActions != -1) {
	    nErrors++;
	    pmprintf("Error, line %d: an actions list named \"%s\" already exists\n",
		    nLines, name);
	    return -1;
	}
	sts = ParseActionListActions(alp, eofMayFollow);
    }
    else {
	// This should be a reference to an already defined action list
	//
	if (name == NULL) {
	    nErrors++;
	    pmprintf("Error, line %d: name or ( expected for actions list\n", nLines);
	    return -1;
	}
	if (!(flags & AL_MAY_BE_REFERENCE)) {
	    nErrors++;
	    pmprintf("Error, line %d: `(' expected for actions list definition\n", nLines);
	    return -1;
	}
	if (existingActions == -1) {
	    nErrors++;
	    pmprintf("Error, line %d: no action list named %s defined\n",
		    nLines, name);
	    return -1;
	}
    }

    if (sts >= 0 && existingActions == -1)
	actionLists.append(alp);

    return sts;
}

static int
ParseLine(int eofMayFollow)
{
    int		x, y, sts;
    unsigned	w, h;

    eofOK = 0;
    NextToken();

    sts = ParseGeometry(x, y, w, h, eofMayFollow);
    if (sts < 0)
	return sts;

    if (token == TOK_UPDATE) {
	nErrors++;
	pmprintf("Error, line %d: lines may not have an update interval\n",
		nLines);
	return -1;
    }

    // Parse optional actions list
    //
    if (token == TOK_ACTIONLIST) {
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    QedLine *l = new QedLine(parent, x, y, w, h);
    if (l) {
	gadgets.append(l);
    } else {
	perror("no memory for line gadget");
	exit(1);
    }

    if (pmDebugOptions.appl0)
	l->dump(stderr);
    return 0;
}

static int
ParseLabel(int eofMayFollow)
{
    int		x, y, sts;
    char*	text;
    char*	font = appData.defaultFont;

    eofOK = 0;
    NextToken();

    sts = ParsePosition(x, y, 0);
    if (sts < 0)
	return sts;

    if (token == TOK_UPDATE) {
	nErrors++;
	pmprintf("Error, line %d: labels may not have an update interval\n",
		nLines);
	return -1;
    }

    if (token != TOK_STRING && token != TOK_IDENTIFIER) {
	nErrors++;
	pmprintf("Error, line %d: label string expected\n", nLines);
	return -1;
    }
    text = tokenStringVal;
    eofOK = eofMayFollow;
    NextToken();
    if (token == TOK_STRING || token == TOK_IDENTIFIER) {
	font = tokenStringVal;
	NextToken();
    }

    if (token == TOK_ACTIONLIST)
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);

    if (sts >= 0) {
	sts = 0;
	// note: constructor specifies baseline pos
	QedLabel* l = new QedLabel(parent, x, y, text, font);
	if (l) {
	    gadgets.append(l);
	} else {
	    perror("no memory for label gadget");
	    exit(1);
	}

	if (pmDebugOptions.appl0)
	    l->dump(stderr);
    }

    if (text)
	free(text);
    if (font && font != appData.defaultFont)
	free(font);
    return sts;
}

// A metric spec is one of
//
//	<metric>
//	<metric>[<inst>]
//	<host>:<metric>
//	<host>:<metric>[<inst>]
//
static int
ParseMetric(MetricData** resultPtr, int eofMayFollow)
{
    char*	host;
    char*	metric;
    char*	inst;
    char*	hostOrMetric = NULL;

    eofOK = 0;
    if (token == TOK_IDENTIFIER)
	hostOrMetric = tokenStringVal;
    else {
	nErrors++;
	pmprintf("Error, line %d: performance metric expected\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    if (token == TOK_COLON) {		// Have "<host>:<metric>"
	host = hostOrMetric;
	eofOK = 0;
	NextToken();
	if (token == TOK_IDENTIFIER)
	    metric = tokenStringVal;
	else {
	    nErrors++;
	    pmprintf("Error, line %d: performance metric expected\n", nLines);
	    return -1;
	}
	eofOK = eofMayFollow;
	NextToken();
    }
    else {				// Optional "<host>:" omitted
	host = NULL;
	metric = hostOrMetric;
    }
    if (token == TOK_LBRACKET) {
	eofOK = 0;
	NextToken();
	if (token == TOK_IDENTIFIER || token == TOK_STRING)
	    inst = tokenStringVal;
	else {
	    nErrors++;
	    pmprintf("Error, line %d: performance metric instance expected\n",
		    nLines);
	    return -1;
	}
	NextToken();
	if (token != TOK_RBRACKET) {
	    nErrors++;
	    pmprintf("Error, line %d: `]' for performance metric instance missing\n", nLines);
	    return -1;
	}
	eofOK = eofMayFollow;
	NextToken();
    }
    else
	inst = NULL;

    *resultPtr = new MetricData(host, metric, inst);
    if (*resultPtr == NULL) {
	perror("No memory for metric spec");
	exit(1);
    }
    return 0;
}

static int
ParseColor(char** colourName, int eofMayFollow)
{
    eofOK = 0;
    NextToken();
    if (token == TOK_IDENTIFIER || token == TOK_STRING) {
	*colourName = strdup(tokenStringVal);
	if (*colourName == NULL) {
	    perror("No memory for color name");
	    exit(1);
	}
    }
    else {
	nErrors++;
	pmprintf("Error, line %d: color name missing\n", nLines);
	return -1;
    }
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParseBar(int eofMayFollow)
{
    int x, y, sts;
    unsigned w, h;
    MetricData* metricData;
    double min = qQNaN(), max = qQNaN();
    int isVertical = 0;
    double update = appData.delta;
    char* colour = NULL;
    int fixMax = 0;

    eofOK = 0;
    NextToken();

    sts = ParseGeometry(x, y, w, h, eofMayFollow);
    if (sts < 0)
	return sts;

    if (token == TOK_UPDATE) {
	sts = ParseUpdate(update, 0);
	if (sts < 0)
	    return sts;
    }

    if (token != TOK_METRIC) {
	nErrors++;
	pmprintf("Error, line %d: _metric expected\n", nLines);
	return -1;
    }
    NextToken();

    sts = ParseMetric(&metricData, eofMayFollow);
    if (sts < 0)
	return sts;

    while (token == TOK_HORIZONTAL ||
	   token == TOK_VERTICAL ||
	   token == TOK_FIXED ||
	   token == TOK_MIN ||
	   token == TOK_MAX) {

	if (token == TOK_FIXED) {
	    eofOK = 0;
	    NextToken();
	    if (token != TOK_MAX) {
		nErrors++;
		pmprintf("Error, line %d: _fixed _max expected\n", nLines);
		return -1;
	    }
	    fixMax = 1;
	}

	if (token == TOK_HORIZONTAL) {
	    eofOK = eofMayFollow;
	    NextToken();
	}
	else if (token == TOK_VERTICAL) {
	    eofOK = eofMayFollow;
	    NextToken();
	    isVertical = 1;
	}
	else if (token == TOK_MIN) {
	    eofOK = 0;
	    NextToken();
	    if (token != TOK_INTEGER && token != TOK_REAL) {
		nErrors++;
		pmprintf("Error, line %d: minimum must be a number\n", nLines);
		return -1;
	    }
	    min = (token == TOK_INTEGER) ? tokenIntVal : tokenRealVal;

	    eofOK = eofMayFollow;
	    NextToken();
	}
	else if (token == TOK_MAX) {
	    eofOK = 0;
	    NextToken();
	    if (token != TOK_INTEGER && token != TOK_REAL) {
		nErrors++;
		pmprintf("Error, line %d: maximum must be a number\n", nLines);
		return -1;
	    }
	    max = (token == TOK_INTEGER) ? tokenIntVal : tokenRealVal;

	    eofOK = eofMayFollow;
	    NextToken();
	}
    }

    if (token == TOK_COLOUR) {
	sts = ParseColor(&colour, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    // Parse optional actions list
    //
    if (token == TOK_ACTIONLIST) {
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    QedBar *b = new QedBar(parent, x, y, w, h);
    QList<MetricData*> mlp;
    if (b) {
	if (min != qQNaN())
	    b->setMinimum(min);
	if (max != qQNaN())
	    b->setMaximum(max);
	if (isVertical)
	    b->setOrientation(Qt::Vertical);
	if (fixMax)
	    b->setScaleRange(0);
	gadgets.append(b);
	mlp.append(metricData);
    } else {
	perror("no memory for bar gadget");
	exit(1);
    }
    if (colour)
	b->setColor(colour);

    if (AddMetrics(update, mlp, b))
	nWarnings++;

    if (pmDebugOptions.appl0)
	b->dump(stderr);
    return 0;
}

static int
ParseMultibar(int eofMayFollow)
{
    int x, y, sts, ci;
    unsigned w, h;
    MetricData* metricData;
    int isVertical = 0;
    int noborder = 0;
    int history = 0;
    double update = appData.delta;

    eofOK = 0;
    NextToken();

    sts = ParseGeometry(x, y, w, h, eofMayFollow);

    if (token == TOK_UPDATE) {
	sts = ParseUpdate(update, 0);
	if (sts < 0)
	    return sts;
    }

    if (token == TOK_NOBORDER) {
	noborder = 1;
	NextToken();
    }

    if (token == TOK_HISTORY) {
	sts = ParseHistory(history, 0);
	if (sts < 0)
	    return sts;
    }

    if (token != TOK_METRICS) {
	nErrors++;
	pmprintf("Error, line %d: _metrics (list) expected\n", nLines);
	return -1;
    }
    NextToken();

    if (token != TOK_LPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `(' expected\n", nLines);
	return -1;
    }
    NextToken();

    QList<MetricData*> mlp;
    do {
	sts = ParseMetric(&metricData, 0);
	if (sts < 0)
	    return sts;
	mlp.append(metricData);
    } while (token == TOK_IDENTIFIER);

    if (token != TOK_RPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `)' expected\n", nLines);
	return -1;
    }
    NextToken();

    // By default a multibar has a floating maximum initially set to a small
    // value.  If a max of zero is specified, the multibar is a utilisation.
    // The optional _fixed keyword before the maximum can be used to specify
    // clipping.
    //
    int clip = 0;
    double max = DBL_MAX;
    if (token == TOK_FIXED) {
	clip = 1;
	NextToken();
	if (token != TOK_MAX) {
	    nErrors++;
	    pmprintf("Error, line %d: _maximum expected\n", nLines);
	    return -1;
	}
    }
    if (token == TOK_MAX) {
	NextToken();
	if (token != TOK_INTEGER && token != TOK_REAL) {
	    nErrors++;
	    pmprintf("Error, line %d: a maximum must be a number\n", nLines);
	    return -1;
	}
	max = (token == TOK_INTEGER) ? tokenIntVal : tokenRealVal;
	if (max < 0.0 || (clip && max == 0.0)) {
	    nErrors++;
	    pmprintf("Error, line %d: a fixed maximum must be positive\n",
		    nLines);
	    return -1;
	}
	NextToken();
    }

    if (token != TOK_COLOURLIST) {
	nErrors++;
	pmprintf("Error, line %d: _colourlist expected\n", nLines);
	return -1;
    }
    NextToken();

    if (token != TOK_IDENTIFIER) {
	nErrors++;
	pmprintf("Error, line %d: colourlist name expected\n", nLines);
	return -1;
    }

    for (ci = 0; ci < colorLists.size(); ci++) {
	if (strcmp(colorLists.at(ci)->identity(), tokenStringVal) == 0)
	    break;
    }
    if (ci == colorLists.size()) {
	nErrors++;
	pmprintf("Error, line %d: no colourlist named %s exists\n", 
		nLines, tokenStringVal);
	return -1;
    }

    QedColorList *colorList = colorLists.at(ci);
    unsigned int nColours = colorList->length();
    unsigned int nMetrics = mlp.length();

    if (nColours != nMetrics) {
	nErrors++;
	pmprintf("Error, line %d: %d colours don't match %d metrics\n",
		nLines, nColours, nMetrics);
	return -1;
    }

    eofOK = eofMayFollow;
    NextToken();

    while (token == TOK_HORIZONTAL || token == TOK_VERTICAL) {
	isVertical = (token == TOK_VERTICAL);
	NextToken();
    }

    // Parse optional actions list
    //
    if (token == TOK_ACTIONLIST) {
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    QedMultiBar* m = new QedMultiBar(parent, x, y, w, h, colorList, history);
    if (m) {
	if (isVertical)
	    m->setOrientation(Qt::Vertical);
	if (max > 0.0)
	    m->setMaximum(max, clip);
	m->setOutline(!noborder);
	gadgets.append(m);
    } else {
	perror("no memory to allocate multibar gadget");
	exit(1);
    }

    if (AddMetrics(update, mlp, m))
	nWarnings++;

    if (pmDebugOptions.appl0)
	m->dump(stderr);
    return 0;
}

static int
ParseBarGraph(int eofMayFollow)
{
    int x, y, sts;
    unsigned w, h;
    MetricData* metricData;
    double min = qQNaN(), max = qQNaN();
    double update = appData.delta;
    int fixMax = 0;

    eofOK = 0;
    NextToken();

    sts = ParseGeometry(x, y, w, h, eofMayFollow);
    if (sts < 0)
	return sts;

    if (token == TOK_UPDATE) {
	sts = ParseUpdate(update, 0);
	if (sts < 0)
	    return sts;
    }

    if (token != TOK_METRIC) {
	nErrors++;
	pmprintf("Error, line %d: _metric expected\n", nLines);
	return -1;
    }
    NextToken();

    // There's only ever one metric for a bargraph, use chunkSize = 1
    QList<MetricData*> mlp;
    sts = ParseMetric(&metricData, eofMayFollow);
    if (sts < 0)
	return sts;
    mlp.append(metricData);

    while (token == TOK_MIN || token == TOK_MAX || token == TOK_FIXED) {

	eofOK = 0;
	if (token == TOK_FIXED) {
	    NextToken();
	    if (token != TOK_MAX) {
		nErrors++;
		pmprintf("Error, line %d: _fixed _max expected\n", nLines);
		return -1;
	    }
	    fixMax = 1;
	}

	if (token == TOK_MIN) {
	    NextToken();
	    if (token != TOK_INTEGER && token != TOK_REAL) {
		nErrors++;
		pmprintf("Error, line %d: minimum must be a number\n", nLines);
		return -1;
	    }
	    min = (token == TOK_INTEGER) ? tokenIntVal : tokenRealVal;
	}
	else {
	    NextToken();
	    if (token != TOK_INTEGER && token != TOK_REAL) {
		nErrors++;
		pmprintf("Error, line %d: maximum must be a number\n", nLines);
		return -1;
	    }
	    max = (token == TOK_INTEGER) ? tokenIntVal : tokenRealVal;
	}
	eofOK = eofMayFollow;
	NextToken();
    }

    // Parse optional actions list
    //
    if (token == TOK_ACTIONLIST) {
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    QedBarGraph* b = new QedBarGraph(parent, x, y, w, h, w - 1);
    if (b) {
	if (min != qQNaN())
	    b->setMinimum(min);
	if (max != qQNaN())
	    b->setMaximum(max);
	if (fixMax)
	    b->clipRange();
	gadgets.append(b);
    } else {
	perror("no memory for bargraph gadget");
	exit(1);
    }

    if (AddMetrics(update, mlp, b))
	nWarnings++;

    if (pmDebugOptions.appl0)
	b->dump(stderr);
    return 0;
}

static int
ParseLed(int eofMayFollow)
{
    int x, y, sts, li;
    unsigned w, h;
    MetricData* metricData;
    double update = appData.delta;

    eofOK = 0;
    NextToken();

    sts = ParseGeometry(x, y, w, h, eofMayFollow);
    if (sts < 0)
	return sts;

    if (token == TOK_UPDATE) {
	sts = ParseUpdate(update, 0);
	if (sts < 0)
	    return sts;
    }

    if (token != TOK_METRIC) {
	nErrors++;
	pmprintf("Error, line %d: _metric expected\n", nLines);
	return -1;
    }
    NextToken();

    QList<MetricData*> mlp;
    sts = ParseMetric(&metricData, eofMayFollow);
    if (sts < 0)
	return sts;
    mlp.append(metricData);

    if (token != TOK_LEGEND) {
	nErrors++;
	pmprintf("Error, line %d: _legend expected for _led\n", nLines);
	return -1;
    }
    NextToken();

    if (token != TOK_IDENTIFIER) {
	nErrors++;
	pmprintf("Error, line %d: legend name expected\n", nLines);
	return -1;
    }

    for (li = 0; li < legends.size(); li++) {
	if (strcmp(legends.at(li)->identity(), tokenStringVal) == 0)
	    break;
    }
    if (li == colorLists.size()) {
	nErrors++;
	pmprintf("Error, line %d: no legend named %s exists\n",
		nLines, tokenStringVal);
	return -1;
    }
    QedLegend *legend = legends.at(li);

    eofOK = eofMayFollow;
    NextToken();

    // Parse optional actions list
    //
    if (token == TOK_ACTIONLIST) {
	sts = ParseActionList(AL_MAY_BE_REFERENCE, eofMayFollow);
	if (sts < 0)
	    return sts;
    }

    QedRoundLED* l = new QedRoundLED(parent, x, y, w, h, legend);
    if (l) {
	gadgets.append(l);
    } else {
	perror("no memory for led gadget");
	exit(1);
    }

    if (AddMetrics(update, mlp, l))
	nWarnings++;

    if (pmDebugOptions.appl0)
	l->dump(stderr);
    return 0;
}

static int
ParseLegend(int eofMayFollow)
{
    eofOK = 0;

    NextToken();
    if (token != TOK_IDENTIFIER) {
	nErrors++;
	pmprintf("Error, line %d: name expected for legend\n", nLines);
	return -1;
    }
    QedLegend* legend = new QedLegend(tokenStringVal);
    if (legend == NULL) {
	nErrors++;
	fprintf(stderr,
		"Error, line %d: out of memory allocating legend\n",
		nLines);
	return -1;
    }
    legends.append(legend);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "legend:\n    name \"%s\"\n", tokenStringVal);

    NextToken();
    if (token != TOK_LPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `(' expected for legend\n", nLines);
	delete legend;
	return -1;
    }
    NextToken();

    while (token == TOK_INTEGER ||
	   token == TOK_REAL ||
	   token == TOK_DEFAULT) {
	double val = 0.0;
	int prevToken;
	if (token != TOK_DEFAULT)
	    val = (token == TOK_REAL) ? tokenRealVal : tokenIntVal;
	prevToken = token;
	NextToken();
	if (token != TOK_IDENTIFIER && token != TOK_STRING) {
	    nErrors++;
	    pmprintf("Error, line %d: colour name expected in legend\n", nLines);
	    delete legend;
	    return -1;
	}
	if (prevToken != TOK_DEFAULT)
	    legend->addThreshold(val, tokenStringVal);
	else
	    legend->setDefaultColor(tokenStringVal);

	if (pmDebugOptions.appl0)
	    fprintf(stderr, "    %.2f = %s\n", val, tokenStringVal);
	NextToken();
    }

    if (pmDebugOptions.appl0)
	putc('\n', stderr);

    if (token != TOK_RPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `)' or threshold expected for legend\n",
		nLines);
	delete legend;
	return -1;
    }

    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

static int
ParseColorList(int eofMayFollow)
{
    eofOK = 0;
    NextToken();
    if (token != TOK_IDENTIFIER) {
	nErrors++;
	pmprintf("Error, line %d: name expected for colourlist\n", nLines);
	return -1;
    }
    char *name = tokenStringVal;
    NextToken();

    if (token != TOK_LPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `(' expected for colourlist\n", nLines);
	return -1;
    }
    NextToken();

    if (pmDebugOptions.appl0)
	fprintf(stderr, "colourlist: name \"%s\"\n", name);

    QedColorList *colorList = new QedColorList(name);
    if (colorList == NULL) {
	nErrors++;
	fprintf(stderr,
		"Error, line %d: out of memory allocating colorList\n",
		nLines);
	return -1;
    }
    while (token == TOK_IDENTIFIER || token == TOK_STRING) {
	colorList->addColor(tokenStringVal);
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "            %s\n", tokenStringVal);
	NextToken();
    }

    if (pmDebugOptions.appl0)
	putc('\n', stderr);

    if (token != TOK_RPAREN) {
	nErrors++;
	pmprintf("Error, line %d: `)' expected for colourlist\n", nLines);
	return -1;
    }
    colorLists.append(colorList);
    eofOK = eofMayFollow;
    NextToken();
    return 0;
}

// Config files are typically created by programs rather than specified by the
// user.  In the config file header, after the version number, such programs
// should put their argv so that the desktop can restart pmgadgets
// (indirectly) by re-running the program that created the config file.
//
QStringList configArgv;

void ParseVersion()
{
    if ((token == TOK_IDENTIFIER || token == TOK_STRING) &&
	strcmp(tokenStringVal, "pmgadgets") == 0) {
	eofOK = 0;
	NextToken();
	if (token != TOK_INTEGER) {
	    pmprintf("Error, line %d: bad pmgadgets config file version\n",
		    nLines);
	    pmflush();
	    exit(1);
	}
	if (tokenIntVal > 1) {
	    pmprintf("Error, line %d: you need a newer version pmgadgets to run this\n",
		    nLines);
	    pmflush();
	    exit(1);
	}
	else if (tokenIntVal < 1) {
	    pmprintf("Error, line %d: ludicrous pmgadgets config file version\n",
		    nLines);
	    pmflush();
	    exit(1);
	}
	eofOK = 1;
	NextToken();
	while (token == TOK_IDENTIFIER || token == TOK_STRING) {
	    configArgv.append(strdup(tokenStringVal));
	    NextToken();
	}
    }
}

int
Parse(void)
{
    int sts;
    NextToken();
    setjmp(scannerEofEnv);		// come back here on unexpected EOF

    ParseVersion();
    while (token != TOK_EOF) {
	eofOK = 0;
	switch (token) {
	    case TOK_LINE:
		sts = ParseLine(1);
		break;

	    case TOK_LABEL:
		sts = ParseLabel(1);
		break;

	    case TOK_BAR:
		sts = ParseBar(1);
		break;

	    case TOK_MULTIBAR:
		sts = ParseMultibar(1);
		break;

	    case TOK_BARGRAPH:
		sts = ParseBarGraph(1);
		break;

	    case TOK_LED:
		sts = ParseLed(1);
		break;

	    case TOK_LEGEND:
		sts = ParseLegend(1);
		break;

	    case TOK_COLOURLIST:
		sts = ParseColorList(1);
		break;

	    case TOK_ACTIONLIST:
		sts = ParseActionList(AL_NEED_NAME, 1);
		break;

	    default:
		sts = -1;
		nErrors++;
		pmprintf("Error, line %u: Bad gadget type\n", nLines);
		break;
	}
	if (sts == -1) {
	    eofOK = 1;
	    FindNewStatement();
	}
	const unsigned maxErrors = 10;
	if (nErrors > maxErrors) {
	    pmprintf("Too many errors, giving up!\n");
	    break;
	}
    }
    if (nErrors) {
	pmprintf("%s: configuration file has errors "
		"(%d lines parsed, %d errors)\n", pmGetProgname(), nLines, nErrors);
	pmflush();
	return -1;
    }
    if (nWarnings) 
	pmflush();
    return 0;
}
