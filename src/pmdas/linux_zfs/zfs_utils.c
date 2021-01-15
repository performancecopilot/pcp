#include <sys/stat.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "zfs_utils.h"

char ZFS_PATH[MAXPATHLEN];

int
zfs_stats_file_check(char *fname, char *sname)
{
    struct stat buffer;
    sprintf(fname, "%s%c%s", ZFS_PATH, pmPathSeparator(), sname);
    if (stat(fname, &buffer) != 0) {
	pmNotifyErr(LOG_WARNING, "File does not exist: %s", fname);
        return 1;
    }
    else return 0;
}


