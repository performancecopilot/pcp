#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_zfetchstats.h"

void
zfs_zfetchstats_refresh(zfs_zfetchstats_t *zfetchstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "zfetchstats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "hits") == 0) zfetchstats->hits = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "misses") == 0) zfetchstats->misses = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "max_streams") == 0) zfetchstats->max_streams = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}

