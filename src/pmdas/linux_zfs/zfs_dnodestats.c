#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#include "zfs_dnodestats.h"

void
zfs_dnodestats_refresh(zfs_dnodestats_t *dnodestats, regex_t *rgx_row)
{
        int len_mn, len_mv, nmatch = 3;
        regmatch_t pmatch[3];
        char *line, *mname, *mval;
	char *fname = "/proc/spl/kstat/zfs/dnodestats";
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
				if (strcmp(mname, "dnode_hold_dbuf_hold")) dnodestats->hold_dbuf_hold = atoi(mval);
				else if (strcmp(mname, "dnode_hold_dbuf_read")) dnodestats->hold_dbuf_read = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_hits")) dnodestats->hold_alloc_hits = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_misses")) dnodestats->hold_alloc_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_interior")) dnodestats->hold_alloc_interior = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_lock_retry")) dnodestats->hold_alloc_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_lock_misses")) dnodestats->hold_alloc_lock_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_alloc_type_none")) dnodestats->hold_alloc_type_none = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_hits")) dnodestats->hold_free_hits = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_misses")) dnodestats->hold_free_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_lock_misses")) dnodestats->hold_free_lock_misses = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_lock_retry")) dnodestats->hold_free_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_overflow")) dnodestats->hold_free_overflow = atoi(mval);
				else if (strcmp(mname, "dnode_hold_free_refcount")) dnodestats->hold_free_refcount = atoi(mval);
				else if (strcmp(mname, "dnode_free_interior_lock_retry")) dnodestats->free_interior_lock_retry = atoi(mval);
				else if (strcmp(mname, "dnode_allocate")) dnodestats->allocate = atoi(mval);
				else if (strcmp(mname, "dnode_reallocate")) dnodestats->reallocate = atoi(mval);
				else if (strcmp(mname, "dnode_buf_evict")) dnodestats->buf_evict = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_next_chunk")) dnodestats->alloc_next_chunk = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_race")) dnodestats->alloc_race = atoi(mval);
				else if (strcmp(mname, "dnode_alloc_next_block")) dnodestats->alloc_next_block = atoi(mval);
				else if (strcmp(mname, "dnode_move_invalid")) dnodestats->move_invalid = atoi(mval);
				else if (strcmp(mname, "dnode_move_recheck1")) dnodestats->move_recheck1 = atoi(mval);
				else if (strcmp(mname, "dnode_move_recheck2")) dnodestats->move_recheck2 = atoi(mval);
				else if (strcmp(mname, "dnode_move_special")) dnodestats->move_special = atoi(mval);
				else if (strcmp(mname, "dnode_move_handle")) dnodestats->move_handle = atoi(mval);
				else if (strcmp(mname, "dnode_move_rwlock")) dnodestats->move_rwlock = atoi(mval);
				else if (strcmp(mname, "dnode_move_active")) dnodestats->move_active = atoi(mval);
                        }
                        free(mname);
                        free(mval);
                }
        }
        fclose(fp);
}
