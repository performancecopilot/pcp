#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_dmu_tx.h"

void
zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "dmu_tx") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "dmu_tx_assigned") == 0) dmu_tx->assigned = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_delay") == 0) dmu_tx->delay = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_error") == 0) dmu_tx->error = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_suspended") == 0) dmu_tx->suspended = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_group") == 0) dmu_tx->group = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_memory_reserve") == 0) dmu_tx->memory_reserve = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_memory_reclaim") == 0) dmu_tx->memory_reclaim = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_dirty_throttle") == 0) dmu_tx->dirty_throttle = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_dirty_delay") == 0) dmu_tx->dirty_delay = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_dirty_over_max") == 0) dmu_tx->dirty_over_max = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_dirty_frees_delay") == 0) dmu_tx->dirty_frees_delay = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dmu_tx_quota") == 0) dmu_tx->quota = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
