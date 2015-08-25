/*
 * Mounts PMDA, info on current tracked filesystem mounts
 *
 * Copyright (c) 2012,2015 Red Hat.
 * Copyright (c) 2001,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2001 Alan Bailey (bailey@mcs.anl.gov or abailey@ncsa.uiuc.edu) 
 * All rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#ifdef IS_SOLARIS
#define MOUNT_FILE "/etc/vfstab"
#else
#define MOUNT_FILE "/proc/mounts"
#endif
#define MAXFSTYPE	32
#define MAXOPTSTR	256

enum {	/* instance domain identifiers */
    MOUNTS_INDOM = 0,
};
enum {	/* metric cluster identifiers */
    MOUNTS_CLUSTER = 0,
};
enum {	/* metric item identifiers */
    MOUNTS_DEVICE = 0,
    MOUNTS_TYPE,
    MOUNTS_OPTIONS,
    MOUNTS_UP,
    MOUNTS_CAPACITY,
    MOUNTS_USED,
    MOUNTS_FREE,
    MOUNTS_MAXFILES,
    MOUNTS_USEDFILES,
    MOUNTS_FREEFILES,
    MOUNTS_FULL,
    MOUNTS_BLOCKSIZE,
    MOUNTS_AVAIL,
    MOUNTS_AVAILFILES,
    MOUNTS_READONLY,
};
enum {	/* internal mount states */
    MOUNTS_FLAG_UP	= 0x1,
    MOUNTS_FLAG_RO	= 0x2,
    MOUNTS_FLAG_STAT	= 0x4,
};

static pmdaInstid *mounts;

static pmdaIndom indomtab[] = {
  { MOUNTS_INDOM, 0, NULL }
};


/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric metrictab[] = {
    { NULL,	/* mounts.device */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_DEVICE),
	PM_TYPE_STRING, MOUNTS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.type */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_TYPE),
	PM_TYPE_STRING, MOUNTS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.options */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_OPTIONS),
	PM_TYPE_STRING, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.up */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_UP),
	PM_TYPE_U32, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.capacity */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_CAPACITY),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { NULL,	/* mounts.used */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_USED),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { NULL,	/* mounts.free */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_FREE),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { NULL,	/* mounts.maxfiles */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_MAXFILES),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.usedfiles */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_USEDFILES),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.freefiles */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_FREEFILES),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.full */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_FULL),
	PM_TYPE_DOUBLE, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.blocksize */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_BLOCKSIZE),
	PM_TYPE_U32, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { NULL,	/* mounts.avail */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_AVAIL),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { NULL,	/* mounts.availfiles */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_AVAILFILES),
	PM_TYPE_U64, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL,	/* mounts.readonly */
      { PMDA_PMID(MOUNTS_CLUSTER, MOUNTS_READONLY),
	PM_TYPE_U32, MOUNTS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

typedef struct mountinfo {
    __uint32_t	flags;
    __uint32_t	bsize;
    char	type[MAXFSTYPE];
    char	device[MAXPATHLEN];
    char	options[MAXOPTSTR];
    __uint64_t	capacity;
    __uint64_t	bfree;
    __uint64_t	bavail;
    __uint64_t	files;
    __uint64_t	ffree;
    __uint64_t	favail;
} mountinfo;

static mountinfo *mount_list;
static struct stat file_change;
static int isDSO = 1;
static char mypath[MAXPATHLEN];
static char *username;

static void mounts_clear_config_info(void);
static void mounts_grab_config_info(void);
static void mounts_config_file_check(void);
static void mounts_refresh_mounts(void);

static void
mounts_config_file_check(void)
{
    struct stat statbuf;
    static int  last_error;
    int sep = __pmPathSeparator();

    snprintf(mypath, sizeof(mypath), "%s%c" "mounts" "%c" "mounts.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    if (stat(mypath, &statbuf) == -1) {
	if (oserror() != last_error) {
	    last_error = oserror();
	    __pmNotifyErr(LOG_WARNING, "stat failed on %s: %s\n",
			mypath, pmErrStr(last_error));
	}
    } else {
	last_error = 0;
#if defined(HAVE_ST_MTIME_WITH_E)
	if (statbuf.st_mtime != file_change.st_mtime)
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	if (statbuf.st_mtimespec.tv_sec != file_change.st_mtimespec.tv_sec ||
	    statbuf.st_mtimespec.tv_nsec != file_change.st_mtimespec.tv_nsec)
#else     
	if (statbuf.st_mtim.tv_sec != file_change.st_mtim.tv_sec ||
	    statbuf.st_mtim.tv_nsec != file_change.st_mtim.tv_nsec)
#endif
	{
	    mounts_clear_config_info();
	    mounts_grab_config_info();
	    file_change = statbuf;
	}
    }
}

static void
mounts_clear_config_info(void)
{
    int i;

    /* Free the memory holding the mount name */
    for (i = 0; i < indomtab[MOUNTS_INDOM].it_numinst; i++) {
	free(mounts[i].i_name);
	mounts[i].i_name = NULL;
    }

    /* Free the mounts structure */
    if (mounts)
	free(mounts);

    /* Free the mount_list structure */
    if (mount_list)
	free(mount_list);

    mount_list = NULL;
    indomtab[MOUNTS_INDOM].it_set = mounts = NULL;
    indomtab[MOUNTS_INDOM].it_numinst = 0;
}

/* 
 * This routine opens the config file and stores the information in the
 * mounts structure.  The mounts structure must be reallocated as
 * necessary, and also the num_procs structure needs to be reallocated
 * as we define new mounts.  When all of that is done, we fill in the
 * values in the indomtab structure, those being the number of instances
 * and the pointer to the mounts structure.
 */
static void
mounts_grab_config_info(void)
{
    FILE *fp;
    char mount_name[MAXPATHLEN];
    char *q;
    size_t size;
    int mount_number = 0;
    int sep = __pmPathSeparator();

    snprintf(mypath, sizeof(mypath), "%s%c" "mounts" "%c" "mounts.conf",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    if ((fp = fopen(mypath, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "fopen on %s failed: %s\n",
			  mypath, pmErrStr(-oserror()));
	if (mounts) {
	    free(mounts);
	    mounts = NULL;
	    mount_number = 0;
	}
	goto done;
    }

    while (fgets(mount_name, sizeof(mount_name), fp) != NULL) {
	if (mount_name[0] == '#')
	    continue;
	/* Remove the newline */
	if ((q = strchr(mount_name, '\n')) != NULL) {
	    *q = '\0';
	} else {
	    /* This means the line was too long */
	    __pmNotifyErr(LOG_WARNING, "line %d in the config file too long\n",
			mount_number+1);
	}
	size = (mount_number + 1) * sizeof(pmdaInstid);
	if ((mounts = realloc(mounts, size)) == NULL)
	    __pmNoMem("process", size, PM_FATAL_ERR);
	mounts[mount_number].i_name = malloc(strlen(mount_name) + 1);
	strcpy(mounts[mount_number].i_name, mount_name);
	mounts[mount_number].i_inst = mount_number;
	mount_number++;
    }
    fclose(fp);

done:
    if (mounts == NULL)
	__pmNotifyErr(LOG_WARNING, "\"mounts\" instance domain is empty");
    indomtab[MOUNTS_INDOM].it_set = mounts;
    indomtab[MOUNTS_INDOM].it_numinst = mount_number;
    mount_list = realloc(mount_list, (mount_number)*sizeof(mountinfo));
}

static void
mounts_refresh_mounts(void)
{
    FILE *fp;
    struct statvfs vfs;
    struct mountinfo *mp;
    char *path, *device, *type, *options;
    char buf[BUFSIZ];
    int item;

    /* Reset all mount structures */
    for (item = 0; item < indomtab[MOUNTS_INDOM].it_numinst; item++) {
	mp = &mount_list[item];
	memset(mp, 0, sizeof(*mp));
	strcpy(mp->device, "none");
	strcpy(mp->type, "none");
	strcpy(mp->options, "none");
    }

    if ((fp = fopen(MOUNT_FILE, "r")) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((device = strtok(buf, " ")) == NULL)
            continue;
#ifdef IS_SOLARIS
	strtok(NULL, " ");	/* device_to_fsck */
#endif
	if ((path = strtok(NULL, " ")) == NULL)
	    continue;
	if ((type = strtok(NULL, " ")) == NULL)
	    continue;
#ifdef IS_SOLARIS
	strtok(NULL, " ");	/* fsck_pass */
	strtok(NULL, " ");	/* mount_at_boot */
#endif
	if ((options = strtok(NULL, " ")) == NULL)
	    continue;

	for (item = 0; item < indomtab[MOUNTS_INDOM].it_numinst; item++) {
	    mp = &mount_list[item];
	    if (strcmp(path, mounts[item].i_name) != 0)
		continue;
	    strncpy(mp->type, type, MAXFSTYPE-1);
	    strncpy(mp->device, device, MAXPATHLEN-1);
	    strncpy(mp->options, options, MAXOPTSTR-1);
	    mp->flags = MOUNTS_FLAG_UP;
	    if (statvfs(path, &vfs) < 0)
		continue;
	    mp->flags |= MOUNTS_FLAG_STAT;
	    if (vfs.f_flag & ST_RDONLY)
		mp->flags |= MOUNTS_FLAG_RO;
	    mp->capacity = (vfs.f_blocks * vfs.f_frsize) / vfs.f_bsize;
	    mp->bsize = vfs.f_bsize;
	    mp->bfree = vfs.f_bfree;
	    mp->bavail = vfs.f_bavail;
	    mp->files = vfs.f_files;
	    mp->ffree = vfs.f_ffree;
	    mp->favail = vfs.f_favail;
	}
    }
    fclose(fp);
}

/*
 * This is the wrapper over the pmdaFetch routine, to handle the problem
 * of varying instance domains.  All this does is delete the previous
 * mount list, and then get the current one, by calling
 * mounts_refresh_mounts.
 */
static int
mounts_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    mounts_config_file_check();
    mounts_refresh_mounts();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
mounts_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    __uint64_t	ull, used;
    mountinfo	*mp;

    if (idp->cluster != MOUNTS_CLUSTER)
	return PM_ERR_PMID;
    if (inst >= indomtab[MOUNTS_INDOM].it_numinst)
	return PM_ERR_INST;
    mp = &mount_list[inst];

    switch (idp->item) {
    case MOUNTS_DEVICE:
	atom->cp = mp->device;
	break;
    case MOUNTS_TYPE:
	atom->cp = mp->type;
	break;
    case MOUNTS_OPTIONS:
	atom->cp = mp->options;
	break;
    case MOUNTS_UP:
	atom->ul = (mp->flags & MOUNTS_FLAG_UP) ? 1 : 0;
	break;
    case MOUNTS_CAPACITY:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->capacity * mp->bsize / 1024;
	break;
    case MOUNTS_USED:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = (mp->capacity - mp->bfree) * mp->bsize / 1024;
	break;
    case MOUNTS_FREE:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->bfree * mp->bsize / 1024;
	break;
    case MOUNTS_MAXFILES:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->files;
	break;
    case MOUNTS_USEDFILES:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->files - mp->ffree;
	break;
    case MOUNTS_FREEFILES:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->ffree;
	break;
    case MOUNTS_FULL:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	used = (__uint64_t)(mp->capacity - mp->bfree);
	ull = used + (__uint64_t)mp->bavail;
	atom->d = (100.0 * (double)used) / (double)ull;
	break;
    case MOUNTS_BLOCKSIZE:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ul = mp->bsize;
	break;
    case MOUNTS_AVAIL:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->bavail * mp->bsize / 1024;
	break;
    case MOUNTS_AVAILFILES:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ull = mp->favail;
	break;
    case MOUNTS_READONLY:
	if ((mp->flags & MOUNTS_FLAG_STAT) == 0)
	    return PM_ERR_AGAIN;
	atom->ul = (mp->flags & MOUNTS_FLAG_RO) ? 1 : 0;
	break;
    default:
	return PM_ERR_PMID;
    }
    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
mounts_init(pmdaInterface *dp)
{
    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "mounts" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_2, "mounts DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
        return;

    dp->version.two.fetch = mounts_fetch;
    pmdaSetFetchCallBack(dp, mounts_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    /* Let's grab the info right away just to make sure it's there. */
    mounts_grab_config_info();
}

static pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions	opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "mounts" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, MOUNTS,
		"mounts.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    mounts_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
