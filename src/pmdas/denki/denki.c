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

#define NUM_RAPL_DOMAINS	10
#define MAX_PACKAGES		16
#define MAX_CPUS		2147483648

static int has_rapl, has_bat;				/* Has the system rapl or battery? */

static int total_cores, total_packages;			/* detected cpu cores and rapl packages */
static int package_map[MAX_PACKAGES];
char event_names[MAX_PACKAGES][NUM_RAPL_DOMAINS][256];	/* rapl domain names */
long long raplvars[MAX_PACKAGES][NUM_RAPL_DOMAINS];	/* rapl domain readings */
static int valid[MAX_PACKAGES][NUM_RAPL_DOMAINS];	/* Is this rapl domain valid? */
static char filenames[MAX_PACKAGES][NUM_RAPL_DOMAINS][256]; /* pathes to the rapl domains */

static int detect_rapl_packages(void);			/* detect RAPL packages, cpu cores */
static int detect_rapl_domains(void);			/* detect RAPL domains */
static int read_rapl(void);				/* read RAPL values */
static int detect_battery(void);			/* detect battery */
static int read_battery(void);				/* read battery values */
static int compute_energy_rate(void);			/* compute discharge rate from battery values */
long long lookup_rapl_dom(int);				/* map instance to 2-dimensional domain matrix */

static char rootpath[512] = "/";			/* path to rootpath, gets changed for regression tests */

/* detect RAPL packages and cpu cores */
static int detect_rapl_packages(void) {

	char filename[MAXPATHLEN];
	FILE *fff;
	int package,i;

	for(i=0;i<MAX_PACKAGES;i++) package_map[i]=-1;

	for(i=0;i<MAX_CPUS;i++) {
		pmsprintf(filename,sizeof(filename),"%s/sys/devices/system/cpu/cpu%d/topology/physical_package_id",rootpath,i);
		fff=fopen(filename,"r");
		if (fff==NULL) break;
		if ( fscanf(fff,"%d",&package) != 1 )
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s!", filename);
		printf("\tcore %d (package %d)\n",i,package);
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

	printf("\tDetected %d cores in %d packages\n\n",
		total_cores,total_packages);

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
    				pmNotifyErr(LOG_ERR, "read_rapl() could not open %s", tempfile);
			return -1;
		}
		if ( fscanf(fff,"%255s",event_names[pkg][i]) != 1)
			if (pmDebugOptions.appl0)
    				pmNotifyErr(LOG_ERR, "read_rapl() could not read %s",event_names[pkg][i]);
		valid[pkg][i]=1;
		fclose(fff);
		pmsprintf(filenames[pkg][i],sizeof(filenames[pkg][i]),"%s/energy_uj",basename[pkg]);

		/* Handle subdomains */
		for(i=1;i<NUM_RAPL_DOMAINS;i++) {
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
		for(dom=0;dom<NUM_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				fff=fopen(filenames[pkg][dom],"r");
				if (fff==NULL) {
					if (pmDebugOptions.appl0)
    						pmNotifyErr(LOG_ERR, "read_rapl() could not open %s",filenames[pkg][dom]);
				}
				else {
					if ( fscanf(fff,"%lld",&raplvars[pkg][dom]) != 1)
						if (pmDebugOptions.appl0)
    							pmNotifyErr(LOG_ERR, "read_rapl() could not read %s",filenames[pkg][dom]);
					fclose(fff);
				}
			}
		}
	}
	return 0;
}



long long energy_now = 0;		/* <battery>/energy_now or <battery>/charge_now readings		*/
long long energy_now_old = 0;
long long power_now=0;			/* <battery>/power_now readings, driver computed power consumption	*/

time_t secondsnow, secondsold;		/* time stamps, to understand if we need to recompute	   		*/
double energy_diff_d, energy_rate_d;	/* amount of used energy / computed energy consumption	  		*/

static int battery_comp_rate = 60;	/* timespan in sec, after which we recompute energy_rate_d      	*/
char battery_basepath[512];		/* path to the battery							*/
char energy_now_file[512];		/* energy now file, different between models				*/
double energy_convert_factor = 10000.0;	/* factor for fixing <battery>/energy_now / charge_now to kwh 		*/

/* detect battery */
static int detect_battery(void) {
	char	filename[MAXPATHLEN],dirname[MAXPATHLEN],type[32];
	DIR	*directory;
	FILE 	*fff;

	struct dirent *ep;

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
			if ( fscanf(fff,"%s",type) != 1) {
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
				pmsprintf(energy_now_file,sizeof(energy_now_file),"charge_now");
				energy_convert_factor = 100000.0;
			} 
			else {
				if (pmDebugOptions.appl0)
			 		pmNotifyErr(LOG_DEBUG, "file %s not found",filename);
				pmsprintf(filename,sizeof(filename),"%s%s/energy_now",dirname,ep->d_name);
				if( access( filename, F_OK ) == 0 ) {
					if (pmDebugOptions.appl0)
			 			pmNotifyErr(LOG_DEBUG, "file %s found",filename);
					pmsprintf(energy_now_file,sizeof(energy_now_file),"energy_now");
					energy_convert_factor = 1000000.0;
				}
				else {
					if (pmDebugOptions.appl0)
			 			pmNotifyErr(LOG_DEBUG, "file %s not found",filename);
					pmsprintf(filename,sizeof(filename),"%s%s/power_now",dirname,ep->d_name);
					if( access( filename, F_OK ) == 0 ) {
			 			pmNotifyErr(LOG_DEBUG, "file %s found",filename);
					}
					else {
						if (pmDebugOptions.appl0)
			 				pmNotifyErr(LOG_DEBUG, "file %s not found, assuming this is no battery.",filename);
						continue;
					}
				}
			}

			pmNotifyErr(LOG_INFO, "Assuming %s%s is a battery we should provide metrics for.",dirname,ep->d_name);
			pmsprintf(battery_basepath,sizeof(battery_basepath),"%s%s",dirname,ep->d_name);
			has_bat=1;
			break;
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
static int read_battery(void) {
	char filename[MAXPATHLEN];
	FILE *fff;

	pmsprintf(filename,sizeof(filename),"%s/%s",battery_basepath,energy_now_file);
	fff=fopen(filename,"r");
	if (fff==NULL) {
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "battery path has no %s file.",filename);
		return 1;
	}
	if ( fscanf(fff,"%lld",&energy_now) != 1)
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "Could not read %s.",filename);
	fclose(fff);

	pmsprintf(filename,sizeof(filename),"%s/power_now",battery_basepath);
	fff=fopen(filename,"r");
	if (fff==NULL) {
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "battery path has no %s file.",filename);
		return 1;
	}
	if ( fscanf(fff,"%lld",&power_now) != 1)
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "Could not read %s.",filename);
	fclose(fff);

	// correct power_now, if we got a negative value
	if ( power_now<0 )
		power_now*=-1.0;

	return 0;
}

/* compute energy consumption from <battery-dir>/energy_now values */
static int compute_energy_rate(void) {

	secondsnow = time(NULL);

	// Special handling for first call after starting pmda-denki
	if ( secondsold == 0) {
		secondsold = secondsnow;
		energy_now_old = energy_now;
		energy_rate_d = 0.0;
	}

	// Do a computation all battery_comp_rate seconds
	if ( ( secondsnow - secondsold ) >= battery_comp_rate ) {

		// computing how many Wh were used up in battery_comp_rate
		energy_diff_d = (energy_now_old - energy_now)/energy_convert_factor;

		// computing how many W would be used in 1h
		energy_rate_d = energy_diff_d * 3600 / battery_comp_rate;
		if (pmDebugOptions.appl0)
			pmNotifyErr(LOG_DEBUG, "DENKI: new computation, currently %f W/h are consumed",energy_rate_d);

		secondsold = secondsnow;
		energy_now_old = energy_now;
	}

	return 0;
}

/*
 * Denki PMDA metrics
 *
 * denki.rapl.rate		- usage rates from RAPL
 * denki.rapl.raw		- plain raw values from RAPL
 * denki.bat.energy_now_raw	- BAT0/energy_now raw reading, 
 *				  current battery charge in Wh
 * denki.bat.energy_now_rate	- BAT0, current rate of discharging if postive value,
 *				  or current charging rate if negative value
 * denki.bat.power_now		- BAT0/power_now raw reading
 */

/*
 * instance domains
 * These use the more recent pmdaCache methods, but also appear in
 * indomtab[] so that the initialization of the pmInDom and the pmDescs
 * in metrictab[] is completed by pmdaInit
 */

static pmdaIndom indomtab[] = {
#define RAPLRATE_INDOM	0	/* serial number for "rapl.rate" instance domain */
    { RAPLRATE_INDOM, 0, NULL },
#define RAPLRAW_INDOM	1	/* serial number for "rapl.raw" instance domain */
    { RAPLRAW_INDOM, 0, NULL }
};

/* this is merely a convenience */
static pmInDom	*rate_indom = &indomtab[RAPLRATE_INDOM].it_indom;
static pmInDom	*raw_indom = &indomtab[RAPLRAW_INDOM].it_indom;

/*
 * All metrics supported in this PMDA - one table entry for each.
 */

static pmdaMetric metrictab[] = {
/* rapl.rate */
	{ NULL,
	{ PMDA_PMID(0,0), PM_TYPE_U32, RAPLRATE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* rapl.raw */
	{ NULL,
	{ PMDA_PMID(0,1), PM_TYPE_U32, RAPLRAW_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* energy_now_raw */
	{ NULL,
	{ PMDA_PMID(1,0), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* energy_now_rate */
	{ NULL,
	{ PMDA_PMID(1,1), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* power_now */
	{ NULL,
	{ PMDA_PMID(1,2), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, }
};

static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;

static void denki_rapl_clear(void);
static void denki_rapl_init(void);
static void denki_rapl_check(void);

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
long long lookup_rapl_dom(int instance) {

	int		pkg,dom,domcnt=0;

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<NUM_RAPL_DOMAINS;dom++) {
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

	if (inst != PM_IN_NULL &&
		!((cluster == 0 && item == 0) || 
		(cluster == 0 && item == 1)))
	return PM_ERR_INST;

	if (cluster == 0) {
		if (item == 0) {			/* rapl.rate */
			if ((sts = pmdaCacheLookup(*rate_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
				if (sts < 0)
					pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
				return PM_ERR_INST;
			}
			atom->ul = lookup_rapl_dom(inst)/1000000;
		}
		else if (item == 1) {			/* rapl.raw */
			if ((sts = pmdaCacheLookup(*raw_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
				if (sts < 0)
					pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
				return PM_ERR_INST;
			}
			atom->ul = lookup_rapl_dom(inst)/1000000;
		}
	}
	else if (cluster == 1) {
		if (item == 0)				/* denki.energy_now_raw */
			atom->d = energy_now/energy_convert_factor;
		else if (item == 1)			/* denki.energy_now_rate */
			atom->d = energy_rate_d;
		else if (item == 2)			/* denki.power_now */
			atom->d = power_now/1000000.0;
		else
			return PM_ERR_PMID;
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
		read_battery();
		compute_energy_rate();
	}
	return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * wrapper for pmdaInstance which we need to ensure is called with the
 * _current_ contents of the rapl.rate/rapl.raw instance domain.
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

	sts = pmdaCacheOp(*rate_indom, PMDA_CACHE_INACTIVE);
	if (sts < 0)
		pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	pmInDomStr(*rate_indom), pmErrStr(sts));

	sts = pmdaCacheOp(*raw_indom, PMDA_CACHE_INACTIVE);
	if (sts < 0)
		pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	pmInDomStr(*raw_indom), pmErrStr(sts));
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
		for(dom=0;dom<NUM_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				/* instance names need to be unique, so if >1 rapl packages,
				   we prepend the rapl-domain counter */
				if (total_packages > 1)
					pmsprintf(tmp,sizeof(tmp),"%d-%s",pkg,event_names[pkg][dom]);
				else
					pmsprintf(tmp,sizeof(tmp),"%s",event_names[pkg][dom]);

				/* rapl.rate */
				sts = pmdaCacheStore(*rate_indom, PMDA_CACHE_ADD, tmp, NULL);
				if (sts < 0) {
					pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
					return;
				}
				/* rapl.raw */
				sts = pmdaCacheStore(*raw_indom, PMDA_CACHE_ADD, tmp, NULL);
				if (sts < 0) {
					pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
					return;
				}
			}
		}
	}

	if (pmdaCacheOp(*rate_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
		pmNotifyErr(LOG_WARNING, "\"rapl.rate\" instance domain is empty");
	if (pmdaCacheOp(*raw_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
		pmNotifyErr(LOG_WARNING, "\"rapl.raw\" instance domain is empty");
}

static int
denki_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
	int		serial;

	switch (type) {
		case PM_LABEL_INDOM:
			serial = pmInDom_serial((pmInDom)ident);
			if (serial == RAPLRATE_INDOM) {
				pmdaAddLabels(lpp, "{\"indom_name\":\"raplrate\"}");
			}
			if (serial == RAPLRAW_INDOM) {
				pmdaAddLabels(lpp, "{\"indom_name\":\"raplraw\"}");
			}
			break;
		case PM_LABEL_ITEM:
			if (pmID_cluster(ident) != 1)
				break;
			if (pmID_item(ident) == 0)	/* energy_now_raw */
				pmdaAddLabels(lpp, "{\"units\":\"watt hours\"}");
			if (pmID_item(ident) == 1)	/* energy_now_rate */
				pmdaAddLabels(lpp, "{\"units\":\"watts\"}");
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
	pmNotifyErr(LOG_INFO, "RAPL detected, with %d cpu-cores and %d rapl-packages.", total_cores, total_packages);
    	detect_rapl_domains();
    	denki_rapl_check();	// now we register the found rapl indoms
    }
    closedir(directory);

    detect_battery();
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
