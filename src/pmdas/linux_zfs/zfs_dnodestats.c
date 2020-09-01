#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_dnodestats.h"

void
zfs_dnodestats_refresh(zfs_dnodestats_t *dnodestats, regex_t *rgx_row)
{
        int len_mn, len_mv;
	size_t nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/dnodestats";
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
				if (strcmp(mname, "dnode_hold_dbuf_hold") == 0) dnodestats->hold_dbuf_hold = atoi(mval);
				else if (strcmp(mname, "dnode_hold_dbuf_read") == 0) dnodestats->hold_dbuf_read = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_hits") == 0) dnodestats->hold_alloc_hits = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_misses") == 0) dnodestats->hold_alloc_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_interior") == 0) dnodestats->hold_alloc_interior = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_lock_retry") == 0) dnodestats->hold_alloc_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_lock_misses") == 0) dnodestats->hold_alloc_lock_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_type_none") == 0) dnodestats->hold_alloc_type_none = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_hits") == 0) dnodestats->hold_free_hits = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_misses") == 0) dnodestats->hold_free_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_lock_misses") == 0) dnodestats->hold_free_lock_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_lock_retry") == 0) dnodestats->hold_free_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_overflow") == 0) dnodestats->hold_free_overflow = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_refcount") == 0) dnodestats->hold_free_refcount = atoi(mval);
				else if (strcmp(mname, "dnode_free_interior_lock_retry") == 0) dnodestats->free_interior_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_allocate") == 0) dnodestats->allocate = atoi(mval);
				else if (strcmp(mname, "dnode_reallocate") == 0) dnodestats->reallocate = atoi(mval);
				else if (strcmp(mname, "dnode_buf_evict") == 0) dnodestats->buf_evict = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_next_chunk") == 0) dnodestats->alloc_next_chunk = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_race") == 0) dnodestats->alloc_race = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_next_block") == 0) dnodestats->alloc_next_block = atoi(mval);
				else if (strcmp(mname, "dnode_move_invalid") == 0) dnodestats->move_invalid = atoi(mval);
				else if (strcmp(mname, "dnode_move_recheck1") == 0) dnodestats->move_recheck1 = atoi(mval);
				else if (strcmp(mname, "dnode_move_recheck2") == 0) dnodestats->move_recheck2 = atoi(mval);
				else if (strcmp(mname, "dnode_move_special") == 0) dnodestats->move_special = atoi(mval);
				else if (strcmp(mname, "dnode_move_handle") == 0) dnodestats->move_handle = atoi(mval);
				else if (strcmp(mname, "dnode_move_rwlock") == 0) dnodestats->move_rwlock = atoi(mval);
				else if (strcmp(mname, "dnode_move_active") == 0) dnodestats->move_active = atoi(mval);
                                free(mname);
                                free(mval);
                        }
                }
                fclose(fp);
        }
}
