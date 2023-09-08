/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Include-file describing prototypes and structures for visualization
** of counters.
**
** Copyright (C) 1996-2014 Gerlof Langeveld
** Copyright (C) 2015,2019-2021 Red Hat
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
#ifndef __SHOWGENERIC__
#define __SHOWGENERIC__

#include <regex.h>

#define USERSTUB	9999999
#define MAXUSERSEL	64
#define AT_MAXPID	32

// minimum screen dimensions
//
#define MINLINES        24
#define MINCOLUMNS      60


struct syscap {
	int	nrcpu;
	int	nrgpu;
	int	nrmemnuma;
	int	nrcpunuma;
	count_t	availcpu;
	count_t	availmem;
	count_t	availdsk;
	count_t	availnet;
	count_t	availgpumem;	// GPU memory in Kb!
};

struct pselection {
	char	username[256];
	uid_t	userid[MAXUSERSEL];

	pid_t	pid[AT_MAXPID];

	char	progname[64];
	int	prognamesz;
	regex_t	progregex;

	char	argname[64];
	int	argnamesz;
	regex_t	argregex;

	char 	container[16];

	char 	states[16];
};

struct sselection {
	char	lvmname[64];	// logical volume selection
	int	lvmnamesz;
	regex_t	lvmregex;

	char	dskname[64];	// disk selection
	int	dsknamesz;
	regex_t	dskregex;

	char	itfname[64];	// network interface selection
	int	itfnamesz;
	regex_t	itfregex;
};

/*
** color definitions
*/
#define COLOR_MYORANGE  20
#define COLOR_MYGREEN   21
#define COLOR_MYGREY    22

#define COLOR_MYBROWN1  24
#define COLOR_MYBROWN2  25

#define COLOR_MYBLUE0   26
#define COLOR_MYBLUE1   27
#define COLOR_MYBLUE2   28
#define COLOR_MYBLUE3   29
#define COLOR_MYBLUE4   30
#define COLOR_MYBLUE5   31

#define COLOR_MYGREEN0  35
#define COLOR_MYGREEN1  36
#define COLOR_MYGREEN2  37

/*
** color pair definitions
*/
#define FGCOLORBORDER   1
#define	FGCOLORINFO	2
#define	FGCOLORALMOST	3
#define	FGCOLORCRIT	4
#define	FGCOLORTHR	5

#define WHITE_GREEN     10
#define WHITE_ORANGE    11
#define WHITE_RED       12
#define WHITE_GREY      13
#define WHITE_BLUE      14
#define WHITE_MAGENTA   15

#define WHITE_BROWN1    18
#define WHITE_BROWN2    19

#define WHITE_BLUE0     20
#define WHITE_BLUE1     21
#define WHITE_BLUE2     22
#define WHITE_BLUE3     23
#define WHITE_BLUE4     24
#define WHITE_BLUE5     25

#define WHITE_GREEN0    30
#define WHITE_GREEN1    31
#define WHITE_GREEN2    32

/*
** text and bar color selections
*/
#define COLOROKAY       WHITE_GREEN
#define COLORWARN       WHITE_ORANGE
#define COLORBAD        WHITE_RED

#define COLORCPUSYS     WHITE_BLUE1
#define COLORCPUUSR     WHITE_BLUE2
#define COLORCPUIDLE    WHITE_BLUE3
#define COLORCPUSTEAL   WHITE_BLUE4
#define COLORCPUGUEST   WHITE_BLUE5

#define COLORMEMFREE    WHITE_GREEN
#define COLORMEMCACH    WHITE_ORANGE
#define COLORMEMUSED    WHITE_GREY
#define COLORMEMSHM     WHITE_BROWN1
#define COLORMEMTMP     WHITE_BLUE
#define COLORMEMSLAB    WHITE_MAGENTA
#define COLORMEMBAR     WHITE_BLUE3

#define COLORDSKREAD	WHITE_GREEN1
#define COLORDSKWRITE	WHITE_GREEN2

#define COLORNETRECV	WHITE_BROWN1
#define COLORNETSEND	WHITE_BROWN2

/*
** list with keystrokes/flags
*/
#define	MPROCGEN	'g'
#define	MPROCMEM	'm'
#define	MPROCDSK	'd'
#define	MPROCNET	'n'
#define MPROCGPU	'e'
#define	MPROCSCH	's'
#define	MPROCVAR	'v'
#define	MPROCARG	'c'
#define	MPROCCGR	'X'
#define MPROCOWN	'o'

#define	MCUMUSER	'u'
#define	MCUMPROC	'p'
#define	MCUMCONT	'j'

#define	MSORTCPU	'C'
#define	MSORTDSK	'D'
#define	MSORTMEM	'M'
#define	MSORTNET	'N'
#define MSORTGPU	'E'
#define	MSORTAUTO	'A'

#define	MTHREAD		'y'
#define	MTHRSORT	'Y'
#define	MCALCPSS	'R'
#define	MGETWCHAN	'W'
#define	MSUPEXITS	'G'
#define	MCOLORS		'x'
#define	MSYSFIXED	'f'
#define	MSYSNOSORT	'F'
#define	MSYSLIMIT	'l'
#define	MRMSPACES	'Z'

#define	MSELUSER	'U'
#define	MSELPROC	'P'
#define	MSELCONT	'J'
#define	MSELPID		'I'
#define	MSELARG		'/'
#define	MSELSTATE	'Q'
#define	MSELSYS		'S'

#define	MALLPROC	'a'
#define	MKILLPROC	'k'
#define	MLISTFW		0x06
#define	MLISTBW		0x02
#define MREDRAW         0x0c
#define	MINTERVAL	'i'
#define	MPAUSE		'z'
#define	MQUIT		'q'
#define	MRESET		'r'
#define	MSAMPNEXT	't'
#define	MSAMPPREV	'T'
#define	MSAMPBRANCH	'b'
#define	MVERSION	'V'
#define	MAVGVAL		'1'
#define	MHELP1		'?'
#define	MHELP2		'h'

#define MBARGRAPH	'B'
#define	MBARLOWER	'L'
#define	MBARMONO	'H'

/*
** extern pause indication
*/
extern int paused;

/*
** general function prototypes
*/
void	totalcap   (struct syscap *, struct sstat *, struct tstat **, int);
void	pricumproc (struct sstat *,  struct devtstat *,
				int, unsigned int, int, double);

void	showgenproc(struct tstat *, double, int, int);
void	showmemproc(struct tstat *, double, int, int);
void	showdskproc(struct tstat *, double, int, int);
void	shownetproc(struct tstat *, double, int, int);
void	showvarproc(struct tstat *, double, int, int);
void	showschproc(struct tstat *, double, int, int);
void	showtotproc(struct tstat *, double, int, int);
void	showcmdproc(struct tstat *, double, int, int);

void	printg     (const char *, ...);
int	prisyst(struct sstat  *, int, double, int, int, struct sselection *,
			char *, int, int, int, int, int, int, int, int, int, int, int);
int	priproc(struct tstat  **, int, int, int, int, int, char, char,
	        struct syscap *, double, int);
void	priphead(int, int, char *, char *, char, count_t);

char	draw_samp(double, double, struct sstat *, char, int);

void	do_username(char *, char *);
void	do_procname(char *, char *);
void	do_maxcpu(char *, char *);
void	do_maxgpu(char *, char *);
void	do_maxdisk(char *, char *);
void	do_maxmdd(char *, char *);
void	do_maxlvm(char *, char *);
void	do_maxintf(char *, char *);
void	do_maxifb(char *, char *);
void	do_maxnfsm(char *, char *);
void	do_maxcont(char *, char *);
void	do_maxnuma(char *, char *);
void	do_maxllc(char *, char *);
void	do_colinfo(char *, char *);
void	do_colalmost(char *, char *);
void	do_colcrit(char *, char *);
void	do_colthread(char *, char *);
void	do_flags(char *, char *);

#endif
