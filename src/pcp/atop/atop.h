/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Include-file describing miscellaneous constants and function-prototypes.
**
** Copyright (C) 1996-2014 Gerlof Langeveld
** Copyright (C) 2015 Red Hat.
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
#define	EQ		0
#define SECSDAY		86400
#define RAWNAMESZ	256
#define	PROCCHUNK	100	/* process-entries for future expansion  */
#define	PROCMIN		256     /* minimum number of processes allocated */

/*
** memory-size formatting possibilities
*/
#define	ANYFORMAT	0
#define	KBFORMAT	1
#define	MBFORMAT	2
#define	GBFORMAT	3
#define	TBFORMAT	4
#define	PBFORMAT	5
#define	OVFORMAT	9

typedef	long long	count_t;

struct pmDesc;
struct pmResult;
struct pmOptions;

struct tstat;
struct sstat;

/* 
** miscellaneous flags
*/
#define RRBOOT		0x0001
#define RRLAST  	0x0002
#define RRNETATOP	0x0004
#define RRNETATOPD	0x0008

struct visualize {
	char	(*show_samp)  (double, double,
	                struct sstat *, struct tstat *, struct tstat **,
			int, int, int, int, int, int, int, int, 
			int, unsigned int, char);
	void	(*show_error) (const char *, ...);
	void	(*show_end)   (void);
	void	(*show_usage) (void);
};

struct sysname {
	char	nodename[MAXHOSTNAMELEN];
	char	release[72];
	char	version[72];
	char	machine[72];
};

/*
** external values
*/
extern struct sysname	sysname;
extern int              nodenamelen;
extern struct timeval	origin;
extern struct timeval   pretime;
extern struct timeval   curtime;
extern struct timeval   interval;
extern unsigned long	sampcnt;
extern char      	screen;
extern int      	linelen;
extern char      	acctreason;
extern char		deviatonly;
extern char		usecolors;
extern char		threadview;
extern char		calcpss;
extern char		rawreadflag;
extern unsigned int	begintime, endtime;
extern char		flaglist[];
extern struct visualize vis;

extern int      	osrel;
extern int		osvers;
extern int      	ossub;

extern unsigned short	hertz;
extern unsigned int	pagesize;

extern int		supportflags;

extern int		fetchmode;
extern int		fetchstep;

extern int		cpubadness;
extern int		membadness;
extern int		swpbadness;
extern int		dskbadness;
extern int		netbadness;
extern int		pagbadness;
extern int		almostcrit;

/*
** bit-values for supportflags
*/
#define	ACCTACTIVE	0x00000001
#define	IOSTAT		0x00000004
#define	NETATOP		0x00000010
#define	NETATOPD	0x00000020

/*
** structure containing the start-addresses of functions for visualization
*/
char		generic_samp (double, double,
		            struct sstat *, struct tstat *, struct tstat **,
		            int, int, int, int, int, int, int, int,
		            int, unsigned int, char);
void		generic_error(const char *, ...);
void		generic_end  (void);
void		generic_usage(void);

/*
** miscellaneous prototypes
*/
int		atopsar(int, char *[]);
char   		*convtime(double, char *, size_t);
char   		*convdate(double, char *, size_t);
int   		hhmm2secs(char *, unsigned int *);

char   		*val2valstr(count_t, char *, size_t, int, int, int);
char   		*val2memstr(count_t, char *, size_t, int, int, int);
char		*val2cpustr(count_t, char *, size_t);
char            *val2Hzstr(count_t, char *, size_t);
int             val2elapstr(int, char *, size_t);

int		compcpu(const void *, const void *);
int		compdsk(const void *, const void *);
int		compmem(const void *, const void *);
int		compnet(const void *, const void *);
int		compusr(const void *, const void *);
int		compnam(const void *, const void *);

int		cpucompar (const void *, const void *);
int		diskcompar(const void *, const void *);
int		intfcompar(const void *, const void *);
int		nfsmcompar(const void *, const void *);
int		contcompar(const void *, const void *);

count_t		subcount(count_t, count_t);
void  		rawread(struct pmOptions *);
void		rawfolio(struct pmOptions *);
void		rawarchive(struct pmOptions *, const char *);
void		rawwrite(struct pmOptions *, const char *, struct timeval *,
			unsigned int, char);

int 		numeric(char *);
void		getalarm(int);
void		setalarm(struct timeval *);
void		setalarm2(int, int);
char 		*getstrvers(void);
unsigned short 	getnumvers(void);
void		ptrverify(const void *, const char *, ...);
void		cleanstop(int);
void		prusage(char *);
void		engine(void);

void		setup_globals(struct pmOptions *);
void		setup_process(void);
void		setup_metrics(char **, unsigned int *, struct pmDesc *, int);
int		fetch_metrics(const char *, int, unsigned int *, struct pmResult **);
int		get_instances(const char *, int, struct pmDesc *, int **, char ***);

struct sstat	*sstat_alloc(const char *);
void		sstat_reset(struct sstat *);

float		extract_float_inst(struct pmResult *, struct pmDesc *, int, int);
int		extract_integer(struct pmResult *, struct pmDesc *, int);
int		extract_integer_inst(struct pmResult *, struct pmDesc *, int, int);
int		extract_integer_index(struct pmResult *, struct pmDesc *, int, int);
count_t		extract_count_t(struct pmResult *, struct pmDesc *, int);
count_t		extract_count_t_inst(struct pmResult *, struct pmDesc *, int, int);
count_t		extract_count_t_index(struct pmResult *, struct pmDesc *, int, int);
char *		extract_string(struct pmResult *, struct pmDesc *, int, char *, int);
char *		extract_string_inst(struct pmResult *, struct pmDesc *, int, char *, int, int);
char *		extract_string_index(struct pmResult *, struct pmDesc *, int, char *, int, int);

/*
** Optional netatop module interfaces
 */
void		netatop_ipopen(void);
void		netatop_probe(void);
void		netatop_signoff(void);
void		netatop_gettask(pid_t, char, struct tstat *);
unsigned int	netatop_exitstore(void);
void		netatop_exiterase(void);
void		netatop_exithash(char);
void		netatop_exitfind(unsigned long, struct tstat *, struct tstat *);

/*
** Optional process accounting module interfaces
 */
#define MAXACCTPROCS	(50*1024*1024/sizeof(struct tstat))
int 		acctswon(void);
void		acctswoff(void);
unsigned long 	acctprocnt(void);
int 		acctphotoproc(struct tstat *, int);
void 		acctrepos(unsigned int);
void		do_pacctdir(char *, char *);
