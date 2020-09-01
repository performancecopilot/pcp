#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdint.h>

#include "zfs_fmstats.h"

void
zfs_fmstats_refresh(zfs_fmstats_t *fmstats, regex_t *rgx_row)
{
        int len_mn, len_mv;
	size_t nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/fm";
	FILE *fp;
        size_t len = 0;

        fp = fopen(fname, "r");
	if (fp != NULL) {
		while (getline(&line, &len, fp) != -1) {
                        if (regexec(rgx_row, line, nmatch, pmatch, 0) == 0) {
                                len_mn = pmatch[1].rm_eo - pmatch[1].rm_so;
                                len_mv = pmatch[2].rm_eo - pmatch[2].rm_so;
                                mname = (char *) malloc((size_t) (len_mn + 1) * sizeof(char));
                                mval  = (char *) malloc((size_t) (len_mv + 1) * sizeof(char));
                                strncpy(mname, line + pmatch[1].rm_so, len_mn);
                                strncpy(mval,  line + pmatch[2].rm_so, len_mv);
                                mname[len_mn] = '\0';
                                mval[len_mv] = '\0';
				if (strcmp(mname, "erpt-dropped") == 0) fmstats->erpt_dropped = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "erpt-set-failed") == 0) fmstats->erpt_set_failed = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "fmri-set-failed") == 0) fmstats->fmri_set_failed = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "payload-set-failed") == 0) fmstats->payload_set_failed = strtoul(mval, NULL, 0);
                                free(mname);
                                free(mval);
                        }
                }
                fclose(fp);
        }
}
