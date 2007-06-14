/*
 * The shim program.
 *
 * Communicates with the Windows PMDA via a pipe (stdin, stdout) with
 * a simple ascii protocol.
 *
 * More complex communication is via a mmap()'d shared memory segment,
 * see shm.h for details of the layout.
 *
 * This program makes the Win32 API calls needed to instantiate metric
 * descriptions, populate instance domains, retrieve help text and
 * fetch values.  Most of this uses the PDH (Performance Data Helper)
 * interfaces.
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "./shim.h"
#include "./domain.h"

shm_hdr_t	*shm = NULL;
shm_hdr_t	*new_hdr;
HANDLE		shm_hfile = NULL;
HANDLE		shm_hmap;
int		hdr_size;
int		shm_oldsize = 0;

shim_query_t	*querytab;
int		querytab_sz;
shim_metric_t	*shim_metrictab;
shm_metric_t	*shm_metrictab;
int		metrictab_sz;

static FILE	*recv_f;
static FILE	*send_f;

/*
 * messy ... this one is private to the shim, and is initialized by the
 * command line arguments ... these are the same as the command line
 * arguments to the Windows PMDA, so both chunks of code should be under the
 * influence of the same debug flag value.
 *
 * APPL0	initialization calls to PDH and setting up the metric
 * 		and instance domain control structures
 * APPL1	shared memory region juggling
 * APPL2	PMDA functions, fetch, help, indom services, etc
 *
 * Defining _both_ APPL0 and APPL2 causes verbose and desperate diagnostics
 * in the intialization phase.
 */
int pmDebug;

int domain = WINDOWS;

static void
usage(void)
{
    fprintf(stderr, "Usage: shim.exe [args ...]\n");
}

int
main(int argc, char **argv)
{
    int		i;
    int		sts;
    char	cmd[100];
    int		base = 0;
    char	c;
    char	*p;
    int		errflag = 0;
    int		pargc = argc;
    char	**pargv = argv;

    /*
     * quick hack for getopt() replacement
     */
    while (pargc > 1) {
	if (pargv[1][0] != '-')
	    break;
	c = pargv[1][1];
	if (pargv[1][2] == '\0') {
	    pargv++;
	    pargc--;
	    p = pargv[1];
	}
	else
	    p = &pargv[1][2];
	switch (c) {

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(p);
		if (sts < 0) {
		    fprintf(stderr, "shim.exe: unrecognized debug flag specification (%s)\n",
			p);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;

	    case 'd':	/* domain */
		sts = atoi(p);
		if (sts != domain) {
		    fprintf(stderr, "Warning: change domain from %d to %d",
			domain, sts);
		    fflush(stderr);
		    domain = sts;
		}
		break;

	    case 'l':	/* log (from pmda), silently skip this one */
	    	break;

	    case 'e':	/* executable (from pmda), silently skip this one */
	    	break;

	}
	pargv++;
	pargc--;
    }

    if (errflag) {
	usage();
	exit(1);
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	for (i = 0; i < argc; i++) {
	    fprintf(stderr, "shim.exe: arg[%d] %s\n", i, argv[i]);
	}
	fflush(stderr);
    }
#endif

    if ((recv_f = fdopen(0, "r")) == NULL) {
	fprintf(stderr, "shim.exe: recv fdopen(%d) failed: ", 0);
	errmsg();
	exit(1);
    }
    if ((send_f = fdopen(1, "w")) == NULL) {
	fprintf(stderr, "shim.exe: send fdopen(%d) failed: ", 1);
	errmsg();
	exit(1);
    }

    while (fgets(cmd, sizeof(cmd), recv_f) != NULL) {
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "shim.exe: got %s", cmd);
	    fflush(stderr);
	}
#endif
	if (strncmp(cmd, "quit", 4) == 0) {
	    exit(0);
	}
	else if (strncmp(cmd, "init", 4) == 0) {
	    if (shim_init() < 0)
		/*
		 * fatal initialization failure, reported already
		 */
		exit(1);
	    fflush(stderr);
	    fprintf(send_f, "ok\n");
	}
	else if (strncmp(cmd, "help", 4) == 0) {
	    int		ident;
	    int		type;
	    int		sts;
	    int		nbyte;
	    int		delta;
	    char	*buf;
	    char	*dst;

	    sscanf(&cmd[5], "%d %d", &ident, &type);
	    sts = help(ident, type, &buf);
	    if (sts == 0) {
		nbyte = strlen(buf) + 1;
		delta = nbyte - shm->segment[SEG_SCRATCH].elt_size * shm->segment[SEG_SCRATCH].nelt;
		if (delta > 0) {
		    memcpy(new_hdr, shm, hdr_size);
		    new_hdr->segment[SEG_SCRATCH].nelt = nbyte;
		    new_hdr->size += delta;
		    shm_reshape(new_hdr);
		}
		dst = (char *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];
		strcpy(dst, buf);
		free(buf);
		fprintf(send_f, "ok\n");
	    }
	    else {
		fprintf(send_f, "err %d\n", sts);
	    }
	}
	else if (strncmp(cmd, "prefetch", 8) == 0) {
	    int		numpmid;
	    int		sts;

	    sscanf(&cmd[9], "%d", &numpmid);
	    sts = prefetch(numpmid);
	    if (sts >= 0) {
		fprintf(send_f, "ok %d\n", sts);
	    }
	    else {
		fprintf(send_f, "err %d\n", sts);
	    }
	}
	else {
	    fprintf(send_f, "err %d %s\n", PM_ERR_NYI, cmd);
	}
	fflush(send_f);
    }
}
