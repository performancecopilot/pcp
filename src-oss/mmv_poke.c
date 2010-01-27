/*
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 * Modify values within an MMV data file for PCPQA.
 */

#include <errno.h>
#include <sys/stat.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>

void *addr;

void
usage(void)
{
    fprintf(stderr,
		"Usage: %s: [options] file\n\n"
		"Options:\n"
		"  -f flag  set flag in header (none, noprefix, process)\n"
		"  -p pid   overwrite MMV file PID with given PID\n",
	    pmProgname);
    exit(1);
}

void
write_flags(char *flags)
{
    mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;

    if (strcmp(flags, "noprefix") == 0)
	hdr->flags |= MMV_FLAG_NOPREFIX;
    if (strcmp(flags, "process") == 0)
	hdr->flags |= MMV_FLAG_PROCESS;
    if (strcmp(flags, "none") == 0)
	hdr->flags = 0;
    hdr->g1 = ++hdr->g2;
}

void
write_pid(int pid)
{
    mmv_disk_header_t *hdr = (mmv_disk_header_t *)addr;

    hdr->process = pid;
    hdr->g1 = ++hdr->g2;
}

int
main(int argc, char **argv)
{
    struct stat sbuf;
    char *file, *flags = NULL;
    int c, err = 0, pid = 0;

    __pmSetProgname(argv[0]);
    while ((c = getopt(argc, argv, "f:p:")) != EOF) {
	switch (c) {
	case 'f':
	    flags = optarg;
	    break;
	case 'p':
	    pid = atoi(optarg);
	    break;
	default:
	    err++;
	}
    }

    if (err || argc != optind + 1)
	usage();

    file = argv[optind];

    c = open(file, O_RDWR, 0644);
    if (c < 0) {
	fprintf(stderr, "Cannot open %s for writing: %s\n",
		file, strerror(errno));
	exit(1);
    }
    fstat(c, &sbuf);
    addr = __pmMemoryMap(c, sbuf.st_size, 1);
    close(c);

    if (flags)
	write_flags(flags);

    if (pid)
	write_pid(pid);

    __pmMemoryUnmap(addr, sbuf.st_size);
    exit(0);
}
