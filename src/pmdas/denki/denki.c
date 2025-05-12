/*
 * Denki (電気, Japanese for 'electricity'), PMDA for electricity related 
 * metrics
 *
 * Copyright (c) 2012-2014,2017,2021-2025 Red Hat.
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
#include <math.h>

#define MAX_RAPL_DOMAINS	10
#define MAX_PACKAGES		16
#define MAX_CPUS			2147483648
#define MAX_BATTERIES		8

#define PACKAGE	1
#define CORES	2
#define UNCORE	4
#define DRAM	8
#define PSYS	16

// RAPL-MSR definitions

#define MSR_RAPL_POWER_UNIT		0x606

/*
 * Platform specific RAPL Domains.
 */

/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS			0x613
#define MSR_PKG_POWER_INFO			0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT			0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY				0x63A
#define MSR_PP0_PERF_STATUS			0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT			0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY				0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO			0x61C

/* PSYS RAPL Domain */
#define MSR_PLATFORM_ENERGY_STATUS	0x64d

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET			0
#define POWER_UNIT_MASK				0x0F

#define ENERGY_UNIT_OFFSET			0x08
#define ENERGY_UNIT_MASK			0x1F00

#define TIME_UNIT_OFFSET			0x10
#define TIME_UNIT_MASK				0xF000

/* We store the energy values for the msr registers for all packages in a 2x matrix.
 * The rows contain the actual MSR's that the systems CPU supplies.
 * These MSR's depend on the cpu, so we need MSR pointers (msr_pnt) which point at
 * the correct row of the matrix.							*/
int msr_pnt_package_energy=-1;
int msr_pnt_cores_energy=-1;
int msr_pnt_uncore_energy=-1;
int msr_pnt_dram_energy=-1;
int msr_pnt_psys_energy=-1;
double msr_energy[5][MAX_PACKAGES];

static double cpu_energy_units[MAX_PACKAGES],dram_energy_units[MAX_PACKAGES];

int has_rapl_sysfs = 0, has_rapl_msr = 0, has_bat = 0;			/* Has the system any rapl or battery? */
int has_msr_dram=0, has_msr_pp0=0, has_msr_pp1=0, has_msr_psys=0, msr_different_units=0;
int cpu_model=0, cpu_core=0, msr_instances=0;

static int total_cores, total_packages;							/* detected cpu cores and rapl packages */
static int package_map[MAX_PACKAGES];
char event_names[MAX_PACKAGES][MAX_RAPL_DOMAINS][256];			/* rapl domain names */
uint64_t raplvars[MAX_PACKAGES][MAX_RAPL_DOMAINS];				/* rapl domain readings */
static int valid[MAX_PACKAGES][MAX_RAPL_DOMAINS];				/* Is this rapl domain valid? */
static char filenames[MAX_PACKAGES][MAX_RAPL_DOMAINS][256];		/* pathes to the rapl domains */

static int detect_rapl_packages(void);			/* detect RAPL packages, cpu cores */
static int detect_rapl_domains(void);			/* detect RAPL domains */
static int detect_cpu(void);					/* detect (x86) cpu */
uint64_t lookup_rapl_dom(int);					/* map instance to 2-dimensional domain matrix */
static int read_rapl_sysfs(void);				/* read RAPL values via /sysfs */
static int read_rapl_msr(int); 					/* read RAPL values via MSRs */
static int open_msr(int);						/* Open an msr for reading */
static long long read_msr(int, int);

static char rootpath[MAXPATHLEN] = "/";			/* path to rootpath, gets changed for regression tests */

/* detect RAPL packages and cpu cores, used for both RAPL-sysfs and RAPL-MSX
   This also detects cpu cores on Asahi Linux, despite no RAPL available there. */
static int detect_rapl_packages(void) {

	char filename[MAXPATHLEN];
	FILE *fff;
	int package,i;

	pmNotifyErr(LOG_INFO, "Looking for RAPL packages.");

	for(i=0;i<MAX_PACKAGES;i++)
		package_map[i]=-1;

	for(i=0;i<MAX_CPUS;i++) {
		pmsprintf(filename,sizeof(filename),
			"%s/sys/devices/system/cpu/cpu%d/topology/physical_package_id",rootpath,i);
		fff=fopen(filename,"r");
		if (fff==NULL) break;
		if ( fscanf(fff,"%d",&package) != 1 )
			if (pmDebugOptions.appl0)
				pmNotifyErr(LOG_DEBUG, "Could not read %s!", filename);
		// if (pmDebugOptions.appl0)
		//	pmNotifyErr(LOG_DEBUG, "found core %d (package %d)",i,package);
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

	pmNotifyErr(LOG_INFO, "Detected %d cpu-cores and %d rapl-packages.", total_cores, total_packages);

	return 0;
}

static int detect_rapl_domains(void) {
	char	basename[MAX_PACKAGES][256];
	char	tempfile[256];
	int		i,pkg;
	FILE	*fff;

	for(pkg=0;pkg<total_packages;pkg++) {
		i=0;
		pmsprintf(basename[pkg],sizeof(basename[pkg]),"%s/sys/class/powercap/intel-rapl/intel-rapl:%d",rootpath,pkg);
		pmsprintf(tempfile,sizeof(tempfile),"%s/name",basename[pkg]);
		fff=fopen(tempfile,"r");
		if (fff==NULL) {
			pmNotifyErr(LOG_INFO, "detect_rapl_domains() could not open %s", tempfile);
			return -1;
		}
		if ( fscanf(fff,"%255s",event_names[pkg][i]) != 1)
			pmNotifyErr(LOG_INFO, "detect_rapl_domains() could not read %s",event_names[pkg][i]);
		valid[pkg][i]=1;
		fclose(fff);
		pmsprintf(filenames[pkg][i],sizeof(filenames[pkg][i]),"%s/energy_uj",basename[pkg]);

		/* Handle subdomains */
		for(i=1;i<MAX_RAPL_DOMAINS;i++) {
			pmsprintf(tempfile,sizeof(tempfile),"%s/intel-rapl:%d:%d/name",
				basename[pkg],pkg,i-1);
			fff=fopen(tempfile,"r");
			if (fff==NULL) {
				// This fails mostly - expectedly -.  Potentially not even log to DEBUG?
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


static long long read_msr(int fd, int which) {

	uint64_t data;

	if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
		pmNotifyErr(LOG_DEBUG, "rdmsr:pread");
		exit(127);
	}

	return (long long)data;
}

#define CPU_SANDYBRIDGE			42
#define CPU_SANDYBRIDGE_EP		45
#define CPU_IVYBRIDGE			58
#define CPU_IVYBRIDGE_EP		62
#define CPU_HASWELL	 			60
#define CPU_HASWELL_ULT			69
#define CPU_HASWELL_GT3E		70
#define CPU_HASWELL_EP			63
#define CPU_BROADWELL			61
#define CPU_BROADWELL_GT3E		71
#define CPU_BROADWELL_EP		79
#define CPU_BROADWELL_DE		86
#define CPU_SKYLAKE	 			78
#define CPU_SKYLAKE_HS			94
#define CPU_SKYLAKE_X			85
#define CPU_KNIGHTS_LANDING		87
#define CPU_KNIGHTS_MILL		133
#define CPU_KABYLAKE_MOBILE		142
#define CPU_KABYLAKE			158
#define CPU_ATOM_SILVERMONT		55
#define CPU_ATOM_AIRMONT		76
#define CPU_ATOM_MERRIFIELD		74
#define CPU_ATOM_MOOREFIELD		90
#define CPU_ATOM_GOLDMONT		92
#define CPU_ATOM_GEMINI_LAKE	122
#define CPU_ATOM_DENVERTON		95

static int detect_cpu(void) {

	FILE *fff;
	char filename[MAXPATHLEN];

	int family,model=-1;
	char buffer[BUFSIZ],*result;
	char vendor[BUFSIZ];

	pmsprintf(filename,sizeof(filename), "%s/proc/cpuinfo",rootpath);
	fff=fopen(filename,"r");
	if (fff==NULL) return -1;

	while(1) {
		result=fgets(buffer,BUFSIZ,fff);
		if (result==NULL) break;

		if (!strncmp(result,"vendor_id",8)) {
			sscanf(result,"%*s%*s%s",vendor);

			if (strncmp(vendor,"GenuineIntel",12)) {
				pmNotifyErr(LOG_INFO, "%s not an Intel chip\n",vendor);
				return -1;
			}
		}

		if (!strncmp(result,"cpu family",10)) {
			sscanf(result,"%*s%*s%*s%d",&family);
			if (family!=6) {
				pmNotifyErr(LOG_INFO, "Wrong CPU family %d\n",family);
				return -1;
			}
		}

		if (!strncmp(result,"model",5))
			sscanf(result,"%*s%*s%d",&model);

	}

	fclose(fff);

	switch(model) {
		case CPU_SANDYBRIDGE:
			pmNotifyErr(LOG_INFO, "Processor family: Sandybridge");
			break;
		case CPU_SANDYBRIDGE_EP:
			pmNotifyErr(LOG_INFO, "Processor family: Sandybridge-EP");
			break;
		case CPU_IVYBRIDGE:
			pmNotifyErr(LOG_INFO, "Processor family: Ivybridge");
			break;
		case CPU_IVYBRIDGE_EP:
			pmNotifyErr(LOG_INFO, "Processor family: Ivybridge-EP");
			break;
		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
			pmNotifyErr(LOG_INFO, "Processor family: Haswell");
			break;
		case CPU_HASWELL_EP:
			pmNotifyErr(LOG_INFO, "Processor family: Haswell-EP");
			break;
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
			pmNotifyErr(LOG_INFO, "Processor family: Broadwell");
			break;
		case CPU_BROADWELL_EP:
			pmNotifyErr(LOG_INFO, "Processor family: Broadwell-EP");
			break;
		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
			pmNotifyErr(LOG_INFO, "Processor family: Skylake");
			break;
		case CPU_SKYLAKE_X:
			pmNotifyErr(LOG_INFO, "Processor family: Skylake-X");
			break;
		case CPU_KABYLAKE:
		case CPU_KABYLAKE_MOBILE:
			pmNotifyErr(LOG_INFO, "Processor family: Kaby Lake");
			break;
		case CPU_KNIGHTS_LANDING:
			pmNotifyErr(LOG_INFO, "Processor family: Knight's Landing");
			break;
		case CPU_KNIGHTS_MILL:
			pmNotifyErr(LOG_INFO, "Processor family: Knight's Mill");
			break;
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			pmNotifyErr(LOG_INFO, "Processor family: Atom");
			break;
		default:
			pmNotifyErr(LOG_INFO, "Unsupported processor model %d\n",model);
			model=-1;
			break;
	}

	/* Based on CPU model, set which MSR's should be available */
	switch(model) {

		case CPU_SANDYBRIDGE_EP:
		case CPU_IVYBRIDGE_EP:
			has_msr_pp0=1;
			has_msr_pp1=0;
			has_msr_dram=1;
			msr_different_units=0;
			has_msr_psys=0;
			break;

		case CPU_HASWELL_EP:
		case CPU_BROADWELL_EP:
		case CPU_SKYLAKE_X:
			has_msr_pp0=1;
			has_msr_pp1=0;
			has_msr_dram=1;
			msr_different_units=1;
			has_msr_psys=0;
			break;

		case CPU_KNIGHTS_LANDING:
		case CPU_KNIGHTS_MILL:
			has_msr_pp0=0;
			has_msr_pp1=0;
			has_msr_dram=1;
			msr_different_units=1;
			has_msr_psys=0;
			break;

		case CPU_SANDYBRIDGE:
		case CPU_IVYBRIDGE:
			has_msr_pp0=1;
			has_msr_pp1=1;
			has_msr_dram=0;
			msr_different_units=0;
			has_msr_psys=0;
			break;

		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			has_msr_pp0=1;
			has_msr_pp1=1;
			has_msr_dram=1;
			msr_different_units=0;
			has_msr_psys=0;
			break;

		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
		case CPU_KABYLAKE:
		case CPU_KABYLAKE_MOBILE:
			has_msr_pp0=1;
			has_msr_pp1=1;
			has_msr_dram=1;
			msr_different_units=0;
			has_msr_psys=1;
			break;
	}

	pmNotifyErr(LOG_INFO, "Found extra MSRs: dram %d, pp0 %d, pp1 %d, psys %d\n",
		has_msr_dram,has_msr_pp0,has_msr_pp1,has_msr_psys);

	return model;
}

static int read_rapl_msr(int core) {

	int			fd;
	long long	result;
	int			j;

	if (cpu_model<0) {
		pmNotifyErr(LOG_INFO, "Unsupported CPU model %d\n",cpu_model);
		return -1;
	}

	for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		/* Package Energy */
		result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
		msr_energy[msr_pnt_package_energy][j]=(double)result*cpu_energy_units[j];

		/* PP0/Cores energy */
		if (has_msr_pp0) {
			result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
			msr_energy[msr_pnt_cores_energy][j]=(double)result*cpu_energy_units[j];
		}

		/* PP1/uncore energy */
		if (has_msr_pp1) {
			result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			msr_energy[msr_pnt_uncore_energy][j]=(double)result*cpu_energy_units[j];
		}

		if (has_msr_dram) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			msr_energy[msr_pnt_dram_energy][j]=(double)result*dram_energy_units[j];
		}

		/* Skylake and newer have Psys			   */
		if (has_msr_psys) {
			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			msr_energy[msr_pnt_psys_energy][j]=(double)result*cpu_energy_units[j];
		}

		close(fd);
	}

	return 0;
}

static int read_rapl_sysfs(void) {

	int	dom,pkg;
	FILE	*fff;

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<MAX_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				fff=fopen(filenames[pkg][dom],"r");
				if (fff==NULL) {
					if (pmDebugOptions.appl0)
							pmNotifyErr(LOG_ERR, "read_rapl_sysfs() could not open %s",filenames[pkg][dom]);
				}
				else {
					if ( fscanf(fff,"%" FMT_UINT64,&raplvars[pkg][dom]) != 1)
						if (pmDebugOptions.appl0)
								pmNotifyErr(LOG_ERR, "read_rapl_sysfs() could not read %s",filenames[pkg][dom]);
					fclose(fff);
				}
			}
		}
	}
	return 0;
}


int batteries = 0;						/*	How many batteries has this system?
											Careful with battery counting!
											If we have one battery (batteries==1), that battery data
											is in energy_now[0], power_now[0] and so on.				*/

uint64_t energy_now[MAX_BATTERIES];		/* <battery>/energy_now or <battery>/charge_now readings		*/
uint64_t energy_now_old[MAX_BATTERIES];
int64_t power_now[MAX_BATTERIES];		/* <battery>/power_now readings, driver computed power consumption	*/
int capacity[MAX_BATTERIES];			/* <battery>/capacity readings, percentage of original capacity		*/

time_t secondsnow, secondsold;			/* time stamps, to understand if we need to recompute	*/
double energy_diff_d[MAX_BATTERIES], energy_rate_d[MAX_BATTERIES];	/* amount of used energy / computed energy consumption	*/

char battery_basepath[MAX_BATTERIES][512];		/* path to the batteries						*/
char energy_now_file[MAX_BATTERIES][512];		/* energy now file, different between models				*/
double energy_convert_factor[MAX_BATTERIES];	/* factor for fixing <battery>/energy_now / charge_now to kwh 		*/

static int detect_rapl_sysfs(void);				/* detect RAPL offered via /sysfs */
static int detect_rapl_msr(int);				/* detect RAPL offered via MSX registers */
static int detect_batteries(void);				/* detect batteries */
static int read_batteries(void);				/* read battery values */

static int detect_rapl_sysfs(void) {			/* detect RAPL offered via /sysfs */
	char		filename[MAXPATHLEN];
	DIR			*directory;

	pmsprintf(filename,sizeof(filename),"%s/sys/class/powercap/intel-rapl",rootpath);
	directory = opendir(filename);
	if ( directory == NULL ) {
		pmNotifyErr(LOG_INFO, "RAPL via /sys-filesystem not found.");
	} else {
		pmNotifyErr(LOG_INFO, "RAPL via /sys-filesystem was found.");
		has_rapl_sysfs=1;
		closedir(directory);
	}

	return 0;
}

static int open_msr(int core) {

	char msr_filename[BUFSIZ];
	int fd;

	pmsprintf(msr_filename,sizeof(msr_filename),"%s/dev/cpu/%d/msr", rootpath, core);
	fd = open(msr_filename, O_RDONLY);
	if ( fd < 0 ) {
		if ( errno == ENXIO ) {
			pmNotifyErr(LOG_INFO, "rdmsr: No CPU %d\n", core);
			exit(2);
		} else if ( errno == EIO ) {
			pmNotifyErr(LOG_INFO, "rdmsr: CPU %d doesn't support MSRs\n",core);
			exit(3);
		} else {
			pmNotifyErr(LOG_INFO, "rdmsr:open: Trying to open %s\n",msr_filename);
			exit(127);
		}
	}

	return fd;
}

static int detect_rapl_msr(int core) {

	int			fd;
	long long	result;
	double		power_units,time_units;
	double		thermal_spec_power,minimum_power,maximum_power,time_window;
	DIR			*directory;
	int			j;
	char		dirname[MAXPATHLEN];

	if (cpu_model<0) {
		pmNotifyErr(LOG_INFO, "CPU model %d not supported for RAPL MSR.\n",cpu_model);
		return -1;
	}

	pmsprintf(dirname,sizeof(dirname),"%s/dev/cpu/0/",rootpath);
	directory = opendir (dirname);
	if (directory == NULL) {
		pmNotifyErr(LOG_INFO, "Could not open %s/dev/cpu/0/",rootpath);
		pmNotifyErr(LOG_INFO, "msr kernel module not loaded?");
		pmNotifyErr(LOG_INFO, "Selinux policy missing, or device permissions?");
		return -1;
	}
	(void) closedir (directory);

	for(j=0;j<total_packages;j++) {

		pmNotifyErr(LOG_INFO, "\tListing paramaters for package #%d\n",j);

		fd=open_msr(package_map[j]);

		/* Calculate the units used */
		result=read_msr(fd,MSR_RAPL_POWER_UNIT);

		power_units=pow(0.5,(double)(result&0xf));
		cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
		time_units=pow(0.5,(double)((result>>16)&0xf));

		/* On Haswell EP and Knights Landing */
		/* The DRAM units differ from the CPU ones */
		if (msr_different_units) {
			dram_energy_units[j]=pow(0.5,(double)16);
			pmNotifyErr(LOG_INFO, "\t\tDRAM: Using %lf instead of %lf\n",
				dram_energy_units[j],cpu_energy_units[j]);
		}
		else {
			dram_energy_units[j]=cpu_energy_units[j];
		}

		pmNotifyErr(LOG_INFO, "\t\tPower units = %.3fW\n",power_units);
		pmNotifyErr(LOG_INFO, "\t\tCPU Energy units = %.8fJ\n",cpu_energy_units[j]);
		pmNotifyErr(LOG_INFO, "\t\tDRAM Energy units = %.8fJ\n",dram_energy_units[j]);
		pmNotifyErr(LOG_INFO, "\t\tTime units = %.8fs\n",time_units);

		if (has_msr_pp1) {
			result=read_msr(fd,MSR_PP1_POLICY);
			int pp1_policy=(int)result&0x001f;
			pmNotifyErr(LOG_INFO, "\tPowerPlane1 (on-core GPU if avail) %d policy: %d\n",
				core,pp1_policy);
		}


		/* Show package power info */
		result=read_msr(fd,MSR_PKG_POWER_INFO);
		thermal_spec_power=power_units*(double)(result&0x7fff);
		pmNotifyErr(LOG_DEBUG, "Package thermal spec: %.3fW\n",thermal_spec_power);
		minimum_power=power_units*(double)((result>>16)&0x7fff);
		pmNotifyErr(LOG_DEBUG, "Package minimum power: %.3fW\n",minimum_power);
		maximum_power=power_units*(double)((result>>32)&0x7fff);
		pmNotifyErr(LOG_DEBUG, "Package maximum power: %.3fW\n",maximum_power);
		time_window=time_units*(double)((result>>48)&0x7fff);
		pmNotifyErr(LOG_DEBUG, "Package maximum time window: %.6fs\n",time_window);
		/* Show package power limit */
		result=read_msr(fd,MSR_PKG_RAPL_POWER_LIMIT);
		pmNotifyErr(LOG_DEBUG, "Package power limits are %s\n", (result >> 63) ? "locked" : "unlocked");
		double pkg_power_limit_1 = power_units*(double)((result>>0)&0x7FFF);
		double pkg_time_window_1 = time_units*(double)((result>>17)&0x007F);
		pmNotifyErr(LOG_DEBUG, "Package power limit #1: %.3fW for %.6fs (%s, %s)\n",
			pkg_power_limit_1, pkg_time_window_1,
			(result & (1LL<<15)) ? "enabled" : "disabled",
			(result & (1LL<<16)) ? "clamped" : "not_clamped");
		double pkg_power_limit_2 = power_units*(double)((result>>32)&0x7FFF);
		double pkg_time_window_2 = time_units*(double)((result>>49)&0x007F);
		pmNotifyErr(LOG_DEBUG, "Package power limit #2: %.3fW for %.6fs (%s, %s)\n",
			pkg_power_limit_2, pkg_time_window_2,
			(result & (1LL<<47)) ? "enabled" : "disabled",
			(result & (1LL<<48)) ? "clamped" : "not_clamped");

		/* Package Energy */
		pmNotifyErr(LOG_INFO, "\tPackage %d:\n",j);
		result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
		msr_energy[msr_pnt_package_energy][j]=(double)result*cpu_energy_units[j];
		pmNotifyErr(LOG_INFO, "\t\tPackage energy: %.6fJ\n", msr_energy[msr_pnt_package_energy][j]);

		/* PP0/Cores energy */
		if (has_msr_pp0) {
			result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
			msr_energy[msr_pnt_cores_energy][j]=(double)result*cpu_energy_units[j];
			pmNotifyErr(LOG_INFO, "\t\tPowerPlane0 (cores): %.6fJ\n", msr_energy[msr_pnt_cores_energy][j]);
		}

		/* PP1/uncore energy */
		if (has_msr_pp1) {
			result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			msr_energy[msr_pnt_uncore_energy][j]=(double)result*cpu_energy_units[j];
			pmNotifyErr(LOG_INFO, "\t\tPowerPlane1 (on-core GPU): %.6f J\n", msr_energy[msr_pnt_uncore_energy][j]);
		}

		if (has_msr_dram) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			msr_energy[msr_pnt_dram_energy][j]=(double)result*dram_energy_units[j];
			pmNotifyErr(LOG_INFO, "\t\tDRAM: %.6fJ\n", msr_energy[msr_pnt_dram_energy][j]);
		}

		/* Skylake and newer have Psys		*/
		if (has_msr_psys) {
			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			msr_energy[msr_pnt_psys_energy][j]=(double)result*cpu_energy_units[j];
			pmNotifyErr(LOG_INFO, "\t\tPSYS: %.6fJ\n", msr_energy[msr_pnt_psys_energy][j]);
		}

		close(fd);

	}

	has_rapl_msr = 1;
	return 0;
}

/* detect batteries */
static int detect_batteries(void) {
	char	filename[MAXPATHLEN],dirname[MAXPATHLEN],type[32];
	DIR		*directory;
	FILE	*fff;
	int		i;

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
		if ( fscanf(fff,"%" FMT_INT64,&power_now[bat]) != 1)
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

/*
 * Denki PMDA metrics
 *
 * denki.rapl.sysfs			- energy counter from RAPL, read from sysfs
 * denki.rapl.msr			- energy counter from RAPL, read from msr registers
 * denki.bat.energy_now		- <battery>/energy_now raw reading, 
 *				  			current battery charge in Wh
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
#define RAPL_SYSFS_INDOM	0	/* serial number for rapl.sysfs instance domain */
	{ RAPL_SYSFS_INDOM, 0, NULL },
#define RAPL_MSR_INDOM		1	/* serial number for rapl.msr instance domain */
	{ RAPL_MSR_INDOM, 0, NULL },
#define BAT_ENERGYNOW_INDOM		2	/* serial number for bat.energy_now instance domain */
	{ BAT_ENERGYNOW_INDOM, 0, NULL },
#define BAT_POWERNOW_INDOM		3	/* serial number for bat.power_now instance domain */
	{ BAT_POWERNOW_INDOM, 0, NULL },
#define BAT_CAPACITY_INDOM		4	/* serial number for bat.capacity instance domain */
	{ BAT_CAPACITY_INDOM, 0, NULL }
};

/* this is merely a convenience */
static pmInDom	*rapl_sysfs_indom = &indomtab[RAPL_SYSFS_INDOM].it_indom;
static pmInDom	*rapl_msr_indom = &indomtab[RAPL_MSR_INDOM].it_indom;
static pmInDom	*bat_energynow_indom = &indomtab[BAT_ENERGYNOW_INDOM].it_indom;
static pmInDom	*bat_powernow_indom = &indomtab[BAT_POWERNOW_INDOM].it_indom;
static pmInDom	*bat_capacity_indom = &indomtab[BAT_CAPACITY_INDOM].it_indom;

/*
 * All metrics supported in this PMDA - one table entry for each.
 */

static pmdaMetric metrictab[] = {
/* rapl.sysfs */
	{ NULL,
	{ PMDA_PMID(0,0), PM_TYPE_U64, RAPL_SYSFS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* rapl.msr */
	{ NULL,
	{ PMDA_PMID(0,1), PM_TYPE_U64, RAPL_MSR_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.energy_now */
	{ NULL,
	{ PMDA_PMID(1,0), PM_TYPE_DOUBLE, BAT_ENERGYNOW_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.power_now */
	{ NULL,
	{ PMDA_PMID(1,1), PM_TYPE_DOUBLE, BAT_POWERNOW_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* bat.capacity */
	{ NULL,
	{ PMDA_PMID(1,2), PM_TYPE_32, BAT_CAPACITY_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, }
};

static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;

static void denki_rapl_sysfs_clear(void);
static void denki_rapl_sysfs_init(void);
static void denki_rapl_msr_clear(void);
static void denki_rapl_msr_init(void);
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
	int				sts;
	unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
	unsigned int	item = pmID_item(mdesc->m_desc.pmid);
	int				reqpackage=0,reqinst; // For which package is this MSR fetch request?

	if (inst != PM_IN_NULL && mdesc->m_desc.indom == PM_INDOM_NULL)
		return PM_ERR_INST;

	if (cluster == 0) {
		switch (item) {
			case 0:				/* rapl.sysfs */
				if ((sts = pmdaCacheLookup(*rapl_sysfs_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->ull = lookup_rapl_dom(inst)/1000000;
				break;
			case 1:				/* rapl.msr */
				if ((sts = pmdaCacheLookup(*rapl_msr_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}

				// For requests for package > 0 we need more computation..
				if (inst>=msr_instances) {
					reqpackage = inst / msr_instances;
					reqinst = inst % msr_instances;
				}
				else {
					reqpackage = 0;
					reqinst = inst;
				}

				//	pmNotifyErr(LOG_DEBUG, "fetch debug: Got fetch request for instance %d, actually meant for inst %d / package %d", inst, reqinst, reqpackage);
				if ( inst < msr_instances * total_packages )
						atom->ull = msr_energy[reqinst][reqpackage];
				else
						atom->ull = 23;
				break;
			default:
				return PM_ERR_PMID;
		}
	}
	else if (cluster == 1) {
		switch (item) {
			case 0:				/* denki.bat.energy_now */
				if ((sts = pmdaCacheLookup(*bat_energynow_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->d = energy_now[inst]/energy_convert_factor[inst];
				break;
			case 1:				/* denki.bat.power_now */
				if ((sts = pmdaCacheLookup(*bat_powernow_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
					if (sts < 0)
						pmNotifyErr(LOG_ERR, "pmdaCacheLookup failed: inst=%d: %s", inst, pmErrStr(sts));
					return PM_ERR_INST;
				}
				atom->d = power_now[inst]/1000000.0;
				break;
			case 2:				/* denki.bat.capacity */
				if ((sts = pmdaCacheLookup(*bat_capacity_indom, inst, NULL, NULL)) != PMDA_CACHE_ACTIVE) {
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
 * instance domain evaluation
 */
static int
denki_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
	if (has_rapl_sysfs)
		read_rapl_sysfs();
	if (has_rapl_msr)
			read_rapl_msr(cpu_core);
	if (has_bat)
		read_batteries();
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
	return pmdaInstance(indom, foo, bar, iresp, pmda);
}


/*
 * clear the rapl sysfs metric instance domains
 */
static void denki_rapl_sysfs_clear(void)
{
	int		sts;

	sts = pmdaCacheOp(*rapl_sysfs_indom, PMDA_CACHE_INACTIVE);
	if (sts < 0)
		pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	pmInDomStr(*rapl_sysfs_indom), pmErrStr(sts));
}

/* 
 * register RAPL sysfs cores as indoms
 */
static void denki_rapl_sysfs_init(void)
{
	int		sts;
	int		dom,pkg;
	char	tmp[80];

	for(pkg=0;pkg<total_packages;pkg++) {
		for(dom=0;dom<MAX_RAPL_DOMAINS;dom++) {
			if (valid[pkg][dom]) {
				/* instance names need to be unique, so if >1 rapl packages,
				   we prepend the rapl-domain counter */
				if (total_packages > 1)
					pmsprintf(tmp,sizeof(tmp),"%d-%s",pkg,event_names[pkg][dom]);
				else
					pmsprintf(tmp,sizeof(tmp),"%s",event_names[pkg][dom]);

				/* rapl.sysfs */
				sts = pmdaCacheStore(*rapl_sysfs_indom, PMDA_CACHE_ADD, tmp, NULL);
				if (sts < 0) {
					pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
					return;
				}
			}
		}
	}

	if (pmdaCacheOp(*rapl_sysfs_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
		pmNotifyErr(LOG_WARNING, "rapl sysfs instance domain is empty");
}

/*
 * clear the rapl msr metric instance domains
 */
static void
denki_rapl_msr_clear(void)
{
	int		sts;

	sts = pmdaCacheOp(*rapl_msr_indom, PMDA_CACHE_INACTIVE);
	if (sts < 0)
		pmNotifyErr(LOG_ERR, "pmdaCacheOp(INACTIVE) failed: indom=%s: %s",
	pmInDomStr(*rapl_msr_indom), pmErrStr(sts));
}

/* 
 * register RAPL msr cores as indoms
 */
static void denki_rapl_msr_init(void)
{
	int		sts;
	char	tmp[80];
	int 	j;

	for(j=0;j<total_packages;j++) {

		msr_instances=0;

		if (total_packages > 1)
			pmsprintf(tmp,sizeof(tmp),"%d-package_energy",j);
		else
			pmsprintf(tmp,sizeof(tmp),"package_energy");

		sts = pmdaCacheStore(*rapl_msr_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}
		msr_pnt_package_energy=msr_instances++;

		if (has_msr_pp0) {
			if (total_packages > 1)
				pmsprintf(tmp,sizeof(tmp),"%d-cores_energy",j);
			else
				pmsprintf(tmp,sizeof(tmp),"cores_energy");
	
			sts = pmdaCacheStore(*rapl_msr_indom, PMDA_CACHE_ADD, tmp, NULL);
			if (sts < 0) {
				pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
				return;
			}
			msr_pnt_cores_energy=msr_instances++;
		}

		/* PP1 energy / on-core GPU / uncore */
		if (has_msr_pp1) {
			if (total_packages > 1)
				pmsprintf(tmp,sizeof(tmp),"%d-uncore_energy",j);
			else
				pmsprintf(tmp,sizeof(tmp),"uncore_energy");

			sts = pmdaCacheStore(*rapl_msr_indom, PMDA_CACHE_ADD, tmp, NULL);
			if (sts < 0) {
				pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
				return;
			}
			msr_pnt_uncore_energy=msr_instances++;
		}

		if (has_msr_dram) {
			if (total_packages > 1)
				pmsprintf(tmp,sizeof(tmp),"%d-dram_energy",j);
			else
				pmsprintf(tmp,sizeof(tmp),"dram_energy");

			sts = pmdaCacheStore(*rapl_msr_indom, PMDA_CACHE_ADD, tmp, NULL);
			if (sts < 0) {
				pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
				return;
			}
			msr_pnt_dram_energy=msr_instances++;
		}

		/* PSys is Skylake+ */
		if (has_msr_psys) {
			if (total_packages > 1)
				pmsprintf(tmp,sizeof(tmp),"%d-psys_energy",j);
			else
				pmsprintf(tmp,sizeof(tmp),"psys_energy");

			sts = pmdaCacheStore(*rapl_msr_indom, PMDA_CACHE_ADD, tmp, NULL);
			if (sts < 0) {
				pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
				return;
			}
			msr_pnt_psys_energy=msr_instances++;
		}

	}
	if (pmdaCacheOp(*rapl_msr_indom, PMDA_CACHE_SIZE_ACTIVE) < 1)
		pmNotifyErr(LOG_WARNING, "rapl msr instance domain is empty");
	pmNotifyErr(LOG_INFO, "We registered %d instances per package.", msr_instances);
}

/* 
 * register batteries as indoms
 */
static void
denki_bat_init(void)
{
	int		sts,battery;
	char	tmp[80];

	pmNotifyErr(LOG_INFO, "running bat_init for %d batteries",batteries);

	for(battery=0; battery<batteries; battery++) {

		pmsprintf(tmp,sizeof(tmp),"battery-%d",battery);

		/* bat.energy_now */
		sts = pmdaCacheStore(*bat_energynow_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}

		/* bat.power_now */
		sts = pmdaCacheStore(*bat_powernow_indom, PMDA_CACHE_ADD, tmp, NULL);
		if (sts < 0) {
			pmNotifyErr(LOG_ERR, "pmdaCacheStore failed: %s", pmErrStr(sts));
			return;
		}

		/* bat.capacity */
		sts = pmdaCacheStore(*bat_capacity_indom, PMDA_CACHE_ADD, tmp, NULL);
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
				case RAPL_SYSFS_INDOM:
					pmdaAddLabels(lpp, "{\"indom_name\":\"rapl sysfs\"}");
					break;
				case RAPL_MSR_INDOM:
					pmdaAddLabels(lpp, "{\"indom_name\":\"rapl msr\"}");
					break;
				case BAT_ENERGYNOW_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"watt hours\"}");
					break;
				case BAT_POWERNOW_INDOM:
					pmdaAddLabels(lpp, "{\"units\":\"watt\"}");
					break;
				case BAT_CAPACITY_INDOM:
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
	if (isDSO) {
		int sep = pmPathSeparator();
	
		if (strcmp(rootpath, "/") == 0) {
			/*
			 * no -r ROOTPATH on the command line ... check for
			 * DENKI_SYSPATH in the environment
			 */
			char	*envpath = getenv("DENKI_SYSPATH");
			if (envpath)
				pmsprintf(rootpath, sizeof(rootpath), "%s", envpath);
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

	pmNotifyErr(LOG_INFO, "Configured to use %s as rootpath.", rootpath);

	cpu_model=detect_cpu();
	detect_rapl_packages();			// needed by both RAPL-sysfs and RAPL-MSR

	detect_rapl_sysfs();
	if (has_rapl_sysfs) {
		detect_rapl_domains();
		denki_rapl_sysfs_clear(); 	// now we clear 
		denki_rapl_sysfs_init();	// and register rapl sysfs domains
	}

	detect_rapl_msr(cpu_core);
	if (has_rapl_msr) {
		read_rapl_msr(cpu_core);
		denki_rapl_msr_clear(); 	// now we clear 
		denki_rapl_msr_init();		// and register rapl msr domains
	}

	detect_batteries();
	if (has_bat)
		denki_bat_init();			// now register batteries as indoms
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
	int				c,sep = pmPathSeparator();
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
				pmsprintf(rootpath, sizeof(rootpath), "%s", opts.optarg);
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
