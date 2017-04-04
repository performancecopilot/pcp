/*
** ATOP - System & Process Monitor 
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** This source-file contains the Linux-specific functions to calculate
** figures to be visualized.
**
** Copyright (C) 2009 	JC van Winkel
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
*/
#define MAXITEMS 80    /* The maximum number of items per line */

/*
 * structure for extra parameters for system related data
*/
typedef struct {
        count_t		totut;
        count_t		totst;
        int		nact; 
        int		nproc;
        int		ntrun;
        int		ntslpi;
        int		ntslpu;
        int		nzomb;
        int		nexit;
        int		noverflow;
        int		avgval;
        int		nsecs;
        count_t		mstot;
        count_t		iotot;
	struct perdsk	*perdsk;
        int		index;
        count_t		cputot;
        count_t		percputot;
} extraparam;

/***************************************************************/
/*
 * structure for system print-list
*/
typedef struct {
        char *configname;                          // name as used to 
                                                   // config print line
        char* (*doconvert)(void *, void *, int, int *); // ptr to convert func
} sys_printdef;


/*
 * structure for system print-list with priority
 * in case of leck of screen space, lowest priority items will be
 * removed first 
*/
typedef struct
{
        sys_printdef    *f;
        int             prio;
} sys_printpair;



/*
** structure for process print-list
*/
typedef struct 
{
        char *head;                      // column header
        char *configname;                // name as used to config print line
        char *(*doactiveconvert)(struct tstat *,int,int); 
                                         // pointer to conv function
                                         // for active process
        char *(*doexitconvert)  (struct tstat *,int,int);   
                                         // pointer to conv function
                                         // for exited process
        int  width;                      // required width
        int  varwidth;                   // width may grow (eg cmd params)
} proc_printdef;


typedef struct 
{
        proc_printdef   *f;
        int             prio;
} proc_printpair;

void showsysline(sys_printpair* elemptr, 
                 struct sstat* sstat, extraparam *extra,
                 char *labeltext, unsigned int badness);


void showhdrline(proc_printpair* elemptr, int curlist, int totlist, 
                  char showorder, char autosort);
void showprocline(proc_printpair* elemptr, struct tstat *curstat, 
                  double perc, int nsecs, int avgval);

extern sys_printdef *prcsyspdefs[];
extern sys_printdef *cpusyspdefs[];
extern sys_printdef *cpisyspdefs[];
extern sys_printdef *cplsyspdefs[];
extern sys_printdef *memsyspdefs[];
extern sys_printdef *swpsyspdefs[];
extern sys_printdef *pagsyspdefs[];
extern sys_printdef *dsksyspdefs[];
extern sys_printdef *nettranssyspdefs[];
extern sys_printdef *netnetsyspdefs[];
extern sys_printdef *netintfsyspdefs[];

extern sys_printdef syspdef_PRCSYS;
extern sys_printdef syspdef_PRCUSER;
extern sys_printdef syspdef_PRCNPROC;
extern sys_printdef syspdef_PRCNRUNNING;
extern sys_printdef syspdef_PRCNSLEEPING;
extern sys_printdef syspdef_PRCNDSLEEPING;
extern sys_printdef syspdef_PRCNZOMBIE;
extern sys_printdef syspdef_PRCCLONES;
extern sys_printdef syspdef_PRCNNEXIT;
extern sys_printdef syspdef_CPUSYS;
extern sys_printdef syspdef_CPUUSER;
extern sys_printdef syspdef_CPUIRQ;
extern sys_printdef syspdef_CPUIDLE;
extern sys_printdef syspdef_CPUWAIT;
extern sys_printdef syspdef_CPUISYS;
extern sys_printdef syspdef_CPUIUSER;
extern sys_printdef syspdef_CPUIIRQ;
extern sys_printdef syspdef_CPUIIDLE;
extern sys_printdef syspdef_CPUIWAIT;
extern sys_printdef syspdef_CPUISTEAL;
extern sys_printdef syspdef_CPUIFREQ;
extern sys_printdef syspdef_CPUFREQ;
extern sys_printdef syspdef_CPUSCALE;
extern sys_printdef syspdef_CPUISCALE;
extern sys_printdef syspdef_CPUSTEAL;
extern sys_printdef syspdef_CPUISTEAL;
extern sys_printdef syspdef_CPUGUEST;
extern sys_printdef syspdef_CPUIGUEST;
extern sys_printdef syspdef_CPLAVG1;
extern sys_printdef syspdef_CPLAVG5;
extern sys_printdef syspdef_CPLAVG15;
extern sys_printdef syspdef_CPLCSW;
extern sys_printdef syspdef_CPLNUMCPU;
extern sys_printdef syspdef_CPLINTR;
extern sys_printdef syspdef_MEMTOT;
extern sys_printdef syspdef_MEMFREE;
extern sys_printdef syspdef_MEMCACHE;
extern sys_printdef syspdef_MEMDIRTY;
extern sys_printdef syspdef_MEMBUFFER;
extern sys_printdef syspdef_MEMSLAB;
extern sys_printdef syspdef_RECSLAB;
extern sys_printdef syspdef_SHMEM;
extern sys_printdef syspdef_SHMRSS;
extern sys_printdef syspdef_SHMSWP;
extern sys_printdef syspdef_VMWBAL;
extern sys_printdef syspdef_HUPTOT;
extern sys_printdef syspdef_HUPUSE;
extern sys_printdef syspdef_SWPTOT;
extern sys_printdef syspdef_SWPFREE;
extern sys_printdef syspdef_SWPCOMMITTED;
extern sys_printdef syspdef_SWPCOMMITLIM;
extern sys_printdef syspdef_PAGSCAN;
extern sys_printdef syspdef_PAGSTEAL;
extern sys_printdef syspdef_PAGSTALL;
extern sys_printdef syspdef_PAGSWIN;
extern sys_printdef syspdef_PAGSWOUT;
extern sys_printdef syspdef_CONTNAME;
extern sys_printdef syspdef_CONTNPROC;
extern sys_printdef syspdef_CONTCPU;
extern sys_printdef syspdef_CONTMEM;
extern sys_printdef syspdef_DSKNAME;
extern sys_printdef syspdef_DSKBUSY;
extern sys_printdef syspdef_DSKNREAD;
extern sys_printdef syspdef_DSKNWRITE;
extern sys_printdef syspdef_DSKMBPERSECWR;
extern sys_printdef syspdef_DSKMBPERSECRD;
extern sys_printdef syspdef_DSKKBPERWR;
extern sys_printdef syspdef_DSKKBPERRD;
extern sys_printdef syspdef_DSKAVQUEUE;
extern sys_printdef syspdef_DSKAVIO;
extern sys_printdef syspdef_NETTRANSPORT;
extern sys_printdef syspdef_NETTCPI;
extern sys_printdef syspdef_NETTCPO;
extern sys_printdef syspdef_NETTCPACTOPEN;
extern sys_printdef syspdef_NETTCPPASVOPEN;
extern sys_printdef syspdef_NETTCPRETRANS;
extern sys_printdef syspdef_NETTCPINERR;
extern sys_printdef syspdef_NETTCPORESET;
extern sys_printdef syspdef_NETUDPNOPORT;
extern sys_printdef syspdef_NETUDPINERR;
extern sys_printdef syspdef_NETUDPI;
extern sys_printdef syspdef_NETUDPO;
extern sys_printdef syspdef_NETNETWORK;
extern sys_printdef syspdef_NETIPI;
extern sys_printdef syspdef_NETIPO;
extern sys_printdef syspdef_NETIPFRW;
extern sys_printdef syspdef_NETIPDELIV;
extern sys_printdef syspdef_NETICMPIN;
extern sys_printdef syspdef_NETICMPOUT;
extern sys_printdef syspdef_NETNAME;
extern sys_printdef syspdef_NETPCKI;
extern sys_printdef syspdef_NETPCKO;
extern sys_printdef syspdef_NETSPEEDMAX;
extern sys_printdef syspdef_NETSPEEDIN;
extern sys_printdef syspdef_NETSPEEDOUT;
extern sys_printdef syspdef_NETCOLLIS;
extern sys_printdef syspdef_NETMULTICASTIN;
extern sys_printdef syspdef_NETRCVERR;
extern sys_printdef syspdef_NETSNDERR;
extern sys_printdef syspdef_NETRCVDROP;
extern sys_printdef syspdef_NETSNDDROP;
extern sys_printdef syspdef_NFMSERVER;
extern sys_printdef syspdef_NFMPATH;
extern sys_printdef syspdef_NFMTOTREAD;
extern sys_printdef syspdef_NFMTOTWRITE;
extern sys_printdef syspdef_NFMNREAD;
extern sys_printdef syspdef_NFMNWRITE;
extern sys_printdef syspdef_NFMDREAD;
extern sys_printdef syspdef_NFMDWRITE;
extern sys_printdef syspdef_NFMMREAD;
extern sys_printdef syspdef_NFMMWRITE;
extern sys_printdef syspdef_NFCRPCCNT;
extern sys_printdef syspdef_NFCRPCREAD;
extern sys_printdef syspdef_NFCRPCWRITE;
extern sys_printdef syspdef_NFCRPCRET;
extern sys_printdef syspdef_NFCRPCARF;
extern sys_printdef syspdef_NFSRPCCNT;
extern sys_printdef syspdef_NFSRPCREAD;
extern sys_printdef syspdef_NFSRPCWRITE;
extern sys_printdef syspdef_NFSBADFMT;
extern sys_printdef syspdef_NFSBADAUT;
extern sys_printdef syspdef_NFSBADCLN;
extern sys_printdef syspdef_NFSNETTCP;
extern sys_printdef syspdef_NFSNETUDP;
extern sys_printdef syspdef_NFSNRBYTES;
extern sys_printdef syspdef_NFSNWBYTES;
extern sys_printdef syspdef_NFSRCHITS;
extern sys_printdef syspdef_NFSRCMISS;
extern sys_printdef syspdef_NFSRCNOCA;
extern sys_printdef syspdef_BLANKBOX;


/*
** functions that print ???? for unavailable data
*/
char *procprt_NOTAVAIL_4(struct tstat *curstat, int avgval, int nsecs);
char *procprt_NOTAVAIL_5(struct tstat *curstat, int avgval, int nsecs);
char *procprt_NOTAVAIL_6(struct tstat *curstat, int avgval, int nsecs);
char *procprt_NOTAVAIL_7(struct tstat *curstat, int avgval, int nsecs);

extern proc_printdef *allprocpdefs[];
extern proc_printdef procprt_PID;
extern proc_printdef procprt_TID;
extern proc_printdef procprt_PPID;
extern proc_printdef procprt_SYSCPU;
extern proc_printdef procprt_USRCPU;
extern proc_printdef procprt_VGROW;
extern proc_printdef procprt_RGROW;
extern proc_printdef procprt_MINFLT;
extern proc_printdef procprt_MAJFLT;
extern proc_printdef procprt_VSTEXT;
extern proc_printdef procprt_VSIZE;
extern proc_printdef procprt_RSIZE;
extern proc_printdef procprt_PSIZE;
extern proc_printdef procprt_VSLIBS;
extern proc_printdef procprt_VDATA;
extern proc_printdef procprt_VSTACK;
extern proc_printdef procprt_SWAPSZ;
extern proc_printdef procprt_CMD;
extern proc_printdef procprt_RUID;
extern proc_printdef procprt_EUID;
extern proc_printdef procprt_SUID;
extern proc_printdef procprt_FSUID;
extern proc_printdef procprt_RGID;
extern proc_printdef procprt_EGID;
extern proc_printdef procprt_SGID;
extern proc_printdef procprt_FSGID;
extern proc_printdef procprt_CTID;
extern proc_printdef procprt_VPID;
extern proc_printdef procprt_STDATE;
extern proc_printdef procprt_STTIME;
extern proc_printdef procprt_ENDATE;
extern proc_printdef procprt_ENTIME;
extern proc_printdef procprt_THR;
extern proc_printdef procprt_TRUN;
extern proc_printdef procprt_TSLPI;
extern proc_printdef procprt_TSLPU;
extern proc_printdef procprt_POLI;
extern proc_printdef procprt_NICE;
extern proc_printdef procprt_PRI;
extern proc_printdef procprt_RTPR;
extern proc_printdef procprt_CURCPU;
extern proc_printdef procprt_ST;
extern proc_printdef procprt_EXC;
extern proc_printdef procprt_S;
extern proc_printdef procprt_COMMAND_LINE;
extern proc_printdef procprt_NPROCS;
extern proc_printdef procprt_RDDSK;
extern proc_printdef procprt_WRDSK;
extern proc_printdef procprt_CWRDSK;
extern proc_printdef procprt_WCANCEL;
extern proc_printdef procprt_TCPRCV;
extern proc_printdef procprt_TCPRASZ;
extern proc_printdef procprt_TCPSND;
extern proc_printdef procprt_TCPSASZ;
extern proc_printdef procprt_UDPRCV;
extern proc_printdef procprt_UDPRASZ;
extern proc_printdef procprt_UDPSND;
extern proc_printdef procprt_UDPSASZ;
extern proc_printdef procprt_RNET;
extern proc_printdef procprt_SNET;
extern proc_printdef procprt_RNETBW;
extern proc_printdef procprt_SNETBW;
extern proc_printdef procprt_SORTITEM;



//extern char *procprt_NRDDSK_ae(struct tstat *, int, int);
//extern char *procprt_NWRDSK_a(struct tstat *, int, int);
//extern char *procprt_NRDDSK_e(struct tstat *, int, int);
//extern char *procprt_NWRDSK_e(struct tstat *, int, int);

extern char *procprt_SNET_a(struct tstat *, int, int);
extern char *procprt_SNET_e(struct tstat *, int, int);
extern char *procprt_RNET_a(struct tstat *, int, int);
extern char *procprt_RNET_e(struct tstat *, int, int);
extern char *procprt_TCPSND_a(struct tstat *, int, int);
extern char *procprt_TCPRCV_a(struct tstat *, int, int);
extern char *procprt_UDPSND_a(struct tstat *, int, int);
extern char *procprt_UDPRCV_a(struct tstat *, int, int);
extern char *procprt_TCPSASZ_a(struct tstat *, int, int);
extern char *procprt_TCPRASZ_a(struct tstat *, int, int);
extern char *procprt_UDPSASZ_a(struct tstat *, int, int);
extern char *procprt_UDPRASZ_a(struct tstat *, int, int);
extern char *procprt_TCPSND_e(struct tstat *, int, int);
extern char *procprt_TCPRCV_e(struct tstat *, int, int);
extern char *procprt_UDPSND_e(struct tstat *, int, int);
extern char *procprt_UDPRCV_e(struct tstat *, int, int);
extern char *procprt_TCPSASZ_e(struct tstat *, int, int);
extern char *procprt_TCPRASZ_e(struct tstat *, int, int);
extern char *procprt_UDPSASZ_e(struct tstat *, int, int);
extern char *procprt_UDPRASZ_e(struct tstat *, int, int);
