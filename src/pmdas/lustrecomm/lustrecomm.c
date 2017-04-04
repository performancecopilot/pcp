/*
 * Lustre common /proc PMDA
 *
 * Original author: Scott Emery <emery@sgi.com> 
 *
 * Copyright (c) 2012,2014 Red Hat.
 * Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "libreadfiles.h"
#include <stdio.h>
#include "domain.h"


/*
 * Lustrecomm PMDA
 *
 * This PMDA gathers Lustre statistics from /proc, it is  constructed using
 * libpcp_pmda.
 *
 *
 * Metrics
 *	lustrecomm.time		- time in seconds since the 1st of Jan, 1970.
 */

#define PROC_SYS_LNET_STATS 0
#define PROC_SYS_LNET_NIS 1
#define PROC_SYS_LNET_PEERS 2
#define FILESTATETABSIZE 3

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* timeout */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* ldlm_timeout */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* dump_on_timeout */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },
/* lustre_memused */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* lnet_memused */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* data pulled from /proc/sys/lnet/stats */
/*0 42 0 22407486 23426580 0 0 135850271989 472430974209 0 0*/
/* stats.msgs_alloc */
    { NULL,
      { PMDA_PMID(1,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.msgs_max */
    { NULL,
      { PMDA_PMID(1,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.errors */
    { NULL,
      { PMDA_PMID(1,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.send_count */
    { NULL,
      { PMDA_PMID(1,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.recv_count */
    { NULL,
      { PMDA_PMID(1,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.route_count */
    { NULL,
      { PMDA_PMID(1,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.drop_count */
    { NULL,
      { PMDA_PMID(1,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.send_length */
    { NULL,
      { PMDA_PMID(1,7), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.recv_length */
    { NULL,
      { PMDA_PMID(1,8), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.route_length */
    { NULL,
      { PMDA_PMID(1,9), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* stats.drop_length */
    { NULL,
      { PMDA_PMID(1,10), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } }

};

struct file_state filestatetab[] = {
	{ { 0 , 0 } , "/proc/sys/lnet/stats", 0, 0, NULL},
	{ { 0 , 0 } , "/proc/sys/lnet/nis", 0, 0, NULL},
	{ { 0 , 0 } , "/proc/sys/lnet/peers", 0, 0, NULL}
};

static int	isDSO = 1;		/* =0 I am a daemon */
static char	mypath[MAXPATHLEN];
static char	*username;

/*
 * callback provided to pmdaFetch
 */

static int
lustrecomm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int 		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    void *vp;

	/* check for PMID errors */
    if ( !((idp->cluster == 0) && (idp->item <= 4)) &&
	 !((idp->cluster == 1) && (idp->item <= 10)) )
	return PM_ERR_PMID;
    else if (inst != PM_IN_NULL)
	return PM_ERR_INST;

	/* refresh and store data */
    if (idp->cluster == 0) {
	vp = malloc (4);
	int aux = 10;
        /* not saving any time fussing with "how recent", just do it */
        switch (idp->item) {
            case 0:
		/* consider mdesc->m_desc.type */
		/* problem: the way it's stored in PCP isn't necessarily */
		/* the way it's presented by the OS */
                file_single("/proc/sys/lustre/timeout", PM_TYPE_32, &aux, &vp);
                atom->l = *((long *) vp);
                break;
            case 1:
                file_single("/proc/sys/lustre/ldlm_timeout", PM_TYPE_32, &aux, &vp);
                atom->l = *((long *) vp);
                break;
            case 2:
                file_single("/proc/sys/lustre/dump_on_timeout", PM_TYPE_32, &aux, &vp);
                atom->l = *((long *) vp);
                break;
            case 3:
                file_single("/proc/sys/lustre/memused", PM_TYPE_32, &aux, &vp);
                atom->l = *((long *) vp);
                break;
            case 4:
                file_single("/proc/sys/lnet/lnet_memused", PM_TYPE_32, &aux, &vp);
                atom->l = *((long *) vp);
                break;
            default:
                printf ("PMID %d:%d non-existant\n",idp->cluster,idp->item);
                break;
        }
	free (vp);
    }
    if (idp->cluster == 1) {
	vp = malloc (4);
	int aux = 10;
	
        switch(idp->item) {
            case 0:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 0);
                atom->l = *((long *) vp);
		break;
            case 1:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 1);
                atom->l = *((long *) vp);
		break;
            case 2:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 2);
                atom->l = *((long *) vp);
		break;
            case 3:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 3);
                atom->l = *((long *) vp);
		break;
            case 4:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 4);
                atom->l = *((long *) vp);
		break;
            case 5:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 5);
                atom->l = *((long *) vp);
		break;
            case 6:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_32,&aux, &vp, 6);
                atom->l = *((long *) vp);
		break;
            case 7:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_64,&aux, &vp, 7);
                atom->l = *((long *) vp);
		break;
            case 8:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_64,&aux, &vp, 8);
                atom->l = *((long *) vp);
		break;
            case 9:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_64,&aux, &vp, 9);
                atom->l = *((long *) vp);
		break;
            case 10:
		file_indexed(&filestatetab[PROC_SYS_LNET_STATS], PM_TYPE_64,&aux, &vp, 10);
                atom->l = *((long *) vp);
		break;
             default:
		break;
        }       
	free(vp);
    }
   return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
lustrecomm_init(pmdaInterface *dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "lustrecomm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_2, "lustrecomm DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, lustrecomm_fetchCallBack);

    pmdaInit(dp, NULL, 0, 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "lustrecomm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, LUSTRECOMM,
		"lustrecomm.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    lustrecomm_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
