#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_abdstats.h"

void
zfs_abdstats_refresh(zfs_abdstats_t *abdstats, regex_t *rgx_row)
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

    if (zfs_stats_file_check(fname, "abdstats") != 0)
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
                    if (strcmp(mname, "struct_size") == 0) abdstats->struct_size = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "linear_cnt") == 0) abdstats->linear_cnt = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "linear_data_size") == 0) abdstats->linear_data_size = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_cnt") == 0) abdstats->scatter_cnt = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_data_size") == 0) abdstats->scatter_data_size = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_chunk_waste") == 0) abdstats->scatter_chunk_waste = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_0") == 0) abdstats->scatter_order_0 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_1") == 0) abdstats->scatter_order_1 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_2") == 0) abdstats->scatter_order_2 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_3") == 0) abdstats->scatter_order_3 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_4") == 0) abdstats->scatter_order_4 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_5") == 0) abdstats->scatter_order_5 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_6") == 0) abdstats->scatter_order_6 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_7") == 0) abdstats->scatter_order_7 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_8") == 0) abdstats->scatter_order_8 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_9") == 0) abdstats->scatter_order_9 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_order_10") == 0) abdstats->scatter_order_10 = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_page_multi_chunk") == 0) abdstats->scatter_page_multi_chunk = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_page_multi_zone") == 0) abdstats->scatter_page_multi_zone = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_page_alloc_retry") == 0) abdstats->scatter_page_alloc_retry = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "scatter_sg_table_retry") == 0) abdstats->scatter_sg_table_retry = strtoul(mval, NULL, 0);
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
