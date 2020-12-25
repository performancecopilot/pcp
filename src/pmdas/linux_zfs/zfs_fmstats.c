#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_fmstats.h"

void
zfs_fmstats_refresh(zfs_fmstats_t *fmstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "fm") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "erpt-dropped") == 0) fmstats->erpt_dropped = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "erpt-set-failed") == 0) fmstats->erpt_set_failed = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "fmri-set-failed") == 0) fmstats->fmri_set_failed = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "payload-set-failed") == 0) fmstats->payload_set_failed = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
