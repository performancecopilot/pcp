/*
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 *
 * This test emulates a client which directly overwrites the
 * MMV data file without unlinking and recreating it (Parfait
 * does this) - we're relying on the generation number change
 * as the mechanism available to pmdammv to detect this.
 *
 * We use this test program in conjunction with a traditional
 * (C) MMV API client (like mmv_genstats.c) to do this test.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>
#include <pcp/impl.h>

static mmv_disk_header_t hdr = {
    .magic = "MMV",
    .version = MMV_VERSION,
    .tocs = 2,
};

static mmv_disk_toc_t toc[] = {
    { .type = MMV_TOC_METRICS, .count = 1, },
    { .type = MMV_TOC_VALUES,  .count = 1, },
};

static mmv_disk_metric_t metric = {
    .name = "noinit",
    .item = 1,
    .type = MMV_TYPE_U32,
    .semantics = MMV_SEM_COUNTER,
    .dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
};

static mmv_disk_value_t values;

static size_t
mmv_filesize(void)
{
    return  sizeof(mmv_disk_header_t) +
	    sizeof(mmv_disk_toc_t) * 2 +
	    sizeof(mmv_disk_metric_t) +
	    sizeof(mmv_disk_value_t);
}

static void *
mmv_noinit(const char *filename)
{
    char path[MAXPATHLEN];
    size_t size = mmv_filesize();
    int fd, sep = __pmPathSeparator();
    void *addr = NULL;
    int sts;

    snprintf(path, sizeof(path), "%s%c" "mmv" "%c%s",
		pmGetConfig("PCP_TMP_DIR"), sep, sep, filename);

    fd = open(path, O_RDWR, 0644);
    if (fd < 0) {
	fprintf(stderr, "noinit: failed to open %s: %s\n",
		path, strerror(errno));
	return NULL;
    }
    sts = ftruncate(fd, size);
    if (sts != 0) {
	fprintf(stderr, "Error: ftruncate() returns %d\n", sts);
	exit(1);
    }
    addr = __pmMemoryMap(fd, size, 1);

    close(fd);
    return addr;
}

static __uint64_t
mmv_generation(void)
{
    struct timeval now;
    __uint32_t gen1, gen2;

    gettimeofday(&now, NULL);
    gen1 = now.tv_sec;
    gen2 = now.tv_usec;
    return (((__uint64_t)gen1 << 32) | (__uint64_t)gen2);
}

int 
main(int ac, char * av[])
{
    char * file = (ac > 1) ? av[1] : "test";
    void * addr = mmv_noinit(file);
    char * data = addr;
    __uint64_t gennum = mmv_generation();

    if (!addr) {
	perror("noinit");
	return 1;
    }

    /*
     * Write ondisk structures directly into file.
     * fixing up offsets, etc, as we go.
     */
    hdr.g1 = gennum;
    hdr.process = getpid();
    memcpy(data, &hdr, sizeof(hdr));

    data += sizeof(hdr);
    toc[0].offset = sizeof(hdr) + sizeof(toc);
    toc[1].offset = sizeof(hdr) + sizeof(toc) + sizeof(metric);
    memcpy(data, &toc, sizeof(toc));

    data += sizeof(toc);
    memcpy(data, &metric, sizeof(metric));

    values.value.ul = 42;
    values.metric = sizeof(hdr) + sizeof(toc);
    data += sizeof(metric);
    memcpy(data, &values, sizeof(values));

    /* "unlock" the file and bail out */
    hdr.g2 = (__uint64_t) gennum;
    memcpy(addr, &hdr, sizeof(hdr));
    __pmMemoryUnmap(addr, mmv_filesize());
    return 0;
}
