/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "./dbpmda.h"
#include "./lex.h"
#include "./gram.h"

extern pmdaInterface	dispatch;
extern int		infd;
extern int		outfd;

__pmProfile		*profile = NULL;
int			profile_changed;
int			timer = 0;
int			get_desc = 0;

static pmID		*pmidlist;
static int		numpmid;
static __pmContext	ctx;

static char	**argv;
static int	argc;

/*
 * Warning: order of these strings _must_ match bit field sequence defined
 *	    in impl.h for DBG_TRACE_* macros
 */
static char* debugFlags[] = {
    "pdu", "fetch", "profile", "value", "context", "indom", "pdubuf", "log",
    "logmeta", "optfetch", "af", "appl0", "appl1", "appl2", "pmns", "libpmda",
    "timecontrol", "pmc", "interp"
};

static int numFlags = sizeof(debugFlags)/sizeof(debugFlags[0]);

void
reset_profile(void)
{
    if ((profile = (__pmProfile *)realloc(profile, sizeof(__pmProfile))) == NULL) {
	__pmNoMem("reset_profile", sizeof(__pmProfile), PM_FATAL_ERR);
	exit(1);
    }
    ctx.c_instprof = profile;
    memset(profile, 0, sizeof(__pmProfile));
    profile->state = PM_PROFILE_INCLUDE;        /* default global state */
    profile_changed = 1;
}

char *
strcons(char *s1, char *s2)
{
    int		i;
    char	*buf;

    i = (int)strlen(s1) + (int)strlen(s2) + 1;

    buf = (char *)malloc(i);
    if (buf == NULL) {
	fprintf(stderr, "strcons: malloc failed: %s\n", strerror(errno));
	exit(1);
    }

    strcpy(buf, s1);
    strcat(buf, s2);

    return buf;
}

char *
strnum(int n)
{
    char	*buf;

    buf = (char *)malloc(13);
    if (buf == NULL) {
	fprintf(stderr, "strnum: malloc failed: %s\n", strerror(errno));
	exit(1);
    }
    sprintf(buf, "%d", n);
    return buf;
}

void
initmetriclist(void)
{
    param.numpmid = 0;
    param.pmidlist = NULL;
}

void
addmetriclist(pmID pmid)
{
    param.numpmid++;

    if (param.numpmid >= numpmid) {
        numpmid = param.numpmid;
	pmidlist = (pmID *)realloc(pmidlist, numpmid * sizeof(pmidlist[0]));
	if (pmidlist == NULL) {
	    fprintf(stderr, "addmetriclist: realloc failed: %s\n", strerror(errno));
	    exit(1);
	}
    }

    pmidlist[param.numpmid-1] = pmid;
    param.pmidlist = pmidlist;
}

void
initarglist(void)
{
    int		i;

    for (i = 0; i < param.argc; i++)
	if (param.argv[i] != NULL)
	    free(param.argv[i]);
    param.argc = 0;
    param.argv = NULL;
    addarglist("");
}

void
addarglist(char *arg)
{
    param.argc++;

    if (param.argc >= argc) {
        argc = param.argc;
	argv = (char **)realloc(argv, argc * sizeof(pmProgname));
	if (argv == NULL) {
	    fprintf(stderr, "addarglist: realloc failed: %s\n", strerror(errno));
	    exit(1);
	}
    }

    if (arg != NULL)
	argv[param.argc-1] = strdup(arg);
    else
	argv[param.argc-1] = arg;
    param.argv = argv;
}

void
watch(char *fname)
{
    char	cmd[200];

    sprintf(cmd, "xwsh -hold -title \"dbpmda watch %s\" -geom 80x16 -bg dodgerblue4 -e tail -f %s &",
	fname, fname);
    
    system(cmd);
}

void
printindom(FILE *f, __pmInResult *irp)
{
    int		i;

    for (i = 0; i < irp->numinst; i++) {
	fprintf(f, "[%3d]", i);
	if (irp->instlist != NULL)
	    fprintf(f, " inst: %d", irp->instlist[i]);
	if (irp->namelist != NULL)
	    fprintf(f, " name: \"%s\"", irp->namelist[i]);
	fputc('\n', f);
    }
}

/*
 * Fake out a context for use with the profile routines
 */

int pmWhichContext(void)
{
    static int		first = 1;

    if (first) {
	memset(&ctx, 0, sizeof(__pmContext));
	ctx.c_type = PM_CONTEXT_HOST;
	reset_profile();
	first = 0;
    }
    return 0;
}

__pmContext *
__pmHandleToPtr(int dummy)
{
    return(&ctx);
}

void
dohelp(int command, int full)
{
    if (command < 0) {
	puts("help [ command ]\n");
	dohelp(CLOSE, HELP_USAGE);
	dohelp(DBG, HELP_USAGE);
	dohelp(DESC, HELP_USAGE);
	dohelp(FETCH, HELP_USAGE);
	dohelp(GETDESC, HELP_USAGE);
	dohelp(INSTANCE, HELP_USAGE);
	dohelp(NAMESPACE, HELP_USAGE);
	dohelp(OPEN, HELP_USAGE);
	dohelp(PROFILE, HELP_USAGE);
	dohelp(QUIT, HELP_USAGE);
	dohelp(STATUS, HELP_USAGE);
	dohelp(STORE, HELP_USAGE);
	dohelp(INFO, HELP_USAGE);
	dohelp(TIMER, HELP_USAGE);
	dohelp(WAIT, HELP_USAGE);
	dohelp(WATCH, HELP_USAGE);
	putchar('\n');
    }
    else {
	if (full == HELP_FULL)
	    putchar('\n');

	switch (command) {
	case OPEN:
	    puts("open dso dsoname init_routine [ domain# ]");
	    puts("open pipe execname [ arg ... ]");
	    break;
	case CLOSE:
	    puts("close");
	    break;
	case DESC:
	    puts("desc metric");
	    break;
	case FETCH:
	    puts("fetch metric [ metric ... ]");
	    break;
	case GETDESC:
	    puts("getdesc on | off");
	    break;
	case NAMESPACE:
	    puts("namespace fname");
	    break;
	case INSTANCE:
	    puts("instance indom# [ number | name | \"name\" ]");
	    break;
	case PROFILE:
	    puts("profile indom# [ all | none ]");
	    puts("profile indom# [ add | delete ] number");
	    break;
	case WATCH:
	    puts("watch logfilename");
	    break;
	case DBG:
	    puts("debug all | none");
	    puts("debug flag [ flag ... ] (flag is decimal or symbolic name)");
	    break;
	case QUIT:
	    puts("quit");
	    break;
	case STATUS:
	    puts("status");
	    break;
	case STORE:
	    puts("store metric \"value\"");
	    break;
	case INFO:
	    puts("text metric");
	    puts("text indom indom#");
	    break;
	case TIMER:
	    puts("timer on | off");
	    break;
	case WAIT:
	    puts("wait seconds");
	    break;
	default:
	    fprintf(stderr, "Help for that command (%d) not supported!\n", command);
	}

	if (full == HELP_FULL) {
	    putchar('\n');
	    switch (command) {
	    case OPEN:
		puts(
"Open a PMDA as either a DSO or a daemon (connected with a pipe).  The\n"
"'dsoname' and 'execname' fields are the path to the PMDA shared object file\n"
"or executable.  The arguments to this command are similar to a line in the\n"
"pmcd.conf file.\n");
		break;
	    case CLOSE:
		puts(
"Close the pipe to a daemon PMDA or dlclose(3) a DSO PMDA. dbpmda does not\n"
"exit, allowing another PMDA to be opened.\n");
		break;
	    case DESC:
		puts(
"Print out the meta data description for the 'metric'.  The metric may be\n"
"specified by name, or as a PMID of the form N, N.N or N.N.N.\n");
		break;
	    case FETCH:
		puts(
"Fetch metrics from the PMDA.  The metrics may be specified as a list of\n"
"metric names, or PMIDs of the form N, N.N or N.N.N.\n");
		break;
	    case GETDESC:
		puts(
"Before doing a fetch, get the descriptor so that the result of a fetch\n"
"can be printed out correctly.\n");
		break;
	    case NAMESPACE:
		puts(
"Unload the current Name Space and load up the given Name Space.\n"
"If unsuccessful then will try to reload the previous Name Space.\n");
		break;
	    case INSTANCE:
		puts(
"List the instances in 'indom'.  The list may be restricted to a specific\n"
"instance 'name' or 'number'.\n");
		break;
	    case PROFILE:
		puts(
"For the instance domain specified, the profile may be changed to include\n"
"'all' instances, no instances, add an instance or delete an instance.\n");
		break;
	    case WATCH:
		puts(
"A xwsh window is opened which tails the specified log file.  This window\n"
"must be closed by the user when no longer required.\n");
		break;
	    case DBG:
		puts(
"Specify which debugging flags should be active (see pmdbg(1)).  Flags may\n"
"be specified as integers or by name, with multiple flags separated by\n"
"white space.  All flags may be selected or deselected if 'all' or 'none' is\n"
"specified.  The current setting is displayed by the status command.\n\n");
		break;
	    case QUIT:
		puts("Exit dbpmda.  This also closes any open PMDAs.\n");
		break;
	    case STATUS:
		puts(
"Display the state of dbpmda, including which PMDA is connected, which\n"
"pmDebug flags are set, and the current profile.\n");
		break;
	    case STORE:
		puts(
"Store the value (int, real or string) into the 'metric'.  The metric may be\n"
"specified by name or as a PMID with the format N, N.N, N.N.N.  The value to\n"
"be stored must be enclosed in quotes.  Unlike the other commands, a store\n"
"must request a metric description and fetch the metric to determine how to\n"
"interpret the value, and to allocate the PDU for transmitting the value,\n"
"respectively.  The current profile will be used.\n");
		break;
	    case INFO:
		puts(
"Retrieve the help text for the 'metric' or 'indom' from the PMDA.  The one\n"
"line message is shown between '[' and ']' with the long message on the next\n"
"line.  To get the help text for an instance domain requires the word\n"
"``indom'' before the indom number\n");
		break;
	    case TIMER:
		puts(
"Report the response time of the PMDA when sending and receiving PDUs.\n");
		break;
	    case WAIT:
		puts("Sleep for this number of seconds\n");
		break;
	    }
	}
    }
}

void
dostatus(void)
{
    int		i = 0;
    __pmIPC	*ipc;
	
    putchar('\n');
    printf("Namespace:              ");
    if (cmd_namespace != NULL)
        printf("%s\n", cmd_namespace);
    else {
        if (pmnsfile == NULL)
            printf("(default)\n");
	else
            printf("%s\n", pmnsfile);
    }


    if (pmdaName == NULL || connmode == PDU_NOT)
	printf("PMDA:                   none\n");
    else {
	printf("PMDA:                   %s\n", pmdaName);
	printf("Connection:             ");
	switch (connmode) {
	case PDU_DSO:
	    printf("dso\n");
	    printf("DSO Interface Version:  %d\n", dispatch.comm.pmda_interface);
	    printf("PMDA PMAPI Version:     %d\n", dispatch.comm.pmapi_version);
	    break;
	case PDU_BINARY:
	    printf("daemon\n");
	    printf("PMDA PMAPI Version:     ");
	    if (__pmFdLookupIPC(infd, &ipc) < 0)
		printf("unknown!\n");
	    else
		printf("%d\n", ipc->version);
	    break;
	default:
	    printf("unknown!\n");
	    break;
	}
    }

    printf("pmDebug:                ");
    if (pmDebug == (unsigned int) -1)
	printf("-1 (all)\n");
    else if (pmDebug == 0)
	printf("0 (none)\n");
    else {
	printf("%u", pmDebug);
	for (i = 0; i < numFlags; i++)
	    if (pmDebug & (1 << i)) {
		printf(" ( %s", debugFlags[i]);
		break;
	    }
	for (i++; i < numFlags; i++)
	    if (pmDebug & (1 << i))
		printf(" + %s", debugFlags[i]);
	printf(" )\n");
    }

    printf("Timer:                  ");
    if (timer == 0)
	printf("off\n");
    else
	printf("on\n");

    printf("Getdesc:                ");
    if (get_desc == 0)
	printf("off\n");
    else
	printf("on\n");

    putchar('\n');
    __pmDumpProfile(stdout, PM_INDOM_NULL, profile);
    putchar('\n');
}


/*
 * Modified version of __pmDumpResult to use a descriptor list
 * instead of calling pmLookupDesc.
 * Notes:
 *   - desc_list should not be NULL
 */

void
_dbDumpResult(FILE *f, pmResult *resp, pmDesc *desc_list)
{
    int		i;
    int		j;
    int		n;
    char	*p;

    fprintf(f,"pmResult dump from 0x%p timestamp: %d.%06d ",
        resp, (int)resp->timestamp.tv_sec, (int)resp->timestamp.tv_usec);
    __pmPrintStamp(f, &resp->timestamp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];
	n = pmNameID(vsp->pmid, &p);
	if (n < 0)
	    fprintf(f,"  %s (%s):", pmIDStr(vsp->pmid), "<noname>");
	else {
	    fprintf(f,"  %s (%s):", pmIDStr(vsp->pmid), p);
	    free(p);
	}
	if (vsp->numval == 0) {
	    fprintf(f, " No values returned!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    fprintf(f, " %s\n", pmErrStr(vsp->numval));
	    continue;
	}
	fprintf(f, " numval: %d", vsp->numval);
	fprintf(f, " valfmt: %d vlist[]:\n", vsp->valfmt);
	for (j = 0; j < vsp->numval; j++) {
	    pmValue	*vp = &vsp->vlist[j];
	    if (vsp->numval > 1 || desc_list[i].indom != PM_INDOM_NULL) {
		fprintf(f,"    inst [%d", vp->inst);
		fprintf(f, " or ???]");
		fputc(' ', f);
	    }
	    else
		fprintf(f, "   ");
	    fprintf(f, "value ");
	    pmPrintValue(f, vsp->valfmt, desc_list[i].type, vp, 1);
	    fputc('\n', f);
	}/*for*/
    }/*for*/
}/*_dbDumpResult*/
