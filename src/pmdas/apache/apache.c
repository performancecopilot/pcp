/*
 * Apache PMDA
 *
 * Copyright (C) 2000 Michal Kara.  All Rights Reserved.
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

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"

#include "http_lib.h"

static char location[260];

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* total_accesses */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }
    },

/* total_kbytes */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0) }
    },

/* uptime */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }
    },

/* requests_per_sec */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, -1, 1,  0, PM_TIME_SEC, PM_COUNT_ONE) }
    },

/* bytes_per_sec */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, -1, 0,  PM_SPACE_BYTE, PM_TIME_SEC, 0) }
    },

/* bytes_per_request */
    { NULL, 
      { PMDA_PMID(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, 0, -1,  PM_SPACE_BYTE, 0, PM_COUNT_ONE) }
    },

/* busy_servers */
    { NULL, 
      { PMDA_PMID(0,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* idle_servers */
    { NULL, 
      { PMDA_PMID(0,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_waiting */
    { NULL, 
      { PMDA_PMID(0,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_starting */
    { NULL, 
      { PMDA_PMID(0,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_reading */
    { NULL, 
      { PMDA_PMID(0,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_writing_reply */
    { NULL, 
      { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_keepalive */
    { NULL, 
      { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_dns_lookup */
    { NULL, 
      { PMDA_PMID(0,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_logging */
    { NULL, 
      { PMDA_PMID(0,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_finishing */
    { NULL, 
      { PMDA_PMID(0,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    },

/* sb_open_slot */
    { NULL, 
      { PMDA_PMID(0,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
				PMDA_PMUNITS(0, 0, 1,  0, 0, PM_COUNT_ONE) }
    }
};

/*
 * To speed everything up, the PMDA is caching the data. They are refreshed if
 * older than one second.
 */

typedef struct {
	time_t timestamp;
	int timeout; // There was a timeout
	unsigned long total_accesses,total_kbytes,uptime;
	double requests_per_sec,bytes_per_sec;
	unsigned long bytes_per_request,busy_servers,idle_servers;
	unsigned long sb_waiting,sb_starting,sb_reading;
	unsigned long sb_writing_reply,sb_keepalive,sb_dns_lookup;
	unsigned long sb_logging,sb_finishing,sb_open_slot;
} apacheData;

static apacheData data;

// Dummy signal handler
void dummy(int a)
{
	data.timeout = 1;
	kill(getpid(),SIGINT);
}

// Dummy signal handler
void dummy2(int a)
{
}

/*
 * Refresh data. Returns 1 of OK, 0 on error.
 */
static int refreshData()
{
	char *res;
	int len;
	char *s,*s2,*s3;
	int r;

//	fprintf(stderr,"Doing httpget - server='%s', location='%s'\n",http_server,location);

	// Setup timeout
	signal(SIGALRM,dummy);
	signal(SIGINT,dummy2);
	data.timeout = 0;
	alarm(1);

	res = NULL;
	r = http_get(location,&res,&len,NULL);
	alarm(0); // Clear alarm

	if (r != OK200) {
		fprintf(stderr,"Cannot get stats: library error #%d\n",r);
		if (data.timeout) data.timestamp = time(NULL);  // Don't try again too soon
		if (res) free(res);
		return(0); // Failed!
	}

	// Reset values
	memset(&data,0,sizeof(data));

	for(s=res;*s;) {
		s2 = s;
		s3 = NULL;
		for(;(*s)&&(*s!=10);s++) {
			if (*s == ':') {
				s3 = s+1;
				if (*s3) {
					*s3++ = 0;
					s++;
				}
			}
		}

		if (*s == 10) *s++ = 0;

		if (strcmp(s2,"CPULoad:") == 0) ;
		else if (strcmp(s2,"Total Accesses:") == 0) {
			data.total_accesses = atoi(s3);
		}
		else if (strcmp(s2,"Total kBytes:") == 0) {
			data.total_kbytes = atoi(s3);
		}
		else if (strcmp(s2,"Uptime:") == 0) {
			data.uptime = atoi(s3);
		}
		else if (strcmp(s2,"ReqPerSec:") == 0) {
			data.requests_per_sec = atof(s3);
		}
		else if (strcmp(s2,"BytesPerSec:") == 0) {
			data.bytes_per_sec = atof(s3);
		}
		else if (strcmp(s2,"BytesPerReq:") == 0) {
			data.bytes_per_request = atoi(s3);
		}
		else if ((strcmp(s2,"BusyServers:") == 0) ||
			 (strcmp(s2,"BusyWorkers:") == 0))
		{
			data.busy_servers = atoi(s3);
		}
		else if ((strcmp(s2,"IdleServers:") == 0) ||
			 (strcmp(s2,"IdleWorkers:") == 0))
		{
			data.idle_servers = atoi(s3);
		}
		else if (strcmp(s2,"Scoreboard:") == 0) {
			while(*s3) {
				switch(*s3) {
				case '_':
					data.sb_waiting++;
					break;
				case 'S':
					data.sb_starting++;
					break;
				case 'R':
					data.sb_reading++;
					break;
				case 'W':
					data.sb_writing_reply++;
					break;
				case 'K':
					data.sb_keepalive++;
					break;
				case 'D':
					data.sb_dns_lookup++;
					break;
				case 'L':
					data.sb_logging++;
					break;
				case 'G':
					data.sb_finishing++;
					break;
				case '.':
					data.sb_open_slot++;
					break;
				default:
					fprintf(stderr,"WARNING: Unknown scoreboard charcter '%c'\n",*s3);
				}
				s3++;
			}
		}
		else {
			fprintf(stderr,"WARNING: Unknown value name '%s'!\n",s2);
		}
	}

	data.timestamp = time(NULL);

	if (res) free(res);
	return(1);
}

/*
 * callback provided to pmdaFetch
 */
static int
apache_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
	__pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

	if (idp->cluster != 0) return PM_ERR_PMID;
	else if (inst != PM_IN_NULL) return PM_ERR_INST;

	// Check timestamp
	if (time(NULL) != data.timestamp) {
		if (!refreshData()) return(PM_ERR_AGAIN);
	}

	if (data.timeout) return(PM_ERR_AGAIN);

	switch(idp->item) {
		case 0:
			atom->ul = data.total_accesses;
			break;
		case 1:
			atom->ul = data.total_kbytes;
			break;
		case 2:
			atom->ul = data.uptime;
			break;
		case 3:
			atom->d = data.requests_per_sec;
			break;
		case 4:
			atom->d = data.bytes_per_sec;
			break;
		case 5:
			atom->ul = data.bytes_per_request;
			break;
		case 6:
			atom->ul = data.busy_servers;
			break;
		case 7:
			atom->ul = data.idle_servers;
			break;
		case 8:
			atom->ul = data.sb_waiting;
			break;
		case 9:
			atom->ul = data.sb_starting;
			break;
		case 10:
			atom->ul = data.sb_reading;
			break;
		case 11:
			atom->ul = data.sb_writing_reply;
			break;
		case 12:
			atom->ul = data.sb_keepalive;
			break;
		case 13:
			atom->ul = data.sb_dns_lookup;
			break;
		case 14:
			atom->ul = data.sb_logging;
			break;
		case 15:
			atom->ul = data.sb_finishing;
			break;
		case 16:
			atom->ul = data.sb_open_slot;
			break;
		default:
			return PM_ERR_PMID;
	}

	return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
apache_init(pmdaInterface *dp)
{
    pmdaSetFetchCallBack(dp, apache_fetchCallBack);

    pmdaInit(dp, NULL, 0, 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -s server    ask server <server>\n"
	  "  -p path      use given path\n",
	      stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			err = 0;
    int			c = 0;
    pmdaInterface	desc;
    char		*p;
    char		mypath[MAXPATHLEN];


    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    snprintf(mypath, sizeof(mypath),
		"%s/apache/help", pmGetConfig("PCP_PMDAS_DIR"));
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, APACHE,
		"apache.log", mypath);

		http_server = "localhost";
		http_port = 80;
		http_proxy_server = NULL;
		http_proxy_port = 3128;
		strcpy(location,"/server-status?auto");
    while((c = pmdaGetOpt(argc, argv, "D:d:l:s:o:?", &desc, &err)) != EOF) {
			switch(c) {
			case 's':
				http_server = optarg;
				break;
			case 'l':
				if (optarg[0] == '/') optarg++;
				snprintf(location,256,"%s?auto",optarg);
				break;
			default:
				usage();
				exit(1);
			}
		}

    pmdaOpenLog(&desc);
    apache_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
    /*NOTREACHED*/
}
