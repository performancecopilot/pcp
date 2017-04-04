/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1998-2001, Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include <sys/param.h>
#include "pmapi.h"
#include "impl.h"
#include "rules.h"

#define MAXSYMLEN	(MAXPATHLEN+1)
#define MAXVARLEN	(512+1)
#define MAXBUFLEN	(1024+1)

static int	verbose;
static int	autocreate;
static int	interactive = 1;
static int	pmiefile_modified;
static char	warn[MAXBUFLEN];	/* buffer for any warning messages */

static char	help[] = \
    "  1. help  [ { . | all | global | <rule> | <group> } [<variable>] ]\n"
    "               descriptive text on one or more variables or rules\n"
    "  2. rules  [ enabled | disabled ]\n"
    "               list of available, enabled, or disabled rules\n"
    "  3. groups    list of available groups of rules\n"
    "  4. status    print status information\n"
    "  5. enable  { . | all | <rule> | <group> }\n"
    "               switch evaluation on for given rule(s)\n"
    "  6. disable  { . | all | <rule> | <group> }\n"
    "               switch evaluation off for given rule(s)\n"
    "  7. list  { . | all | global | <rule> | <group> } [<variable>]\n"
    "               print values of variables for given rule(s)\n"
    "  8. modify  { . | all | global | <rule> | <group> } <variable> <value>\n"
    "               change the value of a rule variable\n"
    "  9. undo  { . | all | global | <rule> | <group> } [<variable>]\n"
    "               revert to default value of rule variable\n"
    " 10. verbose  [ { on | off } ]\n"
    "               change amount of info displayed ('*' denotes global variables)\n"
    " 11. quit      save changes then exit\n"
    " 12. abort     discard changes then exit";

#define MAXARGS	4
static char	inbuf[MAXARGS][MAXBUFLEN+1];	/* input buffer */
static char	previous[MAXVARLEN+1];		/* buffer for last rule name */

static symbol_t commands[] = {
#define COMMAND_HELP	1
    { COMMAND_HELP, "help" }, { COMMAND_HELP, "h" }, { COMMAND_HELP, "?" },
#define COMMAND_RULES	2
    { COMMAND_RULES, "rules" }, { COMMAND_RULES, "r" },
#define COMMAND_GROUPS	3
    { COMMAND_GROUPS, "groups" }, { COMMAND_GROUPS, "g" },
#define COMMAND_STATUS	4
    { COMMAND_STATUS, "status" }, { COMMAND_STATUS, "s" },
#define COMMAND_ENABLE	5
    { COMMAND_ENABLE, "enable" }, { COMMAND_ENABLE, "e" },
#define COMMAND_DISABLE	6
    { COMMAND_DISABLE, "disable" }, { COMMAND_DISABLE, "d" },
#define COMMAND_LIST	7
    { COMMAND_LIST, "list" }, { COMMAND_LIST, "l" },
#define COMMAND_MODIFY	8
    { COMMAND_MODIFY, "modify" }, { COMMAND_MODIFY, "m" },
#define COMMAND_UNDO	9
    { COMMAND_UNDO, "undo" }, { COMMAND_UNDO, "u" },
#define COMMAND_VERBOSE	10
    { COMMAND_VERBOSE, "verbose" }, { COMMAND_VERBOSE, "v" },
#define COMMAND_QUIT	11
    { COMMAND_QUIT, "quit" }, { COMMAND_QUIT, "q" },
#define COMMAND_ABORT	12
    { COMMAND_ABORT, "abort" }, { COMMAND_ABORT, "a" },
#define LAST_COMMAND	12
};
static int	ncommands = (sizeof(commands)/sizeof(commands[0]));

/* io-related stuff */
extern void setio(int);
extern void setscroll(void);
extern void onwinch(int);
extern int pprintf(char *, ...);
extern void error(char *, ...);


/*
 *   ####  simple printing routines  ###
 */

static void
print_helpstring(char *value)
{
    char	*s;
    char	*str;
    int		i = 0;

    if (value == NULL) {
	pprintf("  help: No help available.\n");
	return;
    }
    if ((str = strdup(value)) == NULL) {
	error("insufficient memory to display help");
	exit(1);
    }
    s = strtok(str, "\n");
    while (s != NULL) {
	pprintf("%s%s\n", i++ == 0? "  help: ":"\t", s);
	s = strtok(NULL, "\n");
    }
    free(str);
}

static void
print_predicatestring(char *value)
{
    char	*s;
    char	*str;

    if ((str = strdup(value)) == NULL) {
	error("insufficient memory to display predicate");
	exit(1);
    }
    s = strtok(str, "\n");
    pprintf("\tpredicate = \n");
    while (s != NULL) {
	pprintf("\t  %s\n", s);
	s = strtok(NULL, "\n");
    }
    free(str);
}

/* prints help string for a parameter of a rule */
void
print_help(rule_t *rule, char *attrib)
{
    atom_t	*aptr;

    for (aptr = &rule->self; aptr != NULL; aptr = aptr->next)
	if (strcmp(get_aname(rule, aptr), attrib) == 0) {
	    if (aptr->help == NULL)
		goto nohelp;
	    print_helpstring(aptr->help);
	    return;
	}
    for (aptr = &globals->self; aptr != NULL; aptr = aptr->next)
	if (strcmp(get_aname(globals, aptr), attrib) == 0) {
	    if (aptr->help == NULL)
		goto nohelp;
	    print_helpstring(aptr->help);
	    return;
	}
nohelp:
    error("no help available for variable \"%s\" of rule %s",
	attrib, rule->self.name);
}

/* prints a named parameter from a rule, return -1 on failure */
int
print_attribute(rule_t *rule, char *attrib, int dohelp)
{
    atom_t	*aptr;
    int		isattrib = is_attribute(attrib);

    if (isattrib == ATTRIB_PREDICATE && rule != globals)
	print_predicatestring(rule->predicate);
    else if (isattrib == ATTRIB_HELP)
	print_helpstring(rule->self.help);
    else if (isattrib != -1 && rule != globals) {
	if (dohelp)
	    pprintf("   var: %s\n  help: No help available.\n", attrib);
	else
	    pprintf("\t%s = %s\n", attrib, get_attribute(attrib, &rule->self));
    }
    else {
	for (aptr = rule->self.next; aptr != NULL; aptr = aptr->next)
	    if (strcmp(get_aname(rule, aptr), attrib) == 0) {
		if (dohelp) {
		    pprintf("   var: %s\n", attrib);
		    print_helpstring(aptr->help);
		}
		else
		    pprintf("\t%s = %s\n", attrib, value_string(aptr, 1));
		return 0;
	    }
	for (aptr = globals->self.next; aptr != NULL; aptr = aptr->next)
	    if (strcmp(get_aname(globals, aptr), attrib) == 0) {
		if (dohelp) {
		    pprintf("   var: %s\n", attrib);
		    print_helpstring(aptr->help);
		}
		else
		    pprintf("\t%s = %s\n", attrib, value_string(aptr, 1));
		return 0;
	    }
	error("variable \"%s\" is undefined for rule %s",
		attrib, rule->self.name);
	return -1;
    }
    return 0;
}


/* one line summary (name and short help) for a given rule */
void
print_rule_summary(rule_t *rule, char *prefix)
{
    char	*str;
    char	fmt[] = "%s%s  [%s]\n";

    if ((str = dollar_expand(rule, rule->self.data, 0)) == NULL)
	return;
    pprintf(fmt, prefix, rule->self.name, str);
    free(str);
}


void
print_rule(rule_t *rule)
{
    atom_t		*a;
    int			needvars = 1;

    print_rule_summary(rule, "  rule: ");
    if (rule->self.help != NULL)
	print_helpstring(rule->self.help);
    if (rule != globals) {	/* non-global */
	print_predicatestring(rule->predicate);
	pprintf("  vars: enabled = %s\n", rule->self.enabled?"yes":"no");
    }
    for (a = rule->self.next; a != NULL; a = a->next) {
	if (!a->display)
	    continue;
	pprintf("%s%s = %s\n", (rule == globals && needvars)?
	       "  vars: ":"\t", get_aname(rule, a), value_string(a, 1));
	needvars = 0;
    }
    if (verbose && rule != globals) {
	for (a = globals->self.next; a != NULL; a = a->next) {
	    if (is_overridden(rule, a))
		continue;	/* printed already as part of attribs */
	    pprintf("\t%s = %s (*)\n", get_aname(globals,a), value_string(a,1));
	}
    }
}


/* print out the list of unique group names (sort done previously) */
static int
print_grouplist(int argcount)
{
    int		i;
    char	*j;
    char	lastgroup[MAXVARLEN];

    if (argcount != 0) {
	error("too many arguments for \"groups\" command");
	return -1;
    }
    lastgroup[0] = '\0';
    for (i = 1; i < rulecount; i++) {
	if ((j = strchr(rulelist[i].self.name, '.')) != NULL) {
	    *j = '\0';	/* mark end of group name */
	    if (strcmp(rulelist[i].self.name, lastgroup) != 0) {
		strcpy(lastgroup, rulelist[i].self.name);
		pprintf("  %s\n", lastgroup);
	    }
	    *j = '.';	/* repair the rule name */
	}
    }
    return 0;
}

/*
 * print out the current verbosity setting, running pmies using this
 * pmie file, total number of rules & number of rules switched on.
 */
static int
print_status(int argcount)
{
    int		i, count = 0, pcount = 0;
    char	**processes;

    if (argcount != 0) {
	error("too many arguments for \"status\" command");
	return -1;
    }

    for (i = 1; i < rulecount; i++)		/* find enabled rules */
	if (rulelist[i].self.enabled)
	    count++;
    lookup_processes(&pcount, &processes);	/* find running pmies */
    printf("  verbose:  %s\n"
	   "  enabled rules:  %u of %u\n"
	   "  pmie configuration file:  %s\n"
	   "  pmie %s using this file: ",
		verbose? "on" : "off", count, rulecount-1, get_pmiefile(),
		pcount == 1? "process (PID)" : "processes (PIDs)");
    if (pcount == 0)
	printf(" (none found)");
    else {
	for (i = 0; i < pcount; i++) {
	    printf(" %s", processes[i]);
	    free(processes[i]);
	}
	free(processes);
    }
    printf("\n");
    return 0;
}

int
write_pmie(void)
{
    int		i, count;
    char	*msg;
    dep_t	*list;

    if ((msg = write_pmiefile(pmProgname, autocreate)) != NULL) {
	error(msg);
	return 1;
    }
    if ((count = fetch_deprecated(&list)) > 0) {
	if (interactive)
	    pprintf("  Warning - some rules have been deprecated:\n");
	else
	    pprintf("%s: some rules have been deprecated:\n", pmProgname);
	for (i = 0; i < count; i++) {
	    pprintf("    %s (deprecated, %s)\n", list[i].name, list[i].reason);
	    free(list[i].name);
	}
	free(list);
	pprintf("\n  See %s for details\n", get_pmiefile());
    }
    return 0;
}

/* display the list of available, enabled or disabled rules */
static int
command_rules(int argcount)
{
    int	i;

    if (argcount > 1) {
	error("too many arguments for \"rules\" command");
	return -1;
    }
    if (argcount == 0)
	for (i = 1; i < rulecount; i++)
	    print_rule_summary(&rulelist[i], "  ");
    else {
	if (strcmp(inbuf[1], "enabled") == 0) {
	    for (i = 1; i < rulecount; i++)
		if (rulelist[i].self.enabled)
		    print_rule_summary(&rulelist[i], "  ");
	}
	else if (strcmp(inbuf[1], "disabled") == 0) {
	    for (i = 1; i < rulecount; i++)
		if (!rulelist[i].self.enabled)
		    print_rule_summary(&rulelist[i], "  ");
	}
	else {
	    error("invalid argument for \"rules\" command");
	    return -1;
	}
    }
    return 0;
}

/* display or set the verbosity level */
static int
command_verbose(int argcount)
{
    int sts;

    if (argcount < 1) {
	printf("  verbose:  %s\n", verbose? "on" : "off");
	return 0;
    }
    else if (argcount > 1) {
	error("too many arguments for \"verbose\" command");
	return -1;
    }
    if (strcmp(inbuf[1], "on") == 0)
	sts = 1;
    else if (strcmp(inbuf[1], "off") == 0)
	sts = 0;
    else {
	error("invalid argument, expected \"on\" or \"off\"");
	return -1;
    }
    verbose = sts;
    return 0;
}

static int
command_list(int argcount)
{
    unsigned int	rcount;
    rule_t		**rptr;
    char		*msg;
    int			all = 0;
    int			sts = 0;
    int			i;

    if (argcount < 1) {
	error("too few arguments for \"list\" command");
	return -1;
    }
    else if (argcount > 2) {
	error("too many arguments for \"list\" command");
	return -1;
    }

    if (strcmp(".", inbuf[1]) == 0)
	strcpy(inbuf[1], previous);
    if (strcmp("all", inbuf[1]) == 0)
	all = 1;
    if ((msg = lookup_rules(inbuf[1], &rptr, &rcount, all)) != NULL) {
	error(msg);
	return -1;
    }
    if (argcount == 1) {	/* print out one rule or one group */
	for (i = 0; i < rcount; i++) {
	    if (i > 0) pprintf("\n");
	    print_rule(rptr[i]);
	}
    }
    else {	/* print out one rule/group variable */
	for (i = 0; i < rcount; i++) {
	    print_rule_summary(rptr[i], "  rule: ");
	    if (print_attribute(rptr[i], inbuf[2], 0) == -1)
		sts = -1;	/* failure */
	}
    }
    free(rptr);
    strcpy(previous, inbuf[1]);
    return sts;
}

static int
command_undo(int argcount)
{
    unsigned int	rcount;
    rule_t		**rptr;
    char		*msg = NULL;
    char		*var = NULL;
    int			all = 0;
    int			i;

    if (argcount < 1) {
	error("too few arguments for \"undo\" command");
	return -1;
    }
    else if (argcount > 2) {
	error("too many arguments for \"undo\" command");
	return -1;
    }

    if (strcmp(".", inbuf[1]) == 0)
	strcpy(inbuf[1], previous);
    if (strcmp("all", inbuf[1]) == 0)
	all = 1;
    if ((msg = lookup_rules(inbuf[1], &rptr, &rcount, all)) != NULL) {
	error(msg);
	return -1;
    }
    if (argcount == 2)
	var = inbuf[2];

    for (i = 0; i < rcount; i++)
	if ((msg = rule_defaults(rptr[i], var)) != NULL)
	    break;
    free(rptr);
    if (msg != NULL) {
	error(msg);
	return -1;
    }
    strcpy(previous, inbuf[1]);
    pmiefile_modified = 1;
    return 0;
}

static int
command_modify(int command, int argcount)
{
    unsigned int	rcount;
    rule_t		**rptr;
    char		*msg;
    int			all = 0;
    int			c;

    if (command == COMMAND_MODIFY) {
	if (argcount != 3) {
	    error("too %s arguments for \"modify\" command",
		    argcount < 3? "few":"many");
	    return -1;
	}
    }
    else if (strcmp(inbuf[1], "global") == 0) {
	error("invalid argument - \"global\"");
	return -1;
    }
    else if (command == COMMAND_ENABLE) {
	if (argcount != 1) {
	    error("too %s arguments for \"enable\" command",
		argcount < 1? "few":"many");
	    return -1;
	}
	strcpy(inbuf[2], "enabled");
	strcpy(inbuf[3], "yes");
    }
    else { /* (command == COMMAND_DISABLE) */
	if (argcount != 1) {
	    error("too %s arguments for \"disable\" command",
		argcount < 1? "few":"many");
	    return -1;
	}
	strcpy(inbuf[2], "enabled");
	strcpy(inbuf[3], "no");
    }

    if (strcmp(".", inbuf[1]) == 0)
	strcpy(inbuf[1], previous);
    if (strcmp("all", inbuf[1]) == 0)
	all = 1;
    if ( ((c = is_attribute(inbuf[2])) != -1) && c != ATTRIB_ENABLED ) {
	error("no change - variable \"%s\" is always readonly", inbuf[2]);
	return -1;
    }
    if ((msg = lookup_rules(inbuf[1], &rptr, &rcount, all)) != NULL) {
	error(msg);
	return -1;
    }

    for (c = 0; c < rcount; c++) {
	if ((msg = value_change(rptr[c], inbuf[2], inbuf[3])) != NULL) {
	    error("change aborted - %s", msg);
	    free(rptr);
	    return -1;
	}
    }
    pmiefile_modified = 1;
    free(rptr);
    strcpy(previous, inbuf[1]);
    return 0;
}

static int
command_help(int argcount)
{
    unsigned int	rcount;
    rule_t		**rptr;
    char		*msg;
    int			sts = 0;
    int			all = 0;
    int			i;

    if (argcount < 1) {
	puts(help);
	return 0;
    }
    else if (argcount > 2) {
	error("too many arguments for \"help\" command");
	return -1;
    }

    if (strcmp(".", inbuf[1]) == 0)
	strcpy(inbuf[1], previous);
    if (strcmp("all", inbuf[1]) == 0)
	all = 1;
    if ((msg = lookup_rules(inbuf[1], &rptr, &rcount, all)) != NULL) {
	error(msg);
	return -1;
    }

    if (argcount == 1) {
	for (i = 0; i < rcount; i++) {
	    print_rule_summary(rptr[i], "  rule: ");
	    print_helpstring(rptr[i]->self.help);
	}
    }
    else {
	for (i = 0; i < rcount; i++) {
	    print_rule_summary(rptr[i], "  rule: ");
	    if (print_attribute(rptr[i], inbuf[2], 1) == -1)
		sts = -1;	/* failure */
	}
    }
    free(rptr);
    strcpy(previous, inbuf[1]);
    return sts;
}

static void
command_quit(void)
{
    static int	done = 0;
    int		i, pcount = 0;
    char	**processes;
    char	*msg;

    /* must only come thru here once, but can be called multiple times */
    if (done != 0)
	return;
    done = 1;

    if (pmiefile_modified && write_pmie() != 0)
	exit(1);

    /* show any running pmie processes which use this pmie config */
    if (interactive && pmiefile_modified) {
	if ((msg = lookup_processes(&pcount, &processes)) != NULL)
	    error(msg);
	else if (pcount > 0) {
	    pprintf("  %s is in use by %d running pmie process%s:\n\t",
		    get_pmiefile(), pcount, pcount == 1? "":"es");
	    for (i = 0; i < pcount; i++)
		pprintf("%s ", processes[i]);
	    pprintf("\n  Restart %s for the configuration change to take effect.",
		    pcount == 1 ?  "this process" : "these processes");
	    pprintf("\n  o  Use kill(1) to stop; e.g.\tkill -INT ");
	    for (i = 0; i < pcount; i++) {
		pprintf("%s ", processes[i]);
		free(processes[i]);
	    }
	    free(processes);
	    pprintf("\n\
  o  Refer to pmie_check(1) for a convenient mechanism for restarting pmie\n\
     daemons launched under the control of %s/pmie/control;\n\
     e.g.\t%s/pmie_check -V\n", pmGetConfig("PCP_SYSCONF_DIR"), pmGetConfig("PCP_BINADM_DIR"));
	}
    }
}

/*
 * workhorse routine - dishes out work depending on user command;
 * returns 1 on user-quit, 0 on success, -1 on failure
 */
static int
configure(int count)
{
    int	command = atoi(inbuf[0]);

    if (command <= 0 || command > LAST_COMMAND)
	command = map_symbol(commands, ncommands, inbuf[0]);

    switch(command) {
    case COMMAND_HELP:
	return command_help(count-1);
    case COMMAND_RULES:
	return command_rules(count-1);
    case COMMAND_GROUPS:
	return print_grouplist(count-1);
    case COMMAND_STATUS:
	return print_status(count-1);
    case COMMAND_VERBOSE:
	return command_verbose(count-1);
    case COMMAND_ENABLE:	/* shortcut for modify */
    case COMMAND_DISABLE:	/* shortcut for modify */
    case COMMAND_MODIFY:
	return command_modify(command, count-1);
    case COMMAND_LIST:
	return command_list(count-1);
    case COMMAND_UNDO:
	return command_undo(count-1);
    case COMMAND_QUIT:
	command_quit();
	return 1;
    case COMMAND_ABORT:
	exit(0);
    default:
	error("unrecognised command \"%s\" - try \"help\"", inbuf[0]);
	return -1;
    }
    /*NOTREACHED*/
}


static void
interact(void)
{
    int		done = 0;
    int		n, sts;

    if (interactive)
	printf("Updates will be made to %s\n", get_pmiefile());
    do {
	sts = 0;
	for (n = 0; n < MAXARGS; n++)
	    inbuf[n][0] = '\0';
	if (interactive) {
	    setio(0);
	    printf("\n%s> ", pmProgname);
	    fflush(stdout);
	}

	do {
	    if ((n = read_token(stdin, inbuf[sts], MAXBUFLEN, '\n')) == -1) {
		error("failed to parse argument %d correctly", sts+1);
		break;
	    }
	    else if (n > 0)
		sts++;
	    else {
		if (n == -2) {
		    command_quit();
		    done = 1;
		}
		break;
	    }
	} while (sts <= MAXARGS);

	if (n < 0)	/* done (EOF) or error reported above */
	    continue;
	else if (sts > MAXARGS) {
	    error("too many arguments - try \"help\"");
	    /* consume until '\n' reached... */
	    while ((n = read_token(stdin, inbuf[0], MAXBUFLEN, '\n')) > 0);
	    if (n == -2) {
		command_quit();
	    	done = 1;	/* reached EOF, bail out! */
	    }
	}
	else if (sts > 0 && !done)
	    done = (configure(sts) == 1);
    } while (!done);
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "", 0, 'c', 0, "an automated pmie configuration by the system" },
    { "force", 0, 'F', 0, "force creation/update of pmie file, then exit" },
    { "config", 1, 'f', "FILE", "location of generated pmie configuration file" },
    { "rules", 1, 'r', "PATH", "path specifying groups of rule files" },
    { "verbose", 0, 'v', 0, "increase level of diagnostics" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_NOFLUSH,
    .short_options = "cFf:r:v?",
    .long_options = longopts,
    .short_usage = "[options] [ command [args...] ]",
};

int
main(int argc, char **argv)
{
    int		c;
    int		force = 0;
    char	*p;
    char	*in_rules = NULL;
    char	*in_pmie = NULL;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'c':
	    autocreate = 1;
	    interactive = 0;
	    break;

	case 'F':
	    force = 1;
	    break;

	case 'f':
	    in_pmie = opts.optarg;
	    break;

	case 'r':
	    in_rules = opts.optarg;
	    break;

	case 'v':
	    verbose = 1;
	    break;

	case '?':
	default:
	    opts.errors++;
	}
    }

    if (force && opts.optind < argc) {
	pmprintf("%s: cannot use -F option with a command\n", pmProgname);
	opts.optind = argc;
	opts.errors++;
    }

    for (c = 0; opts.optind < argc && c < MAXARGS; c++) {
	strncpy(inbuf[c], argv[opts.optind++], MAXBUFLEN);
	inbuf[c][MAXBUFLEN] = '\0';
	interactive = 0;
    }
    if (opts.optind < argc) {
	pmprintf("%s: too many arguments\n", pmProgname);
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	pmprintf("\nCommands:\n%s\n", help);
	pmflush();
	exit(1);
    }

    if ((p = initialise(in_rules, in_pmie, warn, sizeof(warn))) != NULL) {
	error(p);
	exit(1);
    }
    sort_rules();

    if (rulecount <= 1) {
	fprintf(stderr, "%s: no rules were found using rule path: %s\n",
		pmProgname, get_rules());
	exit(1);
    }

    if (force || (*warn && p == NULL)) {	/* force/pmie doesn't exist */
	if (write_pmie() != 0)
	    exit(1);
	else if (force)
	    exit(0);
    }
    else if (*warn)		/* some other warning */
	error(warn);

    if (interactive) {
	if (!isatty(0))		/* reading commands from a file */
	    interactive = 0;
	else if (isatty(1)) {	/* be $PAGER, handle window-resize */
	    setscroll();
	    onwinch(0);
	}
	interact();
    }
    else if (configure(c) == -1)
	exit(1);
    command_quit();
    exit(0);
    /*NOTREACHED*/
}
