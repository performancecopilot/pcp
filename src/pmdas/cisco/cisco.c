/*
 * Copyright (c) 1995-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <ctype.h>
#include <signal.h>
#include "cisco.h"
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif
#if defined(HAVE_PRCTL_H)
#include <sys/prctl.h>
#endif

extern int	refreshdelay;

#ifdef HAVE_SPROC
static pid_t	sproc_pid = 0;
#elif defined (HAVE_PTHREAD_H)
#include <pthread.h>
static pthread_t sproc_pid;
#else
#error "Need sproc or pthreads here!"
#endif

/*
 * all metrics supported in this PMD - one table entry for each
 */
static pmdaMetric	metrictab[] = {
    /* 0,0 ... for direct map, sigh */
    { NULL, { PMDA_PMID(0,0), 0, 0, 0, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* bytes-in */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_U64, CISCO_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* bytes-out */
    { NULL, { PMDA_PMID(0,2), PM_TYPE_U64, CISCO_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    /* rate-in */
    { NULL, { PMDA_PMID(0,3), PM_TYPE_U32, CISCO_INDOM, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) } },
    /* rate-out */
    { NULL, { PMDA_PMID(0,4), PM_TYPE_U32, CISCO_INDOM, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) } },
    /* bandwidth */
    { NULL, { PMDA_PMID(0,5), PM_TYPE_U32, CISCO_INDOM, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) } },
    /* bytes_out_bcast */
    { NULL, { PMDA_PMID(0,6), PM_TYPE_U64, CISCO_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    };

/* filled in from command line args in main() ... */
pmdaIndom	indomtab[] = {
    { 0, 0, 0 },
};

#ifdef HAVE_SPROC
static RETSIGTYPE
onhup(int s)
{
    signal(SIGHUP, onhup);
    exit(0);
}
#endif

/*
 * the sproc starts here to refresh the metric values periodically
 */
void
refresh(void *dummy)
{
    int		i;

#ifdef HAVE_SPROC
#if HAVE_PRCTL
    signal(SIGHUP, onhup);
#if HAVE_PR_TERMCHILD
    prctl(PR_TERMCHILD);          	/* SIGHUP when the parent dies */
#elif HAVE_PR_SET_PDEATHSIG
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif
#endif
#endif

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Starting sproc ...\n");
	for (i = 0; i < n_cisco; i++) {
	    int	j;

	    fprintf(stderr, "cisco[%d] host: %s username: %s passwd: %s prompt: %s intf:",
		    i, cisco[i].host, cisco[i].username, cisco[i].passwd, cisco[i].prompt);

	    for (j = 0; j < n_intf; j++) {
	        if (intf[j].cp == (cisco+i))
		    fprintf(stderr, " %d-%s", j, intf[j].interface);
	    }
	    fputc('\n', stderr);
	}
    }

    for ( ; ; ) {
	for (i = 0; i < n_intf; i++) {
	    if (grab_cisco(intf+i) != -1) {
		intf[i].fetched = 1;
	    }
	    else
		intf[i].fetched = 0;
	}

	if (parse_only)
	    exit(0);

	for (i = 0; i < n_cisco; i++) {
	    if (cisco[i].fout != NULL) {
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "... %s voluntary disconnect fout=%d\n", cisco[i].host, fileno(cisco[i].fout));
		/* close CISCO telnet session */
		if (pmDebugOptions.appl1) {
		    fprintf(stderr, "Send: exit\n");
		}
		fprintf(cisco[i].fout, "exit\n");
		fclose(cisco[i].fout);
		cisco[i].fout = NULL;
	    }
	    if (cisco[i].fin != NULL) {
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "... %s close fin=%d\n", cisco[i].host, fileno(cisco[i].fin));
		fclose(cisco[i].fin);
		cisco[i].fin = NULL;
	    }
	}

	sleep(refreshdelay);
    }
}

static int
cisco_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *avp)
{
#ifndef HAVE_SPROC
    /* Check is refresh thread is still with us */
    int err;

    if ( (err = pthread_kill (sproc_pid, 0)) != 0 ) {
        exit (1);
    }
#endif

    if (!intf[inst].fetched)
	return PM_ERR_AGAIN;

    switch (pmID_item(mdesc->m_desc.pmid)) {

	case 1:		/* bytes_in */
		if (intf[inst].bytes_in == -1) return 0;
		avp->ull = intf[inst].bytes_in;
		break;

	case 2:		/* bytes_out */
		if (intf[inst].bytes_out == -1) return 0;
		avp->ull = intf[inst].bytes_out;
		break;

	case 3:		/* rate_in */
		if (intf[inst].rate_in == -1) return 0;
		avp->ul = intf[inst].rate_in;
		break;

	case 4:		/* rate_out */
		if (intf[inst].rate_out == -1) return 0;
		avp->ul = intf[inst].rate_out;
		break;

	case 5:		/* bandwidth */
		if (intf[inst].bandwidth == -1) return 0;
		avp->ul = intf[inst].bandwidth;
		break;

	case 6:		/* bytes_out_bcast */
		if (intf[inst].bytes_out_bcast == -1) return 0;
		avp->ull = intf[inst].bytes_out_bcast;
		break;

	default:
		return PM_ERR_PMID;
    }

    return 1;
}

void
cisco_init(pmdaInterface *dp)
{
    int		i;

    pmdaSetFetchCallBack(dp, cisco_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	sizeof(metrictab)/sizeof(metrictab[0]));

    for (i = 0; i < n_intf; i++)
	intf[i].fetched = 0;

    /* start the sproc for async fetches */
#ifdef HAVE_SPROC
    i = sproc_pid = sproc(refresh, PR_SADDR);
#elif defined (HAVE_PTHREAD_H)
    i = pthread_create(&sproc_pid, NULL, (void (*))refresh, NULL);
#else
#error "Need sproc or pthread here!"
#endif

    if (i < 0)
	dp->status = i;
    else
	dp->status = 0;
}

void
cisco_done(void)
{
    int		i;

    if (sproc_pid > 0) {
#ifndef HAVE_SPROC
	pthread_kill(sproc_pid, SIGHUP);
#else
	kill(sproc_pid, SIGHUP);
#endif
	while (wait(&i) >= 0)
	    ;
    }
}

