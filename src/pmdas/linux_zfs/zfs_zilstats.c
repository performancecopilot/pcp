#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_zilstats.h"

void
zfs_zilstats_refresh(zfs_zilstats_t *zilstats, regex_t *rgx_row)
{
        int len_mn, len_mv;
	size_t nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/zilstats";
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
				if (strcmp(mname, "zil_commit_count") == 0) zilstats->commit_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_commit_writer_count") == 0) zilstats->commit_writer_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_count") == 0) zilstats->itx_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_indirect_count") == 0) zilstats->itx_indirect_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_indirect_bytes") == 0) zilstats->itx_indirect_bytes = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_copied_count") == 0) zilstats->itx_copied_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_copied_bytes") == 0) zilstats->itx_copied_bytes = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_needcopy_count") == 0) zilstats->itx_needcopy_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_needcopy_bytes") == 0) zilstats->itx_needcopy_bytes = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_metaslab_normal_count") == 0) zilstats->itx_metaslab_normal_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_metaslab_normal_bytes") == 0) zilstats->itx_metaslab_normal_bytes = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_metaslab_slog_count") == 0) zilstats->itx_metaslab_slog_count = strtoul(mval, NULL, 0);
				else if (strcmp(mname, "zil_itx_metaslab_slog_bytes") == 0) zilstats->itx_metaslab_slog_bytes = strtoul(mval, NULL, 0);
                                free(mname);
                                free(mval);
                        }
                }
                fclose(fp);
        }
}
