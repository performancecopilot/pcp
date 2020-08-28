#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_xuiostats.h"

void
zfs_xuiostats_refresh(zfs_xuiostats_t *xuiostats, regex_t *rgx_row)
{
        int len_mn, len_mv, nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/xuio_stats";
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
				if (strcmp(mname, "onloan_read_buf")) xuiostats->onloan_read_buf = atoi(mval);
				else if (strcmp(mname, "onloan_write_buf")) xuiostats->onloan_write_buf = atoi(mval);
				else if (strcmp(mname, "read_buf_copied")) xuiostats->read_buf_copied = atoi(mval);
				else if (strcmp(mname, "read_buf_nocopy")) xuiostats->read_buf_nocopy = atoi(mval);
				else if (strcmp(mname, "write_buf_copied")) xuiostats->write_buf_copied = atoi(mval);
				else if (strcmp(mname, "write_buf_nocopy")) xuiostats->write_buf_nocopy = atoi(mval);
                        }
                        free(mname);
                        free(mval);
                }
        }
        fclose(fp);
}
