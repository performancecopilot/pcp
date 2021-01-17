#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_xuiostats.h"

void
zfs_xuiostats_refresh(zfs_xuiostats_t *xuiostats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "xuio_stats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "onloan_read_buf") == 0) xuiostats->onloan_read_buf = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "onloan_write_buf") == 0) xuiostats->onloan_write_buf = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "read_buf_copied") == 0) xuiostats->read_buf_copied = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "read_buf_nocopy") == 0) xuiostats->read_buf_nocopy = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "write_buf_copied") == 0) xuiostats->write_buf_copied = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "write_buf_nocopy") == 0) xuiostats->write_buf_nocopy = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
