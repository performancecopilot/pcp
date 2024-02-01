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
#ifndef __SHOWLINUX__
#define __SHOWLINUX__

#define MAXITEMS 80    /* The maximum number of items per line */

/*
 * structure for extra parameters for system related data
*/
typedef struct {
        count_t		totut;
        count_t		totst;
        int		index;
        int		nact; 
        int		nproc;
        int		ntrun;
        int		ntslpi;
        int		ntslpu;
        int		ntidle;
        int		nzomb;
        int		nexit;
        int		noverflow;
        int		avgval;
        double		nsecs;
        count_t		mstot;
        count_t		iotot;
        struct perdsk	*perdsk;
        count_t		cputot;
        count_t		pernumacputot;
        count_t		percputot;
} extraparam;

/***************************************************************
 *
 * structure for system print-list
 *
 * configname	name as used to identify this field when configuring
 * 		the print line
 * doformat	pointer to function that formats this field into a
 * 		string of 12 positions, to be returned as char pointer
 * dovalidate	pointer to function that determines if this is a 
 * 		valid (i.e. relevant) field on this system, returning
 * 		0 (false) or non-zero (true)
 * 		when this function pointer is NULL, true is considered
*/
typedef struct {
        char *configname;
        char* (*doformat)(struct sstat *, extraparam *, int, int *);
        int   (*dovalidate)(struct sstat *);
} sys_printdef;


/*
 * structure for system print-list with priority
 * in case of lack of screen space, lowest priority items will be
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
        char *(*doactiveconvert)(struct tstat *,int,double); 
                                         // pointer to conv function
                                         // for active process
        char *(*doexitconvert)  (struct tstat *,int,double);   
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
                  double perc, double nsecs, int avgval);

void do_cpucritperc(char *, char *);
void do_gpucritperc(char *, char *);
void do_memcritperc(char *, char *);
void do_swpcritperc(char *, char *);
void do_dskcritperc(char *, char *);
void do_netcritperc(char *, char *);
void do_swoutcritsec(char *, char *);
void do_almostcrit(char *, char *);
void do_ownsysprcline(char *, char *);
void do_ownallcpuline(char *, char *);
void do_ownindivcpuline(char *, char *);
void do_owncplline(char *, char *);
void do_owngpuline(char *, char *);
void do_ownmemline(char *, char *);
void do_ownswpline(char *, char *);
void do_ownpagline(char *, char *);
void do_ownmemnumaline(char *, char *);
void do_owncpunumaline(char *, char *);
void do_ownllcline(char *, char *);
void do_owndskline(char *, char *);
void do_ownnettransportline(char *, char *);
void do_ownnetnetline(char *, char *);
void do_ownnetinterfaceline(char *, char *);
void do_owninfinibandline(char *, char *);
void do_ownprocline(char *, char *);

int  get_posval(char *, char *);

extern sys_printdef *prcsyspdefs[];
extern sys_printdef *cpusyspdefs[];
extern sys_printdef *cpisyspdefs[];
extern sys_printdef *cplsyspdefs[];
extern sys_printdef *memsyspdefs1[];
extern sys_printdef *memsyspdefs2[];
extern sys_printdef *swpsyspdefs[];
extern sys_printdef *pagsyspdefs[];
extern sys_printdef *numasyspdefs[];
extern sys_printdef *numacpusyspdefs[];
extern sys_printdef *dsksyspdefs[];
extern sys_printdef *nettranssyspdefs[];
extern sys_printdef *netnetsyspdefs[];
extern sys_printdef *netintfsyspdefs[];
extern sys_printdef *infinisyspdefs[];

extern sys_printdef syspdef_PRCSYS;
extern sys_printdef syspdef_PRCUSER;
extern sys_printdef syspdef_PRCNPROC;
extern sys_printdef syspdef_PRCNRUNNING;
extern sys_printdef syspdef_PRCNSLEEPING;
extern sys_printdef syspdef_PRCNDSLEEPING;
extern sys_printdef syspdef_PRCNIDLE;
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
extern sys_printdef syspdef_CPUIPC;
extern sys_printdef syspdef_CPUIIPC;
extern sys_printdef syspdef_CPUCYCLE;
extern sys_printdef syspdef_CPUICYCLE;
extern sys_printdef syspdef_CPLAVG1;
extern sys_printdef syspdef_CPLAVG5;
extern sys_printdef syspdef_CPLAVG15;
extern sys_printdef syspdef_CPLCSW;
extern sys_printdef syspdef_CPLNUMCPU;
extern sys_printdef syspdef_CPLINTR;
extern sys_printdef syspdef_GPUBUS;
extern sys_printdef syspdef_GPUTYPE;
extern sys_printdef syspdef_GPUNRPROC;
extern sys_printdef syspdef_GPUMEMPERC;
extern sys_printdef syspdef_GPUMEMOCC;
extern sys_printdef syspdef_GPUGPUPERC;
extern sys_printdef syspdef_GPUMEMTOT;
extern sys_printdef syspdef_GPUMEMUSE;
extern sys_printdef syspdef_GPUMEMAVG;
extern sys_printdef syspdef_MEMTOT;
extern sys_printdef syspdef_MEMFREE;
extern sys_printdef syspdef_MEMAVAIL;
extern sys_printdef syspdef_MEMCACHE;
extern sys_printdef syspdef_MEMDIRTY;
extern sys_printdef syspdef_MEMBUFFER;
extern sys_printdef syspdef_MEMSLAB;
extern sys_printdef syspdef_RECSLAB;
extern sys_printdef syspdef_SHMEM;
extern sys_printdef syspdef_SHMRSS;
extern sys_printdef syspdef_SHMSWP;
extern sys_printdef syspdef_VMWBAL;
extern sys_printdef syspdef_ZFSARC;
extern sys_printdef syspdef_PAGETABS;
extern sys_printdef syspdef_ANONTHP;
extern sys_printdef syspdef_HUPTOT;
extern sys_printdef syspdef_HUPUSE;
extern sys_printdef syspdef_SWPTOT;
extern sys_printdef syspdef_SWPFREE;
extern sys_printdef syspdef_SWPCACHE;
extern sys_printdef syspdef_ZSWPOOL;
extern sys_printdef syspdef_ZSWSTORED;
extern sys_printdef syspdef_KSMSHARING;
extern sys_printdef syspdef_KSMSHARED;
extern sys_printdef syspdef_SWPCOMMITTED;
extern sys_printdef syspdef_SWPCOMMITLIM;
extern sys_printdef syspdef_NUMNUMA;
extern sys_printdef syspdef_NUMANR;
extern sys_printdef syspdef_NUMATOT;
extern sys_printdef syspdef_NUMAFREE;
extern sys_printdef syspdef_NUMAFILEPAGE;
extern sys_printdef syspdef_NUMASLAB;
extern sys_printdef syspdef_NUMADIRTY;
extern sys_printdef syspdef_NUMAACTIVE;
extern sys_printdef syspdef_NUMAINACTIVE;
extern sys_printdef syspdef_NUMASHMEM;
extern sys_printdef syspdef_NUMASLABRECLAIM;
extern sys_printdef syspdef_NUMAFRAG;
extern sys_printdef syspdef_NUMAHUPTOT;
extern sys_printdef syspdef_NUMAHUPUSE;
extern sys_printdef syspdef_NUMANUMCPU;
extern sys_printdef syspdef_NUMACPUSYS;
extern sys_printdef syspdef_NUMACPUUSER;
extern sys_printdef syspdef_NUMACPUNICE;
extern sys_printdef syspdef_NUMACPUIRQ;
extern sys_printdef syspdef_NUMACPUSOFTIRQ;
extern sys_printdef syspdef_NUMACPUIDLE;
extern sys_printdef syspdef_NUMACPUWAIT;
extern sys_printdef syspdef_NUMACPUSTEAL;
extern sys_printdef syspdef_NUMACPUGUEST;
extern sys_printdef syspdef_LLCMBMTOTAL;
extern sys_printdef syspdef_LLCMBMLOCAL;
extern sys_printdef syspdef_NUMLLC;
extern sys_printdef syspdef_PAGSCAN;
extern sys_printdef syspdef_PAGSTEAL;
extern sys_printdef syspdef_PAGSTALL;
extern sys_printdef syspdef_PAGCOMPACT;
extern sys_printdef syspdef_NUMAMIGRATE;
extern sys_printdef syspdef_PGMIGRATE;
extern sys_printdef syspdef_PAGPGIN;
extern sys_printdef syspdef_PAGPGOUT;
extern sys_printdef syspdef_TCPSOCK;
extern sys_printdef syspdef_UDPSOCK;
extern sys_printdef syspdef_PAGSWIN;
extern sys_printdef syspdef_PAGSWOUT;
extern sys_printdef syspdef_PAGZSWIN;
extern sys_printdef syspdef_PAGZSWOUT;
extern sys_printdef syspdef_OOMKILLS;
extern sys_printdef syspdef_PSICPUSTOT;
extern sys_printdef syspdef_PSIMEMSTOT;
extern sys_printdef syspdef_PSIMEMFTOT;
extern sys_printdef syspdef_PSIIOSTOT;
extern sys_printdef syspdef_PSIIOFTOT;
extern sys_printdef syspdef_PSICPUS;
extern sys_printdef syspdef_PSIMEMS;
extern sys_printdef syspdef_PSIMEMF;
extern sys_printdef syspdef_PSIIOS;
extern sys_printdef syspdef_PSIIOF;
extern sys_printdef syspdef_CONTNAME;
extern sys_printdef syspdef_CONTNPROC;
extern sys_printdef syspdef_CONTCPU;
extern sys_printdef syspdef_CONTMEM;
extern sys_printdef syspdef_DSKNAME;
extern sys_printdef syspdef_DSKBUSY;
extern sys_printdef syspdef_DSKNREAD;
extern sys_printdef syspdef_DSKNWRITE;
extern sys_printdef syspdef_DSKNDISC;
extern sys_printdef syspdef_DSKMBPERSECRD;
extern sys_printdef syspdef_DSKMBPERSECWR;
extern sys_printdef syspdef_DSKKBPERRD;
extern sys_printdef syspdef_DSKKBPERWR;
extern sys_printdef syspdef_DSKKBPERDS;
extern sys_printdef syspdef_DSKINFLIGHT;
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
extern sys_printdef syspdef_NETTCPCSUMERR;
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
extern sys_printdef syspdef_IFBNAME;
extern sys_printdef syspdef_IFBPCKI;
extern sys_printdef syspdef_IFBPCKO;
extern sys_printdef syspdef_IFBSPEEDMAX;
extern sys_printdef syspdef_IFBLANES;
extern sys_printdef syspdef_IFBSPEEDIN;
extern sys_printdef syspdef_IFBSPEEDOUT;
extern sys_printdef syspdef_BLANKBOX;


/*
** functions that print ???? for unavailable data
*/
char *procprt_NOTAVAIL_4(struct tstat *curstat, int avgval, double nsecs);
char *procprt_NOTAVAIL_5(struct tstat *curstat, int avgval, double nsecs);
char *procprt_NOTAVAIL_6(struct tstat *curstat, int avgval, double nsecs);
char *procprt_NOTAVAIL_7(struct tstat *curstat, int avgval, double nsecs);

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
extern proc_printdef procprt_LOCKSZ;
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
extern proc_printdef procprt_CID;
extern proc_printdef procprt_STDATE;
extern proc_printdef procprt_STTIME;
extern proc_printdef procprt_ENDATE;
extern proc_printdef procprt_ENTIME;
extern proc_printdef procprt_THR;
extern proc_printdef procprt_TRUN;
extern proc_printdef procprt_TSLPI;
extern proc_printdef procprt_TSLPU;
extern proc_printdef procprt_TIDLE;
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
extern proc_printdef procprt_BANDWI;
extern proc_printdef procprt_BANDWO;
extern proc_printdef procprt_GPULIST;
extern proc_printdef procprt_GPUMEMNOW;
extern proc_printdef procprt_GPUMEMAVG;
extern proc_printdef procprt_GPUGPUBUSY;
extern proc_printdef procprt_GPUMEMBUSY;
extern proc_printdef procprt_SORTITEM;
extern proc_printdef procprt_RUNDELAY;
extern proc_printdef procprt_BLKDELAY;
extern proc_printdef procprt_WCHAN;
extern proc_printdef procprt_NVCSW;
extern proc_printdef procprt_NIVCSW;
extern proc_printdef procprt_CGROUP_PATH;
extern proc_printdef procprt_CGRCPUWGT;
extern proc_printdef procprt_CGRCPUMAX;
extern proc_printdef procprt_CGRCPUMAXR;
extern proc_printdef procprt_CGRMEMMAX;
extern proc_printdef procprt_CGRMEMMAXR;
extern proc_printdef procprt_CGRSWPMAX;
extern proc_printdef procprt_CGRSWPMAXR;


//extern char *procprt_NRDDSK_ae(struct tstat *, int, double);
//extern char *procprt_NWRDSK_a(struct tstat *, int, double);
//extern char *procprt_NRDDSK_e(struct tstat *, int, double);
//extern char *procprt_NWRDSK_e(struct tstat *, int, double);

extern char *procprt_SNET_a(struct tstat *, int, double);
extern char *procprt_SNET_e(struct tstat *, int, double);
extern char *procprt_RNET_a(struct tstat *, int, double);
extern char *procprt_RNET_e(struct tstat *, int, double);
extern char *procprt_TCPSND_a(struct tstat *, int, double);
extern char *procprt_TCPRCV_a(struct tstat *, int, double);
extern char *procprt_UDPSND_a(struct tstat *, int, double);
extern char *procprt_UDPRCV_a(struct tstat *, int, double);
extern char *procprt_TCPSASZ_a(struct tstat *, int, double);
extern char *procprt_TCPRASZ_a(struct tstat *, int, double);
extern char *procprt_UDPSASZ_a(struct tstat *, int, double);
extern char *procprt_UDPRASZ_a(struct tstat *, int, double);
extern char *procprt_TCPSND_e(struct tstat *, int, double);
extern char *procprt_TCPRCV_e(struct tstat *, int, double);
extern char *procprt_UDPSND_e(struct tstat *, int, double);
extern char *procprt_UDPRCV_e(struct tstat *, int, double);
extern char *procprt_TCPSASZ_e(struct tstat *, int, double);
extern char *procprt_TCPRASZ_e(struct tstat *, int, double);
extern char *procprt_UDPSASZ_e(struct tstat *, int, double);
extern char *procprt_UDPRASZ_e(struct tstat *, int, double);

#endif
