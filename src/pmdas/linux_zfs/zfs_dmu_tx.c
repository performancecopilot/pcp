#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_dmu_tx.h"

void
zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx, regex_t *rgx_row)
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

    if (zfs_stats_file_check(fname, "dmu_tx") != 0)
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
