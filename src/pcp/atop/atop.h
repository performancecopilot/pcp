/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Include-file describing miscellaneous constants and function-prototypes.
**
** Copyright (C) 1996-2014 Gerlof Langeveld
** Copyright (C) 2015-2021 Red Hat.
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
#define SECONDSINDAY	86400

/*
** memory-size formatting possibilities
*/
#define	BFORMAT		0
#define	KBFORMAT	1
#define	KBFORMAT_INT	2
#define	MBFORMAT	3
#define	MBFORMAT_INT	4
#define	GBFORMAT	5
#define	GBFORMAT_INT	6
#define	TBFORMAT	7
#define	TBFORMAT_INT	8
#define	PBFORMAT	9
#define	PBFORMAT_INT	10
#define	OVFORMAT	11

typedef	long long	count_t;
typedef	unsigned long long	ucount_t;

struct pmDesc;
struct pmResult;
struct pmOptions;

struct tstat;
struct devtstat;
struct sstat;

/* 
** miscellaneous flags
*/
#define RRBOOT		0x0001
#define RRLAST  	0x0002
#define RRMARK		0x0004
#define RRIOSTAT	0x0020
#define RRDOCKSTAT	0x0040

struct visualize {
	char	(*show_samp)  (double, double,
	                struct devtstat *, struct sstat *,
			int, unsigned int, int);
	void	(*show_error) (const char *, ...);
	void	(*show_end)   (void);
	void	(*show_usage) (void);
	void    (*prep)       (void);
	int     (*next)       (void);
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
extern int		localhost;
extern int              nodenamelen;
extern struct timeval	start;
extern struct timeval	finish;
extern struct timeval	origin;
extern struct timeval   pretime;
extern struct timeval   curtime;
extern struct timeval   interval;
extern unsigned long	sampcnt;
extern unsigned long	sampflags;
extern char      	screen;
extern int      	linelen;
extern char      	acctreason;
extern char		deviatonly;
extern char		usecolors;
extern char		threadview;
extern char		calcpss;
extern char		getwchan;
extern char		hotprocflag;
extern char		rawreadflag;
extern char		rmspaces;
extern unsigned int	begintime, endtime;
extern char		flaglist[];
extern struct visualize vis;

extern int      	os_rel;
extern int		os_vers;
extern int      	os_sub;

extern unsigned short	hertz;
extern unsigned int	pidmax;
extern unsigned int	pidwidth;
extern unsigned int	pagesize;
extern unsigned int	hinv_nrcpus;
extern unsigned int	hinv_nrdisk;
extern unsigned int	hinv_nrgpus;
extern unsigned int	hinv_nrintf;
extern unsigned int	hinv_nrnuma;

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

extern long long	system_boottime;

/*
** bit-values for supportflags
*/
#define	ACCTACTIVE	0x00000001
#define	IOSTAT		0x00000004
#define	NETATOP		0x00000010
#define	NETATOPD	0x00000020
#define	DOCKSTAT	0x00000040
#define	GPUSTAT		0x00000080
#define	CGROUPV2	0x00000100

/*
** structure containing the start-addresses of functions for visualization
*/
char		generic_samp (double, double,
		            struct devtstat *, struct sstat *,
		            int, unsigned int, int);
void		generic_error(const char *, ...);
void		generic_end  (void);
void		generic_usage(void);
void		generic_prep (void);
int            generic_next (void);

/*
** miscellaneous prototypes
*/
int		atopsar(int, char *[]);
char   		*convtime(double, char *, size_t);
char   		*convdate(double, char *, size_t);
int   		getbranchtime(char *, struct timeval *);
time_t		normalize_epoch(time_t, long);
int		time_less_than(struct timeval *, struct timeval *);
int		time_greater_than(struct timeval *, struct timeval *);


char   		*val2valstr(count_t, char *, size_t, int, int, int);
char   		*val2memstr(count_t, char *, size_t, int, int, int);
char		*val2cpustr(count_t, char *, size_t);
char            *val2Hzstr(count_t, char *, size_t);
int             val2elapstr(int, char *, size_t);

int		compcpu(const void *, const void *);
int		compdsk(const void *, const void *);
int		compmem(const void *, const void *);
int		compnet(const void *, const void *);
int		compgpu(const void *, const void *);
int		compusr(const void *, const void *);
int		compnam(const void *, const void *);
int		compcon(const void *, const void *);

int		cpucompar (const void *, const void *);
int		gpucompar (const void *, const void *);
int		diskcompar(const void *, const void *);
int		intfcompar(const void *, const void *);
int		ifbcompar(const void *, const void *);
int		nfsmcompar(const void *, const void *);
int		contcompar(const void *, const void *);
int		memnumacompar(const void *, const void *);
int		cpunumacompar(const void *, const void *);
int		llccompar(const void *, const void *);

void		setup_options(struct pmOptions *, char **, char *);
void		close_options(struct pmOptions *);
void		rawfolio(struct pmOptions *);
void		rawarchive(struct pmOptions *, const char *);
void		rawarchive_from_midnight(struct pmOptions *);
void		rawwrite(struct pmOptions *, const char *, struct timeval *,
			unsigned int, char);

int 		numeric(char *);
void		getalarm(int);
void		setalarm(struct timeval *);
void		setalarm2(int, int);
char 		*getstrvers(void);
unsigned short 	getnumvers(void);
void		ptrverify(const void *, const char *, ...);
void		mcleanstop(int, const char *, ...);
void		cleanstop(int);
int		getpidwidth(void);
void		prusage(char *, struct pmOptions *);
int		run_in_guest(void);
void		show_pcp_usage(struct pmOptions *);
void		engine(void);

char 		*abstime(char *);
void		setup_step_mode(int);
void		setup_globals(struct pmOptions *);
void		setup_process(void);
void		setup_metrics(const char **, unsigned int *, struct pmDesc *, int);
int		fetch_metrics(const char *, int, unsigned int *, struct pmResult **);
int		get_instances(const char *, int, struct pmDesc *, int **, char ***);
int		fetch_instances(const char *, int, struct pmDesc *, int **, char ***);
int		get_instance_index(pmResult *, int, int);

void		add_username(int, const char *);
void		add_groupname(int, const char *);
char		*get_username(int);
char		*get_groupname(int);

struct sstat	*sstat_alloc(const char *);
void		sstat_reset(struct sstat *);

float		extract_float_inst(struct pmResult *, struct pmDesc *, int, int, int);
int		extract_integer(struct pmResult *, struct pmDesc *, int);
int		extract_integer_inst(struct pmResult *, struct pmDesc *, int, int, int);
int		extract_integer_index(struct pmResult *, struct pmDesc *, int, int);
int             extract_integer_instmap_count(struct pmResult *, struct pmDesc *, int, int);
count_t		extract_count_t(struct pmResult *, struct pmDesc *, int);
count_t		extract_count_t_inst(struct pmResult *, struct pmDesc *, int, int, int);
count_t		extract_count_t_index(struct pmResult *, struct pmDesc *, int, int);
ucount_t	extract_ucount_t_inst(struct pmResult *, struct pmDesc *, int, int, int);
char *		extract_string(struct pmResult *, struct pmDesc *, int, char *, int);
char *		extract_string_inst(struct pmResult *, struct pmDesc *, int, char *, int, int, int);
char *		extract_string_index(struct pmResult *, struct pmDesc *, int, char *, int, int);
int		present_metric_value(struct pmResult *, int);

/*
** Optional pmdabcc(1) netproc module interfaces
 */
void		netproc_probe(void);
void		netproc_update_tasks(struct tstat **, unsigned long);
#define	netatop_signoff() do { } while (0)
#define netatop_exiterase() do { } while (0)
#define netatop_exithash(hash) do { (void)(hash); } while (0)
#define netatop_exitfind(find,a,b) do { (void)(find); } while (0)

/*
** Optional process accounting module interfaces
 */
#define MAXACCTPROCS	(50*1024*1024/sizeof(struct tstat))
int 		acctswon(void);
void		acctswoff(void);
int		acctphotoproc(struct tstat **, unsigned int *, struct timeval *, struct timeval *);
void		do_pacctdir(char *, char *);
