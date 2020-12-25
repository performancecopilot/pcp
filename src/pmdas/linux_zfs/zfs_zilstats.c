#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_zilstats.h"

void
zfs_zilstats_refresh(zfs_zilstats_t *zilstats, regex_t *rgx_row)
{
    int len_mn, len_mv;
    size_t nmatch = 3;
    regmatch_t pmatch[3];
    char *line = NULL, *mname, *mval;
    int lineno = 0;
    static int seen_err = 0;
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "zil") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            lineno++;
            if (regexec(rgx_row, line, nmatch, pmatch, 0) == 0) {
                if (pmatch[2].rm_so == -1 || pmatch[1].rm_so == -1) {
                    if (!seen_err) {
                        fprintf(stderr, "%s[%d]: regexec botch \\1: %d..%d \\2: %d..%d line: %s\n",
                            fname, lineno, pmatch[1].rm_so, pmatch[1].rm_eo,
                            pmatch[2].rm_so, pmatch[2].rm_eo, line);
                        seen_err = 1;
                    }
                }
                else {
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
            free(line);
            line = NULL;
            len = 0;
        }
        free(line);
        fclose(fp);
    }
}
