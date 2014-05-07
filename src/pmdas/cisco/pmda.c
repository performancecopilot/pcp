/*
 * Copyright (c) 2012,2014 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * Cisco PMDA, based on generic driver for a daemon-based PMDA
 *  - cisco interfaces to monitor are named in the command line as
 *         hostname:tX[@username]
 *         hostname:tX[?passwd]
 *         hostname:tX[@username?passwd]
 *         hostname:tX[!prompt]
 *         hostname:tX[@username!prompt]
 *         hostname:tX[?passwd!prompt]
 *         hostname:tX[@username?passwd!prompt]
 *
 *    where t identifies an interface type as defined by intf_tab[] in
 *    interface.c
 *    and X is either the ordinal i/f number (base 0, for Series 4000)
 *    or the card/port number (Series 7000 routers)
 *    e.g sydcisco.sydney:s0 (Frame-Relay to Mtn View)
 *	  sydcisco.sydney:s1 (ISDN to Melbourne)
 *        cisco.melbourne:s0 (ISDN to Sydney)
 *        b8u-cisco1-e15.corp:e0 (Ethernet in Bldg 8 upper)
 *        b9u-cisco1-81.engr.sgi.com:f2/0
 *        wanbris.brisbane:B0 (BRI ISDN)
 *    If specified the username (after the @ delimiter) and/or the
 *    user-level password (after the ? delimiter) and/or the prompt (after
 *    the ! delimiter) over-rides the global username and/or user-level
 *    password and/or prompt, as specified via -U and/or -P and/or -s
 *    options and applies for all occurrences of the hostname.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include "./cisco.h"

pmdaInstid	*_router;
cisco_t		*cisco;
int		n_cisco;
intf_t		*intf;
int		n_intf;
int		refreshdelay = 120;	/* default poll every two minutes */
char		*pmdausername;		/* username for the pmda */
char		*username;		/* username */
char		*passwd;		/* user-level password */
char		*prompt = ">";		/* command prompt */
int		port = 23;
int		parse_only;
int		no_lookups;

extern void	cisco_init(pmdaInterface *);
extern void	cisco_done(void);

int
main(int argc, char **argv)
{
    int			err = 0;
    int			sep = __pmPathSeparator();
    char		*endnum;
    pmdaInterface	dispatch;
    int			n;
    int			i;
    int			c;
    char		helptext[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&pmdausername);

    snprintf(helptext, sizeof(helptext), "%s%c" "cisco" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, CISCO,
		"cisco.log", helptext);

    while ((c = pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:6:" "CM:Nn:P:r:s:U:x:?", 
			   &dispatch, &err)) != EOF) {
	switch (c) {

	    case 'C':		/* parser checking mode (debugging) */
		pmDebug = DBG_TRACE_APPL0;
		parse_only++;
		break;

	    case 'N':		/* do not perform name lookups (debugging) */
		no_lookups = 1;
		break;

	    case 'n':		/* set program name, for parse (debugging) */
		pmProgname = optarg;
		break;

	    case 'P':		/* passwd */
		passwd = optarg;
		break;

	    case 'r':
		refreshdelay = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -r requires numeric (number of seconds) argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case 's':		/* command prompt */
		prompt = optarg;
		break;

	    case 'M':		/* username (for the PMDA) */
		pmdausername = optarg;
		break;

	    case 'U':		/* username (for the Cisco) */
		username = optarg;
		break;

	    case 'x':
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -x requires numeric argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case '?':
		err++;
	}
    }

    n_intf = argc - optind;
    if (n_intf == 0 || err) {
	fprintf(stderr, 
	    "Usage: %s [options] host:{a|B|E|e|f|h|s}N[/M[.I]] [...]\n\n", 
	    pmProgname);
	fputs("Options:\n"
	      "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	      "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	      "  -l logfile   redirect diagnostics and trace output to logfile\n"
	      "  -M username  user account to run PMDA under (default \"pcp\")\n"
	      "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	      "  -P password  default user-level Cisco password\n"
	      "  -r refresh   update metrics every refresh seconds\n"
	      "  -s prompt    Cisco command prompt [default >]\n"
	      "  -u socket    expect PMCD to connect on given unix domain socket\n"
	      "  -U username  Cisco username\n"
	      "  -x port      telnet port [default 23]\n"
	      "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	      stderr);		
	exit(1);
    }

    /* force errors from here on into the log */
    if (!parse_only) {
	pmdaOpenLog(&dispatch);
	__pmSetProcessIdentity(pmdausername);
    } else {
	dispatch.version.two.text = NULL;
	dispatch.version.two.ext->e_helptext = NULL;
    }

    /*
     * build the instance domain and cisco data structures from the
     * command line arguments.
     */
    if ((_router = (pmdaInstid *)malloc(n_intf * sizeof(pmdaInstid))) == NULL) {
        __pmNoMem("main.router", n_intf * sizeof(pmdaInstid), PM_FATAL_ERR);
    }
    if ((intf = (intf_t *)malloc(n_intf * sizeof(intf_t))) == NULL) {
        __pmNoMem("main.intf", n_intf * sizeof(intf_t), PM_FATAL_ERR);
    }
    /* pre-allocated cisco[] to avoid realloc and ptr movement */
    if ((cisco = (cisco_t *)malloc(n_intf * sizeof(cisco_t))) == NULL) {
	__pmNoMem("main.cisco", n_intf * sizeof(cisco_t), PM_FATAL_ERR);
    }

    indomtab[CISCO_INDOM].it_numinst = n_intf;
    indomtab[CISCO_INDOM].it_set = _router;

    for (n = 0 ; optind < argc; optind++, n++) {
        char    *p = strdup(argv[optind]);
	char	*q;
	char	*myusername;
	char	*mypasswd;
	char	*myprompt;

	myprompt = strchr(p, '!');
	if (myprompt) {
	    /* save prompt for later */
	    *myprompt++ = '\0';
	}
	else
	    myprompt = NULL;
	mypasswd = strchr(p, '?');
	if (mypasswd) {
	    /* save user-level password for later */
	    *mypasswd++ = '\0';
	}
	else
	    mypasswd = passwd;
	myusername = strchr(p, '@');
	if (myusername) {
	    /* save username for later */
	    *myusername++ = '\0';
	}
	else
	    myusername = username;

        _router[n].i_inst = n;
        _router[n].i_name = strdup(p);

	if ((q = strchr(p, ':')) == NULL)
	    goto badintfspec;
	*q++ = '\0';
	for (i = 0; i < num_intf_tab; i++) {
	    if (strncmp(q, intf_tab[i].type, strlen(intf_tab[i].type)) == 0)
		break;
	}
	if (i == num_intf_tab)
            goto badintfspec;
	if (strcmp(intf_tab[i].type, "E") == 0) {
	    /*
	     * Cisco parser is case insensitive, so 'E' means "Ethernet"
	     * and 'F' means "Fddi", need to use "FastEthernet" here
	     */
	    q++;
	    intf[n].interface = (char *)malloc(strlen("FastEthernet")+strlen(q)+1);
	    if ((intf[n].interface = (char *)malloc(strlen("FastEthernet")+strlen(q)+1)) == NULL) {
		__pmNoMem("main.cisco", strlen("FastEthernet")+strlen(q)+1, PM_FATAL_ERR);
	    }
	    strcpy(intf[n].interface, "FastEthernet");
	    strcat(intf[n].interface, q);
	}
	else
	    intf[n].interface = q;

	for (i = 0; i < n_cisco; i++) {
	    if (strcmp(p, cisco[i].host) == 0)
		break;
	}
	if (i == n_cisco) {
	    __pmHostEnt	*hostInfo = NULL;

	    if (!no_lookups)
		hostInfo = __pmGetAddrInfo(p);

	    if (!hostInfo && parse_only) {
		FILE	*f;

		/*
		 * for debugging, "host" may be a file ...
		 */
		if ((f = fopen(p, "r")) == NULL) {
		    fprintf(stderr, "%s: unknown hostname or filename %s: %s\n",
			pmProgname, argv[optind], hoststrerror());
		    /* abandon this host (cisco) */
		    continue;
		}
		else {
		    fprintf(stderr, "%s: assuming file %s contains output from \"show int\" command\n",
			pmProgname, p);

		    cisco[i].host = p;
		    cisco[i].username = myusername != NULL ? myusername : username;
		    cisco[i].passwd = mypasswd != NULL ? mypasswd : passwd;
		    cisco[i].prompt = myprompt != NULL ? myprompt : prompt;
		    cisco[i].fin = f;
		    cisco[i].fout = stdout;
		    n_cisco++;
		}
	    } else if (!hostInfo) {
		fprintf(stderr, "%s: unknown hostname %s: %s\n",
			pmProgname, p, hoststrerror());
		/* abandon this host (cisco) */
		continue;
	    } else {
		cisco[i].host = p;
		cisco[i].username = myusername != NULL ? myusername : username;
		cisco[i].passwd = mypasswd != NULL ? mypasswd : passwd;
		cisco[i].prompt = myprompt != NULL ? myprompt : prompt;
		cisco[i].fin = NULL;
		cisco[i].fout = NULL;
		cisco[i].hostinfo = hostInfo;
		cisco[i].port = port;

		n_cisco++;
		fprintf(stderr, "Adding new host %s\n", p);
		fflush(stderr);
	    }
	}
	else {
	    if (cisco[i].username == NULL) {
		if (myusername != NULL)
		    /* username on 2nd or later interface ... applies to all */
		    cisco[i].username = myusername;
	    }
	    else {
		if (myusername != NULL) {
		    if (strcmp(cisco[i].username, myusername) != 0) {
			fprintf(stderr, 
				"%s: conflicting usernames (\"%s\" "
				"and \"%s\") for cisco \"%s\"\n",
				pmProgname, cisco[i].username, myusername, 
				cisco[i].host);
			exit(1);
		    }
		}
	    }
	    if (cisco[i].passwd == NULL) {
		if (mypasswd != NULL)
		    /* passwd on 2nd or later interface ... applies to all */
		    cisco[i].passwd = mypasswd;
	    }
	    else {
		if (mypasswd != NULL) {
		    if (strcmp(cisco[i].passwd, mypasswd) != 0) {
			fprintf(stderr, 
				"%s: conflicting user-level passwords (\"%s\" "
				"and \"%s\") for cisco \"%s\"\n",
				pmProgname, cisco[i].passwd, mypasswd, 
				cisco[i].host);
			exit(1);
		    }
		}
	    }
	    if (cisco[i].prompt == NULL) {
		if (myprompt != NULL)
		    /* prompt on 2nd or later interface ... applies to all */
		    cisco[i].prompt = myprompt;
	    }
	    else {
		if (myprompt != NULL) {
		    if (strcmp(cisco[i].prompt, myprompt) != 0) {
			fprintf(stderr, 
				"%s: conflicting user-level prompts (\"%s\" "
				"and \"%s\") for cisco \"%s\"\n",
				pmProgname, cisco[i].prompt, myprompt, 
				cisco[i].host);
			exit(1);
		    }
		}
	    }
	}

	intf[n].cp = cisco+i;
	/*
	 * special one-trip initialization for Frame-Relay over serial
	 * lines ... see grab_cisco()
	 */
	intf[n].bandwidth = -2;

	fprintf(stderr, "Interface %s(%d) is on host %s\n",
		 intf[n].interface, n, cisco[i].host);
	fflush(stderr);

        continue;

badintfspec:
        fprintf(stderr, "%s: bad interface specification \"%s\"\n", pmProgname, argv[optind]);
        fprintf(stderr, "      should be like sydcisco.sydney:s1 or b9u-cisco1-81.engr.sgi.com:f2/0\n");
        fprintf(stderr, "      or cisco.melbourne:e0?secret\n");
        exit(1);
    }

    if (n_cisco == 0) {
	fprintf(stderr, "%s: Nothing to monitor\n", pmProgname);
	exit(1);
    }

    if (parse_only) {
	fprintf(stderr, "Sleeping while sproc does the work ... SIGINT to terminate\n");
        cisco_init(&dispatch);
	for (i = 0; i < n_intf; i++)
	    intf[i].fetched = 0;
	pause();
    } else {
	/* set up connection to PMCD */
	cisco_init(&dispatch);
	pmdaConnect(&dispatch);
	pmdaMain(&dispatch);
    }

    cisco_done();
    exit(0);
}
