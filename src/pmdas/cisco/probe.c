/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <syslog.h>
#include "./cisco.h"

int		port = 23;
int		seen_fr = 0;
char		*prompt = ">";		/* unique suffix to IOS prompt */

char *
mygetfirstwd(FILE *f)
{
    char	*p;
    int		c;
    char	line[1024];
    char	*lp;

    for ( ; ; ) {
	c = fgetc(f);
	if (c == EOF)
	    break;
	if (c == '\r' || c == '\n')
	    continue;
	if (c != ' ') {
	    ungetc(c, f);
	    break;
	}
	lp = line;
	while ((c = fgetc(f)) != EOF) {
	    *lp++ = c;
	    if (c == '\r' || c == '\n') {
		*lp = '\0';
		break;
	    }
	}
	/*
	 * some interesting things to look for here ...
	 */
	if (strncmp(&line[1], "Encapsulation FRAME-RELAY", strlen("Encapsulation FRAME-RELAY")) == 0) {
	    seen_fr = 1;
	}
    }
    /* either EOF, or line starts with a non-space */

    p = mygetwd(f, prompt);

    if (p != NULL && (strlen(p) < strlen(prompt) ||
	    strcmp(&p[strlen(p)-strlen(prompt)], prompt)) != 0) {
	/* skip to end of line, ready for next one */
	while ((c = fgetc(f)) != EOF) {
	    if (c == '\r' || c == '\n')
		break;
	}
    }

    if (pmDebugOptions.appl2)
	    fprintf(stderr, "mygetfirstwd: %s\n", p == NULL ? "<NULL>" : p);

    return p;
}

#define	PREAMBLE	1
#define IN_BODY		2

static void
probe_cisco(cisco_t * cp)
{
    char	*w;
    int		fd;
    int		fd2;
    int		first = 1;
    char	*pass = NULL;
    int		defer = 0;
    int		state = PREAMBLE;
    int		i;
    int		namelen;
    char	*ctype = NULL;
    char	*name = NULL;

    if (cp->fin == NULL) {
	fd = conn_cisco(cp);
	if (fd == -1) {
	    fprintf(stderr, "grab_cisco(%s): connect failed: %s\n",
		cp->host, osstrerror());
	    return;
	}
	else {
	    cp->fin = fdopen (fd, "r");
	    if ((fd2 = dup(fd)) < 0) {
	    	perror("dup");
		exit(1);
	    }
	    cp->fout = fdopen (fd2, "w");
	    if (cp->username != NULL) {
		/*
		 * Username stuff ...
		 */
		if (dousername(cp, &pass) == 0) {
		    exit(1);
		}
	    }
	    if (cp->passwd != NULL) {
		/*
		 * User-level password stuff ...
		 */
		if (dopasswd(cp, pass) == 0) {
		    exit(1);
		}
	    }
	}
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "Send: \n");
	    fprintf(stderr, "Send: terminal length 0\n");
	}
	fprintf(cp->fout, "\n");
	fflush(cp->fout);
	fprintf(cp->fout, "terminal length 0\n");
	fflush(cp->fout);

	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "Send: show int\n");
	}
	fprintf(cp->fout, "show int\n");
	fflush(cp->fout);

    }
    else {
	/*
	 * parsing text from a file, not a TCP/IP connection to a
	 * Cisco device
	 */
	;
    }

    for ( ; ; ) {
	w = mygetfirstwd(cp->fin);
	if (defer && ctype != NULL && name != NULL) {
	    if (seen_fr) {
		if (first)
		    first = 0;
		else
		    putchar(' ');
		printf("%s%s", ctype, name);
		free(name);
		name = NULL;
	    }
	}
	defer = 0;
	if (w == NULL) {
	    /*
	     * End of File (telenet timeout?)
	     */
	    fprintf(stderr, "grab_cisco(%s): forced disconnect fin=%d\n",
		cp->host, fileno(cp->fin));
	    return;
	}
	if (*w == '\0')
	    continue;
	if (state == PREAMBLE) {
	    if (strcmp(w, "show") == 0)
		state = IN_BODY;
	    else if (strcmp(w, PWPROMPT) == 0) {
		fprintf(stderr, 
			"Error: user-level password required for \"%s\"\n",
			cp->host);
		exit(1);
	    }
	    continue;
	}
	else {
	    if (strlen(w) >= strlen(prompt) &&
		strcmp(&w[strlen(w)-strlen(prompt)], prompt) == 0)
		break;
	    ctype = NULL;
	    for (i = 0; i < num_intf_tab; i++) {
		namelen = strlen(intf_tab[i].name);
		if (strncmp(w, intf_tab[i].name, namelen) == 0) {
		    if (pmDebugOptions.appl2) {
			fprintf(stderr, "Match: if=%s word=%s\n", intf_tab[i].name, w);
		    }
		    name = strdup(&w[namelen]);
		    ctype = intf_tab[i].type;
		    if (intf_tab[i].type != NULL && strcmp(intf_tab[i].type, "a") == 0) {
			/*
			 * skip ATMN.M ... need ATMN
			 */
			if (strchr(&w[namelen], '.') != NULL)
			    ctype = NULL;
		    }
		    else if (intf_tab[i].type != NULL && strcmp(intf_tab[i].type, "s") == 0) {
			/*
			 * skip SerialN.M ... need SerialN, unless frame-relay
			 */
			if (strchr(&w[namelen], '.') != NULL)
			    defer = 1;
		    }
		    break;
		}
	    }
	    if (i == num_intf_tab)
		fprintf(stderr, "%s: Warning, unknown interface: %s\n", pmGetProgname(), w);
	    if (ctype != NULL && name != NULL && !defer) {
		if (first)
		    first = 0;
		else
		    putchar(' ');
		printf("%s%s", ctype, name);
		free(name);
		name = NULL;
	    }
	}
    }
    putchar('\n');

    /* close CISCO telnet session */
    if (pmDebugOptions.appl1) {
	fprintf(stderr, "Send: exit\n");
    }
    fprintf(cp->fout, "exit\n");
    fflush(cp->fout);

    return;
}

int
main(int argc, char **argv)
{
    int			errflag = 0;
    int			Nflag = 0;
    int			c;
    int			sts;
    char		*endnum;
    char		*passwd = NULL;
    char		*username = NULL;
    __pmHostEnt		*hostInfo = NULL;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "ND:P:s:U:x:?")) != EOF) {
	switch (c) {

	    case 'N':	/* check flag */
		Nflag = 1;
		break;

	    case 'D':	/* debug options */
		sts = pmSetDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
			pmGetProgname(), optarg);
		    errflag++;
		}
		break;

	    case 'P':		/* passwd */
		passwd = optarg;
		break;

	    case 's':		/* prompt */
	    	prompt = optarg;
		break;

	    case 'U':		/* username */
		username = optarg;
		break;

	    case 'x':
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -x requires numeric argument\n",
			    pmGetProgname());
		    errflag++;
		}
		break;

	    case '?':
		errflag++;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr, "Usage: %s [-U username] [-P passwd] [-s prompt] [-x port] host\n\n", pmGetProgname());
	exit(1);
    }

    if (!Nflag)
	hostInfo = __pmGetAddrInfo(argv[optind]);

    if (hostInfo == NULL) {
	FILE	*f;

	if ((f = fopen(argv[optind], "r")) == NULL) {
	    fprintf(stderr, "%s: unknown hostname or filename %s: %s\n",
		pmGetProgname(), argv[optind], hoststrerror());
	    exit(1);
	}
	else {
	    cisco_t c;

	    fprintf(stderr, "%s: assuming file %s contains output from \"show int\" command\n",
	    	pmGetProgname(), argv[optind]);

	    c.host = argv[optind];
	    c.username = NULL;
	    c.passwd = NULL;
	    c.fin = f;
	    c.fout = fopen("/dev/null", "w");
	    c.prompt = prompt;

	    probe_cisco(&c);
	}
    } else {
	cisco_t c;

	c.host = argv[optind];
	c.username = username;
	c.passwd = passwd;
	c.fin = NULL;
	c.fout = NULL;
	c.prompt = prompt;
	c.hostinfo = hostInfo;
	c.port = 23; /* telnet */

	probe_cisco(&c);
    }

    exit(0);
}
