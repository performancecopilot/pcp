/*
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

%{
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "pmapi.h"
#include "impl.h"
#include "./pmlc.h"

#ifdef YYDEBUG
int yydebug=1;
#endif

int	mystate = GLOBAL;
int	logfreq;
int	parse_stmt;
char	emess[160];
char	*hostname;
int	state;
int	control;
int	qa_case;

static int	sts;

extern int	port;
extern int	pid;
extern int	is_local;
extern int	is_unix;
extern int	is_socket_path;

%}

%union {
    long lval;
    char * str;
}

%term	LSQB
	RSQB
	COMMA
	LBRACE
	RBRACE
	AT
	EOL

	LOG
	MANDATORY ADVISORY
	ON OFF MAYBE
	EVERY ONCE
	MSEC SECOND MINUTE HOUR

	QUERY SHOW LOGGER CONNECT PRIMARY QUIT STATUS HELP
	TIMEZONE LOCAL PORT SOCK
	NEW VOLUME

	SYNC
	QA

%token<str>	NAME HOSTNAME STRING URL
%token<lval>	NUMBER

%type<lval> timeunits
%%

stmt   		: dowhat
		{
		    mystate |= INSPEC;
		    if (!connected()) {
			metric_cnt = -1;
			return 0;
		    }
		    if (ConnectPMCD()) {
			yyerror("");
			metric_cnt = -1;
			return 0;
		    }
		    beginmetrics();
		}
		somemetrics
		{
		    mystate = GLOBAL;
		    endmetrics();
		}
		EOL
		{
		    parse_stmt = LOG;
		    YYACCEPT;
		}
		| SHOW loggersopt hostopt EOL
		{
		    parse_stmt = SHOW;
		    YYACCEPT;
		}
		| CONNECT towhom hostopt EOL
		{
		    parse_stmt = CONNECT;
		    YYACCEPT;
		}
		| HELP EOL
		{
		    parse_stmt = HELP;
		    YYACCEPT;
		}
		| QUIT EOL
		{
		    parse_stmt = QUIT;
		    YYACCEPT;
		}
		| STATUS EOL
		{
		    parse_stmt = STATUS;
		    YYACCEPT;
		}
		| NEW VOLUME EOL
		{
		    parse_stmt = NEW;
		    YYACCEPT;
		}
		| TIMEZONE tzspec EOL
		{
		    parse_stmt = TIMEZONE;
		    YYACCEPT;
		}
		| SYNC EOL
		{
		    parse_stmt = SYNC;
		    YYACCEPT;
		}
		| QA NUMBER EOL
		{
		    parse_stmt = QA;
		    qa_case = $2;
		    YYACCEPT;
		}
		| EOL
		{
		    parse_stmt = 0;
		    YYACCEPT;
		}
		| { YYERROR; }
		;

dowhat		: action 
		;

action		: QUERY				{ state = PM_LOG_ENQUIRE; }
		| logopt cntrl ON frequency	{ state = PM_LOG_ON; }
		| logopt cntrl OFF		{ state = PM_LOG_OFF; }
		| logopt MANDATORY MAYBE
		{
		    control = PM_LOG_MANDATORY;
		    state = PM_LOG_MAYBE;
		}
		;

logopt		: LOG 
		| /* nothing */
		;

cntrl		: MANDATORY			{ control = PM_LOG_MANDATORY; }
		| ADVISORY			{ control = PM_LOG_ADVISORY; }
		;

frequency	: everyopt NUMBER timeunits	{ logfreq = $2 * $3; }
		| ONCE				{ logfreq = -1; }
		;

everyopt	: EVERY
		| /* nothing */
		;

timeunits	: MSEC		{ $$ = 1; }
		| SECOND	{ $$ = 1000; }
		| MINUTE	{ $$ = 60000; }
		| HOUR		{ $$ = 3600000; }
		;

somemetrics	: LBRACE { mystate |= INSPECLIST; } metriclist RBRACE
		| metricspec
		;

metriclist	: metricspec
		| metriclist metricspec
		| metriclist COMMA metricspec
		;

metricspec	: NAME
		{
		    beginmetgrp();
		    if ((sts = pmTraversePMNS($1, addmetric)) < 0)
			/* metric_cnt is set by addmetric but if
			 * traversePMNS fails, set it so that the bad
			 * news is visible to other routines
			 */
			metric_cnt = sts;
		    else if (metric_cnt < 0) /* addmetric failed */
			sts = metric_cnt;

		    if (sts < 0 || metric_cnt == 0) {
			sprintf(emess, 
				"Problem with lookup for metric \"%s\" ...",$1);
			yywarn(emess);
			if (sts < 0) {
			    fprintf(stderr, "Reason: ");
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
		    }
		}
		optinst		{ endmetgrp(); }
		;

optinst		: LSQB instancelist RSQB
		| /* nothing */
		;

instancelist	: instance
		| instance instancelist
		| instance COMMA instancelist
		;

instance	: NAME		{ addinst($1, 0); }
		| NUMBER	{ addinst(NULL, $1); }
		| STRING	{ addinst($1, 0); }
		;

loggersopt	: LOGGER
		| /* nothing */
		;

hostopt		: AT NAME	{ hostname = strdup($2); }
		| AT HOSTNAME	{ hostname = strdup($2); }
		| AT URL
		{
		    char	*prefix_end;
		    size_t	prefix_len;
		    hostname = strdup($2);
		    prefix_end = strchr(hostname, ':');
		    if (prefix_end != NULL) {
			prefix_len = prefix_end - hostname + 1;
			if (prefix_len == 6 && strncmp(hostname, "local:", prefix_len) == 0)
			    is_local = 1;
			else if (prefix_len == 5 && strncmp(hostname, "unix:", prefix_len) == 0)
			    is_unix = 1;
			if (is_local || is_unix) {
			    const char *p;
			    /*
			     * Find out is a path was specified.
			     * Skip any initial path separators.
			     */
			    for (p = hostname + prefix_len; *p == __pmPathSeparator(); ++p)
				;
			    if (*p != '\0')
				is_socket_path = 1;
			}
		    }
		}
		| AT NUMBER
		{ 
			/* That MUST be a mistake! */
			char tb[64];
			sprintf (tb, "%d", (int)$2);
			hostname = strdup(tb); 
		}
		| AT STRING	{ hostname = strdup($2); }
		| /* nothing */
		;

towhom		: PRIMARY	{ pid = PM_LOG_PRIMARY_PID; port = PM_LOG_NO_PORT; }
		| NUMBER	{ pid = $1; port = PM_LOG_NO_PORT; }
		| PORT NUMBER	{ pid = PM_LOG_NO_PID; port = $2; }
		| SOCK		{ pid = PM_LOG_NO_PID; port = PM_LOG_NO_PORT; }
		;

tzspec		: LOCAL		{ tztype = TZ_LOCAL; }
		| LOGGER	{ tztype = TZ_LOGGER; }
		| STRING		
		{
		    tztype = TZ_OTHER;
		    /* ignore the quotes: skip the leading one and
		     * clobber the trailing one with a null to
		     * terminate the string really required.
		     */
		    if (tz != NULL)
			free(tz);
		    if ((tz = strdup($1)) == NULL) {
			__pmNoMem("setting up timezone",
				 strlen($1), PM_FATAL_ERR);
		    }
		}
		;

%%

extern char	*configfile;
extern int	lineno;

void
yywarn(char *s)
{
    fprintf(stderr, "Warning [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, lineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);
}

void
yyerror(char *s)
{
    fprintf(stderr, "Error [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, lineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);

    skipAhead ();
    yyclearin;
    mystate = GLOBAL;
}
