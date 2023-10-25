/*
 * Denki (電気, Japanese for 'electricity'), PMDA for electricity related 
 * metrics
 *
 * Copyright (c) 2012-2014,2017,2021,2022 Red Hat.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * - adaptions to pmda denki: Christian Horn <chorn@fluxcoil.net>
 * - rapl readcode from rapl-plot, Vince Weaver -- vincent.weaver @ maine.edu
 *   ( https://github.com/deater/uarch-configure / GPL-2.0 License )
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <sys/stat.h>
#include <dirent.h>

#define MAX_RAPL_DOMAINS	10
#define MAX_PACKAGES		16
#define MAX_CPUS		2147483648
#define MAX_BATTERIES		8

static int has_rapl = 0, has_bat = 0;			/* Has the system any rapl or battery? */

static int total_cores, total_packages;			/* detected cpu cores and rapl packages */
static int package_map[MAX_PACKAGES];
char event_names[MAX_PACKAGES][MAX_RAPL_DOMAINS][256];	/* rapl domain names */
uint64_t raplvars[MAX_PACKAGES][MAX_RAPL_DOMAINS];	/* rapl domain readings */
static int valid[MAX_PACKAGES][MAX_RAPL_DOMAINS];	/* Is this rapl domain valid? */
static char filenames[MAX_PACKAGES][MAX_RAPL_DOMAINS][256]; /* pathes to the rapl domains */

static int detect_rapl_packages(void);			/* detect RAPL packages, cpu cores */
static int detect_rapl_domains(void);			/* detect RAPL domains */
uint64_t lookup_rapl_dom(int);				/* map instance to 2-dimensional domain matrix */
static int read_rapl(void);				/* read RAPL values */

static char rootpath[MAXPATHLEN] = "/";			/* path to rootpath, gets changed for regression tests */

/* detect RAPL packages and cpu cores */
static int detect_rapl_packages(void) {

	char filename[MAXPATHLEN];
	FILE *fff;
	int package,i;

	for(i=0;i<MAX_PACKAGES;i++)
		package_map[i]=-1;

	for(i=0;i<MAX_CPUS;i++) {
		pmsprintf(filename,sizeof(filename),"%s/sys/devices/system/cpu/cpu%d/topology/physical_package_id",rootpath,i);
		fff=fopen(filename,"r");
		if (fff==NULL) break;
		if ( fscanf(fff,"%d",&package) != 1 )
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s!", filename);
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "found core %d (package %d)",i,package);
		fclose(fff);

		if (package < 0 || package >= MAX_PACKAGES) {
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_ERR, "package number %d invalid, range 0-%u in %s", package, MAX_PACKAGES, filename);
			continue;
		}

		if (package_map[package]==-1) {
			total_packages++;
			package_map[package]=i;
		}
	}

	total_cores=i;

	pmNotifyErr(LOG_INFO, "RAPL detected, with %d cpu-cores and %d rapl-packages.", total_cores, total_packages);

	return 0;
}

static int detect_rapl_domains(void) {
	char	basename[MAX_PACKAGES][256];
	char	tempfile[256];
	int	i,pkg;
	FILE	*fff;

	for(pkg=0;pkg<total_packages;pkg++) {
		i=0;
		pmsprintf(basename[pkg],sizeof(basename[pkg]),"%s/sys/class/powercap/intel-rapl/intel-rapl:%d",rootpath,pkg);
		pmsprintf(tempfile,sizeof(tempfile),"%s/name",basename[pkg]);
		fff=fopen(tempfile,"r");
		if (fff==NULL) {
			if (pmDebugOptions.appl0)
    				pmNotifyErr(LOG_ERR, "detect_rapl_domains() could not open %s", tempfile);
			return -1;
		}
		if ( fscanf(fff,"%255s",event_names[pkg][i]) != 1)
			if (pmDebugOptions.appl0)
    				pmNotifyErr(LOG_ERR, "detect_rapl_domains() could not read %s",event_names[pkg][i]);
		valid[pkg][i]=1;
		fclose(fff);
		pmsprintf(filenames[pkg][i],sizeof(filenames[pkg][i]),"%s/energy_uj",basename[pkg]);

		/* Handle subdomains */
		for(i=1;i<MAX_RAPL_DOMAINS;i++) {
			pmsprintf(tempfile,sizeof(tempfile),"%s/intel-rapl:%d:%d/name",
				basename[pkg],pkg,i-1);
			fff=fopen(tempfile,"r");
			if (fff==NULL) {
				if (pmDebugOptions.appl0)
    					pmNotifyErr(LOG_DEBUG, "Could not open %s", tempfile);
				valid[pkg][i]=0;
				continue;
			}
			valid[pkg][i]=1;
			if ( fscanf(fff,"%255s",event_names[pkg][i]) != 1 )
				if (pmDebugOptions.appl0)
    					pmNotifyErr(LOG_DEBUG, "Could not read from %s", event_names[pkg][i]);
			fclose(fff);
			pmsprintf(filenames[pkg][i],sizeof(filenames[pkg][i]),"%s/intel-rapl:%d:%d/energy_uj",
				basename[pkg],pkg,i-1);
		}
	}
	return 0;
}

static int read_rapl(void) {

	int	dom,pkg;
	FILE	*fff;

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<MAX_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				fff=fopen(filenames[pkg][dom],"r");
				if (fff==NULL) {
					if (pmDebugOptions.appl0)
    						pmNotifyErr(LOG_ERR, "read_rapl() could not open %s",filenames[pkg][dom]);
				}
				else {
					if ( fscanf(fff,"%" FMT_UINT64,&raplvars[pkg][dom]) != 1)
						if (pmDebugOptions.appl0)
    							pmNotifyErr(LOG_ERR, "read_rapl() could not read %s",filenames[pkg][dom]);
					fclose(fff);
				}
			}
		}
	}
	return 0;
}



int batteries = 0;					/* How many batteries has this system?					*/
static int battery_comp_rate = 60;			/* timespan in sec, after which we recompute energy_rate_d      	*/

							/* Careful with battery counting!
							   If we have one battery (batteries==1), that battery data
							   is in energy_now[0], power_now[0] and so on.				*/

int64_t energy_now[MAX_BATTERIES];			/* <battery>/energy_now or <battery>/charge_now readings		*/
int64_t energy_now_old[MAX_BATTERIES];
int64_t power_now[MAX_BATTERIES];			/* <battery>/power_now readings, driver computed power consumption	*/
uint32_t capacity[MAX_BATTERIES];				/* <battery>/capacity readings, percentage of original capacity		*/

time_t secondsnow, secondsold;						/* time stamps, to understand if we need to recompute	*/
double energy_diff_d[MAX_BATTERIES], energy_rate_d[MAX_BATTERIES];	/* amount of used energy / computed energy consumption	*/

char battery_basepath[MAX_BATTERIES][512];		/* path to the batteries						*/
char energy_now_file[MAX_BATTERIES][512];		/* energy now file, different between models				*/
double energy_convert_factor[MAX_BATTERIES];		/* factor for fixing <battery>/energy_now / charge_now to kwh 		*/

static int detect_batteries(void);			/* detect batteries */
static int read_batteries(void);			/* read battery values */
static int compute_energy_rate(void);			/* compute discharge rate from battery values */

/* detect batteries */
static int detect_batteries(void) {
	char	filename[MAXPATHLEN],dirname[MAXPATHLEN],type[32];
	DIR	*directory;
	FILE 	*fff;
	int	i;

	struct dirent *ep;

	// Initialize per battery counters
	for(i=0;i<MAX_BATTERIES;i++) {
		energy_now[i]=0;
		energy_now_old[i]=0;
		power_now[i]=0;
		energy_convert_factor[i] = 10000.0;
		energy_rate_d[i] = 0.0;
	}

	pmsprintf(dirname,sizeof(dirname),"%s/sys/class/power_supply/",rootpath);
	directory = opendir (dirname);
	if (directory != NULL) {
		while ( ( ep = readdir (directory) ) ) {

			// skip entries '.' and '..'
			if ( ep->d_name[0] == '.')
				continue;

			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Is %s%s a battery we should provide metrics for?",dirname,ep->d_name);

			pmsprintf(filename,sizeof(filename),"%s%s/type",dirname,ep->d_name);
			fff=fopen(filename,"r");
			if (fff==NULL) {
				if (pmDebugOptions.appl0)
					pmNotifyErr(LOG_DEBUG, "Could not access file 'type' in that directory, assuming it's no battery.");
				continue;
			}
			if ( fscanf(fff,"%31s",type) != 1) {
				if (pmDebugOptions.appl0)
					pmNotifyErr(LOG_DEBUG, "Could not read contents of %s, assuming it's no battery.",filename);
				fclose(fff);
				continue;
			}
			fclose(fff);

			if ( strcmp(type,"Battery") != 0 ) {
				if (pmDebugOptions.appl0)
			 		pmNotifyErr(LOG_DEBUG, "No, contents of file 'type' in the directory is not 'Battery'.");
				continue;
			}

			// We need at least one of charge_now / energy_now / power_now for a battery
			pmsprintf(filename,sizeof(filename),"%s%s/charge_now",dirname,ep->d_name);
			if( access( filename, F_OK ) == 0 ) {
				if (pmDebugOptions.appl0)
			 		pmNotifyErr(LOG_DEBUG, "file %s found",filename);
				batteries++;
				energy_convert_factor[batteries-1] = 100000.0;
				pmsprintf(energy_now_file[batteries-1],sizeof(energy_now_file[batteries-1]),"charge_now");
			} 
			else {
				if (pmDebugOptions.appl0)
			 		pmNotifyErr(LOG_DEBUG, "file %s not found",filename);
				pmsprintf(filename,sizeof(filename),"%s%s/energy_now",dirname,ep->d_name);
				if( access( filename, F_OK ) == 0 ) {
					if (pmDebugOptions.appl0)
			 			pmNotifyErr(LOG_DEBUG, "file %s found",filename);
					batteries++;
					energy_convert_factor[batteries-1] = 1000000.0;
					pmsprintf(energy_now_file[batteries-1],sizeof(energy_now_file[batteries-1]),"energy_now");
				}
				else {
					if (pmDebugOptions.appl0)
			 			pmNotifyErr(LOG_DEBUG, "file %s not found",filename);
					pmsprintf(filename,sizeof(filename),"%s%s/power_now",dirname,ep->d_name);
					if( access( filename, F_OK ) == 0 ) {
			 			pmNotifyErr(LOG_DEBUG, "file %s found",filename);
						batteries++;
					}
					else {
						if (pmDebugOptions.appl0)
			 				pmNotifyErr(LOG_DEBUG, "file %s not found, assuming this is no battery.",filename);
						continue;
					}
				}
			}

			pmNotifyErr(LOG_INFO, "Assuming %s%s is a battery we should provide metrics for.",dirname,ep->d_name);
			pmsprintf(battery_basepath[batteries-1],sizeof(battery_basepath[batteries-1]),"%s%s",dirname,ep->d_name);
			has_bat=1;
			continue;
		}
		(void) closedir (directory);
    	}
	else {
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "Couldn't open directory %s/sys/class/power_supply.",rootpath);
		return 0;
	}

	return 0;
}

/* read the current battery values */
static int read_batteries(void) {
	int bat;
	char filename[MAXPATHLEN];
	FILE *fff;

	for (bat=0; bat<batteries; bat++) {

		// energy_now
		pmsprintf(filename,sizeof(filename),"%s/%s",battery_basepath[bat],energy_now_file[bat]);
		fff=fopen(filename,"r");
		if (fff==NULL) {
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "battery path has no %s file.",filename);
			continue;
		}
		if ( fscanf(fff,"%" FMT_UINT64,&energy_now[bat]) != 1)
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s.",filename);
		fclose(fff);

		// power_now
		pmsprintf(filename,sizeof(filename),"%s/power_now",battery_basepath[bat]);
		fff=fopen(filename,"r");
		if (fff==NULL) {
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "battery path has no %s file.",filename);
			continue;
		}
		if ( fscanf(fff,"%" FMT_UINT64,&power_now[bat]) != 1)
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s.",filename);
		fclose(fff);
	
		// correct power_now, if we got a negative value
		if ( power_now[bat]<0 )
			power_now[bat]*=-1.0;

		// capacity
		pmsprintf(filename,sizeof(filename),"%s/capacity",battery_basepath[bat]);
		fff=fopen(filename,"r");
		if (fff==NULL) {
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "battery path has no %s file.",filename);
			continue;
		}
		if ( fscanf(fff,"%u",&capacity[bat]) != 1)
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s.",filename);
		fclose(fff);
	}

	return 0;
}

/* compute energy consumption from <battery-dir>/energy_now values */
static int compute_energy_rate(void) {

	int i;
	secondsnow = time(NULL);

	// Special handling for first call after starting pmda-denki
	if ( secondsold == 0) {
		secondsold = secondsnow;
		for (i=0; i<batteries; i++)
			energy_now_old[i] = energy_now[i];
        }

	// Time for a new computation?
	if ( ( secondsnow - secondsold ) >= battery_comp_rate ) {
		for (i=0; i<batteries; i++) {

			// computing how many Wh were used up in battery_comp_rate
			energy_diff_d[i] = (energy_now_old[i] - energy_now[i])/energy_convert_factor[i];

			// computing how many W would be used in 1h
			energy_rate_d[i] = energy_diff_d[i] * 3600 / battery_comp_rate;
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "new computation, currently %f W/h are consumed",energy_rate_d[i]);

			energy_now_old[i] = energy_now[i];
		}
		secondsold = secondsnow;
	}

	return 0;
}

/*
 * Denki PMDA metrics
 *
 * denki.rapl			- cummulative energy from RAPL
 * denki.bat.energy_now_raw	- <battery>/energy_now raw reading, 
 *				  current battery charge in Wh
 * denki.bat.energy_now_rate	- <battery>, current rate of discharging if postive value,
 *				  or current charging rate if negative value
 * denki.bat.power_now		- <battery>/power_now raw reading
 * denki.bat.capacity		- <battery>/capacity raw reading
 */

/*
 * instance domains
 * These use the more recent pmdaCache methods, but also appear in
 * indomtab[] so that the initialization of the pmInDom and the pmDescs
 * in metrictab[] is completed by pmdaInit
 */

static pmdaIndom indomtab[] = {
#define RAPL_INDOM		0	/* serial number for RAPL instance domain */
    { RAPL_INDOM, 0, NULL },
#define ENERGYNOWRAW_INDOM	2	/* serial number for "energy_now_raw" instance domain */
    { ENERGYNOWRAW_INDOM, 0, NULL },
#define ENERGYNOWRATE_INDOM	3	/* serial number for "energy_now_rate" instance domain */
    { ENERGYNOWRATE_INDOM, 0, NULL },
#define POWERNOW_INDOM		4	/* serial number for "power_now" instance domain */
    { POWERNOW_INDOM, 0, NULL },
#define CAPACITY_INDOM		5	/* serial number for "capacity" instance domain */
    { CAPACITY_INDOM, 0, NULL }
};

/* this is merely a convenience */
static pmInDom	*rapl_indom = &indomtab[RAPL_INDOM].it_indom;
static pmInDom	*energynowraw_indom = &indomtab[ENERGYNOWRAW_INDOM].it_indom;
static pmInDom	*energynowrate_indom = &indomtab[ENERGYNOWRATE_INDOM].it_indom;
static pmInDom	*powernow_indom = &indomtab[POWERNOW_INDOM].it_indom;
static pmInDom	*capacity_indom = &indomtab[CAPACITY_INDOM].it_indom;

/*
 * All metrics supported in this PMDA - one table entry for each.
 */

static pmdaMetric metrictab[] = {
/* rapl */
	{ NULL,
	{ PMDA_PMID(0,0), PM_TYPE_U64, RAPL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.energy_now_raw */
	{ NULL,
	{ PMDA_PMID(1,0), PM_TYPE_DOUBLE, ENERGYNOWRAW_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.energy_now_rate */
	{ NULL,
	{ PMDA_PMID(1,1), PM_TYPE_DOUBLE, ENERGYNOWRATE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.power_now */
	{ NULL,
	{ PMDA_PMID(1,2), PM_TYPE_DOUBLE, POWERNOW_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.capacity */
	{ NULL,
	{ PMDA_PMID(1,3), PM_TYPE_U32, CAPACITY_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, }
};

static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;

static void denki_rapl_clear(void);
static void denki_rapl_init(void);
static void denki_rapl_check(void);
static void denki_bat_init(void);

static char	mypath[MAXPATHLEN];

/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "rootpath", 1, 'r', "ROOTPATH", "use non-default rootpath instead of /" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:i:l:r:pu:U:6:?",
    .long_options = longopts,
};

/*
 * Our rapl readings are in a 2-dimensional array, map them here
 * to the 1-dimentional indom numbers
 */
uint64_t lookup_rapl_dom(int instance) {

	int		pkg,dom,domcnt=0;

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<MAX_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				if (instance == domcnt)
					return raplvars[pkg][dom];
				domcnt++;
			}
		}
	}
	return 0;
}

/*
 * callback provided to pmdaFetch
 */
static int
denki_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
	int		sts;
	unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
	unsigned int	item = pmID_item(mdesc->m_desc.pmid);

	if (inst != PM_IN_NULL && mdesc->m_desc.indom == PM_INDOM_NULL)
		return PM_ERR_INST;

	if (cluster == 0) {
		switch (item) {
			case 0:				/* rapl */
				if ((sts = pmdaCacheLookup(*rapl_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->ull = lookup_rapl_dom(inst)/1000000;
				break;
			default:
				return PM_ERR_PMID;
		}
	}
	else if (cluster == 1) {
		switch (item) {
			case 0:				/* denki.energy_now_raw */
				if ((sts = pmdaCacheLookup(*energynowraw_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->d = energy_now[inst]/energy_convert_factor[inst];
				break;
			case 1:				/* denki.energy_now_rate */
				if ((sts = pmdaCacheLookup(*energynowrate_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->d = energy_rate_d[inst];
				break;
			case 2:				/* denki.power_now */
				if ((sts = pmdaCacheLookup(*powernow_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->d = power_now[inst]/1000000.0;
				break;
			case 3:				/* denki.capacity */
				if ((sts = pmdaCacheLookup(*capacity_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->ul = capacity[inst];
				break;
			default:
				return PM_ERR_PMID;
		}
	}
	else
		return PM_ERR_PMID;

	return PMDA_FETCH_STATIC;
}

/*
 * This routine is called once for each pmFetch(3) operation, so is a
 * good place to do once-per-fetch functions, such as value caching or
 * instance domain evaluation (as we do in denki_rapl_check).
 */
static int
denki_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
	if (has_rapl)
		read_rapl();
	if (has_bat) {
		read_batteries();
		compute_energy_rate();
	}
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * wrapper for pmdaInstance which we need to ensure is called with the
 * _current_ contents of the rapl instance domain.
 */
static int
denki_instance(pmInDom indom, int foo, char *bar, pmInResult **iresp, pmdaExt *pmda)
{
	// We could call the check each fetch cycle here, but rapl
	// should not change dynamically, so we can spare those cycles.
	// denki_rapl_check();
	return pmdaInstance(indom, foo, bar, iresp, pmda);
}

/*
 * Initialize the instances
 */
static void
denki_rapl_check(void)
{
	denki_rapl_clear();
	denki_rapl_init();
}

/*
 * clear the rapl.rate and rapl.raw metric instance domains
 */
static void
denki_rapl_clear(void)
{
	int		sts;

	sts = pmdaCacheOp(*rapl_indom, PMDA_CACHE_INACTIVE);
	if (sts < 0)
		pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	pmInDomStr(*rapl_indom), pmErrStr(sts));
}

/* 
 * register RAPL cores as indoms
 */
static void
denki_rapl_init(void)
{
	int		sts;
	int		dom,pkg;
	char		tmp[80];

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<MAX_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				/* instance names need to be unique, so if >1 rapl packages,
				   we prepend the rapl-domain counter */
				if (total_packages > 1)
					pmsprintf(tmp,sizeof(tmp),"%d-%s",pkg,event_names[pkg][dom]);
				else
					pmsprintf(tmp,sizeof(tmp),"%s",event_names[pkg][dom]);

				/* rapl.rate */
				sts = pmdaCacheStore(*rapl_indom, PMDA_CACHE_ADD, tmp, NULL);
				if (sts < 0) {
					pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
					return;
				}
			}
		}
	}

	if (pmdaCacheOp(*rapl_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
		pmNotifyErr(LOG_WARNING, "\"rapl\" instance domain is empty");
}

/* 
 * register batteries as indoms
 */
static void
denki_bat_init(void)
{
	int		sts,battery;
	char		tmp[80];

	if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "bat_init, batteries:%d",batteries);

	for(battery=0; battery<batteries; battery++) {

		pmsprintf(tmp,sizeof(tmp),"battery-%d",battery);

		/* bat.energynowraw */
		sts = pmdaCacheStore(*energynowraw_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}

		/* bat.energynowrate */
		sts = pmdaCacheStore(*energynowrate_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}

		/* bat.powernow */
		sts = pmdaCacheStore(*powernow_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}

		/* bat.capacity */
		sts = pmdaCacheStore(*capacity_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}
	}
}


static int
denki_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
	int		serial;

	switch (type) {
		case PM_LABEL_INDOM:
			serial = pmInDom_serial((pmInDom)ident);
			switch (serial) {
				case RAPL_INDOM:
					pmdaAddLabels(lpp, "{\"indom_name\":\"rapl\"}");
					break;
				case ENERGYNOWRAW_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"watt hours\"}");
					break;
				case ENERGYNOWRATE_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"watt\"}");
					break;
				case POWERNOW_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"watt\"}");
					break;
				case CAPACITY_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"percent\"}");
					break;
			}
			break;
		/* no labels to add for these types, fall through */
		case PM_LABEL_DOMAIN:
		case PM_LABEL_CLUSTER:
		default:
			break;
	}
	return pmdaLabel(ident, type, lpp, pmda);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
denki_init(pmdaInterface *dp)
{
    char		filename[MAXPATHLEN];
    DIR			*directory;

    if (isDSO) {
	int sep = pmPathSeparator();

	if (strcmp(rootpath, "/") == 0) {
	    /*
	     * no -r ROOTPATH on the command line ... check for
	     * DENKI_SYSPATH in the environment
	     */
	    char	*envpath = getenv("DENKI_SYSPATH");
	    if (envpath != NULL)
		strcpy(rootpath, envpath);
	}
	pmsprintf(mypath, sizeof(mypath), "%s%c" "denki" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "denki DSO", mypath);
    } else {
	pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.any.fetch = denki_fetch;
    dp->version.any.instance = denki_instance;
    dp->version.seven.label = denki_label;

    pmdaSetFetchCallBack(dp, denki_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
		sizeof(metrictab)/sizeof(metrictab[0]));

    if (pmDebugOptions.appl0)
	pmNotifyErr(LOG_DEBUG, "configured to use this rootpath: %s", rootpath);

    pmsprintf(filename,sizeof(filename),"%s/sys/class/powercap/intel-rapl",rootpath);
    directory = opendir(filename);
    if ( directory == NULL ) {
	if (pmDebugOptions.appl0)
	    pmNotifyErr(LOG_DEBUG, "RAPL not detected");
    } else {
	has_rapl=1;
    	detect_rapl_packages();
    	detect_rapl_domains();
    	denki_rapl_check();	// now we register the found rapl indoms
    }
    closedir(directory);

    detect_batteries();
    if (has_bat)
	    denki_bat_init();
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			c,sep = pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "denki" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), DENKI,
		"denki.log", mypath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
        switch (c) {
	        case 'r':
        		strncpy(rootpath, opts.optarg, sizeof(rootpath));
			rootpath[sizeof(rootpath)-1] = '\0';
            		break;
        }
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    denki_init(&dispatch);

    pmdaMain(&dispatch);

    exit(0);
}
