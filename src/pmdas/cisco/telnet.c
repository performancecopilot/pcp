/*
 * Copyright (c) 1995-2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <syslog.h>
#include <errno.h>
#include "./cisco.h"

extern int	port;

int
conn_cisco(cisco_t * cp)
{
    int	fd;
    int	i;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	fprintf(stderr, "conn_cisco(%s) socket: %s\n",
	    cp->host, strerror(errno));
	return -1;
    }

    i = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, (mysocklen_t)sizeof(i)) < 0) {
	fprintf(stderr, "conn_cisco(%s): setsockopt: %s\n",
	    cp->host, strerror(errno));
	close(fd);
	return -1;
    }

    if ( connect(fd, (struct sockaddr *)&cp->ipaddr, sizeof(cp->ipaddr)) < 0) {
	fprintf(stderr, "conn_cisco(%s): connect: %s\n",
	    cp->host, strerror(errno));
	close(fd);
	return -1;
    }

    return fd;
}

static void
skip2eol(FILE *f)
{
    int		c;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "skip2eol:");
#endif

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    break;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    fprintf(stderr, "%c", c);
#endif
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fputc('\n', stderr);
#endif
}

char *
mygetwd(FILE *f)
{
    char	*p;
    int		c;
    static char	buf[1024];

    p = buf;

    while ((c = fgetc(f)) != EOF) {
	if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
	    if (p == buf)
		continue;
	    break;
	}
        *p++ = c;
        if (c == '>')
	    break;
    }
    *p = '\0';

    if (feof(f)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    fprintf(stderr, "mygetwd: EOF fd=%d\n", fileno(f));
#endif
	return NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "mygetwd: fd=%d wd=\"%s\"\n", fileno(f), buf);
#endif

    return buf;
}

/*
 * CISCO "show interface" command has one of the following formats ...
 * the parser below is sensitive to this!
 *
 * Note lines are terminated with \r
 */

#define DONE -1
#define NOISE 0
#define IN_REPORT 1
#define RATE 2
#define RATE_IN 3
#define RATE_OUT 4
#define BYTES_IN 5
#define BYTES_OUT 6
#define BW 7
#define BYTES_OUT_BCAST 8

#ifdef PCP_DEBUG
static char *statestr[] = {
    "done", "noise", "in_report",
    "rate", "rate_in", "rate_out", "bytes_in", "bytes_out", "bw",
    "bytes_out_bcast"
};
#endif

int
dousername(FILE *fin, FILE *fout, char *username, char *host)
{
    char	*w;
    int		done = 0;

    for ( ; ; ) {
	w = mygetwd(fin);
	if (w == NULL || w[strlen(w)-1] == '>')
	    break;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "Username:? got - %s\n", w);
#endif
	if (strcmp(w, USERPROMPT) == 0) {
	    fprintf(fout, "%s\n", username);
	    fflush(fout);
	    for ( ; ; ) {
		w = mygetwd(fin);
		if (w == NULL || strcmp(w, USERPROMPT) == 0)
		    /* closed connection or Username re-prompt */
		    break;
		if (w[strlen(w)-1] == '>') {
		    /* command prompt */
		    done = 1;
		    break;
		}
	    }
	    break;
	}
    }

    if (done == 0) {
	fprintf(stderr, "Error: Cisco username negotiation failed for \"%s\"\n",
		    host);
	fprintf(stderr,
"To check that a username is required, enter the following command:\n"
"   $ telnet %s\n"
"If the prompt \"%s\" does not appear, no username is required.\n"
"Otherwise, enter the username \"%s\" to check that this\n"
"is correct.\n",
host, USERPROMPT, username);
    }

    return done;
}

int
dopasswd(FILE *fin, FILE *fout, char *passwd, char *host)
{
    char	*w;
    int		done = 0;

    for ( ; ; ) {
	w = mygetwd(fin);
	if (w == NULL || w[strlen(w)-1] == '>')
	    break;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "Password:? got - %s\n", w);
#endif
	if (strcmp(w, PWPROMPT) == 0) {
	    fprintf(fout, "%s\n", passwd);
	    fflush(fout);
	    for ( ; ; ) {
		w = mygetwd(fin);
		if (w == NULL || strcmp(w, PWPROMPT) == 0)
		    /* closed connection or user-level password re-prompt */
		    break;
		if (w[strlen(w)-1] == '>') {
		    /* command prompt */
		    done = 1;
		    break;
		}
	    }
	    break;
	}
    }

    if (done == 0) {
	fprintf(stderr, "Error: Cisco user-level password negotiation failed for \"%s\"\n",
		    host);
	fprintf(stderr,
"To check that a user-level password is required, enter the following command:\n"
"   $ telnet %s\n"
"If the prompt \"%s\" does not appear, no user-level password is required.\n"
"Otherwise, enter the user-level password \"%s\" to check that this\n"
"is correct.\n",
host, PWPROMPT, passwd);
    }

    return done;
}

static int	timeout;

void
onalarm(int dummy)
{
    timeout = 1;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "Alarm timeout!\n");
    }
#endif

}

static int
get_fr_bw(cisco_t *cp, char *interface)
{
    int		state = NOISE;
    int		bandwidth = -1;
    char	*w;

    fprintf(cp->fout, "show int s%s\n", interface);
    fflush(cp->fout);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "BW Parse:");
#endif
    while (state != DONE) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "[%s] ", statestr[state+1]);
#endif
	w = mygetwd(cp->fin);
	if (w == NULL || timeout) {
	    /*
	     * End of File (telenet timeout?)
	     */
	    alarm(0);
	    return -1;
	}
	switch (state) {

	    case NOISE:
		if (strncmp(w, "Serial", 6) == 0 && strcmp(&w[6], interface) == 0)
		    state = IN_REPORT;
		break;
	    
	    case IN_REPORT:
		if (strcmp(w, "Description:") == 0)
		    skip2eol(cp->fin);
		else if (w[strlen(w)-1] == '>')
		    state = DONE;
		else if (strcmp(w, "BW") == 0)
		    state = BW;
		break;

	    case BW:
		sscanf(w, "%d", &bandwidth);
		bandwidth *= 1024;		/* Kbit -> bytes/sec */
		bandwidth /= 10;
		state = IN_REPORT;
		break;

	}
    }
    alarm(0);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "Extracted bandwidth: %d bytes/sec\n", bandwidth);
    }
#endif
    return bandwidth;
}

#define SHOW_INT	1
#define SHOW_FRAME	2

int
grab_cisco(intf_t *ip)
{
    int		style;
    int		next_state;
    int		state = NOISE;
    int		skip = 0;		/* initialize to pander to gcc */
    int		i;
    int		namelen;
    char	*w;
    int		fd;
    int		nval = 0;
    cisco_t	*cp = ip->cp;
    intf_t	tmp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "grab_cisco(%s:%s):\n", cp->host, ip->interface);
    }
#endif

    tmp.bandwidth = tmp.rate_in = tmp.rate_out = tmp.bytes_in = tmp.bytes_out = tmp.bytes_out_bcast = -1;

    if (cp->fin == NULL) {
	fd = conn_cisco(cp);
	if (fd < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "grab_cisco(%s:%s): connect failed: %s\n",
			cp->host, ip->interface, strerror(errno));
	    }
#endif
	    return -1;
	}
	else {
	    cp->fin = fdopen (fd, "r");
	    cp->fout = fdopen (dup(fd), "w");
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		fprintf(stderr, "grab_cisco(%s:%s): connected fin=%d fout=%d",
		    cp->host, ip->interface, fileno(cp->fin), fileno(cp->fout));
		if (cp->username != NULL)
		    fprintf(stderr, " username=%s", cp->username);
		else
		    fprintf(stderr, " NO username");
		if (cp->passwd != NULL)
		    fprintf(stderr, " passwd=%s", cp->passwd);
		else
		    fprintf(stderr, " NO passwd");
		fputc('\n', stderr);
	    }
#endif

	    if (cp->username != NULL) {
		/*
		 * Username stuff ...
		 */
		if (dousername(cp->fin, cp->fout, cp->username, cp->host) == 0) {
		    fclose(cp->fin);
		    fclose(cp->fout);
		    cp->fin = NULL;
		    return -1;
		}
	    }
	    if (cp->passwd != NULL) {
		/*
		 * User-level password stuff ...
		 */
		if (dopasswd(cp->fin, cp->fout, cp->passwd, cp->host) == 0) {
		    fclose(cp->fin);
		    fclose(cp->fout);
		    cp->fin = NULL;
		    return -1;
		}
	    }

	    fprintf(cp->fout, "\n");
	    fflush(cp->fout);
	}
	fprintf(cp->fout, "terminal length 0\n");
	fflush(cp->fout);
    }

    timeout = 0;
    signal(SIGALRM, onalarm);
    alarm(5);

    style = SHOW_INT;			/* default Cisco command */
    if (ip->interface[0] == 's' && strchr(ip->interface, '.') != NULL) {
	/*
	 * Frame-relay PVC on subinterface for s2/3.7 style interface name
	 */
	style = SHOW_FRAME;
	if (ip->bandwidth == -2) {
	    /*
	     * one-trip initialzation ... need show int s2/3.7 to
	     * get bandwidth
	     */
	    ip->bandwidth = get_fr_bw(cp, &ip->interface[1]);
	}
	tmp.bandwidth = ip->bandwidth;
	if (tmp.bandwidth != -1)
	    nval++;
    }
    if (style == SHOW_FRAME) {
	fprintf(cp->fout, "show frame pvc int s%s\n", &ip->interface[1]);
	next_state = BYTES_IN;
    }
    else
	fprintf(cp->fout, "show int %s\n", ip->interface);
    fflush(cp->fout);
    state = NOISE;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "Parse:");
#endif
    while (state != DONE) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "[%s] ", statestr[state+1]);
#endif
	w = mygetwd(cp->fin);
	if (w == NULL || timeout) {
	    /*
	     * End of File (telenet timeout?)
	     * ... mark as closed, and try again at next request
	     */
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "grab_cisco(%s:%s): forced disconnect fin=%d\n",
		    cp->host, ip->interface, fileno(cp->fin));
#endif
	    fclose(cp->fin);
	    fclose(cp->fout);
	    cp->fin = NULL;
	    alarm(0);
	    return -1;
	}
	switch (state) {

	    case NOISE:
		for (i = 0; i < num_intf_tab; i++) {
		    namelen = strlen(intf_tab[i].name);
		    if (strncmp(w, intf_tab[i].name, namelen) == 0) {
			state = IN_REPORT;
			break;
		    }
		}
		break;
	    
	    case IN_REPORT:
		if (strcmp(w, "Description:") == 0)
		    skip2eol(cp->fin);
		else if (w[strlen(w)-1] == '>')
		    state = DONE;
		else if (style == SHOW_INT) {
		    if (strcmp(w, "minute") == 0 || strcmp(w, "second") == 0)
			state = RATE;
		    else if (strcmp(w, "input,") == 0)
			state = BYTES_IN;
		    else if (strcmp(w, "output,") == 0)
			state = BYTES_OUT;
		    else if (strcmp(w, "BW") == 0)
			state = BW;
		}
		else if (style == SHOW_FRAME) {
		    if (strcmp(w, "bytes") == 0) {
			if (next_state == BYTES_IN) {
			    state = BYTES_IN;
			    next_state = BYTES_OUT;
			}
			else if (next_state == BYTES_OUT) {
			    state = BYTES_OUT;
			    next_state = BYTES_OUT_BCAST;
			}
			else if (next_state == BYTES_OUT_BCAST) {
			    state = BYTES_OUT_BCAST;
			    next_state = IN_REPORT;
			}
			else
			    state = next_state;
		    }
		}
		break;

	    case RATE:
		if (strcmp(w, "input") == 0) {
		    skip = 1;
		    state = RATE_IN;
		}
		else if (strcmp(w, "output") == 0) {
		    skip = 1;
		    state = RATE_OUT;
		}
		break;

	    case RATE_IN:
		if (skip-- == 0) {
		    tmp.rate_in = atol(w) / 10;
		    nval++;
		    state = IN_REPORT;
		}
		break;

	    case RATE_OUT:
		if (skip-- == 0) {
		    tmp.rate_out = atol(w) / 10;
		    nval++;
		    state = IN_REPORT;
		}
		break;

	    case BYTES_IN:
		sscanf(w, "%u", &tmp.bytes_in);
		nval++;
		state = IN_REPORT;
		break;

	    case BYTES_OUT:
		sscanf(w, "%u", &tmp.bytes_out);
		nval++;
		state = IN_REPORT;
		break;

	    case BYTES_OUT_BCAST:
		sscanf(w, "%u", &tmp.bytes_out_bcast);
		nval++;
		state = IN_REPORT;
		break;

	    case BW:
		sscanf(w, "%d", &tmp.bandwidth);
		tmp.bandwidth *= 1024;		/* Kbit -> bytes/sec */
		tmp.bandwidth /= 10;
		nval++;
		state = IN_REPORT;
		break;

	}
    }
    alarm(0);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "Extracted %d values ...\nbandwidth: %d bytes/sec\nrecent rate (bytes/sec): %d in %d out  total bytes: %u in %u out %u out_bcast\n\n",
	    nval, tmp.bandwidth, tmp.rate_in, tmp.rate_out, tmp.bytes_in, tmp.bytes_out, tmp.bytes_out_bcast);
    }
#endif

    /* pretend this is atomic */
    ip->bandwidth = tmp.bandwidth;
    ip->rate_in = tmp.rate_in;
    ip->rate_out = tmp.rate_out;
    ip->bytes_in = tmp.bytes_in;
    ip->bytes_out = tmp.bytes_out;
    ip->bytes_out_bcast = tmp.bytes_out_bcast;

    return nval;
}
