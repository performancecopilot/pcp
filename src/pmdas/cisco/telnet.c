/*
 * Copyright (c) 2012-2015 Red Hat.
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
#include "./cisco.h"

extern int	port;

int
conn_cisco(cisco_t * cp)
{
    __pmFdSet		wfds;
    __pmSockAddr	*myaddr;
    void		*enumIx;
    int			flags = 0;
    int			fd;
    int			ret;

    fd = -1;
    enumIx = NULL;
    for (myaddr = __pmHostEntGetSockAddr(cp->hostinfo, &enumIx);
	 myaddr != NULL;
	 myaddr = __pmHostEntGetSockAddr(cp->hostinfo, &enumIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myaddr))
	    fd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myaddr))
	    fd = __pmCreateIPv6Socket();
	else
	    fd = -1;

	if (fd < 0) {
	    __pmSockAddrFree(myaddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	flags = __pmConnectTo(fd, myaddr, cp->port);
	__pmSockAddrFree(myaddr);

	if (flags < 0) {
	    /*
	     * Mark failure in case we fall out the end of the loop
	     * and try next address. fd has been closed in __pmConnectTo().
	     */
	    setoserror(ECONNREFUSED);
	    fd = -1;
	    continue;
	}

	/* FNDELAY and we're in progress - wait on select */
	__pmFD_ZERO(&wfds);
	__pmFD_SET(fd, &wfds);
	ret = __pmSelectWrite(fd+1, &wfds, NULL);

	/* Was the connection successful? */
	if (ret == 0)
	    setoserror(ETIMEDOUT);
	else if (ret > 0) {
	    ret = __pmConnectCheckError(fd);
	    if (ret == 0)
		break;
	    setoserror(ret);
	}
	
	/* Unsuccessful connection. */
	__pmCloseSocket(fd);
	fd = -1;
    } /* loop over addresses */

    if (fd == -1) {
	fprintf(stderr, "conn_cisco(%s): connect: %s\n",
		cp->host, netstrerror());
	return -1;
    }

    fd = __pmConnectRestoreFlags(fd, flags);
    if (fd < 0) {
	fprintf(stderr, "conn_cisco(%s): setsockopt: %s\n",
		cp->host, netstrerror());
	return -1;
    }

    return fd;
}

static void
skip2eol(FILE *f)
{
    int		c;

    if (pmDebugOptions.appl2)
	fprintf(stderr, "skip2eol:");

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    break;
	if (pmDebugOptions.appl2)
	    fprintf(stderr, "%c", c);
    }
    if (pmDebugOptions.appl2)
	fputc('\n', stderr);
}

char *
mygetwd(FILE *f, char *prompt)
{
    char	*p;
    int		c;
    static char	buf[1024];
    int		len_prompt = strlen(prompt);
    int		found_prompt = 0;

    p = buf;

    while ((c = fgetc(f)) != EOF) {
	if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
	    if (p == buf)
		continue;
	    break;
	}
        *p++ = c;
	if (p-buf >= len_prompt && strncmp(&p[-len_prompt], prompt, len_prompt) == 0) {
	    found_prompt = 1;
	    break;
	}
    }
    *p = '\0';

    if (feof(f)) {
	if (pmDebugOptions.appl2)
	    fprintf(stderr, "mygetwd: EOF fd=%d\n", fileno(f));
	return NULL;
    }

    if (pmDebugOptions.appl2)
	fprintf(stderr, "mygetwd: fd=%d wd=\"%s\"%s\n", fileno(f), buf, found_prompt ? " [prompt]" : "");

    return buf;
}

/*
 * The CISCO "show interface" command output is parsed.
 *
 * See the file Samples for examples.
 *
 * The parser is a Finite State Automaton (FSA) that follows these
 * rules:
 *
 * SHOW_INT style ... uses "show int <interface>" command
 * state	token			next state
 * NOISE	<interface name>	IN_REPORT
 * IN_REPORT	Description:		skip rest of line, IN_REPORT
 * IN_REPORT	<prompt>		DONE
 * IN_REPORT	minute			RATE
 * IN_REPORT	second			RATE
 * IN_REPORT	input,			BYTES_IN
 * IN_REPORT	output,			BYTES_OUT
 * IN_REPORT	BW			BW
 * RATE		input			skip next token, RATE_IN
 * RATE		output			skip next token, RATE_OUT
 * RATE_IN	<number> (rate_in)	IN_REPORT
 * RATE_OUT	<number> (rate_out)	IN_REPORT
 * BYTES_IN	<number> (bytes_in)	IN_REPORT
 * BYTES_OUT	<number> (bytes_out)	IN_REPORT
 * BW		<number> (bandwidth)	IN_REPORT
 *
 * SHOW_FRAME style ... uses "show frame pvc int <interface>" command
 * state		token			next state
 * NOISE		<interface name>	IN_REPORT
 * IN_REPORT		Description:		skip rest of line, IN_REPORT
 * IN_REPORT		<prompt>		DONE
 * IN_REPORT		1st bytes		BYTES_IN
 * IN_REPORT		2nd bytes		BYTES_OUT
 * IN_REPORT		3rd bytes		BYTES_OUT_BCAST
 * BYTES_IN		<number> (bytes_in)	IN_REPORT
 * BYTES_OUT		<number> (bytes_out)	IN_REPORT
 * BYTES_OUT_BCAST	<number> (bytes_out_bcast)	IN_REPORT
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

static char *statestr[] = {
    "done", "noise", "in_report",
    "rate", "rate_in", "rate_out", "bytes_in", "bytes_out", "bw",
    "bytes_out_bcast"
};

int
dousername(cisco_t *cp, char **pw_prompt)
{
    char	*w;
    int		len, done = 0;
    int		len_prompt = strlen(cp->prompt);

    for ( ; ; ) {
	w = mygetwd(cp->fin, cp->prompt);
	if (w == NULL)
	    break;
	if (strlen(w) >= len_prompt && strncmp(&w[strlen(w)-len_prompt], cp->prompt, len_prompt) == 0)
	    break;
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "Username:? got - %s\n", w);
	if (strcmp(w, USERPROMPT) == 0) {
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "Send username: %s\n", cp->username);
	    }
	    fprintf(cp->fout, "%s\n", cp->username);
	    fflush(cp->fout);
	    for ( ; ; ) {
		w = mygetwd(cp->fin, cp->prompt);
		if (w == NULL || strcmp(w, USERPROMPT) == 0)
		    /* closed connection or Username re-prompt */
		    break;
		len = strlen(w);
		if ((len >= len_prompt && strncmp(&w[len-len_prompt], cp->prompt, len_prompt) == 0) ||
		    w[len-1] == ':') {
		    /* command prompt or passwd */
		    if (w[len-1] == ':')
			*pw_prompt = w;
		    done = 1;
		    break;
		}
	    }
	    break;
	}
    }

    if (done == 0) {
	fprintf(stderr, "Error: Cisco username negotiation failed for \"%s\"\n",
		    cp->host);
	fprintf(stderr,
"To check that a username is required, enter the following command:\n"
"   $ telnet %s\n"
"If the prompt \"%s\" does not appear, no username is required.\n"
"Otherwise, enter the username \"%s\" to check that this\n"
"is correct.\n",
cp->host, USERPROMPT, cp->username);
    }

    return done;
}

int
dopasswd(cisco_t *cp, char *pw_prompt)
{
    char	*w;
    int		done = 0;
    int		len_prompt = strlen(cp->prompt);

    for ( ; ; ) {
	if (pw_prompt)	/* dousername may have read passwd prompt */
	    w = pw_prompt;
	else
	    w = mygetwd(cp->fin, cp->prompt);
	pw_prompt = NULL;
	if (w == NULL)
	    break;
	if (strlen(w) >= len_prompt && strncmp(&w[strlen(w)-len_prompt], cp->prompt, len_prompt) == 0)
	    break;
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "Password:? got - %s\n", w);
	if (strcmp(w, PWPROMPT) == 0) {
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "Send passwd: %s\n", cp->passwd);
		}
	    fprintf(cp->fout, "%s\n", cp->passwd);
	    fflush(cp->fout);
	    for ( ; ; ) {
		w = mygetwd(cp->fin, cp->prompt);
		if (w == NULL || strcmp(w, PWPROMPT) == 0)
		    /* closed connection or user-level password re-prompt */
		    break;
		if (strlen(w) >= len_prompt && strncmp(&w[strlen(w)-len_prompt], cp->prompt, len_prompt) == 0) {
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
		    cp->host);
	fprintf(stderr,
"To check that a user-level password is required, enter the following command:\n"
"   $ telnet %s\n"
"If the prompt \"%s\" does not appear, no user-level password is required.\n"
"Otherwise, enter the user-level password \"%s\" to check that this\n"
"is correct.\n",
cp->host, PWPROMPT, cp->passwd);
    }

    return done;
}

static int	timeout;

void
onalarm(int dummy)
{
    timeout = 1;
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Alarm timeout!\n");
    }

}

static int
get_fr_bw(cisco_t *cp, char *interface)
{
    int		state = NOISE;
    int		bandwidth = -1;
    char	*w;
    int		len_prompt = strlen(cp->prompt);

    if (pmDebugOptions.appl1) {
	fprintf(stderr, "Send: s%s\n", interface);
    }
    fprintf(cp->fout, "show int s%s\n", interface);
    fflush(cp->fout);
    if (pmDebugOptions.appl2)
	fprintf(stderr, "BW Parse:");
    while (state != DONE) {
	if (pmDebugOptions.appl2)
	    fprintf(stderr, "[%s] ", statestr[state+1]);
	w = mygetwd(cp->fin, cp->prompt);
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
		else if (strlen(w) >= len_prompt && strncmp(&w[strlen(w)-len_prompt], cp->prompt, len_prompt) == 0)
		    state = DONE;
		else if (strcmp(w, "BW") == 0)
		    state = BW;
		break;

	    case BW:
		sscanf(w, "%d", &bandwidth);
		bandwidth *= 1000;		/* Kbit -> bytes/sec */
		bandwidth /= 8;
		state = IN_REPORT;
		break;

	}
    }
    alarm(0);
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Extracted bandwidth: %d bytes/sec\n", bandwidth);
    }
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
    int		skip = 0;
    int		i;
    int		namelen;
    char	*pw_prompt = NULL;
    char	*w;
    int		fd;
    int		fd2;
    int		nval = 0;
    cisco_t	*cp = ip->cp;
    intf_t	tmp;
    int		len_prompt = strlen(cp->prompt);

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "grab_cisco(%s:%s):\n", cp->host, ip->interface);
    }

    tmp.bandwidth = tmp.rate_in = tmp.rate_out = -1;
    tmp.bytes_in = tmp.bytes_out = tmp.bytes_out_bcast = -1;

    if (cp->fin == NULL) {
	fd = conn_cisco(cp);
	if (fd < 0) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "grab_cisco(%s:%s): connect failed: %s\n",
			cp->host, ip->interface, netstrerror());
	    return -1;
	}
	else {
	    cp->fin = fdopen (fd, "r");
	    if ((fd2 = dup(fd)) < 0) {
	    	perror("dup");
		exit(1);
	    }
	    cp->fout = fdopen (fd2, "w");
	    if (pmDebugOptions.appl0) {
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

	    if (cp->username != NULL) {
		/*
		 * Username stuff ...
		 */
		if (dousername(cp, &pw_prompt) == 0) {
		    fclose(cp->fin);
		    fclose(cp->fout);
		    cp->fin = cp->fout = NULL;
		    return -1;
		}
	    }
	    if (cp->passwd != NULL) {
		/*
		 * User-level password stuff ...
		 */
		if (dopasswd(cp, pw_prompt) == 0) {
		    fclose(cp->fin);
		    fclose(cp->fout);
		    cp->fin = cp->fout = NULL;
		    return -1;
		}
	    }
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "Send: \n");
	    }
	    fprintf(cp->fout, "\n");
	    fflush(cp->fout);
	}
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "Send: terminal length 0\n");
	}
	fprintf(cp->fout, "terminal length 0\n");
	fflush(cp->fout);
    }

    timeout = 0;
    signal(SIGALRM, onalarm);
    /*
     * Choice of timeout here is somewhat arbitrary ... for a long
     * time this was 5 (seconds), but then testing with an entry
     * level Model 800 ADSL router revealed that up to 20 seconds
     * was required to generate the expected output.
     */
    alarm(20);

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
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "Send: show frame pvc int s%s\n", &ip->interface[1]);
	}
	fprintf(cp->fout, "show frame pvc int s%s\n", &ip->interface[1]);
	next_state = BYTES_IN;
    }
    else {
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "Send: show int %s\n", ip->interface);
	}
	fprintf(cp->fout, "show int %s\n", ip->interface);
    }
    fflush(cp->fout);
    state = NOISE;
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "Parse:");
	fflush(stderr);
    }
    while (state != DONE) {
	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "[%s] ", statestr[state+1]);
	    fflush(stderr);
	}
	w = mygetwd(cp->fin, cp->prompt);
	if (w == NULL || timeout) {
	    /*
	     * End of File (telenet timeout?)
	     * ... mark as closed, and try again at next request
	     */
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "grab_cisco(%s:%s): forced disconnect fin=%d\n",
		    cp->host, ip->interface, fileno(cp->fin));
	    fclose(cp->fin);
	    fclose(cp->fout);
	    cp->fin = cp->fout = NULL;
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
		if (strlen(w) >= len_prompt && strncmp(&w[strlen(w)-len_prompt], cp->prompt, len_prompt) == 0)
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
		    tmp.rate_in = atol(w) / 8;
		    nval++;
		    state = IN_REPORT;
		}
		break;

	    case RATE_OUT:
		if (skip-- == 0) {
		    tmp.rate_out = atol(w) / 8;
		    nval++;
		    state = IN_REPORT;
		}
		break;

	    case BYTES_IN:
		tmp.bytes_in = strtoull(w, NULL, 10);
		nval++;
		state = IN_REPORT;
		break;

	    case BYTES_OUT:
		tmp.bytes_out = strtoull(w, NULL, 10);
		nval++;
		state = IN_REPORT;
		break;

	    case BYTES_OUT_BCAST:
		tmp.bytes_out_bcast = strtoull(w, NULL, 10);
		nval++;
		state = IN_REPORT;
		break;

	    case BW:
		sscanf(w, "%d", &tmp.bandwidth);
		tmp.bandwidth *= 1000;		/* Kbit -> bytes/sec */
		tmp.bandwidth /= 8;
		nval++;
		state = IN_REPORT;
		break;

	}
    }
    alarm(0);
    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Extracted %d values ...\n", nval);
	if (tmp.bandwidth != 0xffffffff)
	    fprintf(stderr, "bandwidth: %d bytes/sec\n", tmp.bandwidth);
	else
	    fprintf(stderr, "bandwidth: ? bytes/sec\n");
	fprintf(stderr, "recent rate (bytes/sec):");
	if (tmp.rate_in != 0xffffffff)
	    fprintf(stderr, " %d in", tmp.rate_in);
	else
	    fprintf(stderr, " ? in");
	if (tmp.rate_out != 0xffffffff)
	    fprintf(stderr, " %d out", tmp.rate_out);
	else
	    fprintf(stderr, " ? out");
	fprintf(stderr, "\ntotal bytes:");
	if (tmp.bytes_in != 0xffffffffffffffffLL)
	    fprintf(stderr, " %llu in", (unsigned long long)tmp.bytes_in);
	else
	    fprintf(stderr, " ? in");
	if (tmp.bytes_out != 0xffffffffffffffffLL)
	    fprintf(stderr, " %llu out", (unsigned long long)tmp.bytes_out);
	else
	    fprintf(stderr, " ? out");
	if (tmp.bytes_out_bcast != 0xffffffffffffffffLL)
	    fprintf(stderr, " %llu out_bcast", (unsigned long long)tmp.bytes_out_bcast);
	else
	    fprintf(stderr, " ? out_bcast");
	fprintf(stderr, "\n\n");
    }

    /* pretend this is atomic */
    ip->bandwidth = tmp.bandwidth;
    ip->rate_in = tmp.rate_in;
    ip->rate_out = tmp.rate_out;
    ip->bytes_in = tmp.bytes_in;
    ip->bytes_out = tmp.bytes_out;
    ip->bytes_out_bcast = tmp.bytes_out_bcast;

    return nval;
}
