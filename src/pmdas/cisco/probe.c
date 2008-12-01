/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
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
#include <errno.h>
#include "./cisco.h"

static FILE	*fin;
static FILE	*fout;

int		port = 23;
int		seen_fr = 0;

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

    p = mygetwd(f);

    if (p != NULL && p[strlen(p)-1] != '>') {
	/* skip to end of line, ready for next one */
	while ((c = fgetc(f)) != EOF) {
	    if (c == '\r' || c == '\n')
		break;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "mygetfirstwd: %s\n", p == NULL ? "<NULL>" : p);
#endif

    return p;
}

#define	PREAMBLE	1
#define IN_BODY		2

static void
probe_cisco(cisco_t * cp)
{
    char	*w;
    int		fd;
    int		first = 1;
    int		defer;
    int		state = PREAMBLE;
    int		i;
    int		namelen;
    char	*ctype;
    char	*name;

    fd = conn_cisco(cp);
    if (fd == -1) {
	fprintf(stderr, "grab_cisco(%s): connect failed: %s\n",
	    cp->host, strerror(errno));
	return;
    }
    else {
	fin = fdopen (fd, "r");
	fout = fdopen (dup(fd), "w");
	if (cp->username != NULL) {
	    /*
	     * Username stuff ...
	     */
	    if (dousername(fin, fout, cp->username, cp->host) == 0) {
		exit(1);
	    }
	}
	if (cp->passwd != NULL) {
	    /*
	     * User-level password stuff ...
	     */
	    if (dopasswd(fin, fout, cp->passwd, cp->host) == 0) {
		exit(1);
	    }
	}
    }
    fprintf(fout, "\n");
    fflush(fout);
    fprintf(fout, "terminal length 0\n");
    fflush(fout);

    fprintf(fout, "show int\n");
    fflush(fout);

    for ( ; ; ) {
	w = mygetfirstwd(fin);
	if (defer) {
	    if (seen_fr) {
		if (first)
		    first = 0;
		else
		    putchar(' ');
		printf("%s%s", ctype, name);
		free(name);
	    }
	}
	defer = 0;
	if (w == NULL) {
	    /*
	     * End of File (telenet timeout?)
	     */
	    fprintf(stderr, "grab_cisco(%s): forced disconnect fin=%d\n",
		cp->host, fileno(fin));
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
	    if (w[strlen(w)-1] == '>')
		break;
	    ctype = NULL;
	    for (i = 0; i < num_intf_tab; i++) {
		namelen = strlen(intf_tab[i].name);
		if (strncmp(w, intf_tab[i].name, namelen) == 0) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL1) {
			fprintf(stderr, "Match: if=%s word=%s\n", intf_tab[i].name, w);
		    }
#endif
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
		fprintf(stderr, "%s: Warning, unknown interface: %s\n", pmProgname, w);
	    if (ctype != NULL && !defer) {
		if (first)
		    first = 0;
		else
		    putchar(' ');
		printf("%s%s", ctype, name);
		free(name);
	    }
	}
    }
    putchar('\n');

    /* close CISCO telnet session */
    fprintf(fout, "exit\n");
    fflush(fout);

    return;
}

int
main(int argc, char **argv)
{
    int			errflag = 0;
    int			c;
    int			sts;
    char		*endnum;
    char		*passwd = NULL;
    char		*username = NULL;
    struct hostent	*hostInfo;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:P:U:x:?")) != EOF) {
	switch (c) {

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;

	    case 'P':		/* passwd */
		passwd = optarg;
		break;

	    case 'U':		/* username */
		username = optarg;
		break;

	    case 'x':
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -x requires numeric argument\n",
			    pmProgname);
		    errflag++;
		}
		break;

	    case '?':
		errflag++;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr, "Usage: %s [-U username] [-P passwd] [-x port] host\n\n", pmProgname);
	exit(1);
    }
	    
    if ((hostInfo = gethostbyname(argv[optind])) == NULL) {
	fprintf(stderr, "%s: unknown hostname %s: %s\n",
		pmProgname, argv[optind], hstrerror(h_errno));
    } else {
	cisco_t c;
	struct sockaddr_in *sinp = & c.ipaddr;
	
	c.host = argv[optind];
	c.username = username;
	c.passwd = passwd;
	c.fin = NULL;
	c.fout = NULL;
		
	memset(sinp, 0, sizeof(c.ipaddr));
	sinp->sin_family = AF_INET;
	memcpy(&sinp->sin_addr, hostInfo->h_addr, hostInfo->h_length);
	sinp->sin_port = htons(23);	/* telnet */
	
	probe_cisco(&c);
    }

    exit(0);
}
