/*
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 *
 * Note on NetBSD - at least in NetBSD 6.1.5 (vm09) circa Nov 2015,
 * statfs() is defined in libc.so but there is no prototype anywhere
 * below /usr/include ... I suspect this is some sort of backwards
 * compatibility hook for statfs() as per the statvfs(3) man page ...
 *	The statvfs(), statvfs1(), fstatvfs(), and fstatvfs1()
 *	functions first appeared in NetBSD 3.0 to replace the
 *	statfs() family of functions which first appeared in 4.4BSD.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pcp/pmapi.h>
#define build_me
#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#else
#undef build_me
#endif

#if defined(HAVE_SYS_STATFS_H)
/* Linux and Solaris style */
#include <sys/statfs.h>
#elif defined(HAVE_SYS_PARAM_H) && defined(HAVE_SYS_MOUNT_H) && !defined(IS_NETBSD)
/* FreeBSD style */
#include <sys/param.h>
#include <sys/mount.h>
#else
#undef build_me
#endif

#ifdef build_me
int
main(int argc, char **argv)
{
    int			sts;
    struct statvfs	vbuf;
    struct statfs	buf;

    while (--argc > 0) {
	printf("%s N (statfs) [N (statvfs)]:\n", argv[1]);
#if defined(IS_SOLARIS)
	sts = statfs(argv[1], &buf, 0, 0);
#else
	sts = statfs(argv[1], &buf);
#endif
	if (sts < 0) {
	    printf("Error: statfs: %s\n", strerror(errno));
	    argv++;
	    continue;
	}
	sts = statvfs(argv[1], &vbuf);
	if (sts < 0) {
	    printf("Error: statvfs: %s\n", strerror(errno));
	    argv++;
	    continue;
	}
	else {
	    printf("f_bsize=%lu [%lu] ", (unsigned long)buf.f_bsize, (unsigned long)vbuf.f_bsize);
#if defined(HAVE_SYS_STATFS_H)
	    printf("f_frsize=%lu [%lu] ", (unsigned long)buf.f_frsize, (unsigned long)vbuf.f_frsize);
#else
	    printf("f_frsize=[%lu] ", (unsigned long)vbuf.f_frsize);
#endif
	    putchar('\n');
	    printf("f_blocks=%" FMT_UINT64 " [%" FMT_UINT64 "] ", (__uint64_t)buf.f_blocks, (__uint64_t)vbuf.f_blocks);
	    printf("f_bfree=%" FMT_UINT64 " [%" FMT_UINT64 "] ", (__uint64_t)buf.f_bfree, (__uint64_t)vbuf.f_bfree);
#if !defined(IS_SOLARIS)
	    printf("f_bavail=%" FMT_UINT64 " [%" FMT_UINT64 "] ", (__uint64_t)buf.f_bavail, (__uint64_t)vbuf.f_bavail);
#endif
	    putchar('\n');
	    printf("f_files=%" FMT_UINT64 " [%" FMT_UINT64 "] ", (__uint64_t)buf.f_files, (__uint64_t)vbuf.f_files);
	    printf("f_ffree=%" FMT_UINT64 " [%" FMT_UINT64 "] ", (__uint64_t)buf.f_ffree, (__uint64_t)vbuf.f_ffree);
#if !defined(IS_SOLARIS)
	    printf("f_favail=[%" FMT_UINT64 "] ", (__uint64_t)vbuf.f_favail);
#endif
	    putchar('\n');
#if defined(IS_SOLARIS)
	    /* no f_fsid field */
#elif defined(HAVE_SYS_STATFS_H)
	    printf("f_fsid={%d,%d} [%lu] ", buf.f_fsid.__val[0], buf.f_fsid.__val[1], (unsigned long)vbuf.f_fsid);
#else
	    printf("f_fsid=[%lu] ", (unsigned long)vbuf.f_fsid);
#endif
	    putchar('\n');
	    printf("f_flag=[%lu] ", vbuf.f_flag);
#if defined(IS_SOLARIS)
	    /* no f_name[len,max] fields */
#elif defined(HAVE_SYS_STATFS_H)
	    printf("f_namelen[f_namemax]=%lu [%lu] ", (unsigned long)buf.f_namelen, (unsigned long)vbuf.f_namemax);
#else
	    printf("f_namemax=[%lu] ", (unsigned long)vbuf.f_namemax);
#endif
	    putchar('\n');
	}
	argv++;
    }

    return(0);
}
#else
int
main(int argc, char **argv)
{
    printf("No statvfs() and/or statfs() on this platform\n");
    return(1);
}
#endif
