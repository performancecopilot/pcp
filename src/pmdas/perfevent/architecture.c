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

#include "architecture.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define SYSFS_NODE_PATH "devices/system/node"

static int numanodefilter(const struct dirent *entry)
{
    unsigned int tmp;
    return (1 == sscanf(entry->d_name, "node%u", &tmp));
}

static void init_cpulist(cpulist_t *this, size_t count)
{
    this->count = count;
    this->index = malloc( count * sizeof *(this->index) );
}

static void free_cpulist(cpulist_t *del)
{
    free(del->index);
}

/* \brief convert list format ASCII string into an array of ints
 * \param line NULL terminated C string containg list of cpu indices
 * \param output if non NULL then this array is populated with the cpu indices
 * in the order they appear on the line.
 *
 * Input format is a comma-separated list of decimal numbers and/or ranges.
 * Ranges are two hyphen-separated decimal numbers indicating a closed range
 *
 * \returns number of parsed indices or -1 on failure.
 */
int parse_delimited_list(const char *line, int *output) { const char
    *start = line; char *end = NULL; long res = 0; int count = 0;

    long i;
    int in_range = 0;
    long rangestart = LONG_MAX;

    while(*start)
    {
        res = strtol(start, &end, 10);
        if(start == end) {
            return -1;
        }

        if(in_range) 
        {
            for(i = rangestart; i <= res; ++i)
            {
                if(output) *output++ = i;
                ++count;
            }
            in_range = 0;
        }
        else
        {
            switch(*end) 
            {
                case ',':
                case '\n':
                case '\0':
                    /* Normal delimiter */
                    if(output) *output++ = res; 
                    ++count;
                    break;
                case '-':
                    /* range delimiter */
                    in_range = 1;
                    rangestart = res;
                    break;
                default:
                    fprintf(stderr, "Syntax error \'%c\'\n", *end);
                    return -1;
            }
        }

        if(*end)
            start = end + 1;
        else
            break;
    }

    return count;
}

/* This function must be called after the cpus and nodes are setup
 */
static void populate_cpunodes(archinfo_t *inst)
{
    int i,j;
    int max;

    max = 0;
    for(i = 0; i < inst->nnodes; ++i)
    {
        max = max > inst->nodes[i].count ? max : inst->nodes[i].count;
    }

    inst->ncpus_per_node = max;
    inst->cpunodes = malloc(max * sizeof *(inst->cpunodes) );

    for(i = 0; i < max; ++i)
    {
        init_cpulist(&inst->cpunodes[i], inst->nnodes);
        inst->cpunodes[i].count = 0;

        for(j = 0; j < inst->nnodes; ++j)
        {
            if(i < inst->nodes[j].count) {
                inst->cpunodes[i].index[inst->cpunodes[i].count] = inst->nodes[j].index[i];
                ++inst->cpunodes[i].count;
            }
        }
    }
}

static void retrieve_numainfo(archinfo_t *inst)
{
    struct dirent **namelist;
    int n,i;
    char buf[PATH_MAX];
    const char *basepath;
    FILE *cpulist;
    char *line;
    size_t rdlen;

    namelist = NULL;
    line = NULL;
    rdlen = 0;

    inst->nodes = NULL;

    basepath = getenv("SYSFS_MOUNT_POINT");
    if(NULL == basepath) {
        basepath = "/sys";
    }
    snprintf(buf, sizeof(buf), "%s/" SYSFS_NODE_PATH, basepath);

    n = scandir(buf, &namelist, &numanodefilter, versionsort);
    if (n <= 0)
    {
        inst->nnodes = 1;
        inst->nodes = malloc( inst->nnodes * sizeof *(inst->nodes) );
        init_cpulist(&inst->nodes[0], inst->cpus.count);
        memcpy(inst->nodes[0].index, inst->cpus.index, inst->cpus.count * sizeof(*inst->nodes[0].index));
        return;
    }

    inst->nodes = malloc(n * sizeof *(inst->nodes) );
    inst->nnodes = 0;

    for(i=0; i < n; ++i)
    {
        snprintf(buf, sizeof(buf), "%s/" SYSFS_NODE_PATH "/%s/cpulist", basepath, namelist[i]->d_name);
        cpulist = fopen(buf, "r");
        if(cpulist)
        {
            if(getline(&line, &rdlen, cpulist) > 0)
            {
                int ncpus = parse_delimited_list(line, NULL);
                if(ncpus > 0) {
                    init_cpulist(&inst->nodes[inst->nnodes], ncpus);
                    parse_delimited_list(line, inst->nodes[inst->nnodes].index );
                    ++(inst->nnodes);
                }
            }
            fclose(cpulist);
        }

        free(namelist[i]);
    }

    free(namelist);
    free(line);

    return;
}

static void retrieve_cpuinfo(archinfo_t *inst)
{
    int i;
    long ncpus;
   
    ncpus = sysconf(_SC_NPROCESSORS_ONLN);

    if(ncpus < 0 )
    {
        fprintf(stderr, "Unable to determine number of CPUs: assuming 1\n");
        ncpus = 1;
    }

    init_cpulist(&inst->cpus, ncpus);
    
    for(i = 0; i < ncpus; ++i)
    {
        inst->cpus.index[i] = i;
    }
}

archinfo_t *get_architecture()
{
    archinfo_t *ret; 

    ret = malloc( sizeof *ret);

    if(ret)
    {
        retrieve_cpuinfo(ret);
        retrieve_numainfo(ret);
        populate_cpunodes(ret);
    }

    return ret;
}

void free_architecture(archinfo_t *del)
{
    int i;

    if(NULL == del)
        return;

    free_cpulist(&del->cpus);
    for(i = 0; i < del->nnodes; ++i)
    {
        free_cpulist(&del->nodes[i]);
    }
    free(del->nodes);

    for(i = 0; i < del->ncpus_per_node; ++i)
    {
        free_cpulist(&del->cpunodes[i]);
    }
    free(del->cpunodes);
}
