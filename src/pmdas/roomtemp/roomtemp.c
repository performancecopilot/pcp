/*
 * Roomtemp PMDA
 *
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "dsread.h"

/*
 * Roomtemp PMDA
 *
 * This PMDA exports the temperature from one or more sensors built using
 * the DS2480 and DS1280 chipsets and MicroLAN technology from Dallas
 * Semiconductor Corporation.
 */

/*
 * Serial device
 */
static char *tty;

/*
 * list of instances
 */

static pmdaInstid *device = NULL;

/*
 * list of instance domains
 */

static pmdaIndom indomtab[] = {
#define DEVICE	0
    { DEVICE, 0, NULL },
};

typedef struct {
    unsigned char	sn[8];
} sn_t;

sn_t *sntab = NULL;

/*
 * All metrics supported in this PMDA - one table entry for each.
 * The 4th field specifies the serial number of the instance domain
 * for the metric, and must be either PM_INDOM_NULL (denoting a
 * metric that only ever has a single value), or the serial number
 * of one of the instance domains declared in the instance domain table
 * (i.e. in indomtab, above).
 */

static pmdaMetric metrictab[] = {
/* roomtemp.celsius */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_FLOAT, DEVICE, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* roomtemp.fahrenheit */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_FLOAT, DEVICE, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

/*
 * callback provided to pmdaFetch
 */
static int
roomtemp_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    char		return_msg[128];
    int			numval = 0;

    if (idp->cluster == 0) {
	if (inst >= indomtab[DEVICE].it_numinst)
	    return PM_ERR_INST;
	switch (idp->item) {
	    case 0:		/* roomtemp.celsius */
	    case 1:		/* roomtemp.fahrenheit */
		if (!Aquire1WireNet(tty, return_msg, sizeof(return_msg))) {
		    fputs(return_msg, stderr);
		    exit(1);
		}
		if (ReadTemperature(sntab[inst].sn, &atom->f))
		    numval = 1;
		Release1WireNet(return_msg, sizeof(return_msg));
		if (idp->item == 1)
		    /* convert to fahrenheit */
		    atom->f = atom->f * 9 / 5 + 32;
		break;

	    default:
		return PM_ERR_PMID;
	}
    }
    else
	return PM_ERR_PMID;

    return numval;
}

/*
 * Initialise the agent
 */
void 
roomtemp_init(pmdaInterface *dp)
{
    int			i;
    char		return_msg[128];
    unsigned char	*p;

    if (dp->status != 0)
	return;

    pmdaSetFetchCallBack(dp, roomtemp_fetchCallBack);

    if (!Aquire1WireNet(tty, return_msg, sizeof(return_msg))) {
	fputs(return_msg, stderr);
	exit(1);
    }
    for (i = 0; ; i++) {
	if ((p = nextsensor()) == NULL)
	    break;
	if ((sntab = (sn_t *)realloc(sntab, (i+1) * sizeof(sn_t))) == NULL) {
	    __pmNoMem("roomtemp_init: realloc sntab", (i+1) * sizeof(sn_t), PM_FATAL_ERR);
	}
	if ((device = (pmdaInstid *)realloc(device, (i+1) * sizeof(pmdaInstid))) == NULL) {
	    __pmNoMem("roomtemp_init: realloc device", (i+1) * sizeof(pmdaInstid), PM_FATAL_ERR);
	}
	if ((device[i].i_name = (char *)malloc(17)) == NULL) {
	    __pmNoMem("roomtemp_init: malloc name", 17, PM_FATAL_ERR);
	}
	memcpy(sntab[i].sn, p, 8);	/* SN for later fetch */
	device[i].i_inst = i;		/* internal name is ordinal number */
					/* external name is SN in hex */
	pmsprintf(device[i].i_name, 17, "%02X%02X%02X%02X%02X%02X%02X%02X",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	fprintf(stderr, "Found temp sensor SN %s\n", device[i].i_name);
    }
    Release1WireNet(return_msg, sizeof(return_msg));
    indomtab[DEVICE].it_numinst = i;
    indomtab[DEVICE].it_set = device;

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));

}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] tty ...\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			err = 0;
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "roomtemp" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, ROOMTEMP,
		"roomtemp.log", mypath);

    if (pmdaGetOpt(argc, argv, "D:d:i:l:pu:6:?", &dispatch, &err) != EOF)
    	err++;
    if (err)
    	usage();
    if (argc != optind+1)
	usage();
    tty = argv[optind];

    pmdaOpenLog(&dispatch);
    roomtemp_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
