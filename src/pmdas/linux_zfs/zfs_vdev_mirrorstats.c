#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_vdev_mirrorstats.h"

void
zfs_vdev_mirrorstats_fetch(zfs_vdev_mirrorstats_t *vdev_mirrorstats, regex_t *rgx_row)
{
        int len_mn, len_mv, nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/vdev_mirror_stats";
	FILE *fp;
        size_t len = 0;

        fp = fopen(fname, "r");
	if (fp != NULL) {
		while (getline(&line, &len, fp) != -1) {
                        if (regexec(rgx_row, line, nmatch, pmatch, 0) == 0) {
                                len_mn = pmatch[1].rm_eo - pmatch[1].rm_so + 1;
                                len_mv = pmatch[2].rm_eo - pmatch[2].rm_so + 1;
                                mname = (char *) malloc((size_t) (len_mn + 1) * sizeof(char));
                                mval  = (char *) malloc((size_t) (len_mv + 1) * sizeof(char));
                                strncpy(mname, line + pmatch[1].rm_so, len_mn);
                                strncpy(mval,  line + pmatch[2].rm_so, len_mv);
                                mname[len_mn] = '\0';
                                mval[len_mv] = '\0';
				if (strcmp(mname, "rotating_linear")) vdev_mirrorstats->rotating_linear = atoi(mval);
				else if (strcmp(mname, "rotating_offset")) vdev_mirrorstats->rotating_offset = atoi(mval);
				else if (strcmp(mname, "rotating_seek")) vdev_mirrorstats->rotating_seek = atoi(mval);
				else if (strcmp(mname, "non_rotating_linear")) vdev_mirrorstats->non_rotating_linear = atoi(mval);
				else if (strcmp(mname, "non_rotating_seek")) vdev_mirrorstats->non_rotating_seek = atoi(mval);
				else if (strcmp(mname, "preferred_found")) vdev_mirrorstats->preferred_found = atoi(mval);
				else if (strcmp(mname, "preferred_not_found")) vdev_mirrorstats->preferred_not_found = atoi(mval);
                        }
                        free(mname);
                        free(mval);
                }
        }
        fclose(fp);
}
