/*
** ATOP - System & Process Monitor 
** 
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
** 
** This source-file contains functions to read all relevant system-level
** figures.
** ==========================================================================
** Author:      Gerlof Langeveld
** E-mail:      gerlof.langeveld@atoptool.nl
** Date:        November 1996
** LINUX-port:  June 2000
** --------------------------------------------------------------------------
** Copyright (C) 2000-2012 Gerlof Langeveld
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
** --------------------------------------------------------------------------
**
** $Log: photosyst.c,v $
** Revision 1.38  2010/11/19 07:40:40  gerlof
** Support of virtual disk vd... (kvm).
**
** Revision 1.37  2010/11/14 06:42:18  gerlof
** After opening /proc/cpuinfo, the file descriptor was not closed any more.
**
** Revision 1.36  2010/10/23 14:09:50  gerlof
** Add support for mmcblk disks (MMC/SD cardreaders)
** Credits: Anssi Hannula
**
** Revision 1.35  2010/05/18 19:20:30  gerlof
** Introduce CPU frequency and scaling (JC van Winkel).
**
** Revision 1.34  2010/04/23 12:19:35  gerlof
** Modified mail-address in header.
**
** Revision 1.33  2010/03/04 10:58:05  gerlof
** Added recognition of device-type /dev/fio...
**
** Revision 1.32  2010/03/04 10:52:47  gerlof
** Support I/O-statistics on logical volumes and MD devices.
**
** Revision 1.31  2009/12/17 11:59:16  gerlof
** Gather and display new counters: dirty cache and guest cpu usage.
**
** Revision 1.30  2008/02/25 13:47:00  gerlof
** Experimental code for HTTP statistics.
**
** Revision 1.29  2007/08/17 09:45:44  gerlof
** Experimental: gather info about HTTP statistics.
**
** Revision 1.28  2007/08/16 12:00:49  gerlof
** Add support for atopsar reporting.
** Gather more counters, mainly related to networking.
**
** Revision 1.27  2007/07/03 09:01:56  gerlof
** Support Apache-statistics.
**
** Revision 1.26  2007/02/13 10:32:28  gerlof
** Removal of external declarations.
**
** Revision 1.25  2007/02/13 09:29:57  gerlof
** Removal of external declarations.
**
** Revision 1.24  2007/01/22 14:57:12  gerlof
** Support of special disks used by virtual machines.
**
** Revision 1.23  2007/01/22 08:28:50  gerlof
** Support steal-time from /proc/stat.
**
** Revision 1.22  2006/11/13 13:48:20  gerlof
** Implement load-average counters, context-switches and interrupts.
**
** Revision 1.21  2006/01/30 09:14:16  gerlof
** Extend memory counters (a.o. page scans).
**
** Revision 1.20  2005/10/21 09:50:08  gerlof
** Per-user accumulation of resource consumption.
**
** Revision 1.19  2004/10/28 08:31:23  gerlof
** New counter: vm committed space
**
** Revision 1.18  2004/09/24 10:02:35  gerlof
** Wrong cpu-numbers for system level statistics.
**
** Revision 1.17  2004/05/07 05:27:37  gerlof
** Recognize new disk-names and support of diskname-modification.
**
** Revision 1.16  2004/05/06 09:53:31  gerlof
** Skip statistics of ram-disks.
**
** Revision 1.15  2004/05/06 09:46:14  gerlof
** Ported to kernel-version 2.6.
**
** Revision 1.14  2003/07/08 13:53:21  gerlof
** Cleanup code.
**
** Revision 1.13  2003/07/07 09:27:06  gerlof
** Cleanup code (-Wall proof).
**
** Revision 1.12  2003/06/30 11:30:37  gerlof
** Enlarge counters to 'long long'.
**
** Revision 1.11  2003/06/24 06:21:40  gerlof
** Limit number of system resource lines.
**
** Revision 1.10  2003/01/17 14:23:05  root
** Change-directory to /proc to optimize opening /proc-files
** via relative path-names i.s.o. absolute path-names.
**
** Revision 1.9  2003/01/14 07:50:26  gerlof
** Consider IPv6 counters on IP and UDP level (add them to the IPv4 counters).
**
** Revision 1.8  2002/07/24 11:13:38  gerlof
** Changed to ease porting to other UNIX-platforms.
**
** Revision 1.7  2002/07/11 09:12:41  root
** Parsing of /proc/meminfo made 2.5 proof.
**
** Revision 1.6  2002/07/10 05:00:21  root
** Counters pin/pout renamed to swin/swout (Linux conventions).
**
** Revision 1.5  2002/07/08 09:31:11  gerlof
** *** empty log message ***
**
** Revision 1.4  2002/07/02 07:36:45  gerlof
** *** empty log message ***
**
** Revision 1.3  2002/07/02 07:08:36  gerlof
** Recognize more disk driver types via regular expressions
**
** Revision 1.2  2002/01/22 13:40:11  gerlof
** Support for number of cpu's.
**
** Revision 1.1  2001/10/02 10:43:31  gerlof
** Initial revision
**
*/

static const char rcsid[] = "$Id: photosyst.c,v 1.38 2010/11/19 07:40:40 gerlof Exp $";

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// #define	_GNU_SOURCE
#include <sys/ipc.h>
#include <sys/shm.h>

#include "atop.h"
#include "photosyst.h"

#define	MAXCNT	64

/* return value of isdisk() */
#define	NONTYPE	0
#define	DSKTYPE	1
#define	MDDTYPE	2
#define	LVMTYPE	3

static int	isdisk(unsigned int, unsigned int,
			char *, struct perdsk *, int);

static struct ipv6_stats	ipv6_tmp;
static struct icmpv6_stats	icmpv6_tmp;
static struct udpv6_stats	udpv6_tmp;

struct v6tab {
	char 	*nam;
	count_t *val;
};

static struct v6tab 		v6tab[] = {
    {"Ip6InReceives",		&ipv6_tmp.Ip6InReceives,                     },
    {"Ip6InHdrErrors",		&ipv6_tmp.Ip6InHdrErrors,                    },
    {"Ip6InTooBigErrors",	&ipv6_tmp.Ip6InTooBigErrors,                 },
    {"Ip6InNoRoutes",		&ipv6_tmp.Ip6InNoRoutes,                     },
    {"Ip6InAddrErrors",		&ipv6_tmp.Ip6InAddrErrors,                   },
    {"Ip6InUnknownProtos",	&ipv6_tmp.Ip6InUnknownProtos,                },
    {"Ip6InTruncatedPkts",	&ipv6_tmp.Ip6InTruncatedPkts,                },
    {"Ip6InDiscards",		&ipv6_tmp.Ip6InDiscards,                     },
    {"Ip6InDelivers",		&ipv6_tmp.Ip6InDelivers,                     },
    {"Ip6OutForwDatagrams",	&ipv6_tmp.Ip6OutForwDatagrams,               },
    {"Ip6OutRequests",		&ipv6_tmp.Ip6OutRequests,                    },
    {"Ip6OutDiscards",		&ipv6_tmp.Ip6OutDiscards,                    },
    {"Ip6OutNoRoutes",		&ipv6_tmp.Ip6OutNoRoutes,                    },
    {"Ip6ReasmTimeout",		&ipv6_tmp.Ip6ReasmTimeout,                   },
    {"Ip6ReasmReqds",		&ipv6_tmp.Ip6ReasmReqds,                     },
    {"Ip6ReasmOKs",		&ipv6_tmp.Ip6ReasmOKs,                       },
    {"Ip6ReasmFails",		&ipv6_tmp.Ip6ReasmFails,                     },
    {"Ip6FragOKs",		&ipv6_tmp.Ip6FragOKs,                        },
    {"Ip6FragFails",		&ipv6_tmp.Ip6FragFails,                      },
    {"Ip6FragCreates",		&ipv6_tmp.Ip6FragCreates,                    },
    {"Ip6InMcastPkts",		&ipv6_tmp.Ip6InMcastPkts,                    },
    {"Ip6OutMcastPkts",		&ipv6_tmp.Ip6OutMcastPkts,                   },
 
    {"Icmp6InMsgs",		&icmpv6_tmp.Icmp6InMsgs,                     },
    {"Icmp6InErrors",		&icmpv6_tmp.Icmp6InErrors,                   },
    {"Icmp6InDestUnreachs",	&icmpv6_tmp.Icmp6InDestUnreachs,             },
    {"Icmp6InPktTooBigs",	&icmpv6_tmp.Icmp6InPktTooBigs,               },
    {"Icmp6InTimeExcds",	&icmpv6_tmp.Icmp6InTimeExcds,                },
    {"Icmp6InParmProblems",	&icmpv6_tmp.Icmp6InParmProblems,             },
    {"Icmp6InEchos",		&icmpv6_tmp.Icmp6InEchos,                    },
    {"Icmp6InEchoReplies",	&icmpv6_tmp.Icmp6InEchoReplies,              },
    {"Icmp6InGroupMembQueries",	&icmpv6_tmp.Icmp6InGroupMembQueries,         },
    {"Icmp6InGroupMembResponses",
				&icmpv6_tmp.Icmp6InGroupMembResponses,       },
    {"Icmp6InGroupMembReductions",
				&icmpv6_tmp.Icmp6InGroupMembReductions,      },
    {"Icmp6InRouterSolicits",	&icmpv6_tmp.Icmp6InRouterSolicits,           },
    {"Icmp6InRouterAdvertisements",
				&icmpv6_tmp.Icmp6InRouterAdvertisements,     },
    {"Icmp6InNeighborSolicits",	&icmpv6_tmp.Icmp6InNeighborSolicits,         },
    {"Icmp6InNeighborAdvertisements",
				&icmpv6_tmp.Icmp6InNeighborAdvertisements,   },
    {"Icmp6InRedirects",	&icmpv6_tmp.Icmp6InRedirects,                },
    {"Icmp6OutMsgs",		&icmpv6_tmp.Icmp6OutMsgs,                    },
    {"Icmp6OutDestUnreachs",	&icmpv6_tmp.Icmp6OutDestUnreachs,            },
    {"Icmp6OutPktTooBigs",	&icmpv6_tmp.Icmp6OutPktTooBigs,              },
    {"Icmp6OutTimeExcds",	&icmpv6_tmp.Icmp6OutTimeExcds,               },
    {"Icmp6OutParmProblems",	&icmpv6_tmp.Icmp6OutParmProblems,            },
    {"Icmp6OutEchoReplies",	&icmpv6_tmp.Icmp6OutEchoReplies,             },
    {"Icmp6OutRouterSolicits",	&icmpv6_tmp.Icmp6OutRouterSolicits,          },
    {"Icmp6OutNeighborSolicits",&icmpv6_tmp.Icmp6OutNeighborSolicits,        },
    {"Icmp6OutNeighborAdvertisements",
				&icmpv6_tmp.Icmp6OutNeighborAdvertisements,  },
    {"Icmp6OutRedirects",	&icmpv6_tmp.Icmp6OutRedirects,               },
    {"Icmp6OutGroupMembResponses",
				&icmpv6_tmp.Icmp6OutGroupMembResponses,      },
    {"Icmp6OutGroupMembReductions",
				&icmpv6_tmp.Icmp6OutGroupMembReductions,     },

    {"Udp6InDatagrams",		&udpv6_tmp.Udp6InDatagrams,                   },
    {"Udp6NoPorts",		&udpv6_tmp.Udp6NoPorts,                       },
    {"Udp6InErrors",		&udpv6_tmp.Udp6InErrors,                      },
    {"Udp6OutDatagrams",	&udpv6_tmp.Udp6OutDatagrams,                  },
};

static int	v6tab_entries = sizeof(v6tab)/sizeof(struct v6tab);

void
photosyst(struct sstat *si)
{
	register int	i, nr;
	count_t		cnts[MAXCNT];
	float		lavg1, lavg5, lavg15;
	FILE 		*fp;
	char		linebuf[1024], nam[64], origdir[1024];
	static char	part_stats = 1; /* per-partition statistics ? */
	unsigned int	major, minor;
	struct shm_info	shminfo;
#if	HTTPSTATS
	static int	wwwvalid = 1;
#endif

	memset(si, 0, sizeof(struct sstat));

	if ( getcwd(origdir, sizeof origdir) == NULL)
		cleanstop(53);

	if ( chdir("/proc") == -1)
		cleanstop(53);

	/*
	** gather various general statistics from the file /proc/stat and
	** store them in binary form
	*/
	if ( (fp = fopen("stat", "r")) != NULL)
	{
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			nr = sscanf(linebuf,
			            "%s   %lld %lld %lld %lld %lld %lld %lld "
			            "%lld %lld %lld %lld %lld %lld %lld %lld ",
			  	nam,
			  	&cnts[0],  &cnts[1],  &cnts[2],  &cnts[3],
			  	&cnts[4],  &cnts[5],  &cnts[6],  &cnts[7],
			  	&cnts[8],  &cnts[9],  &cnts[10], &cnts[11],
			  	&cnts[12], &cnts[13], &cnts[14]);

			if (nr < 2)		/* headerline ? --> skip */
				continue;

			if ( strcmp("cpu", nam) == EQ)
			{
				si->cpu.all.utime	= cnts[0];
				si->cpu.all.ntime	= cnts[1];
				si->cpu.all.stime	= cnts[2];
				si->cpu.all.itime	= cnts[3];

				if (nr > 5)	/* 2.6 kernel? */
				{
					si->cpu.all.wtime	= cnts[4];
					si->cpu.all.Itime	= cnts[5];
					si->cpu.all.Stime	= cnts[6];

					if (nr > 8)	/* steal support */
						si->cpu.all.steal = cnts[7];

					if (nr > 9)	/* guest support */
						si->cpu.all.guest = cnts[8];
				}
				continue;
			}

			if ( strncmp("cpu", nam, 3) == EQ)
			{
				i = atoi(&nam[3]);

				if (i >= MAXCPU)
				{
					fprintf(stderr,
						"cpu %s exceeds maximum of %d\n",
						nam, MAXCPU);
					continue;
				}

				si->cpu.cpu[i].cpunr	= i;
				si->cpu.cpu[i].utime	= cnts[0];
				si->cpu.cpu[i].ntime	= cnts[1];
				si->cpu.cpu[i].stime	= cnts[2];
				si->cpu.cpu[i].itime	= cnts[3];

				if (nr > 5)	/* 2.6 kernel? */
				{
					si->cpu.cpu[i].wtime	= cnts[4];
					si->cpu.cpu[i].Itime	= cnts[5];
					si->cpu.cpu[i].Stime	= cnts[6];

					if (nr > 8)	/* steal support */
						si->cpu.cpu[i].steal = cnts[7];

					if (nr > 9)	/* guest support */
						si->cpu.cpu[i].guest = cnts[8];
				}

				si->cpu.nrcpu++;
				continue;
			}

			if ( strcmp("ctxt", nam) == EQ)
			{
				si->cpu.csw	= cnts[0];
				continue;
			}

			if ( strcmp("intr", nam) == EQ)
			{
				si->cpu.devint	= cnts[0];
				continue;
			}

			if ( strcmp("processes", nam) == EQ)
			{
				si->cpu.nprocs	= cnts[0];
				continue;
			}

			if ( strcmp("swap", nam) == EQ)   /* < 2.6 */
			{
				si->mem.swins	= cnts[0];
				si->mem.swouts	= cnts[1];
				continue;
			}
		}

		fclose(fp);

		if (si->cpu.nrcpu == 0)
			si->cpu.nrcpu = 1;
	}

	/*
	** gather loadaverage values from the file /proc/loadavg and
	** store them in binary form
	*/
	if ( (fp = fopen("loadavg", "r")) != NULL)
	{
		if ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			if ( sscanf(linebuf, "%f %f %f",
				&lavg1, &lavg5, &lavg15) == 3)
			{
				si->cpu.lavg1	= lavg1;
				si->cpu.lavg5	= lavg5;
				si->cpu.lavg15	= lavg15;
			}
		}

		fclose(fp);
	}

	/*
	** gather frequency scaling info.
        ** sources (in order of preference): 
        **     /sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state
        ** or
        **     /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
        **     /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
        **
	** store them in binary form
	*/
        static char fn[80];
        int didone=0;

        for (i = 0; i < si->cpu.nrcpu; ++i)
        {
                long long f=0;

                sprintf(fn,
                   "/sys/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state",
                   i);

                if ((fp=fopen(fn, "r")) != 0)
                {
                        long long hits=0;
                        long long maxfreq=0;
                        long long cnt=0;
                        long long sum=0;

                        while (fscanf(fp, "%lld %lld", &f, &cnt) == 2)
                        {
                                f	/= 1000;
                                sum 	+= (f*cnt);
                                hits	+= cnt;

                                if (f > maxfreq)
                        		maxfreq=f;
                        	didone=1;
                        }

	                si->cpu.cpu[i].freqcnt.maxfreq	= maxfreq;
	                si->cpu.cpu[i].freqcnt.cnt	= sum;
	                si->cpu.cpu[i].freqcnt.ticks	= hits;

                        fclose(fp);
                }
		else
		{    // governor statistics not available
                     sprintf(fn,  
                      "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq",
		      i);

                        if ((fp=fopen(fn, "r")) != 0)
                        {
                                if (fscanf(fp, "%lld", &f) == 1)
                                {
  					// convert KHz to MHz
	                                si->cpu.cpu[i].freqcnt.maxfreq =f/1000; 
                                }

                                fclose(fp);
                        }
                        else 
                        {
	                        si->cpu.cpu[i].freqcnt.maxfreq=0;
                        }

                       sprintf(fn,  
                       "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
		       i);
        
                        if ((fp=fopen(fn, "r")) != 0)
                        {
                                if (fscanf(fp, "%lld", &f) == 1)
                                {
   					// convert KHz to MHz
                                        si->cpu.cpu[i].freqcnt.cnt   = f/1000;
                                        si->cpu.cpu[i].freqcnt.ticks = 0;
                                        didone=1;
                                }

                                fclose(fp);
                        }
                        else
                        {
                                si->cpu.cpu[i].freqcnt.cnt	= 0;
                                si->cpu.cpu[i].freqcnt.ticks 	= 0;
                        }
                }
        } // for all CPUs

        if (!didone)     // did not get processor freq statistics.
                         // use /proc/cpuinfo
        {
	        if ( (fp = fopen("cpuinfo", "r")) != NULL)
                {
                        // get information from the lines
                        // processor\t: 0
                        // cpu MHz\t\t: 800.000
                        
                        int cpuno=-1;

		        while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
                        {
                                if (memcmp(linebuf, "processor", 9)== EQ)
					sscanf(linebuf, "%*s %*s %d", &cpuno);

                                if (memcmp(linebuf, "cpu MHz", 7) == EQ)
				{
                                        if (cpuno >= 0 && cpuno < si->cpu.nrcpu)
					{
						sscanf(linebuf,
							"%*s %*s %*s %lld",
                                	     		&(si->cpu.cpu[cpuno].freqcnt.cnt));
					}
                                }
                        }

			fclose(fp);
                }

        }

	/*
	** gather virtual memory statistics from the file /proc/vmstat and
	** store them in binary form (>= kernel 2.6)
	*/
	if ( (fp = fopen("vmstat", "r")) != NULL)
	{
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			nr = sscanf(linebuf, "%s %lld", nam, &cnts[0]);

			if (nr < 2)		/* headerline ? --> skip */
				continue;

			if ( strcmp("pswpin", nam) == EQ)
			{
				si->mem.swins   = cnts[0];
				continue;
			}

			if ( strcmp("pswpout", nam) == EQ)
			{
				si->mem.swouts  = cnts[0];
				continue;
			}

			if ( strncmp("pgscan_", nam, 7) == EQ)
			{
				si->mem.pgscans += cnts[0];
				continue;
			}

			if ( strncmp("pgsteal_", nam, 8) == EQ)
			{
				si->mem.pgsteal += cnts[0];
				continue;
			}

			if ( strcmp("allocstall", nam) == EQ)
			{
				si->mem.allocstall = cnts[0];
				continue;
			}
		}

		fclose(fp);
	}

	/*
	** gather memory-related statistics from the file /proc/meminfo and
	** store them in binary form
	**
	** in the file /proc/meminfo a 2.4 kernel starts with two lines
	** headed by the strings "Mem:" and "Swap:" containing all required
	** fields, except proper value for page cache
        ** if these lines are present we try to skip parsing the rest
	** of the lines; if these lines are not present we should get the
	** required field from other lines
	*/
	si->mem.physmem	 	= (count_t)-1; 
	si->mem.freemem		= (count_t)-1;
	si->mem.buffermem	= (count_t)-1;
	si->mem.cachemem  	= (count_t)-1;
	si->mem.slabmem		= (count_t) 0;
	si->mem.slabreclaim	= (count_t) 0;
	si->mem.shmem 		= (count_t) 0;
	si->mem.totswap  	= (count_t)-1;
	si->mem.freeswap 	= (count_t)-1;
	si->mem.committed 	= (count_t) 0;

	if ( (fp = fopen("meminfo", "r")) != NULL)
	{
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			nr = sscanf(linebuf,
				"%s %lld %lld %lld %lld %lld %lld %lld "
			        "%lld %lld %lld\n",
				nam,
			  	&cnts[0],  &cnts[1],  &cnts[2],  &cnts[3],
			  	&cnts[4],  &cnts[5],  &cnts[6],  &cnts[7],
			  	&cnts[8],  &cnts[9]);

			if (nr < 2)		/* headerline ? --> skip */
				continue;

			if ( strcmp("Mem:", nam) == EQ)
			{
				si->mem.physmem	 	= cnts[0] / pagesize; 
				si->mem.freemem		= cnts[2] / pagesize;
				si->mem.buffermem	= cnts[4] / pagesize;
			}
			else	if ( strcmp("Swap:", nam) == EQ)
				{
					si->mem.totswap  = cnts[0] / pagesize;
					si->mem.freeswap = cnts[2] / pagesize;
				}
			else	if (strcmp("Cached:", nam) == EQ)
				{
					if (si->mem.cachemem  == (count_t)-1)
					{
						si->mem.cachemem  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("Dirty:", nam) == EQ)
				{
					si->mem.cachedrt  =
							cnts[0]*1024/pagesize;
				}
			else	if (strcmp("MemTotal:", nam) == EQ)
				{
					if (si->mem.physmem  == (count_t)-1)
					{
						si->mem.physmem  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("MemFree:", nam) == EQ)
				{
					if (si->mem.freemem  == (count_t)-1)
					{
						si->mem.freemem  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("Buffers:", nam) == EQ)
				{
					if (si->mem.buffermem  == (count_t)-1)
					{
						si->mem.buffermem  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("Shmem:", nam) == EQ)
				{
					si->mem.shmem = cnts[0]*1024/pagesize;
				}
			else	if (strcmp("SwapTotal:", nam) == EQ)
				{
					if (si->mem.totswap  == (count_t)-1)
					{
						si->mem.totswap  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("SwapFree:", nam) == EQ)
				{
					if (si->mem.freeswap  == (count_t)-1)
					{
						si->mem.freeswap  =
							cnts[0]*1024/pagesize;
					}
				}
			else	if (strcmp("Slab:", nam) == EQ)
				{
					si->mem.slabmem = cnts[0]*1024/pagesize;
				}
			else	if (strcmp("SReclaimable:", nam) == EQ)
				{
					si->mem.slabreclaim = cnts[0]*1024/
								pagesize;
				}
			else	if (strcmp("Committed_AS:", nam) == EQ)
				{
					si->mem.committed = cnts[0]*1024/
								pagesize;
				}
			else	if (strcmp("CommitLimit:", nam) == EQ)
				{
					si->mem.commitlim = cnts[0]*1024/
								pagesize;
				}
			else	if (strcmp("HugePages_Total:", nam) == EQ)
				{
					si->mem.tothugepage = cnts[0];
				}
			else	if (strcmp("HugePages_Free:", nam) == EQ)
				{
					si->mem.freehugepage = cnts[0];
				}
			else	if (strcmp("Hugepagesize:", nam) == EQ)
				{
					si->mem.hugepagesz = cnts[0]*1024;
				}
		}

		fclose(fp);
	}

	/*
	** gather vmware-related statistics from /proc/vmmemctl
	** (only present if balloon driver enabled)
	*/ 
	si->mem.vmwballoon = (count_t) 0;

	if ( (fp = fopen("vmmemctl", "r")) != NULL)
	{
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			nr = sscanf(linebuf, "%s %lld ", nam, &cnts[0]);

			if ( strcmp("current:", nam) == EQ)
			{
				si->mem.vmwballoon = cnts[0];
				break;
			}
		}

		fclose(fp);
	}

	/*
	** gather network-related statistics
 	** 	- interface stats from the file /proc/net/dev
 	** 	- IPv4      stats from the file /proc/net/snmp
 	** 	- IPv6      stats from the file /proc/net/snmp6
	*/

	/*
	** interface statistics
	*/
	if ( (fp = fopen("net/dev", "r")) != NULL)
	{
		char *cp;

		i = 0;

		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			if ( (cp = strchr(linebuf, ':')) != NULL)
				*cp = ' ';      /* substitute ':' by space */

			nr = sscanf(linebuf,
                                    "%15s %lld %lld %lld %lld %lld %lld %lld "
                                    "%lld %lld %lld %lld %lld %lld %lld %lld "
                                    "%lld\n",
				  si->intf.intf[i].name,
				&(si->intf.intf[i].rbyte),
				&(si->intf.intf[i].rpack),
				&(si->intf.intf[i].rerrs),
				&(si->intf.intf[i].rdrop),
				&(si->intf.intf[i].rfifo),
				&(si->intf.intf[i].rframe),
				&(si->intf.intf[i].rcompr),
				&(si->intf.intf[i].rmultic),
				&(si->intf.intf[i].sbyte),
				&(si->intf.intf[i].spack),
				&(si->intf.intf[i].serrs),
				&(si->intf.intf[i].sdrop),
				&(si->intf.intf[i].sfifo),
				&(si->intf.intf[i].scollis),
				&(si->intf.intf[i].scarrier),
				&(si->intf.intf[i].scompr));

			if (nr == 17)	/* skip header & lines without stats */
			{
				if (++i >= MAXINTF-1)
					break;
			}
		}

		si->intf.intf[i].name[0] = '\0'; /* set terminator for table */
		si->intf.nrintf = i;

		fclose(fp);
	}

	/*
	** IP version 4 statistics
	*/
	if ( (fp = fopen("net/snmp", "r")) != NULL)
	{
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
			nr = sscanf(linebuf,
			 "%s   %lld %lld %lld %lld %lld %lld %lld %lld %lld "
			 "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
			 "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
			 "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
			 "%lld\n",
				nam,
				&cnts[0],  &cnts[1],  &cnts[2],  &cnts[3],
				&cnts[4],  &cnts[5],  &cnts[6],  &cnts[7],
				&cnts[8],  &cnts[9],  &cnts[10], &cnts[11],
				&cnts[12], &cnts[13], &cnts[14], &cnts[15],
				&cnts[16], &cnts[17], &cnts[18], &cnts[19],
				&cnts[20], &cnts[21], &cnts[22], &cnts[23],
				&cnts[24], &cnts[25], &cnts[26], &cnts[27],
				&cnts[28], &cnts[29], &cnts[30], &cnts[31],
				&cnts[32], &cnts[33], &cnts[34], &cnts[35],
				&cnts[36], &cnts[37], &cnts[38], &cnts[39]);

			if (nr < 2)		/* headerline ? --> skip */
				continue;

			if ( strcmp("Ip:", nam) == 0)
			{
				memcpy(&si->net.ipv4, cnts,
						sizeof si->net.ipv4);
				continue;
			}
	
			if ( strcmp("Icmp:", nam) == 0)
			{
				memcpy(&si->net.icmpv4, cnts,
						sizeof si->net.icmpv4);
				continue;
			}
	
			if ( strcmp("Tcp:", nam) == 0)
			{
				memcpy(&si->net.tcp, cnts,
						sizeof si->net.tcp);
				continue;
			}
	
			if ( strcmp("Udp:", nam) == 0)
			{
				memcpy(&si->net.udpv4, cnts,
						sizeof si->net.udpv4);
				continue;
			}
		}
	
		fclose(fp);
	}

	/*
	** IP version 6 statistics
	*/
	memset(&ipv6_tmp,   0, sizeof ipv6_tmp);
	memset(&icmpv6_tmp, 0, sizeof icmpv6_tmp);
	memset(&udpv6_tmp,  0, sizeof udpv6_tmp);

	if ( (fp = fopen("net/snmp6", "r")) != NULL)
	{
		count_t	countval;
		int	cur = 0;

		/*
		** one name-value pair per line
		*/
		while ( fgets(linebuf, sizeof(linebuf), fp) != NULL)
		{
		   	nr = sscanf(linebuf, "%s %lld", nam, &countval);

			if (nr < 2)		/* unexpected line ? --> skip */
				continue;

		   	if (strcmp(v6tab[cur].nam, nam) == 0)
		   	{
		   		*(v6tab[cur].val) = countval;
		   	}
		   	else
		   	{
		   		for (cur=0; cur < v6tab_entries; cur++)
					if (strcmp(v6tab[cur].nam, nam) == 0)
						break;

				if (cur < v6tab_entries) /* found ? */
		   			*(v6tab[cur].val) = countval;
			}

			if (++cur >= v6tab_entries)
				cur = 0;
		}

		memcpy(&si->net.ipv6,   &ipv6_tmp,   sizeof ipv6_tmp);
		memcpy(&si->net.icmpv6, &icmpv6_tmp, sizeof icmpv6_tmp);
		memcpy(&si->net.udpv6,  &udpv6_tmp,  sizeof udpv6_tmp);

		fclose(fp);
	}

	/*
	** check if extended partition-statistics are provided < kernel 2.6
	*/
	if ( part_stats && (fp = fopen("partitions", "r")) != NULL)
	{
		char diskname[256];

		i = 0;

		while ( fgets(linebuf, sizeof(linebuf), fp) )
		{
			nr = sscanf(linebuf,
			      "%*d %*d %*d %255s %lld %*d %lld %*d "
			      "%lld %*d %lld %*d %*d %lld %lld",
			        diskname,
				&(si->dsk.dsk[i].nread),
				&(si->dsk.dsk[i].nrsect),
				&(si->dsk.dsk[i].nwrite),
				&(si->dsk.dsk[i].nwsect),
				&(si->dsk.dsk[i].io_ms),
				&(si->dsk.dsk[i].avque) );

			/*
			** check if this line concerns the entire disk
			** or just one of the partitions of a disk (to be
			** skipped)
			*/
			if (nr == 7)	/* full stats-line ? */
			{
				if ( isdisk(0, 0, diskname,
				                 &(si->dsk.dsk[i]),
						 MAXDKNAM) != DSKTYPE)
				       continue;
			
				if (++i >= MAXDSK-1)
					break;
			}
		}

		si->dsk.dsk[i].name[0] = '\0'; /* set terminator for table */
		si->dsk.ndsk = i;

		fclose(fp);

		if (i == 0)
			part_stats = 0;	/* do not try again for next cycles */
	}


	/*
	** check if disk-statistics are provided (kernel 2.6 onwards)
	*/
	if ( (fp = fopen("diskstats", "r")) != NULL)
	{
		char 		diskname[256];
		struct perdsk	tmpdsk;

		si->dsk.ndsk = 0;
		si->dsk.nmdd = 0;
		si->dsk.nlvm = 0;

		while ( fgets(linebuf, sizeof(linebuf), fp) )
		{
			nr = sscanf(linebuf,
			      "%d %d %255s %lld %*d %lld %*d "
			      "%lld %*d %lld %*d %*d %lld %lld",
				&major, &minor, diskname,
				&tmpdsk.nread,  &tmpdsk.nrsect,
				&tmpdsk.nwrite, &tmpdsk.nwsect,
				&tmpdsk.io_ms,  &tmpdsk.avque );

			/*
			** check if this line concerns the entire disk
			** or just one of the partitions of a disk (to be
			** skipped)
			*/
			if (nr == 9)	/* full stats-line ? */
			{
				switch ( isdisk(major, minor, diskname,
							 &tmpdsk, MAXDKNAM) )
				{
				   case NONTYPE:
				       continue;

				   case DSKTYPE:
					if (si->dsk.ndsk < MAXDSK-1)
					  si->dsk.dsk[si->dsk.ndsk++] = tmpdsk;
					break;

				   case MDDTYPE:
					if (si->dsk.nmdd < MAXMDD-1)
					  si->dsk.mdd[si->dsk.nmdd++] = tmpdsk;
					break;

				   case LVMTYPE:
					if (si->dsk.nlvm < MAXLVM-1)
					  si->dsk.lvm[si->dsk.nlvm++] = tmpdsk;
					break;
				}
			}
		}

		/*
 		** set terminator for table
 		*/
		si->dsk.dsk[si->dsk.ndsk].name[0] = '\0';
		si->dsk.mdd[si->dsk.nmdd].name[0] = '\0';
		si->dsk.lvm[si->dsk.nlvm].name[0] = '\0'; 

		fclose(fp);
	}

	/*
 	** get information about the shared memory statistics
	*/
	if ( shmctl(0, SHM_INFO, (struct shmid_ds *)&shminfo) != -1)
	{
		si->mem.shmrss = shminfo.shm_rss;
		si->mem.shmswp = shminfo.shm_swp;
	}

	if ( chdir(origdir) == -1)
		cleanstop(53);

	/*
	** fetch application-specific counters
	*/
#if	HTTPSTATS
	if ( wwwvalid)
		wwwvalid = getwwwstat(80, &(si->www));
#endif
}

/*
** set of subroutines to determine which disks should be monitored
** and to translate name strings into (shorter) name strings
*/
static void
nullmodname(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	strncpy(px->name, curname, maxlen-1);
	*(px->name+maxlen-1) = 0;
}

static void
abbrevname1(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	char	cutype[128];
	int	hostnum, busnum, targetnum, lunnum;

	sscanf(curname, "%[^/]/host%d/bus%d/target%d/lun%d",
			cutype, &hostnum, &busnum, &targetnum, &lunnum);

	snprintf(px->name, maxlen, "%c-h%db%dt%d", 
			cutype[0], hostnum, busnum, targetnum);
}

/*
** recognize LVM logical volumes
*/
#define	NUMDMHASH	64
#define	DMHASH(x,y)	(((x)+(y))%NUMDMHASH)	
#define	MAPDIR		"/dev/mapper"

struct devmap {
	unsigned int	major;
	unsigned int	minor;
	char		name[MAXDKNAM];
	struct devmap	*next;
};

static void
lvmmapname(unsigned int major, unsigned int minor,
		char *curname, struct perdsk *px, int maxlen)
{
	static int		firstcall = 1;
	static struct devmap	*devmaps[NUMDMHASH], *dmp;
	int			hashix;

	/*
 	** setup a list of major-minor numbers of dm-devices with their
	** corresponding name
	*/
	if (firstcall)
	{
		DIR		*dirp;
		struct dirent	*dentry;
		struct stat	statbuf;
		char		path[64];

		if ( (dirp = opendir(MAPDIR)) )
		{
			/*
	 		** read every directory-entry and search for
			** block devices
			*/
			while ( (dentry = readdir(dirp)) )
			{
				snprintf(path, sizeof path, "%s/%s", 
						MAPDIR, dentry->d_name);

				if ( stat(path, &statbuf) == -1 )
					continue;

				if ( ! S_ISBLK(statbuf.st_mode) )
					continue;
				/*
 				** allocate struct to store name
				*/
				if ( !(dmp = malloc(sizeof (struct devmap))))
					continue;

				/*
 				** store info in hash list
				*/
				strncpy(dmp->name, dentry->d_name, MAXDKNAM);
				dmp->name[MAXDKNAM-1] = 0;
				dmp->major 	= major(statbuf.st_rdev);
				dmp->minor 	= minor(statbuf.st_rdev);

				hashix = DMHASH(dmp->major, dmp->minor);

				dmp->next	= devmaps[hashix];

				devmaps[hashix]	= dmp;
			}

			closedir(dirp);
		}

		firstcall = 0;
	}

	/*
 	** find info in hash list
	*/
	hashix  = DMHASH(major, minor);
	dmp	= devmaps[hashix];

	while (dmp)
	{
		if (dmp->major == major && dmp->minor == minor)
		{
			/*
		 	** info found in hash list; fill proper name
			*/
			strncpy(px->name, dmp->name, maxlen-1);
			*(px->name+maxlen-1) = 0;
			return;
		}

		dmp = dmp->next;
	}

	/*
	** info not found in hash list; fill original name
	*/
	strncpy(px->name, curname, maxlen-1);
	*(px->name+maxlen-1) = 0;
}

/*
** this table is used in the function isdisk()
**
** table contains the names (in regexp format) of disks
** to be recognized, together with a function to modify
** the name-strings (i.e. to abbreviate long strings);
** some frequently found names (like 'loop' and 'ram')
** are also recognized to skip them as fast as possible
*/
static struct {
	char 	*regexp;
	regex_t	compreg;
	void	(*modname)(unsigned int, unsigned int,
				char *, struct perdsk *, int);
	int	retval;
} validdisk[] = {
	{ "^ram[0-9][0-9]*$",			{0},  (void *)0,   NONTYPE, },
	{ "^loop[0-9][0-9]*$",			{0},  (void *)0,   NONTYPE, },
	{ "^sd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^dm-[0-9][0-9]*$",			{0},  lvmmapname,  LVMTYPE, },
	{ "^md[0-9][0-9]*$",			{0},  nullmodname, MDDTYPE, },
	{ "^vd[a-z][a-z]*$",                    {0},  nullmodname, DSKTYPE, },
	{ "^hd[a-z]$",				{0},  nullmodname, DSKTYPE, },
	{ "^rd/c[0-9][0-9]*d[0-9][0-9]*$",	{0},  nullmodname, DSKTYPE, },
	{ "^cciss/c[0-9][0-9]*d[0-9][0-9]*$",	{0},  nullmodname, DSKTYPE, },
	{ "^fio[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "/host.*/bus.*/target.*/lun.*/disc",	{0},  abbrevname1, DSKTYPE, },
	{ "^xvd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^dasd[a-z][a-z]*$",			{0},  nullmodname, DSKTYPE, },
	{ "^mmcblk[0-9][0-9]*$",		{0},  nullmodname, DSKTYPE, },
	{ "^emcpower[a-z][a-z]*$",		{0},  nullmodname, DSKTYPE, },
};

static int
isdisk(unsigned int major, unsigned int minor,
           char *curname, struct perdsk *px, int maxlen)
{
	static int	firstcall = 1;
	register int	i;

	if (firstcall)		/* compile the regular expressions */
	{
		for (i=0; i < sizeof validdisk/sizeof validdisk[0]; i++)
			regcomp(&validdisk[i].compreg, validdisk[i].regexp,
								REG_NOSUB);
		firstcall = 0;
	}

	/*
	** try to recognize one of the compiled regular expressions
	*/
	for (i=0; i < sizeof validdisk/sizeof validdisk[0]; i++)
	{
		if (regexec(&validdisk[i].compreg, curname, 0, NULL, 0) == 0)
		{
			/*
			** name-string recognized; modify name-string
			*/
			if (validdisk[i].retval != NONTYPE)
				(*validdisk[i].modname)(major, minor,
						curname, px, maxlen);

			return validdisk[i].retval;
		}
	}

	return NONTYPE;
}

/*
** LINUX SPECIFIC:
** Determine boot-time of this system (as number of jiffies since 1-1-1970).
*/
unsigned long long
getbootlinux(long hertz)
{
	int    		 	cpid;
	char  	  		tmpbuf[1280];
	FILE    		*fp;
	unsigned long 		startticks;
	unsigned long long	bootjiffies = 0;
	struct timespec		ts;

	/*
	** dirty hack to get the boottime, since the
	** Linux 2.6 kernel (2.6.5) does not return a proper
	** boottime-value with the times() system call   :-(
	*/
	if ( (cpid = fork()) == 0 )
	{
		/*
		** child just waiting to be killed by parent
		*/
		pause();
	}
	else
	{
		/*
		** parent determines start-time (in jiffies since boot) 
		** of the child and calculates the boottime in jiffies
		** since 1-1-1970
		*/
		(void) clock_gettime(CLOCK_REALTIME, &ts);	// get current
		bootjiffies = 1LL * ts.tv_sec  * hertz +
		              1LL * ts.tv_nsec * hertz / 1000000000LL;

		snprintf(tmpbuf, sizeof tmpbuf, "/proc/%d/stat", cpid);

		if ( (fp = fopen(tmpbuf, "r")) != NULL)
		{
			if ( fscanf(fp, "%*d (%*[^)]) %*c %*d %*d %*d %*d "
			                "%*d %*d %*d %*d %*d %*d %*d %*d "
			                "%*d %*d %*d %*d %*d %*d %lu",
			                &startticks) == 1)
			{
				bootjiffies -= startticks;
			}

			fclose(fp);
		}

		/*
		** kill the child and get rid of the zombie
		*/
		kill(cpid, SIGKILL);
		(void) wait((int *)0);
	}

	return bootjiffies;
}

#if	HTTPSTATS
/*
** retrieve statistics from local HTTP daemons
** via http://localhost/server-status?auto
*/
int
getwwwstat(unsigned short port, struct wwwstat *wp)
{
	int 			sockfd, tobefound;
	FILE			*sockfp;
	struct sockaddr_in	sockname;
	char			linebuf[4096];
	char			label[512];
	long long		value;

	memset(wp, 0, sizeof *wp);

	/*
	** allocate a socket and connect to the local HTTP daemon
	*/
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return 0;

	sockname.sin_family		= AF_INET;
	sockname.sin_addr.s_addr	= htonl(INADDR_LOOPBACK);
	sockname.sin_port		= htons(port);

	if ( connect(sockfd, (struct sockaddr *) &sockname,
						sizeof sockname) == -1)
	{
		close(sockfd);
		return 0;
	}

	/*
	** write a GET-request for /server-status
	*/
	if ( write(sockfd, HTTPREQ, sizeof HTTPREQ) < sizeof HTTPREQ)
	{
		close(sockfd);
		return 0;
	}

	/*
	** remap socket descriptor to a stream to allow stdio calls
	*/
	sockfp = fdopen(sockfd, "r+");

	/*
	** read response line by line
	*/
	tobefound = 5;		/* number of values to be searched */

	while ( fgets(linebuf, sizeof linebuf, sockfp) && tobefound)
	{
		/*
		** handle line containing status code
		*/
		if ( strncmp(linebuf, "HTTP/", 5) == 0)
		{
			sscanf(linebuf, "%511s %lld %*s\n", label, &value);

			if (value != 200)	/* HTTP-request okay? */
			{
				fclose(sockfp);
				close(sockfd);
				return 0;
			}

			continue;
		}

		/*
		** decode line and search for the required counters
		*/
		if (sscanf(linebuf, "%511[^:]: %lld\n", label, &value) == 2)
		{
			if ( strcmp(label, "Total Accesses") == 0)
			{
				wp->accesses = value;
				tobefound--;
			}

			if ( strcmp(label, "Total kBytes") == 0)
			{
				wp->totkbytes = value;
				tobefound--;
			}

			if ( strcmp(label, "Uptime") == 0)
			{
				wp->uptime = value;
				tobefound--;
			}

			if ( strcmp(label, "BusyWorkers") == 0)
			{
				wp->bworkers = value;
				tobefound--;
			}

			if ( strcmp(label, "IdleWorkers") == 0)
			{
				wp->iworkers = value;
				tobefound--;
			}
		}
	}

	fclose(sockfp);
	close(sockfd);

	return 1;
}
#endif
