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

#ifndef CONFIGPARSER_H_
#define CONFIGPARSER_H_

#include <stddef.h> /* for size_t */

typedef struct pmctype {
    char *name;
    struct pmctype *next;
} pmctype_t;

#define CPUCONFIG_EACH_CPU -1
#define CPUCONFIG_EACH_NUMANODE -2
#define CPUCONFIG_ROUNDROBIN_CPU -3
#define CPUCONFIG_ROUNDROBIN_NUMANODE -4

typedef struct pmcsetting {
    char *name;
    int cpuConfig;
    double scale;    /* Currently, only used by derived events */
    int need_perf_scale;  /* Currently, only used by derived events */
    struct pmcsetting *next;
} pmcsetting_t; 

typedef struct pmcconfiguration {
    pmctype_t *pmcTypeList;
    pmcsetting_t *pmcSettingList;
} pmcconfiguration_t;

typedef struct settingLists {
    int nsettings;
    pmcsetting_t *derivedSettingList;
    struct settingLists *next;
} pmcSettingLists_t;

typedef struct pmcderived {
    char *name;
    pmcSettingLists_t *setting_lists;
    /* pmcsetting_t *derivedSettingList; */
} pmcderived_t;

typedef struct pmcdynamic {
    char *name;
    pmcsetting_t *dynamicSettingList;
} pmcdynamic_t;

typedef struct configuration {
    pmcconfiguration_t *configArr;
    size_t nConfigEntries;
    pmcderived_t *derivedArr;
    size_t nDerivedEntries;
    pmcdynamic_t *dynamicpmc;
} configuration_t;

int context_newpmc;
int context_derived;        /* A flag to check the current pmc */
int context_dynamic;        /* check the current dynamic pmc */

/* \brief parse the perf event configuration file
 * This function allocates memory. The returned object should be passed to
 * free_configuration() to clean up the memory.
 *
 * \returns a pointer to an configuration object on success or NULL on failure.
 */
configuration_t *parse_configfile(const char *filename);

/* \brief returns the memory allocated by the parse_configfile() function
 */
void free_configuration(configuration_t *);
#endif 
