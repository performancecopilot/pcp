/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "./dbpmda.h"
#include "./lex.h"

extern int stmt_type;

static union {
    pmID	whole;
    __pmID_int	part;
} pmid;

static union {
    pmInDom		whole;
    __pmInDom_int	part;
} indom;


static int	sts;
static int	inst;
static char	*str;
#define MYWARNSTRSZ 80
static char	warnStr[MYWARNSTRSZ];

param_t	param;

/*
 * pmidp may contain a dynamic PMID ... if so, ask the PMDA to
 * translate name if possible
 */
static int
fix_dynamic_pmid(char *name, pmID *pmidp)
{
    int		sts;
    __pmPDU	*pb;
    extern int	outfd;
    extern int	infd;
    extern pmdaInterface	dispatch;

    if (IS_DYNAMIC_ROOT(*pmidp)) {
	if (connmode == CONN_DSO) {
	    if (dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		sts = dispatch.version.four.pmid(name, pmidp, dispatch.version.four.ext);
		if (sts < 0) return sts;
	    }
	}
	else if (connmode == CONN_DAEMON) {
	    sts = __pmSendNameList(outfd, FROM_ANON, 1, &name, NULL);
	    if (sts < 0) return sts;
	    sts = __pmGetPDU(infd, ANY_SIZE, TIMEOUT_NEVER, &pb);
	    if (sts < 0) return sts;
	    if (sts == PDU_PMNS_IDS) {
		int	xsts;
		sts = __pmDecodeIDList(pb, 1, pmidp, &xsts);
		__pmUnpinPDUBuf(pb);
		if (sts < 0) return sts;
		return xsts;
	    }
	    else if (sts == PDU_ERROR) {
		__pmDecodeError(pb, &sts);
		__pmUnpinPDUBuf(pb);
		return sts;
	    }
	}
    }
    return 0;
}


%}

%union {
	char		*y_str;
	int		y_num;
	twodot_num      y_2num;
	threedot_num    y_3num;
	}

%token <y_2num>
	NUMBER2D

%token <y_3num>
	NUMBER3D

%token <y_num>
	NUMBER
	NEGNUMBER
	FLAG

%token <y_str>
	NAME
	PATHNAME
	MACRO
	STRING

%term	COMMA EQUAL
	OPEN CLOSE DESC GETDESC FETCH INSTANCE PROFILE HELP 
	WATCH DBG QUIT STATUS STORE INFO TIMER NAMESPACE WAIT
	PMNS_NAME PMNS_PMID PMNS_CHILDREN PMNS_TRAVERSE ATTR
	DSO PIPE SOCK UNIX INET IPV6
	ADD DEL ALL NONE INDOM ON OFF
	PLUS EOL

%type <y_num>
	metric
	indom
	optdomain
	debug
	raw_pmid
	attribute
	servport

%type <y_str>
	fname
	arglist
	inst

%%

stmt	: OPEN EOL				{
		param.number = OPEN; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| OPEN DSO fname NAME optdomain EOL	{
		opendso($3, $4, $5);
		stmt_type = OPEN; YYACCEPT;
	    }
	| OPEN PIPE fname arglist 		{
		openpmda($3);
		stmt_type = OPEN; YYACCEPT;
	    }
	| OPEN SOCK fname			{
		open_unix_socket($3);
		stmt_type = OPEN; YYACCEPT;
	    }
	| OPEN SOCK UNIX fname			{
		open_unix_socket($4);
		stmt_type = OPEN; YYACCEPT;
	    }
	| OPEN SOCK INET servport		{
		open_inet_socket($4);
		stmt_type = OPEN; YYACCEPT;
	    }
	| OPEN SOCK IPV6 servport		{
		open_ipv6_socket($4);
		stmt_type = OPEN; YYACCEPT;
	    }
	| CLOSE EOL				{ 
		stmt_type = CLOSE; YYACCEPT; 
	    }
	| DESC EOL				{
		param.number = DESC; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| DESC metric EOL			{
		param.pmid = $2;
		stmt_type = DESC; YYACCEPT;
	    }
	| FETCH EOL				{
		param.number = FETCH; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| FETCH metriclist EOL			{ 
		stmt_type = FETCH; YYACCEPT; 
	    }
	| STORE EOL				{
		param.number = STORE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| STORE metric STRING EOL		{
		param.name = $3;
		param.pmid = (pmID)$2;
		stmt_type = STORE; YYACCEPT;
	    }
	| INFO EOL				{
		param.number = INFO; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| INFO metric EOL			{
		param.number = PM_TEXT_PMID;
		param.pmid = (pmID)$2;
		stmt_type = INFO; YYACCEPT;
	    }
	| INFO INDOM indom EOL			{
		param.number = PM_TEXT_INDOM;
		param.indom = indom.whole;
		stmt_type = INFO; YYACCEPT;
	    }
	| INSTANCE EOL				{
		param.number = INSTANCE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| INSTANCE indom EOL			{
		param.indom = indom.whole;
		param.number = PM_IN_NULL;
		param.name = NULL;
		stmt_type = INSTANCE; YYACCEPT;
	    }
	| INSTANCE indom NUMBER EOL		{
		param.indom = indom.whole;
		param.number = $3;
		param.name = NULL;
		stmt_type = INSTANCE; YYACCEPT;
	    }
	| INSTANCE indom inst EOL		{
		param.indom = indom.whole;
		param.number = PM_IN_NULL;
		param.name = $3;
		stmt_type = INSTANCE; YYACCEPT;
	    }
	| PROFILE EOL				{
		param.number = PROFILE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| PROFILE indom ALL EOL			{
		sts = pmAddProfile($2, 0, NULL);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
	| PROFILE indom NONE EOL		{
		sts = pmDelProfile($2, 0, NULL);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
	| PROFILE indom ADD NUMBER EOL		{
		inst = $4;
		sts = pmAddProfile($2, 1, &inst);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
	| PROFILE indom DEL NUMBER EOL		{
		inst = $4;
		sts = pmDelProfile($2, 1, &inst);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		profile_changed = 1;
		stmt_type = EOL;
		YYACCEPT;
	    }
	| WATCH EOL				{
		param.number = WATCH; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| WATCH fname EOL			{
		watch($2);
		stmt_type = WATCH; YYACCEPT;
	    }
	| PMNS_NAME EOL				{
		param.number = PMNS_NAME; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| PMNS_NAME raw_pmid EOL		{
		param.pmid = $2;
		stmt_type = PMNS_NAME; YYACCEPT;
	    }
	| PMNS_PMID EOL				{
		param.number = PMNS_PMID; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| PMNS_PMID NAME EOL			{
		param.name = $2;
		stmt_type = PMNS_PMID; YYACCEPT;
	    }
	| PMNS_CHILDREN EOL				{
		param.number = PMNS_CHILDREN; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| PMNS_CHILDREN NAME EOL			{
		param.name = $2;
		stmt_type = PMNS_CHILDREN; YYACCEPT;
	    }
	| PMNS_TRAVERSE EOL				{
		param.number = PMNS_TRAVERSE; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| PMNS_TRAVERSE NAME EOL			{
		param.name = $2;
		stmt_type = PMNS_TRAVERSE; YYACCEPT;
	    }
	| NAMESPACE fname EOL			{
		param.name = $2;
		stmt_type = NAMESPACE; YYACCEPT;
	    }
	| ATTR EOL				{
		param.number = ATTR; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| ATTR attribute EOL		{
		param.number = $2;
		param.name = NULL;
		stmt_type = ATTR; YYACCEPT;
	    }
	| ATTR attribute STRING EOL	{
		param.number = $2;
		param.name = $3;
		stmt_type = ATTR; YYACCEPT;
	    }

	| HELP EOL				{ 
		param.number = -1; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT; 
	    }
	| HELP CLOSE EOL			{
		param.number = CLOSE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP DBG EOL				{
		param.number = DBG; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP DESC EOL				{
		param.number = DESC; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP FETCH EOL			{
		param.number = FETCH; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP GETDESC EOL			{
		param.number = GETDESC; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP INFO EOL				{
		param.number = INFO; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP INSTANCE EOL			{
		param.number = INSTANCE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP NAMESPACE EOL			{
		param.number = NAMESPACE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP OPEN EOL				{
		param.number = OPEN; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP PMNS_CHILDREN EOL			{
		param.number = PMNS_CHILDREN; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP PMNS_NAME EOL			{
		param.number = PMNS_NAME; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP PMNS_PMID EOL			{
		param.number = PMNS_PMID; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP PMNS_TRAVERSE EOL			{
		param.number = PMNS_TRAVERSE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP ATTR EOL			{
		param.number = ATTR; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP PROFILE EOL			{
		param.number = PROFILE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP QUIT EOL				{
		param.number = QUIT; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP STATUS EOL			{
		param.number = STATUS; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP STORE EOL			{
		param.number = STORE; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP TIMER EOL				{
		param.number = TIMER; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP WAIT EOL				{
		param.number = WAIT; param.pmid = HELP_FULL;
		stmt_type = HELP; YYACCEPT;
	    }
	| HELP WATCH EOL			{
		param.number = WATCH; param.pmid = HELP_FULL; 
		stmt_type = HELP; YYACCEPT;
	    }

	| QUIT EOL				{ stmt_type = QUIT; YYACCEPT; }
	| DBG EOL				{
		param.number = DBG; param.pmid = HELP_USAGE; 
		stmt_type = HELP; YYACCEPT;
	    }
	| DBG ALL EOL				{
		sts = pmSetDebug("all");
		if (sts < 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "pmSetDebug(\"all\") failed\n");
		    yywarn(warnStr);
		    YYERROR;
		}
		stmt_type = DBG; YYACCEPT;
	    }
	| DBG NONE EOL				{
		sts = pmClearDebug("all");
		if (sts < 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "pmClearDebug(\"all\") failed\n");
		    yywarn(warnStr);
		    YYERROR;
		}
		stmt_type = DBG; YYACCEPT;
	    }
	| DBG debug EOL				{
	        stmt_type = DBG; YYACCEPT;
	    }
	| STATUS EOL				{ 
		stmt_type = STATUS; YYACCEPT; 
	    }
	| TIMER EOL				{
		param.number = TIMER; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| TIMER ON EOL				{ 
		timer = 1; stmt_type = EOL; YYACCEPT; 
	    }
	| TIMER OFF EOL				{ 
		timer = 0; stmt_type = EOL; YYACCEPT; 
	    }
	| GETDESC EOL				{
		param.number = GETDESC; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| GETDESC ON EOL				{ 
		get_desc = 1; stmt_type = EOL; YYACCEPT; 
	    }
	| GETDESC OFF EOL				{ 
		get_desc = 0; stmt_type = EOL; YYACCEPT; 
	    }
	| WAIT EOL				{
		param.number = WAIT; param.pmid = HELP_USAGE;
		stmt_type = HELP; YYACCEPT;
	    }
	| WAIT NUMBER EOL			{ 
		stmt_type = EOL; sleep($2); YYACCEPT;
	    }
	| EOL					{ stmt_type = EOL; YYACCEPT; }
	|					{
		if (yywrap())
		    YYACCEPT;
		else {
		    yyerror("Unrecognized command");
		    YYERROR;
		}
	    }
	;

fname	: NAME					{ $$ = strcons("./", $1); }
	| PATHNAME				{ $$ = $1; }
	;

optdomain : NUMBER				{ $$ = $1; }
	| /* nothing */				{ $$ = 0; }
	;

attribute : NUMBER				{
		sts = __pmAttrKeyStr_r($1, warnStr, sizeof(warnStr));
		if (sts <= 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "Attribute (%d) is not recognised", $1);
		    yyerror(warnStr);
		    YYERROR;
		}
		$$ = $1;
	    }
	| STRING				{ 
		sts = __pmLookupAttrKey($1, strlen($1)+1);
		if (sts <= 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "Attribute (%s) is not recognised", $1);
		    yyerror(warnStr);
		    YYERROR;
		}
		$$ = sts;
	    }
	;

servport : NUMBER				{ $$ = $1; }
	| STRING				{
		struct servent *srv = getservbyname($1, NULL);
		if (srv == NULL) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "Failed to map (%s) to a port number", $1);
		    yyerror(warnStr);
		    YYERROR;
		}
		pmsprintf(warnStr, MYWARNSTRSZ, "Mapped %s to port number %d", $1, srv->s_port);
		yywarn(warnStr);
		$$ = srv->s_port;
	    }
	;

metric	: NUMBER				{ 
		pmid.whole = $1;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		$$ = (int)pmid.whole; 
	    }
        | NUMBER2D				{
		pmid.whole = 0;
		pmid.part.cluster = $1.num1;
		pmid.part.item = $1.num2;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		$$ = (int)pmid.whole;
	    }
	| NUMBER3D 				{
		pmid.whole = 0;
		pmid.part.domain = $1.num1;
		pmid.part.cluster = $1.num2;
		pmid.part.item = $1.num3;
		sts = pmNameID(pmid.whole, &str);
		if (sts < 0) {
		    pmsprintf(warnStr, MYWARNSTRSZ, "PMID (%s) is not defined in the PMNS",
			    pmIDStr(pmid.whole));
		    yywarn(warnStr);
		}
		else
		    free(str);
		$$ = (int)pmid.whole;
	    }
	| NAME					{
		sts = pmLookupName(1, &$1, &pmid.whole);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		sts = fix_dynamic_pmid($1, &pmid.whole);
		if (sts < 0) {
		    yyerror(pmErrStr(sts));
		    YYERROR;
		}
		$$ = (int)pmid.whole;
	    }
	;

indom	: NUMBER				{ 
		indom.whole = $1;
	        $$ = (int)indom.whole;
	    }
	| NEGNUMBER				{ 
		indom.whole = $1;
		$$ = (int)indom.whole;
	    }
	| NUMBER2D 				{
		indom.whole = 0;
		indom.part.domain = $1.num1;
		indom.part.serial = $1.num2;
		$$ = (int)indom.whole;
	    }
	;

raw_pmid: NUMBER3D 				{
		pmid.whole = 0;
		pmid.part.domain = $1.num1;
		pmid.part.cluster = $1.num2;
		pmid.part.item = $1.num3;
		$$ = (int)pmid.whole;
	    }
	;

metriclist : metric				{ addmetriclist((pmID)$1); }
	| metriclist metric			{ addmetriclist((pmID)$2); }
	| metriclist COMMA metric		{ addmetriclist((pmID)$3); }
	;


arglist : /* nothing, a trick */		{ doargs(); }
	;

inst	: STRING				{ $$ = $1; }
	| NAME					{ $$ = $1; }
	;

debug   : NUMBER 			{
			char	nbuf[12];
			pmsprintf(nbuf, sizeof(nbuf), "%d", $1);
			sts = pmSetDebug(nbuf);
			if (sts < 0) {
			    pmsprintf(warnStr, MYWARNSTRSZ, "Bad debug flag (%s)", nbuf);
			    yyerror(warnStr);
			    YYERROR;
			}
			$$ = 0;
		    }
	| NAME					{
			sts = pmSetDebug($1);
			if (sts < 0) {
			    pmsprintf(warnStr, MYWARNSTRSZ, "Bad debug flag (%s)", $1);
			    yyerror(warnStr);
			    YYERROR;
			}
			$$ = 0;
		    }
	| debug NUMBER				{
			char	nbuf[12];
			pmsprintf(nbuf, sizeof(nbuf), "%d", $2);
			sts = pmSetDebug(nbuf);
			if (sts < 0) {
			    pmsprintf(warnStr, MYWARNSTRSZ, "Bad debug flag (%s)", nbuf);
			    yyerror(warnStr);
			    YYERROR;
			}
			$$ = 0;
		    }
	| debug NAME					{
			sts = pmSetDebug($2);
			if (sts < 0) {
			    pmsprintf(warnStr, MYWARNSTRSZ, "Bad debug flag (%s)", $2);
			    yyerror(warnStr);
			    YYERROR;
			}
			$$ = 0;
		    }
	;

%%
