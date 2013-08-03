/*
 * XFS Filesystem Cluster
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2004,2007 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/vfs.h>
#include <sys/quota.h>

#define XQM_CMD(x)	(('X'<<8)+(x))	/* note: forms first QCMD argument */
#define XQM_COMMAND(x)	(((x) & (0xff<<8)) == ('X'<<8))	/* test if for XFS */
#define XQM_PRJQUOTA	2
#define Q_XGETQUOTA	XQM_CMD(3)	/* get disk limits and usage */
#define Q_XGETQSTAT	XQM_CMD(5)	/* get quota subsystem status */
#define Q_XQUOTASYNC	XQM_CMD(7)	/* delalloc flush, updates dquots */

#define XFS_QUOTA_PDQ_ACCT	(1<<4)	/* project quota accounting */
#define XFS_QUOTA_PDQ_ENFD	(1<<5)	/* project quota limits enforcement */

#define FS_QSTAT_VERSION	1	/* fs_quota_stat.qs_version */

/*
 * Some basic information about 'quota files'.
 */
typedef struct fs_qfilestat {
    uint64_t	qfs_ino;	/* inode number */
    uint64_t	qfs_nblks;	/* number of BBs 512-byte-blks */
    uint32_t	qfs_nextents;	/* number of extents */
} fs_qfilestat_t;

typedef struct fs_quota_stat {
    char		qs_version;	/* version number for future changes */
    uint16_t		qs_flags;	/* XFS_QUOTA_{U,P,G}DQ_{ACCT,ENFD} */
    char		qs_pad;		/* unused */
    fs_qfilestat_t	qs_uquota;	/* user quota storage information */
    fs_qfilestat_t	qs_gquota;	/* group quota storage information */
    uint32_t	  	qs_incoredqs;	/* number of dquots incore */
    int32_t		qs_btimelimit;	/* limit for blks timer */
    int32_t		qs_itimelimit;	/* limit for inodes timer */
    int32_t		qs_rtbtimelimit;/* limit for rt blks timer */
    uint16_t		qs_bwarnlimit;	/* limit for num warnings */
    uint16_t		qs_iwarnlimit;	/* limit for num warnings */
} fs_quota_stat_t;

#define FS_DQUOT_VERSION        1       /* fs_disk_quota.d_version */
typedef struct fs_disk_quota {
    char		d_version;      /* version of this structure */
    char		d_flags;        /* XFS_{USER,PROJ,GROUP}_QUOTA */
    uint16_t		d_fieldmask;    /* field specifier */
    uint32_t		d_id;           /* user, project, or group ID */
    uint64_t		d_blk_hardlimit;/* absolute limit on disk blks */
    uint64_t		d_blk_softlimit;/* preferred limit on disk blks */
    uint64_t		d_ino_hardlimit;/* maximum # allocated inodes */
    uint64_t		d_ino_softlimit;/* preferred inode limit */
    uint64_t		d_bcount;       /* # disk blocks owned by the user */
    uint64_t		d_icount;       /* # inodes owned by the user */
    int32_t		d_itimer;       /* zero if within inode limits */
    int32_t		d_btimer;       /* similar to above; for disk blocks */
    uint16_t		d_iwarns;       /* # warnings issued wrt num inodes */
    uint16_t		d_bwarns;       /* # warnings issued wrt disk blocks */
    int32_t		d_padding2;     /* padding2 - for future use */
    uint64_t		d_rtb_hardlimit;/* absolute limit on realtime blks */
    uint64_t		d_rtb_softlimit;/* preferred limit on RT disk blks */
    uint64_t		d_rtbcount;     /* # realtime blocks owned */
    int32_t		d_rtbtimer;     /* similar to above; for RT disk blks */
    uint16_t		d_rtbwarns;     /* # warnings issued wrt RT disk blks */
    int16_t		d_padding3;     /* padding3 - for future use */
    char		d_padding4[8];  /* yet more padding */
} fs_disk_quota_t;

typedef struct project {
    int32_t	  space_time_left;	/* seconds */
    int32_t	  files_time_left;	/* seconds */
    uint64_t	  space_hard;		/* blocks */
    uint64_t	  space_soft;		/* blocks */
    uint64_t	  space_used;		/* blocks */
    uint64_t	  files_hard;
    uint64_t	  files_soft;
    uint64_t	  files_used;
} project_t;

/* Values for flags in filesys_t */
#define FSF_FETCHED		(1U << 0)
#define FSF_QUOT_PROJ_ACC	(1U << 1)
#define FSF_QUOT_PROJ_ENF	(1U << 2)

typedef struct filesys {
    int		  id;
    unsigned int  flags;
    char	  *device;
    char	  *path;
    char	  *options;
    struct statfs stats;
} filesys_t;

extern int refresh_filesys(pmInDom, pmInDom);
extern char *scan_filesys_options(const char *, const char *);
