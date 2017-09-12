/*
 * Linux /proc/buddyinfo metrics cluster
 *
 * Copyright (c) 2016 Fujitsu.
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
#include "linux.h"
#include "proc_buddyinfo.h"

enum {
    eSTATE_SPACE,
    eSTATE_CHAR
};

#define SPLIT_MAX 128

static int MAX_ORDER = -1; /* maximum number of page order (This value is determined by kernel config) */

int
node_name_check(const char* data)
{
    int i;
    int len = strlen(data);
    for (i = 0; i < len; i++) {
        if (data[i] == ' ') {
	    return i;
        }
    }
    return len;
}

static int read_node_name(const char *data, char *buf)
{
    int i, j;
    int len = strlen(data);
    char tmp_buf[64];
    int start;
    int end;
    for (i = 0; i < len; i++) {
        if (data[i] == ',') {
            tmp_buf[i] = '\0';
            break;
        }
        tmp_buf[i] = data[i];
	if (data[i] == 'N') {
	    tmp_buf[i] = 'n';
	}
    }

    start = node_name_check(tmp_buf);
    end = strlen(tmp_buf);
    for (i = 0, j = 0; i < end; i++) {
	if (i == start)
	    continue;
	buf[j] = tmp_buf[i];
	j++;
    }
    buf[end-1] = '\0';
    return i;
}

static int read_buddyinfo(const char *data, char (*buf)[SPLIT_MAX], int max)
{
    int index = 0;
    int n = 0;
    int i = 0;
    int len = strlen(data);
    for (; index < len; index++)
        if (data[index] != ' ')
            break;
    int state = eSTATE_CHAR;/* before charactor is space or not */
    for (; index < len; index++) {
        switch(state) {
        case eSTATE_CHAR:
            if(data[index] == ' ') {
                state=eSTATE_SPACE;
                if(n < max)
                    buf[n][i] = '\0';
                i = 0;
                n++;
            } else {
                if (n < max)
                    buf[n][i] = data[index];
                i++;
            }
            break;
        case eSTATE_SPACE:
            if (data[index] != ' ') {
                state = eSTATE_CHAR;
                index--;
            }
            break;
        }
    }
    if (n < max)
        buf[n][i] = '\0';
    return n+1;
}

int
refresh_proc_buddyinfo(proc_buddyinfo_t *proc_buddyinfo)
{
    int i, j;
    char buf[2048];
    char read_buf[SPLIT_MAX][128];
    FILE *fp;
    static int next_id = -1;

    if (next_id < 0) {
        next_id = 0;
        proc_buddyinfo->nbuddys = 0;
        if ((fp = linux_statsfile("/proc/buddyinfo", buf, sizeof(buf))) == NULL)
            return -oserror();
        if (fgets(buf,sizeof(buf),fp) == NULL) { /* read first line */
            fclose(fp);
            return -oserror();
        }
        fclose(fp);
        MAX_ORDER = read_buddyinfo(buf,read_buf,0) - 5; /* get maximum page order */
    }

    if ((fp = linux_statsfile("/proc/buddyinfo", buf, sizeof(buf))) == NULL)
        return -oserror();

    while (fgets(buf,sizeof(buf),fp) != NULL) {
        char node_name[64];
        char *zone_name;
        int values[SPLIT_MAX];
        i = read_node_name(buf, node_name);
        i+=6; /* erase ", zone" */
        read_buddyinfo(buf+i, read_buf, MAX_ORDER+1); /* read zone name and page order */
        zone_name=read_buf[0];
        for (i=0; i < MAX_ORDER; i++)
            values[i] = atoi(read_buf[i+1]);
        for (i=0; i < proc_buddyinfo->nbuddys; i++) {
            if (strcmp(proc_buddyinfo->buddys[i].node_name, node_name)==0 && strcmp(proc_buddyinfo->buddys[i].zone_name, zone_name)==0 )
                break;
        }
        if (i==proc_buddyinfo->nbuddys) {
            proc_buddyinfo->nbuddys += MAX_ORDER;
            proc_buddyinfo->buddys = (buddyinfo_t *)realloc(proc_buddyinfo->buddys, proc_buddyinfo->nbuddys * sizeof(buddyinfo_t));
            for (j=0; j < MAX_ORDER; j++) {
                proc_buddyinfo->buddys[i+j].id = next_id++;
                strncpy(proc_buddyinfo->buddys[i+j].node_name, node_name,
			sizeof(proc_buddyinfo->buddys[i+j].node_name) - 1);
                strncpy(proc_buddyinfo->buddys[i+j].zone_name, zone_name,
			sizeof(proc_buddyinfo->buddys[i+j].zone_name) - 1);
                pmsprintf(proc_buddyinfo->buddys[i+j].id_name,
			 sizeof(proc_buddyinfo->buddys[i+j].id_name),
			 "%s::order%u::%s", zone_name, j, node_name);
            }
        }
        /* update data */
        for (j=0; j < MAX_ORDER; j++) {
            proc_buddyinfo->buddys[i+j].value = values[j];
        }
    }
    fclose(fp);

    /* refresh buddyinfo indom */
    if (proc_buddyinfo->indom->it_numinst != proc_buddyinfo->nbuddys) {
        proc_buddyinfo->indom->it_numinst = proc_buddyinfo->nbuddys;
        proc_buddyinfo->indom->it_set = (pmdaInstid *)realloc(proc_buddyinfo->indom->it_set,
                proc_buddyinfo->nbuddys * sizeof(pmdaInstid));
        memset(proc_buddyinfo->indom->it_set, 0, proc_buddyinfo->nbuddys * sizeof(pmdaInstid));
    }
    for (i=0; i < proc_buddyinfo->nbuddys; i++) {
        proc_buddyinfo->indom->it_set[i].i_inst = proc_buddyinfo->buddys[i].id;
        proc_buddyinfo->indom->it_set[i].i_name = proc_buddyinfo->buddys[i].id_name;
    }

    return 0;
}
