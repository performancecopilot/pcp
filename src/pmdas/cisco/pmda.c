/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 *
 * Cisco PMDA, based on generic driver for a daemon-based PMDA
 *  - cisco interfaces to monitor are named in the command line as
 *         hostname:tX[?passwd]
 *         hostname:tX[@username]
 *         hostname:tX[@username?passwd]
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
 *    user-level password (after the ? delimiter) over-rides the global
 *    username and/or user-level password, as specified via -U and/or -P
 *    options and applies for all occurrences of the hostname.
 */

#ident "$Id: pmda.c,v 2.30 2004/06/15 09:39:44 kenmcd Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "./cisco.h"

pmdaInstid	*_router;
cisco_t		*cisco = NULL;
int		n_cisco = 0;
intf_t		*intf = NULL;
int		n_intf = 0;
int		refreshdelay = 120;	/* default poll every two minutes */
char		*username = NULL;	/* username */
char		*passwd = NULL;		/* user-level password */
int		port = 23;

int
main(int argc, char **argv)
{
    int			err = 0;
#ifdef PARSE_ONLY
    extern int		pmDebug;
#endif
    char		*endnum;
    pmdaInterface	dispatch;
    int			n;
    int			i;
    int			c;
    extern char		*optarg;
    extern int		optind;
    extern void		cisco_init(pmdaInterface *);
    extern void		cisco_done(void);
    char		helptext[MAXPATHLEN];

    /* trim cmd name of leading directory components */
    pmProgname = basename(argv[0]);

#ifdef PARSE_ONLY
    pmDebug = DBG_TRACE_APPL0 | DBG_TRACE_APPL1;
#endif

    snprintf(helptext, sizeof(helptext), "%s/cisco/help", pmGetConfig("PCP_PMDAS_DIR"));
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, CISCO,
		"cisco.log", helptext);

    while ((c = pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:" "P:r:U:x:?", 
			   &dispatch, &err)) != EOF) {
	switch (c) {

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

	    case 'U':		/* username */
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
	      "  -l logfile   redirect diagnostics and trace output to logfile\n"
	      "  -P password  default user-level Cisco password\n"
	      "  -r refresh   update metrics every refresh seconds\n"
	      "  -U username  Cisco username\n"
	      "  -x port      telnet port [default 23]\n",
	      stderr);		
	exit(1);
    }

#ifdef PARSE_ONLY
#else
    /* force errors from here on into the log */
    pmdaOpenLog(&dispatch);
#endif

    /*
     * build the instance domain and cisco data structures from the
     * command line arguments.
     */
    if ((_router = (pmdaInstid *)malloc(n_intf * sizeof(pmdaInstid))) == NULL) {
        __pmNoMem("main.router", n_intf * sizeof(pmdaInstid), PM_FATAL_ERR);
        /*NOTREACHED*/
    }
    if ((intf = (intf_t *)malloc(n_intf * sizeof(intf_t))) == NULL) {
        __pmNoMem("main.intf", n_intf * sizeof(intf_t), PM_FATAL_ERR);
        /*NOTREACHED*/
    }
    /* pre-allocated cisco[] to avoid realloc and ptr movement */
    if ((cisco = (cisco_t *)malloc(n_intf * sizeof(cisco_t))) == NULL) {
	__pmNoMem("main.cisco", n_intf * sizeof(cisco_t), PM_FATAL_ERR);
	/*NOTREACHED*/
    }

#ifdef MALLOC_AUDIT
    _persistent_(_router);
    _persistent_(intf);
    _persistent_(cisco);
#endif

    indomtab[CISCO_INDOM].it_numinst = n_intf;
    indomtab[CISCO_INDOM].it_set = _router;

    for (n = 0 ; optind < argc; optind++, n++) {
        char    *p = strdup(argv[optind]);
	char	*q;
	char	*myusername;
	char	*mypasswd;

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
	     * and 'F' means "Fddi", need to use "Fast" here
	     */
	    q++;
	    intf[n].interface = (char *)malloc(strlen("Fast")+strlen(q)+1);
	    if ((intf[n].interface = (char *)malloc(strlen("Fast")+strlen(q)+1)) == NULL) {
		__pmNoMem("main.cisco", strlen("Fast")+strlen(q)+1, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    strcpy(intf[n].interface, "Fast");
	    strcat(intf[n].interface, q);
	}
	else
	    intf[n].interface = q;

	for (i = 0; i < n_cisco; i++) {
	    if (strcmp(p, cisco[i].host) == 0)
		break;
	}
	if (i == n_cisco) {
	    struct hostent *hostInfo;

	    if ((hostInfo = gethostbyname(p)) == NULL) {
		fprintf(stderr, "%s: unknown hostname %s: %s\n",
			pmProgname, p, hstrerror(h_errno));
		/* abandon this host (cisco) */
		continue;
	    }
	    else {
		struct sockaddr_in *sinp = & cisco[i].ipaddr;

		cisco[i].host = p;
		cisco[i].username = myusername != NULL ? myusername : username;
		cisco[i].passwd = mypasswd != NULL ? mypasswd : passwd;
		cisco[i].fin = NULL;
		cisco[i].fout = NULL;

		memset(sinp, 0, sizeof(cisco[i].ipaddr));
		sinp->sin_family = AF_INET;
		memcpy(&sinp->sin_addr, hostInfo->h_addr, hostInfo->h_length);
		sinp->sin_port = htons(port);	/* telnet */

		n_cisco++;
		fprintf (stderr, "Adding new host %s\n", p);
		fflush (stderr);
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
				"%s: conflicting usernames\n(\"%s\" "
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
				"%s: conflicting user-level passwords\n(\"%s\" "
				"and \"%s\") for cisco \"%s\"\n",
				pmProgname, cisco[i].passwd, mypasswd, 
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

	fprintf (stderr, "Interface %s(%d) is on host %s\n",
		 intf[n].interface, n, cisco[i].host);
	fflush (stderr);


        continue;

badintfspec:
        fprintf(stderr, "%s: bad interface specification \"%s\"\n", pmProgname, argv[optind]);
        fprintf(stderr, "      should be like sydcisco.sydney:s1 or b9u-cisco1-81.engr.sgi.com:f2/0\n");
        fprintf(stderr, "      or cisco.melbourne:e0?secret\n");
        exit(1);
    }

    if (! n_cisco ) {
	fprintf(stderr, "%s: Nothing to monitor\n", pmProgname);
	exit(1);
    }


    /* initialize */
    cisco_init(&dispatch);

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

#ifdef PARSE_ONLY
    for (i = 0; i < n_intf; i++)
	intf[i].fetched = 0;

    fprintf(stderr, "Sleeping while sproc does the work ... SIGINT to terminate\n");
    pause();
#else
    /* set up connection to PMCD */
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
#endif

    cisco_done();
    exit(0);
    /*NOTREACHED*/
}
