/*
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

int
main(int argc, char **argv)
{
    int			sts;
    struct statvfs	vbuf;
    struct statfs	buf;

    while (--argc > 0) {
	printf("%s N (statfs) [N (statvfs)]:\n", argv[1]);
	sts = statfs(argv[1], &buf);
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
	    printf("f_frsize=%lu [%lu] ", (unsigned long)buf.f_frsize, (unsigned long)vbuf.f_frsize);
	    putchar('\n');
	    printf("f_blocks=%llu [%llu] ", (unsigned long long)buf.f_blocks, (unsigned long long)vbuf.f_blocks);
	    printf("f_bfree=%llu [%llu] ", (unsigned long long)buf.f_bfree, (unsigned long long)vbuf.f_bfree);
	    printf("f_bavail=%llu [%llu] ", (unsigned long long)buf.f_bavail, (unsigned long long)vbuf.f_bavail);
	    putchar('\n');
	    printf("f_files=%llu [%llu] ", (unsigned long long)buf.f_files, (unsigned long long)vbuf.f_files);
	    printf("f_ffree=%llu [%llu] ", (unsigned long long)buf.f_ffree, (unsigned long long)vbuf.f_ffree);
	    printf("f_favail=[%llu] ", (unsigned long long)vbuf.f_favail);
	    putchar('\n');
	    printf("f_fsid={%d,%d} [%lu] ", buf.f_fsid.__val[0], buf.f_fsid.__val[1], (unsigned long)vbuf.f_fsid);
	    putchar('\n');
	    printf("f_flag=[%lu] ", vbuf.f_flag);
	    printf("f_namelen[f_namemax]=%lu [%lu] ", (unsigned long)buf.f_namelen, (unsigned long)vbuf.f_namemax);
	    putchar('\n');
	}
	argv++;
    }

    return(0);
}
