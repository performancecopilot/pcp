#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "zfs_poolstats.h"

void
zfs_pools_init(zfs_poolstats *poolstats, pmdaInstid *pools, pmdaIndom *indomtab)
{
        DIR *zfs_dp;
        struct dirent *ep;
        int pool_num = 0;
        size_t size;

        // Discover the pools by looking for directories in /proc/spl/kstat/zfs
        if ((zfs_dp = opendir(ZFS_PROC_DIR)) != NULL) {
                while (ep = readdir(zfs_dp)) {
                        if (ep->d_type == DT_DIR) {
                                if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
                                        continue;
                                else {
                                        size = (pool_num + 1) * sizeof(pmdaInstid);
                                        if ((pools = realloc(pools, size)) == NULL)
                                                pmNoMem("process", size, PM_FATAL_ERR);
                                        pools[pool_num]->i_name = malloc(strlen(ep->d_name) + 1);
                                        strcpy(pools[pool_num]->i_name, ep->d_name);
                                        pools[pool_num]->i_inst = pool_num;
                                        pool_num++;
                                }
                        }
                }
                closedir(zfs_dp);
        }
        if (pools == NULL)
                pmNotifyErr(LOG_WARNING, "no ZFS pools found, instance domain is empty.");
        indomtab[ZFS_POOLS_INDOM]->it_set = pools;
        indomtab[ZFS_POOLS_INDOM]->it_numinst = pool_num;
        poolstats = malloc(pool_num*sizeof(zfs_poolstats));
}

void
zfs_pools_clear(zfs_poolstats *poolstats, pmdaInstid *pools, pmdaIndom *indomtab)
{
        int i;

        for (i = 0; i < indomtab[ZFS_POOLS_INDOM]->it_numinst; i++) {
                free(pools[i]->i_name);
                pools[i]->i_name = NULL;
        }
        if (pools)
                free(pools);
        if (poolstats)
                free(poolstats);
        indomtab[ZFS_POOLS_INDOM]->it_set = pools = poolstats = NULL;
        indomtab[ZFS_POOLS_INDOM]->it_numinst = 0;
}

void
zfs_poolstats_refresh(zfs_poolstats *poolstats, pmdaInstid *pools, pmdaIndom *indomtab)
{
        int i, len;
        char *line, pool_dir[PATH_MAX], fname[PATH_MAX];
        FILE *fp;
        regex_t rgx_io;
        size_t nmatch = 1;
        regmatch_t pmatch[1];
        
        regcomp(&rgx_io, "^([0-9]+ ){11}[0-9]+$", REG_EXTENDED);
        for (i = 0; i < indomtab[ZFS_POOLS_INDOM]->it_numinst; i++) {
                strcpy(pool_dir, ZFS_PROC_DIR);
                strcat(pool_dir, pools[i]->i_name);
                if (stat(pool_dir, (struct stat*) NULL) != 0) {
                        // Pools setup changed, the instance domain must follow
                        zfs_pools_clear(poolstats, pools, indomtab);
                        zfs_pools_init(poolstats, pools, indomtab);
                        zfs_pools_refresh(poolstats, pools, indomtab);
                        regfree(&rgx_io);
                        return;
                }
                // Read the state if exists
                strcpy(pools[i]->state, "UNKNOWN");
                strcpy(fname, pool_dir);
                strcat(fname, "state");
                fp = fopen(fname, "r");
                if (fp != NULL) {
                        if (getline(&line, &len, fp) != -1) {
                                if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
                                strcpy(pools[i]->state, line);
                        }
                        fclose(fp);
                }
                // Read the IO stats
                strcpy(fname, pool_dir);
                strcat(fname, "io");
                fp = fopen(fname, "r");
                if (fp != NULL) {
                        while (getline(&line, &len, fp) != -1) {
                                if (regexec(rgx_io, line, nmatch, pmatch, 0) == 0)
                                        sscanf(line, "%u %u %u %u %u %u %u %u %u %u %u %u",
					&pools[i]->nread,
 					&pools[i]->nwritten,
 					&pools[i]->reads,
 					&pools[i]->writes,
 					&pools[i]->wtime,
 					&pools[i]->wlentime,
 					&pools[i]->wupdate,
 					&pools[i]->rtime,
 					&pools[i]->rlentime,
 					&pools[i]->rupdate,
 					&pools[i]->wcnt,
 					&pools[i]->rcnt);
                        }
                        fclose(fp);
                }
        }
        regfree(&rgx_io);
/
