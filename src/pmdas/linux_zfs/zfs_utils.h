#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

extern char ZFS_DEFAULT_PATH[MAXPATHLEN];
extern char ZFS_PATH[MAXPATHLEN];

int zfs_stats_file_check(char *fname, char *sname);
