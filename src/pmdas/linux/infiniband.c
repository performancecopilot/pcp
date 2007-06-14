/*
* Linux Infiniband metrics cluster
*
* this code invokes the OFED utility perfquery -r to regularly read
* and reset the counters.. so we better be the only ones doing it! 
* Future consideration may be given to working out how to use
* sample counters instead, to avoid this.
*
 * Copyright (c) * 2006 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: infiniband.c,v 1.5 2007/02/26 06:06:46 kimbrr Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "infiniband.h"

#define CARD0DIR	"/sys/class/infiniband/mthca0"
#define OFEDBIN		"/usr/local/ofed/bin/"
#define PERFQUERY	OFEDBIN "perfquery"
#define IBSTATUS	OFEDBIN "ibstatus"

#define MAX_COUNTER_NAME 21

#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

static char scratch[BUFSIZ];

static char perfquery[MAXPATHLEN] = PERFQUERY;
static char ibstatus[MAXPATHLEN] = IBSTATUS;

static pthread_t fetch_thread;
static pthread_mutex_t fetch_mutex; /* mutex to update/retrieve values */
static int fetched = 0;

/* 
 * Default time between workproc counter updates.
 * Calculated to expire at least twice in the time it takes any 
 * h/w config to peg out
 */
static int sleeptime = 2;

char *counter_names[IB_COUNTERS] = {
	"RcvBytes",				/* 0 */
	"RcvPkts",				/* 1 */
	"RcvSwRelayErrors",			/* 2 */
	"RcvConstraintErrors",	 		/* 3 */
        "RcvErrors",				/* 4 */
        "RcvRemotePhysErrors",			/* 5 */
	"XmtBytes",				/* 6 */
	"XmtPkts",				/* 7 */
        "XmtDiscards",				/* 8 */
        "XmtConstraintErrors",			/* 9 */ 
        "LinkDowned",				/* 10 */
        "LinkRecovers",				/* 11 */
        "LinkIntegrityErrors",			/* 12 */
        "VL15Dropped",				/* 13 */
	"ExcBufOverrunErrors", 			/* 14 */
        "SymbolErrors",				/* 15 */	
};

/* Assume counters are always listed in the same order, at least during
 * the life of a pmcd, and build a cross-index from their order of appearance
 * to the order in PMNS (and in counter_names). Allow plenty of space for
 * lines we're not interested in (marked -1)
 */
#define XIX_SIZ (IB_COUNTERS<<1)
int counter_xix[XIX_SIZ] = { 0 };

static int fail_count = 0;

struct port_list;
typedef struct port_list port_list_t;
struct port_list {
    ib_port_t	*port;
    port_list_t	*next;
};

port_list_t *ports = NULL;
port_list_t **port_tl = &ports;

static inline void
port_list_free(void)
{
    port_list_t *this = ports;
    while (this) {
	free(this);
	this = this->next;
    }
}

static void 
do_refresh(void)
{
    uint64_t	llval;
    int		ix, j;
    FILE	*fp = NULL;
    port_list_t	*this;

    for (this = ports; this; this = this->next) {
	sprintf(perfquery, PERFQUERY " -r -C %s -P %" PRIu64,
		this->port->card, this->port->portnum);

	fp = popen(perfquery, "r");
	if (fp != NULL) {
	    while (fgetc(fp) != '\n'); /* skip header line */
	    for (j = 0 ; j < XIX_SIZ && 
		         fscanf(fp, " %*[^:]:%*[.]%" SCNi64 " ", 
				(int64_t *)&llval) == 1 ; j++) {
		ix = counter_xix[j];
		if (ix != -1) this->port->counters[ix] += llval;
	    }
	    pclose(fp);
#ifdef PCP_DEBUG
	} else {
	    if (++fail_count < 10)
		fprintf(stderr, "IB:Cmd failed:%s(%d)\n", perfquery, errno);
#endif
	}
    }
}

static int
cache_name(pmInDom indom, char *name, char *port)
{
    int sts;
    char *tail = name + strlen(name);
    port_list_t         *eltp = *port_tl 
	= (port_list_t *)calloc(1, sizeof(port_list_t));

    if (NULL == eltp) return -errno;
    *tail = ':';
    strcpy(tail+1, port);
#ifdef PCP_DEBUG
    fprintf(stderr, "ib instance:%s\n", name);
#endif
    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&eltp->port);

    if (sts == PM_ERR_INST || (sts >= 0 && eltp->port == NULL)) {
        /* first time since re-loaded, else new one */
        eltp->port = (ib_port_t *)calloc(1, sizeof(ib_port_t));
        if (eltp->port == NULL) return -errno;
    } else if (sts < 0) {
        return sts;
    }

    *tail = '\0';
    if (NULL == (eltp->port->card = strdup(name))
     || 1 != sscanf(port, "%" SCNi64, (int64_t *)&eltp->port->portnum)) 
	return -errno;
    *tail = ':';

    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)eltp->port);
    if (sts < 0) return sts;

    port_tl = &eltp->next;

    return 0;
}

void *
fetch_workproc(void *foo)
{
    for (;;) {
	sleep(sleeptime);
	pthread_mutex_lock(&fetch_mutex);
	if (fetched != 0) fetched = 0;
	else do_refresh();

	pthread_mutex_unlock(&fetch_mutex);
    } 
    /*NOTREACHED*/
    return NULL;
}

int
init_ib(pmInDom indom)
{
    /* at least 21 for counter name. name[] is also used above to put together
     * card and port num to name instance
     */
    char port[MAX_COUNTER_NAME], name[MAX_COUNTER_NAME << 1];
    FILE *fp = NULL;
    struct stat statbuf;
    int sts, j=0, ix=0;

    pmdaCacheOp(indom, PMDA_CACHE_LOAD);

    if (stat(CARD0DIR, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode)) {
	return PM_ERR_VALUE; /* No IB */
    }
    if (stat(ibstatus, &statbuf) < 0) { /* OFED tools not installed? */
#ifdef PCP_DEBUG
	fprintf(stderr, "IB: %s not found\n", ibstatus);
#endif
	return PM_ERR_VALUE;
    }
    fp = popen(ibstatus, "r");
    if (fp == NULL) return -errno;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    for (;;) {
	sts = fgetc(fp);
	if (sts == (int)'I') {
	    sts = fscanf(fp, "nfiniband device \'%[^']\' port %s status: %*[\n]", 
			 name, port);
	    if (sts == 2) {
	        sts = cache_name(indom, name, port);
        	if (sts != 0) goto init_ib_err;
		continue;
	    }
	}
	for (;;sts = fgetc(fp)) {
	    switch (sts) {
	    case EOF: goto ibstatus_end;
	    case (int)'\n':
		sts = fgetc(fp);
		break;
	    default:
		continue;
	    }
	    break;
	}
    }
 ibstatus_end:
    pclose(fp);
    if (ports == NULL) {
#ifdef PCP_DEBUG
	    fprintf(stderr, "IB:No IB ports found\n");
#endif
	    return PM_ERR_VALUE;
    }
    fp = popen(perfquery, "r");
    while (fgetc(fp) != '\n'); /* skip header line */

    while (1 == fscanf(fp, " %[^:]:%*[.]%*i ", name)) {
	if (j == XIX_SIZ) {
#ifdef PCP_DEBUG
	    fprintf(stderr, "IB:Too many perfquery counters:%d\n", j);
#endif
	    break;
	}
	ix = 0;
	for (;;) {
	    if (strcmp(name, counter_names[ix])==0) {
	        counter_xix[j++] = ix;
		break;
	    }
	    if (++ix == IB_COUNTERS) {
		counter_xix[j++] = -1;
		break;
	    }
	}
    }
    pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    pthread_mutex_init(&fetch_mutex, NULL);
    if (pthread_create(&fetch_thread, NULL, fetch_workproc, NULL) != 0)  {
        sts = -errno;

      init_ib_err:
	port_list_free();
    }
    pclose(fp);
    return sts;
}

int
refresh_ib(pmInDom indom)
{
    int sts;

    if (ports == NULL && (0 != (sts = init_ib(indom))))
	return sts;

    pthread_mutex_lock(&fetch_mutex);
    fetched = 1;
    do_refresh();
    pthread_mutex_unlock(&fetch_mutex);

    return 0;
}

int
status_ib(ib_port_t * portp)
{
    FILE	*fp	= NULL;
    size_t	cur	= 0;
    int		inspace	= 1;
    int		newlines= 1;
    int		sts;
    char	*str;

    sprintf(ibstatus, IBSTATUS " %s:%" PRIu64,
	    portp->card, portp->portnum);
    fp = popen(ibstatus, "r");
    if (fp == NULL) return -errno;

    /* consume header */
    while (fgetc(fp) != (int)'\n') ;

    for (sts = fgetc(fp); cur < BUFSIZ-1; sts = fgetc(fp)) {
	switch (sts) {
	case EOF: 
	    break;
	case (int)'\n':
	    if (!newlines) scratch[cur++] = ';';
	    newlines = 1;
	    inspace = 0;
	    continue;
	case (int)' ':
	case (int)'\t':
	    if (inspace) continue;
	    inspace = 1;
	    scratch[cur++] = ' ';
	    continue;
	default:
	    newlines = 0;
	    inspace = 0;
	    scratch[cur++] = (char)sts;
	    continue;
	}
	break;
    }
    scratch[cur] = '\0';
    str = strdup(scratch);
    if (str == NULL) return -errno;
    if (portp->status != NULL) free(portp->status);
    portp->status = str;
    return 0;
}
