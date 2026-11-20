/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2013-2017,2019-2020 Red Hat.
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Portions Copyright (c) 2001 Gilly Ran (gilly@exanet.com) - for the
 * portions supporting the Alpha platform.  All rights reserved.
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
 */
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "linux.h"
#include "proc_stat.h"
#include "proc_cpuinfo.h"
#include "arm_cpuinfo.h"

static const char *sysfs_path = "sys/devices/system";

/*
 * Refresh state of NUMA node and CPU online state for one
 * CPU or NUMA node ("node" parameter).
 */
int
refresh_sysfs_online(char *instname, const char *node_or_cpu)
{
    char path[MAXPATHLEN];
    unsigned int online;
    FILE *fp;
    int n;

    pmsprintf(path, sizeof(path), "%s/%s/%s/%s/online",
		linux_statspath, sysfs_path, node_or_cpu, instname);
    if ((fp = fopen(path, "r")) == NULL)
	return 1;
    n = fscanf(fp, "%u", &online);
    fclose(fp);
    if (n != 1)
	return 1;
    return online;
}

unsigned long
refresh_sysfs_thermal_throttle(char *instname,
		const char *core_or_package, const char *count_or_time)
{
    char path[MAXPATHLEN];
    unsigned long value;
    FILE *fp;
    int n;

    pmsprintf(path, sizeof(path),
		"%s/%s/cpu/%s/thermal_throttle/%s_throttle_%s",
		linux_statspath, sysfs_path, instname,
		core_or_package, count_or_time);
    if ((fp = fopen(path, "r")) == NULL)
	return 0;
    n = fscanf(fp, "%lu", &value);
    fclose(fp);
    if (n != 1)
	return 0;
    return value;
}

int
refresh_sysfs_frequency_scaling_cur_freq(char *instname, int item, percpu_t *cpu)
{
    char path[MAXPATHLEN];
    unsigned long freq;
    FILE *fp;

    if (cpu->freq.flags & CPUFREQ_SCALE)
	return 0;

    pmsprintf(path, sizeof(path),
		"%s/%s/cpu/%s/cpufreq/scaling_cur_freq",
		linux_statspath, sysfs_path, instname);
    if ((fp = fopen(path, "r")) != NULL) {
	if (fscanf(fp, "%lu", &freq) == 1) {
	    cpu->freq.scale = (float)freq / 1000.0;	/* KHz to MHz */
	    cpu->freq.count = freq / 1000;	/* convert KHz to MHz */
	    cpu->freq.flags |= CPUFREQ_COUNT;
	}
	fclose(fp);
    }
    cpu->freq.flags |= CPUFREQ_SCALE;
    return 0;
}

int
refresh_sysfs_frequency_scaling(char *instname, int item, percpu_t *cpu)
{
    unsigned long long count, hits, total;
    unsigned long freq, maxfreq, minfreq;
    char path[MAXPATHLEN];
    FILE *fp;

    /* already read the value during this sample? */
    if (cpu->freq.flags & CPUFREQ_SAMPLED)
	return 0;

    /*
     * gather frequency scaling info from (in order of preference): 
     *     /sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state
     * or
     *     /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
     *     /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
     *     /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
     */

    pmsprintf(path, sizeof(path),
		"%s/%s/cpu/%s/cpufreq/stats/time_in_state",
		linux_statspath, sysfs_path, instname);
    if ((fp = fopen(path, "r")) != NULL) {
	cpu->freq.flags = CPUFREQ_COUNT|CPUFREQ_TIME|CPUFREQ_MIN|CPUFREQ_MAX;
	maxfreq = minfreq = 0;
	hits = total = 0;
	while (fscanf(fp, "%lu %llu", &freq, &count) == 2) {
	    freq /= 1000;	/* convert KHz to MHz */
	    total += (freq * count);
	    hits += count;
	    if (freq > maxfreq)
		maxfreq = freq;
	    if (minfreq == 0 || freq < minfreq)
		minfreq = freq;
	}
	fclose(fp);

	cpu->freq.max = maxfreq;
	cpu->freq.min = minfreq;
	cpu->freq.time = hits;
	cpu->freq.count = total;
	cpu->freq.flags |= CPUFREQ_SAMPLED;
	return 0;
    }

    /* governor statistics not available, try alternate files */
    pmsprintf(path, sizeof(path),
		"%s/%s/cpu/%s/cpufreq/cpuinfo_max_freq",
		linux_statspath, sysfs_path, instname);
    if ((fp = fopen(path, "r")) != NULL) {
	if (fscanf(fp, "%lu", &maxfreq) == 1) {
	    cpu->freq.max = maxfreq / 1000;	/* convert KHz to MHz */
	    cpu->freq.flags |= CPUFREQ_MAX;
	}
	fclose(fp);
    }
    pmsprintf(path, sizeof(path),
		"%s/%s/cpu/%s/cpufreq/cpuinfo_min_freq",
		linux_statspath, sysfs_path, instname);
    if ((fp = fopen(path, "r")) != NULL) {
	if (fscanf(fp, "%lu", &minfreq) == 1) {
	    cpu->freq.min = minfreq / 1000;	/* convert KHz to MHz */
	    cpu->freq.flags |= CPUFREQ_MIN;
	}
	fclose(fp);
    }
    cpu->freq.flags |= CPUFREQ_SAMPLED;
    return refresh_sysfs_frequency_scaling_cur_freq(instname, item, cpu);
}

static char *
trim_whitespace(char *s)
{
    char *end;

    while (isspace(*s))
	s++;	/* trim leading whitespace */
    if (*s == '\0') 
	return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end))
	end--;	/* trim trailing whitespace */
    *(end + 1) = '\0';
    return s;
}

int
refresh_proc_cpuinfo(void)
{
#define PROCESSOR_LINE 1
    char buf[4096];
    FILE *fp;
    int sts, cpunum, cpumax;
    int dups = 0, previous = -1;
    pmInDom cpus = INDOM(CPU_INDOM);
    percpu_t *cp;
    cpuinfo_t saved = { 0 };
    cpuinfo_t *info = NULL;
    char *val, *p;
    int lineno = 0;

    if ((fp = linux_statsfile("/proc/cpuinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    cpunum = -1;
    cpumax = pmdaCacheOp(INDOM(CPU_INDOM), PMDA_CACHE_SIZE);
    setup_cpu_info(&saved);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	lineno++;
	/* comment seen for (at least) IBM/S390 */
	if (buf[0] == '#')
	    continue;
	/*
	 * on aarch64 may have Processor : info and processor : <cpunum>
	 * lines ... skip first of these
	 */
	if (strncmp(buf, "Processor", 9) == 0)
	    continue;
	if ((p = strrchr(buf, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buf, ':')) == NULL)
	    continue;
	/* val -> value after ': ' */
	val = p+2;
	/*
	 * now null terminate the initial tag words before the whitespace
	 * preceding the :
	 */
	p--;
	while (p > buf && (*p == ' ' || *p == '\t'))
	    p--;
	*++p = '\0';
	/* now convert tag word(s) to lowercase ... */
	for (p = buf; *p && *p != ':'; p++)
	    *p = (char)tolower((int)*p);
	/*
	 * so now, for example,
	 * buf="cpu MHz		: 624.194"
	 * becomes
	 * buf="cpu mhz" and val="624.194"
	 */
	if (pmDebugOptions.appl9 && pmDebugOptions.desperate) {
	    fprintf(stderr, "[%d] machine=%d tag=\"%s\" val=\"%s\"\n", lineno, machine, buf, buf);
	    if (info) {
		fprintf(stderr, "     info: cpu[%d]", cpunum);
		if (info->vendor >= 0)
		    fprintf(stderr, " vendor \"%s\"", linux_strings_lookup(info->vendor));
		if (info->model >= 0)
		    fprintf(stderr, " model \"%s\"", linux_strings_lookup(info->model));
		if (info->model_name >= 0)
		    fprintf(stderr, " model_name \"%s\"", linux_strings_lookup(info->model_name));
		if (info->sapic >= 0)
		    fprintf(stderr, " sapic \"%s\"", linux_strings_lookup(info->sapic));
		if (info->stepping >= 0)
		    fprintf(stderr, " stepping \"%s\"", linux_strings_lookup(info->stepping));
		if (info->flags >= 0)
		    fprintf(stderr, " flags \"%s\"", linux_strings_lookup(info->flags));
		if (info->cache > 0)
		    fprintf(stderr, " cache %d", info->cache);
		if (info->cache_align > 0)
		    fprintf(stderr, " cache_align %d", info->cache_align);
		if (info->bogomips > 0)
		    fprintf(stderr, " bogomips %.1f", info->bogomips);
		if (info->clock > 0)
		    fprintf(stderr, " clock %.1f", info->clock);
		fputc('\n', stderr);
	    }
	}

	if (strcmp(buf, "processor") == 0) {
	    cpunum++;
	    if (previous == PROCESSOR_LINE)
		dups = 1;	/* aarch64-mode; dup values at the end */
	    previous = PROCESSOR_LINE;
	    continue;
	}
	previous = !PROCESSOR_LINE;

	if (cpunum >= cpumax) {
	    if (pmDebugOptions.appl9 && pmDebugOptions.desperate) {
		fprintf(stderr, "line skipped: cpunum (%d) >= cpumax (%d)\n", cpunum, cpumax);
	    }
	    continue;
	}

	/* we may need to save up state before seeing any processor ID */
	if (dups || cpunum < 0) {
	    dups = 1;
	    info = &saved;
	}
	else {
	    cp = NULL;
	    sts = pmdaCacheLookup(cpus, cpunum, NULL, (void **)&cp);
	    if (sts < 0 || !cp) {
		if (pmDebugOptions.appl9 && pmDebugOptions.desperate) {
		    fprintf(stderr, "line skipped: pmdaCacheLookup fail: sts=%d cp=%p\n", sts, cp);
		}
		continue;
	    }
	    memset(&cp->freq, 0, sizeof(cp->freq));
	    info = &cp->info;
	}

	if (info->vendor < 0) {
	    if (strcmp(buf, "vendor") == 0 ||
		strcmp(buf, "vendor_id") == 0) {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] vendor=%s (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
		    info->vendor = linux_strings_insert(val);
	    }
	    else if (machine == MACHINE_AARCH64 && strcmp(buf, "cpu implementer") == 0) {
		const char	*arm;
		if ((arm = arm_cpuinfo_vendor(val)) != NULL) {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] vendor=%s from %s (machine=%d tag=\"%s\")\n", lineno, arm, val, machine, buf);
		    info->vendor = linux_strings_insert(arm);
		}
		else {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] vendor=%s (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
		    info->vendor = linux_strings_insert(val);
		}
	    }
	}
	if (info->model < 0) {
	    if (strcmp(buf, "model") == 0 ||
		(machine == MACHINE_ALPHA && strcmp(buf, "cpu model") == 0) ||
		((machine == MACHINE_PPC || machine == MACHINE_IA64) && strcmp(buf, "revision") == 0) ||
		(machine == MACHINE_AARCH64 && strcmp(buf, "cpu revision") == 0)) {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] model=%s (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
		    info->model = linux_strings_insert(val);
	    }
	}
	if (info->model_name < 0) {
	    if (strcmp(buf, "model name") == 0 ||
		(machine == MACHINE_RISCV64 && strcmp(buf, "uarch") == 0) ||
		((machine == MACHINE_ALPHA || machine == MACHINE_PPC) && strcmp(buf, "cpu") == 0)) {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] model_name=%s (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
		    info->model_name = linux_strings_insert(val);
	    }
	    else if (machine == MACHINE_AARCH64 && strcmp(buf, "cpu part") == 0) {
		const char	*arm;
		if ((arm = arm_cpuinfo_part(val)) != NULL) {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] model_name=%s from %s (machine=%d tag=\"%s\")\n", lineno, arm, val, machine, buf);
		    info->model_name = linux_strings_insert(arm);
		}
		else {
		    if (pmDebugOptions.appl9)
			fprintf(stderr, "[%d] model_name=%s (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
		    info->model_name = linux_strings_insert(val);
		}
	    }
	}
	if (machine == MACHINE_AARCH64 && strcmp(buf, "hardware") == 0) {
	    /*
	     * Hardware : ... line from older ARM64 systems gazumps any
	     * possible model_name from other lines
	     */
	    info->model_name = linux_strings_insert(val);
	}
	if (info->sapic < 0 && strcmp(buf, "sapic") == 0)
	    info->sapic = linux_strings_insert(val);
	if (info->stepping < 0 &&
	    (strcmp(buf, "stepping") == 0))
	     //strcmp(buf, "revision") == 0 ||
	     //strcmp(buf, "cpu revision") == 0)
		info->stepping = linux_strings_insert(val);
	if (info->flags < 0 && strcmp(buf, "flags") == 0)
	    info->flags = linux_strings_insert(val);
	if (info->flags < 0 && strcmp(buf, "features") == 0)
	    info->flags = linux_strings_insert(trim_whitespace(val));
	if (info->cache == 0 && strcmp(buf, "cache size") == 0)
	    info->cache = atoi(val);
	if (info->cache_align == 0 && strcmp(buf, "cache_alignment") == 0)
	    info->cache_align = atoi(val);
	if (info->bogomips == 0.0 &&
	    (strcmp(buf, "bogomips") == 0 || strcmp(buf, "bogomips per cpu") == 0)) {
	    if (pmDebugOptions.appl9)
		fprintf(stderr, "[%d] bogomips=atof(%s) (machine=%d tag=\"%s\")\n", lineno, val, machine, buf);
	    info->bogomips = atof(val);
	}
	if (strcmp(buf, "cpu mhz") == 0) /* cpu MHz can change */
	    info->clock = atof(val);
	if (strcmp(buf, "cycle frequency [hz]") == 0) { /* ditto */
	    if ((p = strchr(val, ' ')) != NULL)
		*p = '\0';
	    info->clock = (atof(val))/1000000;
	}
    }
    fclose(fp);

    /* all identical processors, duplicate last through earlier instances */
    if (dups) {
	for (pmdaCacheOp(cpus, PMDA_CACHE_WALK_REWIND), cp = NULL;; cp = NULL) {
	    if ((sts = pmdaCacheOp(cpus, PMDA_CACHE_WALK_NEXT)) < 0)
		break;
	    if (pmdaCacheLookup(cpus, sts, NULL, (void **)&cp) < 0 || !cp)
		continue;
	    memcpy(&cp->info, &saved, sizeof(cpuinfo_t));
	}
    }

    /* success */
    return 0;
}
