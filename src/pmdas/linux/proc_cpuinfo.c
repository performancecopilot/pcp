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

    if ((fp = linux_statsfile("/proc/cpuinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    cpunum = -1;
    cpumax = pmdaCacheOp(INDOM(CPU_INDOM), PMDA_CACHE_SIZE);
    setup_cpu_info(&saved);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((val = strrchr(buf, '\n')) != NULL)
	    *val = '\0';
	if ((val = strchr(buf, ':')) == NULL)
	    continue;
	val += 2;

	if (strncmp(buf, "processor", 9) == 0) {
	    cpunum++;
	    if (previous == PROCESSOR_LINE)
		dups = 1;	/* aarch64-mode; dup values at the end */
	    previous = PROCESSOR_LINE;
	    continue;
	}
	previous = !PROCESSOR_LINE;

	if (cpunum >= cpumax)
	    continue;

	/* we may need to save up state before seeing any processor ID */
	if (dups || cpunum < 0) {
	    dups = 1;
	    info = &saved;
	}
	else {
	    cp = NULL;
	    sts = pmdaCacheLookup(cpus, cpunum, NULL, (void **)&cp);
	    if (sts < 0 || !cp)
		continue;
	    memset(&cp->freq, 0, sizeof(cp->freq));
	    info = &cp->info;
	}

	/* note: order is important due to strNcmp comparisons */
	if (info->sapic < 0 && strncasecmp(buf, "sapic", 5) == 0)
	    info->sapic = linux_strings_insert(val);
	else if (info->model_name < 0 && strncasecmp(buf, "model name", 10) == 0)
	    info->model_name = linux_strings_insert(val);
	else if (info->model_name < 0 && strncasecmp(buf, "hardware", 8) == 0)
	    info->model_name = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "model", 5) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "cpu model", 9) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "cpu variant", 11) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->vendor < 0 && strncasecmp(buf, "vendor", 6) == 0)
	    info->vendor = linux_strings_insert(val);
	else if (info->vendor < 0 && strncasecmp(buf, "cpu implementer", 15) == 0)
	    info->vendor = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "step", 4) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "revision", 8) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "cpu revision", 12) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->flags < 0 && strncasecmp(buf, "flags", 5) == 0)
	    info->flags = linux_strings_insert(val);
	else if (info->flags < 0 && strncasecmp(buf, "features", 8) == 0)
	    info->flags = linux_strings_insert(trim_whitespace(val));
	else if (info->cache == 0 && strncasecmp(buf, "cache size", 10) == 0)
	    info->cache = atoi(val);
	else if (info->cache_align == 0 && strncasecmp(buf, "cache_align", 11) == 0)
	    info->cache_align = atoi(val);
	else if (info->bogomips == 0.0 && strncasecmp(buf, "bogo", 4) == 0)
	    info->bogomips = atof(val);
	else if (strncasecmp(buf, "cpu MHz", 7) == 0) /* cpu MHz can change */
	    info->clock = atof(val);
	else if (info->clock == 0.0 && strncasecmp(buf, "cycle frequency", 15) == 0) {
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
