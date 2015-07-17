/*
 * Copyright (C) 2013  Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "rapl-interface.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

/* This device is purely for convience of unit testing
 */
#ifndef FILESYSTEM_ROOT
#define FILESYSTEM_ROOT "/"
#endif

#define NOT_SUPPORTED  0
#define SANDYBRIDGE    (1ull << 0)
#define SANDYBRIDGE_EP (1ull << 1)
#define IVYBRIDGE      (1ull << 2)
#define IVYBRIDGE_EP   (1ull << 3)
#define HASWELL        (1ull << 4)

struct rapl_event_info {
    char *name;
    char *units;
    uint64_t supported_cpusets;
};


struct rapl_event_info rapl_events[] = {
    { "RAPL:PACKAGE_ENERGY", "mJ", SANDYBRIDGE | SANDYBRIDGE_EP | IVYBRIDGE | IVYBRIDGE_EP | HASWELL },
    { "RAPL:PP0_ENERGY",     "mJ", SANDYBRIDGE | SANDYBRIDGE_EP | IVYBRIDGE | IVYBRIDGE_EP | HASWELL },
    { "RAPL:PP1_ENERGY",     "mJ", SANDYBRIDGE | IVYBRIDGE | HASWELL  },
    { "RAPL:DRAM_ENERGY",    "mJ", SANDYBRIDGE_EP | IVYBRIDGE_EP },
    { "RAPL:THERMAL_SPEC",   "mW", SANDYBRIDGE | SANDYBRIDGE_EP | IVYBRIDGE | IVYBRIDGE_EP | HASWELL },
    { "RAPL:MINIMUM_POWER",  "mW", SANDYBRIDGE | SANDYBRIDGE_EP | IVYBRIDGE | IVYBRIDGE_EP | HASWELL },
    { "RAPL:MAXIMUM_POWER",  "mW", SANDYBRIDGE | SANDYBRIDGE_EP | IVYBRIDGE | IVYBRIDGE_EP | HASWELL },
};

#define RAPL_PACKAGE_ENERGY 0
#define RAPL_PP0_ENERGY 1
#define RAPL_PP1_ENERGY 2
#define RAPL_DRAM_ENERGY 3
#define RAPL_THERMAL_SPEC 4
#define RAPL_MINIMUM_POWER 5
#define RAPL_MAXIMUM_POWER 6

#define N_RAPL_EVENTS (sizeof(rapl_events) / sizeof(rapl_events[0]) )

#define MSR_RAPL_POWER_UNIT		0x606

/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F

#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F00

#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF000

typedef struct rapl_cpudata_t_ {
    int msrfd;
} rapl_cpudata_t;

rapl_cpudata_t *rapl_cpudata = NULL;
int rapl_cpumodel = -1;
int rapl_ncpus = 0;

#define CPU_SANDYBRIDGE		42
#define CPU_SANDYBRIDGE_EP	45
#define CPU_IVYBRIDGE		58
#define CPU_IVYBRIDGE_EP	62
#define CPU_HASWELL		60

static uint64_t detect_cpu(void) {

	FILE *fff;

	int family,model=-1;
	char buffer[BUFSIZ],*result;
	char vendor[BUFSIZ];

	fff=fopen( FILESYSTEM_ROOT "proc/cpuinfo","r");
	if (fff==NULL) return -1;

	while(1) {
		result=fgets(buffer,BUFSIZ,fff);
		if (result==NULL) break;

		if (!strncmp(result,"vendor_id",8)) {
			sscanf(result,"%*s%*s%s",vendor);

			if (strncmp(vendor,"GenuineIntel",12)) {
			    	fclose(fff);
				return -1;
			}
		}

		if (!strncmp(result,"cpu family",10)) {
			sscanf(result,"%*s%*s%*s%d",&family);
			if (family!=6) {
			    	fclose(fff);
				return -1;
			}
		}

		if (!strncmp(result,"model",5)) {
			sscanf(result,"%*s%*s%d",&model);
		}

	}

	fclose(fff);

    uint64_t cputype = NOT_SUPPORTED;

	switch(model) {
		case CPU_SANDYBRIDGE:
			cputype = SANDYBRIDGE;
			break;
		case CPU_SANDYBRIDGE_EP:
			cputype = SANDYBRIDGE_EP;
			break;
		case CPU_IVYBRIDGE:
			cputype = IVYBRIDGE;
			break;
		case CPU_IVYBRIDGE_EP:
			cputype = IVYBRIDGE_EP;
			break;
		case CPU_HASWELL:
			cputype = HASWELL;
			break;
		default:	
            cputype = NOT_SUPPORTED;
            break;
	}

	return cputype;
}

static uint64_t read_msr(int fd, int which) {

  uint64_t data;

  if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
    perror("rdmsr:pread");
  }

  return data;
}

void rapl_init()
{
    int i;

    rapl_ncpus = sysconf(_SC_NPROCESSORS_ONLN);

    if( rapl_ncpus ==-1 ) {
        rapl_ncpus = 1;
    }

    rapl_cpumodel = detect_cpu(); // TODO reuse the cpu architecture detection code

    rapl_cpudata = malloc( rapl_ncpus * sizeof( rapl_cpudata_t ) );
    
    if( rapl_cpudata ) {
        for(i = 0; i < rapl_ncpus; ++i) {
            rapl_cpudata[i].msrfd = -1;
        }
    }
}

void rapl_destroy()
{
    int i;

    if( rapl_cpudata == NULL ) {
        return;
    }

    for(i = 0; i < rapl_ncpus; ++i) {
        if( rapl_cpudata[i].msrfd != -1 ) {
            close( rapl_cpudata[i].msrfd );
        }
    }

    free(rapl_cpudata);
    rapl_cpudata = 0;
    rapl_ncpus = 0;
}

int rapl_read(rapl_data_t *arg, uint64_t *result)
{
    uint64_t msrval;
    double power_units;
    double energy_units;
    int fd;
    int retval = 0;

    if( arg == NULL || rapl_cpudata == NULL ) {
        return -1;
    }

    fd = rapl_cpudata[arg->cpuidx].msrfd;

    if( fd == -1 ) {
        return -2;
    }

    /* Calculate the units used */
    // TODO could cache this information 
    msrval=read_msr(fd,MSR_RAPL_POWER_UNIT);
    power_units=pow(0.5,(double)(msrval&0xf));
    energy_units=pow(0.5,(double)((msrval>>8)&0x1f));

    switch(arg->eventcode)
    {
        case RAPL_PACKAGE_ENERGY:
            msrval=read_msr(fd,MSR_PKG_ENERGY_STATUS);
            *result = (uint64_t)(1.0e3 * (double)msrval*energy_units );
            break;
        case RAPL_PP0_ENERGY:
            msrval=read_msr(fd,MSR_PP0_ENERGY_STATUS);
            *result = (uint64_t)(1.0e3 * (double)msrval*energy_units );
            break;
        case RAPL_PP1_ENERGY:
            msrval=read_msr(fd,MSR_PP1_ENERGY_STATUS);
            *result = (uint64_t)(1.0e3 * (double)msrval*energy_units );
            break;
        case RAPL_DRAM_ENERGY:
            msrval=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
            *result = (uint64_t)(1.0e3 *  (double)msrval*energy_units );
            break;
        case RAPL_THERMAL_SPEC:
            msrval=read_msr(fd,MSR_PKG_POWER_INFO);
            *result = (uint64_t)(1.0e3 * power_units*(double)(msrval&0x7fff) );
            break;
        case RAPL_MINIMUM_POWER:
            msrval=read_msr(fd,MSR_PKG_POWER_INFO);
            *result = (uint64_t)(1.0e3 * power_units*(double)((msrval>>16)&0x7fff) );
            break;
        case RAPL_MAXIMUM_POWER:
            msrval=read_msr(fd,MSR_PKG_POWER_INFO);
            *result = (uint64_t)(1.0e3 * power_units*(double)((msrval>>32)&0x7fff) );
            break;
        default:
            retval = -3;
            break;
    }

    return retval;
}

int rapl_open(rapl_data_t *arg)
{
    char msr_filename[BUFSIZ];

    if( rapl_cpudata == NULL || arg == NULL || arg->cpuidx >= rapl_ncpus ) {
        errno = EINVAL;
        return -1;
    }

    if( rapl_cpudata[arg->cpuidx].msrfd == -1 ) {
        sprintf(msr_filename, FILESYSTEM_ROOT "dev/cpu/%d/msr", arg->cpuidx);
        rapl_cpudata[arg->cpuidx].msrfd = open(msr_filename, O_RDONLY);
    }

    return (rapl_cpudata[arg->cpuidx].msrfd == -1) ? -3 : 0; 
}


int rapl_get_os_event_encoding(const char *eventname, int cpu, rapl_data_t *arg)
{
    int i;

    if( arg == NULL ) {
        return -1;
    }

    arg->eventcode = -1;
    for(i=0 ; i < N_RAPL_EVENTS; ++i )
    {
        if( 0 == strcmp(eventname, rapl_events[i].name) ) {
            if( rapl_events[i].supported_cpusets & rapl_cpumodel ) {
                /* Found event that is supported on this cpu */
                arg->eventcode = i;
                arg->cpuidx = cpu;
                return 0;
            }
        }
    }

    return -1;
}

